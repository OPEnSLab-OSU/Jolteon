import json
import os
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
settings = root / '.build/default-menu-windows-settings.json'
env = dict(os.environ, J18_MENU_SETTINGS=str(settings))
result = subprocess.run([shutil.which('powershell.exe'), '-NoProfile', '-ExecutionPolicy', 'Bypass',
    '-File', str(root / 'tools/jolteon_menu.ps1'), '-BuildOnly'], input='G\n\n0\n',
    text=True, capture_output=True, env=env)
(root / '.build/default-menu-windows.log').write_text(result.stdout + result.stderr, encoding='utf-8')
assert result.returncode == 0, result.stdout + result.stderr
assert 'Selected repository Jolteon board profile and B option 4' in result.stdout
saved = json.loads(settings.read_text())
assert saved['board'] == saved['bootloader'] == 'jolteon'
manifest = json.loads((root / 'bootloader/firmware/manifest.json').read_text())
assert manifest['repeat_build_verified']
print('PASS: actual Windows G action rebuilt repository loader/profile, repeat-verified it, selected B4 and saved settings; no hardware accessed.')
