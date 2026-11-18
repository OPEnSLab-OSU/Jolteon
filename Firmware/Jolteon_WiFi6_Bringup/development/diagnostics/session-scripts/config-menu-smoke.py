import hashlib
import json
import os
from pathlib import Path
import sys
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from bootloader_config import configure, checked_target
from jolteon_menu import Engine

folder = root / '.build/config-menu-smoke/Config_Smoke'
folder.mkdir(parents=True, exist_ok=True)
(folder / 'Config_Smoke.ino').write_text('#include <Arduino.h>\nvoid setup(){pinMode(LED_BUILTIN,OUTPUT);}\nvoid loop(){digitalWrite(LED_BUILTIN,!digitalRead(LED_BUILTIN));delay(100);}\n')
tracked = [root / 'bootloader/board.json', root / 'bootloader/src/jolteon_board.h',
           root / 'bootloader/src/board_definitions_jolteon_j18.h',
           root / 'bootloader/firmware/manifest.json', root / 'bootloader/firmware/samd21_sam_ba.bin']
before = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
with patch('builtins.input', side_effect=['1', '2', 'C', '0']):
    built, source = configure(folder)
assert built
target, manifest = checked_target(folder)
assert manifest['board']['crystalless'] is True
assert manifest['repeat_build_verified'] is True
assert before == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
assert '-DCRYSTALLESS' in (target / 'arduino-profile/hardware/opens/samd/boards.txt').read_text()
os.environ['J18_ARDUINO_CLI'] = str(Path(os.environ['LOCALAPPDATA']) / 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe')
os.environ['J18_OPENOCD'] = str(Path(os.environ['ProgramFiles(x86)']) / 'OpenOCD/bin/openocd.exe')
Engine('linux').action(folder, 'target', 'combined', 'github', None, True, True, 'stlink', False)
saved = json.loads((folder / '.j18-build/sketch-build.json').read_text())
assert saved['profile_signature']
combined = (folder / '.j18-build/firmware/firmware.with_bootloader.bin').read_bytes()
boot = (target / 'firmware/samd21_sam_ba.bin').read_bytes()
assert combined[:len(boot)] == boot
assert before == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in tracked}
print('PASS: crystal-less loader and matching sketch compiled, combined image checked, default release unchanged; no hardware accessed.')
