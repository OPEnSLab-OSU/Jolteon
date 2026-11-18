import os
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import jolteon_menu as m

os.environ['J18_ARDUINO_CLI'] = str(Path(os.environ['LOCALAPPDATA']) / 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')
os.environ['J18_OPENOCD'] = r'C:\Program Files (x86)\OpenOCD\bin\openocd.exe'
fixture = ROOT / '.build/unix-menu-compile-fixture'
fixture.mkdir(exist_ok=True)
sketch = fixture / 'Unix_Menu_Blinky'
sketch.mkdir(exist_ok=True)
(sketch / 'Unix_Menu_Blinky.ino').write_bytes((ROOT / 'sketches/Jolteon_Blinky/Jolteon_Blinky.ino').read_bytes())
(sketch / 'J18BuildOverrides.h').write_bytes((ROOT / 'sketches/Jolteon_Blinky/J18BuildOverrides.h').read_bytes())
engine = m.Engine('linux')
for board in ('zero', 'jolteon'):
    engine.action(sketch, board, 'app', 'current', None, True, True, 'stlink', False)
    print('PASS real Unix backend compile for ' + board, flush=True)
engine.action(sketch, 'jolteon', 'app', 'current', None, True, False, 'stlink', False)
engine.action(sketch, 'jolteon', 'separate', 'github', None, True, False, 'stlink', False)
engine.action(sketch, 'jolteon', 'combined', 'jolteon', None, True, False, 'stlink', False)
engine.action(sketch, 'zero', 'boot', 'jolteon', None, True, False, 'stlink', False)
mac = m.Engine('mac')
mac.action(sketch, 'jolteon', 'separate', 'current', None, True, False, 'stlink', False)
app = (sketch / '.j18-build/firmware/firmware.sketch.bin').read_bytes()
assert struct.unpack_from('<I', app)[0] == 0x20007ff8
print('PASS Linux/macOS engines: both profiles, all four modes, readback config parsing, static reuse. No target access.', flush=True)
