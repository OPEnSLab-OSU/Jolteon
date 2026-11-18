@echo off
setlocal EnableExtensions DisableDelayedExpansion

rem WT02E40E / nRF5340 sysbuild flash helper via ST-Link V2 + OpenOCD.
rem
rem Usage:
rem   flash_wt02e40e_stlink_openocd.bat
rem   flash_wt02e40e_stlink_openocd.bat C:\ncs\v3.3.1\nrf\samples\wifi\sta\build
rem
rem Expected sysbuild outputs:
rem   build\sta\zephyr\zephyr.hex                  = nRF5340 application core image
rem   build\hci_ipc\zephyr\zephyr.hex              = nRF5340 network core BLE controller image
rem   build\hci_ipc\zephyr\merged_CPUNET.hex       = network core image with CPUNET addressing, if generated
rem   build\merged.hex                             = application image; NOT proof of a dual-core merge

set "SCRIPT_DIR=%~dp0"
set "OPENOCD=openocd"

if "%~1"=="" (
    set "BUILD_DIR=%SCRIPT_DIR%..\..\.build\bridge"
) else (
    set "BUILD_DIR=%~1"
)

set "CFG=%SCRIPT_DIR%wt02e40e_nrf5340_stlink_dap.cfg"

if not exist "%CFG%" (
    echo Missing OpenOCD config:
    echo   %CFG%
    exit /b 2
)

echo.
echo === WT02E40E ST-Link/OpenOCD flash helper ===
echo BUILD_DIR = %BUILD_DIR%
echo CFG       = %CFG%
echo.

rem App core candidates.
set "APP_HEX="
if exist "%BUILD_DIR%\application.hex" set "APP_HEX=%BUILD_DIR%\application.hex"
if not defined APP_HEX if exist "%BUILD_DIR%\merged.hex" set "APP_HEX=%BUILD_DIR%\merged.hex"
if not defined APP_HEX if exist "%BUILD_DIR%\bridge\zephyr\zephyr.hex" set "APP_HEX=%BUILD_DIR%\bridge\zephyr\zephyr.hex"
if exist "%BUILD_DIR%\sta\zephyr\merged.hex" set "APP_HEX=%BUILD_DIR%\sta\zephyr\merged.hex"
if not defined APP_HEX if exist "%BUILD_DIR%\sta\zephyr\zephyr.hex" set "APP_HEX=%BUILD_DIR%\sta\zephyr\zephyr.hex"
if not defined APP_HEX if exist "%BUILD_DIR%\zephyr\zephyr.hex" set "APP_HEX=%BUILD_DIR%\zephyr\zephyr.hex"

rem Network core candidates.
set "NET_HEX="
if exist "%BUILD_DIR%\network_core.hex" set "NET_HEX=%BUILD_DIR%\network_core.hex"
if not defined NET_HEX if exist "%BUILD_DIR%\merged_CPUNET.hex" set "NET_HEX=%BUILD_DIR%\merged_CPUNET.hex"
if exist "%BUILD_DIR%\hci_ipc\zephyr\merged_CPUNET.hex" set "NET_HEX=%BUILD_DIR%\hci_ipc\zephyr\merged_CPUNET.hex"
if not defined NET_HEX if exist "%BUILD_DIR%\hci_ipc\merged_CPUNET.hex" set "NET_HEX=%BUILD_DIR%\hci_ipc\merged_CPUNET.hex"
if not defined NET_HEX if exist "%BUILD_DIR%\hci_ipc\zephyr\zephyr.hex" set "NET_HEX=%BUILD_DIR%\hci_ipc\zephyr\zephyr.hex"

if not defined APP_HEX (
    echo Could not find application core hex.
    echo Looked under:
    echo   %BUILD_DIR%\sta\zephyr\zephyr.hex
    echo   %BUILD_DIR%\sta\zephyr\merged.hex
    echo   %BUILD_DIR%\zephyr\zephyr.hex
    exit /b 3
)

if not defined NET_HEX (
    echo Could not find network core hex.
    echo BLE needs the network core image.
    echo Looked under:
    echo   %BUILD_DIR%\hci_ipc\zephyr\merged_CPUNET.hex
    echo   %BUILD_DIR%\hci_ipc\merged_CPUNET.hex
    echo   %BUILD_DIR%\hci_ipc\zephyr\zephyr.hex
    exit /b 4
)

echo Application core hex:
echo   %APP_HEX%
echo Network core hex:
echo   %NET_HEX%
echo.
echo Flash order:
echo   1. nrf53.cpunet: BLE controller / hci_ipc
echo   2. nrf53.cpuapp: WT02E40E application
echo.
if /I "%~2"=="--check" exit /b 0
set "APP_HEX=%APP_HEX:\=/%"
set "NET_HEX=%NET_HEX:\=/%"

"%OPENOCD%" -f "%CFG%" ^
    -c "init" ^
    -c "targets" ^
    -c "targets nrf53.cpunet" ^
    -c "reset init" ^
    -c "program {%NET_HEX%} verify" ^
    -c "targets nrf53.cpuapp" ^
    -c "reset init" ^
    -c "program {%APP_HEX%} verify" ^
    -c "reset run" ^
    -c "shutdown"

exit /b %ERRORLEVEL%
