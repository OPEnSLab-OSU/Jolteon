#!/usr/bin/env python3
"""Compile this Feather M0 sketch and record its inputs. Never uploads or flashes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


def source_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p) for p in ROOT.rglob('*')
            if p.is_file() and p.suffix in ('.ino', '.h', '.cpp', '.c', '.py')
            and not any(x in p.relative_to(ROOT).parts for x in ('.build', 'firmware', '__pycache__'))}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--arduino-cli', type=Path)
    p.add_argument('--arduino-data', type=Path)
    p.add_argument('--mqtt-library', type=Path, help='ArduinoMqttClient dependency required by the installed Loom sources')
    p.add_argument('--firmware-dir', type=Path, help='Save images separately from the default firmware folder')
    a = p.parse_args()
    if sys.platform == 'win32':
        data_default = Path.home() / 'AppData/Local/Arduino15'
    elif sys.platform == 'darwin':
        data_default = Path.home() / 'Library/Arduino15'
    else:
        data_default = Path.home() / '.arduino15'
    data = (a.arduino_data or data_default).resolve()
    cli = a.arduino_cli or shutil.which('arduino-cli')
    if cli is None and sys.platform == 'win32':
        cli = Path.home() / 'AppData/Local/Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
    if cli is None or not Path(cli).is_file():
        p.error('Arduino CLI not found; pass --arduino-cli.')
    core = data / 'packages/loom4/hardware/samd/4.9'
    if not core.is_dir():
        p.error('Install Loomified SAMD 4.9 (with its bundled Loom 4.9.1, SdFat 1.1.0 and RTCZero 1.6.0 libraries).')
    mqtt = (a.mqtt_library or Path.home() / 'Documents/Arduino/libraries/ArduinoMqttClient').resolve()
    if not (mqtt / 'library.properties').is_file():
        p.error('Installed Loom needs ArduinoMqttClient; pass --mqtt-library to its installed folder.')
    scratch = ROOT / '.build'
    scratch.mkdir(exist_ok=True)
    variant = scratch / 'variant'
    shutil.copytree(core / 'variants/feather_m0', variant, dirs_exist_ok=True)
    linker = variant / 'linker_scripts/gcc/flash_with_bootloader.ld'
    text = linker.read_text()
    old = 'RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00008000'
    if text.count(old) != 1:
        p.error('Unrecognized Feather linker script; review RAM reservation before building.')
    write(linker, text.replace(old, 'RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00007FF8 /* preserve double-tap word and align MSP */'))
    cfg = scratch / 'cli.yaml'
    # An empty sketchbook prevents an unrelated user library overriding the
    # bundled Loom dependencies. No global CLI configuration is changed.
    sketchbook = scratch / 'sketchbook'
    sketchbook.mkdir(exist_ok=True)
    write(cfg, json.dumps({'directories': {'data': str(data), 'user': str(sketchbook),
          'downloads': str(scratch / 'downloads')}, 'build_cache': {'path': str(scratch / 'cache')}}))
    # Snapshot platform and dependency sources as well as the sketch itself.
    inputs = source_hashes()
    dependencies = {x.relative_to(core).as_posix(): digest(x) for x in core.rglob('*')
                    if x.is_file() and x.suffix in ('.cpp', '.h', '.c', '.S', '.ld', '.txt', '.properties')
                    and 'examples' not in x.relative_to(core).parts}
    mqtt_inputs = {x.relative_to(mqtt).as_posix(): digest(x) for x in mqtt.rglob('*')
                   if x.is_file() and (x.suffix in ('.cpp', '.h', '.c') or x.name == 'library.properties')
                   and 'examples' not in x.relative_to(mqtt).parts}
    fqbn = 'loom4:samd:adafruit_feather_m0'
    command = [str(cli), '--config-file', str(cfg), 'compile', '--fqbn', fqbn,
               '--library', str(mqtt),
               '--build-property', 'build.variant.path=' + variant.as_posix(),
               '--build-path', str(scratch / 'compiler'), '--output-dir', str(scratch / 'output'), str(ROOT)]
    print('Compiling Loomified Feather M0. No hardware will be programmed.', flush=True)
    with (scratch / 'compile.log').open('w', encoding='utf-8') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print((scratch / 'compile.log').read_text(encoding='utf-8', errors='replace')[-18000:])
        raise SystemExit(result.returncode)
    if source_hashes() != inputs:
        raise RuntimeError('Sketch inputs changed while compiling; repeat build.')
    if any(digest(core / name) != value for name, value in dependencies.items()):
        raise RuntimeError('Core/dependency inputs changed while compiling; repeat build.')
    if any(digest(mqtt / name) != value for name, value in mqtt_inputs.items()):
        raise RuntimeError('ArduinoMqttClient inputs changed while compiling; repeat build.')
    name = ROOT.name + '.ino'
    app = scratch / 'output' / (name + '.bin')
    image = app.read_bytes()
    sp, pc = struct.unpack_from('<II', image)
    if not (sp == 0x20007FF8 and pc & 1 and 0x2000 <= (pc & ~1) < 0x2000 + len(image)
            and len(image) <= 262144 - 8192):
        raise RuntimeError('Application vector/layout check failed.')
    destination = a.firmware_dir or ROOT / 'firmware'
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(app, destination / 'firmware.sketch.bin')
    shutil.copy2(scratch / 'output' / (name + '.elf'), destination / 'firmware.elf')
    manifest = {'fqbn': fqbn, 'hardware_tested': False, 'flash_offset': 8192,
                'initial_msp': hex(sp), 'double_tap_cell': '0x20007ffc',
                'source_sha256': inputs, 'core_dependency_sha256': dependencies,
                'ArduinoMqttClient_sha256': mqtt_inputs,
                'linker_sha256': digest(linker), 'cli_sha256': digest(Path(cli)),
                'images': {x.name: {'bytes': x.stat().st_size, 'sha256': digest(x)}
                           for x in (destination / 'firmware.sketch.bin', destination / 'firmware.elf')}}
    write(destination / 'BUILD_MANIFEST.json', json.dumps(manifest, indent=2) + '\n')
    print((scratch / 'compile.log').read_text(encoding='utf-8', errors='replace')[-2500:])
    print('PASS: app at 0x2000, aligned MSP=0x20007ff8; double-tap cell reserved.')
    print('Build saved in ' + str(destination) + '. Nothing flashed.')


if __name__ == '__main__':
    main()
