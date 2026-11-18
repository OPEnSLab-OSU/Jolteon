@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist .j18-build\host-tests mkdir .j18-build\host-tests
cl /nologo /EHsc /W4 /WX /D_CRT_SECURE_NO_WARNINGS /std:c++14 /Itests\host /I. %* tests\test_logic.cpp Protocol.cpp Telemetry.cpp Indicators.cpp /Fe.j18-build\host-tests\test_logic.exe /Fo.j18-build\host-tests\
if errorlevel 1 exit /b 1
.j18-build\host-tests\test_logic.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /W4 /WX /wd4127 /D_CRT_SECURE_NO_WARNINGS /std:c++14 /Itests\host /I. /DJ18_REVISED_SDI12=1 tests\test_sdi.cpp Sdi12.cpp /Fe.j18-build\host-tests\test_sdi.exe /Fo.j18-build\host-tests\
if errorlevel 1 exit /b 1
.j18-build\host-tests\test_sdi.exe
if errorlevel 1 exit /b 1
if not "%*"=="" exit /b 0
cl /nologo /c /EHsc /D_CRT_SECURE_NO_WARNINGS /std:c++14 /Itests\host /I. /DJ18_REVISED_SDI12=1 /DJ18_STOCK_BOOTLOADER_PINMAP=1 tests\test_logic.cpp /Fo.j18-build\host-tests\invalid_config.obj >.j18-build\host-tests\invalid_config.log 2>&1
if not errorlevel 1 (
    echo FAIL: overlapping SDI power / LTE RX profile compiled unexpectedly.
    exit /b 1
)
findstr /C:"both use PA18" .j18-build\host-tests\invalid_config.log >nul
if errorlevel 1 exit /b 1
echo PASS: overlapping SDI power / LTE RX configuration rejected at compile time.
exit /b 0
