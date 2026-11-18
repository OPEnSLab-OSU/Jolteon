import os
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from jolteon_menu import Engine
from swd_probe import run_probe

sketch = root / '.build/config-menu-smoke/Config_Smoke'
os.environ['J18_ARDUINO_CLI'] = str(Path(os.environ['LOCALAPPDATA']) / 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')
os.environ['J18_OPENOCD'] = str(Path(os.environ['ProgramFiles(x86)']) / 'OpenOCD/bin/openocd.exe')
Engine('mac').action(sketch, 'target', 'combined', 'github', None, True, True, 'stlink', False)
assert run_probe(sketch, Path(os.environ['J18_OPENOCD']), check_only=True) == 0
print('PASS: macOS engine compiles configured profile; probe preparation accepts compiled pin snapshot. No hardware accessed.')
