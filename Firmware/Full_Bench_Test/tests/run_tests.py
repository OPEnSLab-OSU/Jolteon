"""Host tests of the actual rail, RTC and AT parsing policy; no hardware access."""
from pathlib import Path
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
folder = root / '.build/host'
folder.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
if sys.platform == 'win32':
    finder = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    vs = subprocess.check_output([str(finder), '-latest', '-products', '*', '-requires',
         'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vcvars = Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    command = f'"{os.environ.get("COMSPEC", "cmd.exe")}" /d /s /c ""{vcvars}" >nul && set"'
    setup = subprocess.check_output(command, text=True)
    env.update({k.upper(): v for k, v in (s.split('=', 1) for s in setup.splitlines() if '=' in s and not s.startswith('='))})
    compiler = shutil.which('cl.exe', path=env['PATH'])
    exe = folder / 'test.exe'
    args = [compiler, '/nologo', '/std:c++14', '/EHsc', '/W4', '/wd4127', '/Fe' + str(exe), '/Fo' + str(folder) + os.sep, str(root / 'tests/test_logic.cpp')]
else:
    exe = folder / 'test'
    args = ['c++', '-std=c++14', '-Wall', '-Wextra', '-o', str(exe), str(root / 'tests/test_logic.cpp')]
json_headers = Path.home() / 'AppData/Local/Arduino15/packages/loom4/hardware/samd/4.9/libraries/ArduinoJson/src'
args += [('/I' if sys.platform == 'win32' else '-I') + str(json_headers)]
subprocess.run(args, check=True, cwd=folder, env=env)
subprocess.run([str(exe)], check=True, env=env)

loom_headers = json_headers.parent.parent / 'Loom/src'
prefix = '/I' if sys.platform == 'win32' else '-I'
pixel_args = [str(root / 'tests/test_neopixel.cpp') if arg == str(root / 'tests/test_logic.cpp') else arg for arg in args]
pixel_args += [prefix + str(root / 'tests/neopixel_fakes'), prefix + str(loom_headers)]
subprocess.run(pixel_args, check=True, cwd=folder, env=env)
subprocess.run([str(exe)], check=True, env=env)
data_args = [str(root / 'tests/test_sara_data.cpp') if arg == str(root / 'tests/test_neopixel.cpp') else arg for arg in pixel_args]
subprocess.run(data_args, check=True, cwd=folder, env=env)
subprocess.run([str(exe)], check=True, env=env)
rtc_args = [str(root / 'tests/test_rtc_driver.cpp') if arg == str(root / 'tests/test_logic.cpp') else arg for arg in args]
rtc_args += [prefix + str(root / 'tests/rtc_fakes'), prefix + str(json_headers.parent.parent / 'OPEnS_RTC/src')]
subprocess.run(rtc_args, check=True, cwd=folder, env=env)
subprocess.run([str(exe)], check=True, env=env)
