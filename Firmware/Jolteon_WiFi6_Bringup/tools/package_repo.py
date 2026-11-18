#!/usr/bin/env python3
"""Validate current firmware/source manifests and make a portable repository ZIP."""
import hashlib
import json
from pathlib import Path
import sys
from zipfile import ZipFile,ZipInfo,ZIP_DEFLATED
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'bootloader'))
from build import validate_release
from combine import combine
from intel_hex import parse,checked_dual_core
from build_receipt import verify_inputs

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    validate_release(ROOT/'bootloader/firmware')
    releases=[]
    for manifest_path in sorted((ROOT/'releases/samd').rglob('BUILD_MANIFEST.json')):
        m=json.loads(manifest_path.read_text()); folder=manifest_path.parent
        sketch=ROOT/'sketches'/folder.name
        for name,expected in m['source_sha256'].items():
            if sha(sketch/name)!=expected: raise ValueError('Stale SAMD source: '+str(sketch/name))
        current={x.relative_to(sketch).as_posix() for x in sketch.rglob('*') if x.is_file() and
            x.suffix in ('.ino','.h','.cpp','.c') and not any(part in x.relative_to(sketch).parts
            for part in ('build','.j18-build','tests','firmware'))}
        if current!=set(m['source_sha256']): raise ValueError('SAMD source file set changed: '+folder.name)
        for name,metadata in m['images'].items():
            if sha(folder/name)!=metadata['sha256']: raise ValueError('SAMD image mismatch: '+str(folder/name))
        expected=combine((folder/'samd21_sam_ba.bin').read_bytes(),(folder/'firmware.sketch.bin').read_bytes())
        if (folder/'firmware.with_bootloader.bin').read_bytes()!=expected: raise ValueError('Wrong combined layout: '+str(folder))
        releases.append(manifest_path.relative_to(ROOT).as_posix())
    bridge=ROOT/'releases/bridge/repository-build'
    m=json.loads((bridge/'BUILD_MANIFEST.json').read_text())
    verify_inputs(m['build_receipt'],ROOT/'bridge')
    if m['source_sha256']!=m['build_receipt']['source_sha256']:
        raise ValueError('Bridge manifest and build receipt disagree.')
    if sha(ROOT/'tools/build_bridge.ps1')!=m['build_receipt']['build_helper_sha256']:
        raise ValueError('Bridge build helper changed; rebuild to record the new workflow.')
    if sha(ROOT/'tools/build_receipt.py')!=m['build_receipt']['receipt_tool_sha256']:
        raise ValueError('Bridge receipt tool changed; rebuild before packaging.')
    for name,metadata in m['images'].items():
        if sha(bridge/name)!=metadata['sha256']: raise ValueError('Bridge image mismatch: '+name)
    merged=checked_dual_core(parse((bridge/'application.hex').read_text()),parse((bridge/'network_core.hex').read_text()))
    if parse((bridge/'full_dual_core.hex').read_text())!=merged: raise ValueError('Dual-core HEX mismatch.')
    excluded={'.git','.build','.j18-build','.validation','__pycache__','node_modules','objects','dist','build'}
    files=[]
    for x in sorted(ROOT.rglob('*')):
        relative=x.relative_to(ROOT)
        if not x.is_file() or any(part in excluded for part in relative.parts): continue
        if x.name in ('build.log','.validation-disassembly.txt','ARCHIVE_MANIFEST.json'): continue
        if x.suffix in ('.o','.obj','.exe','.pyc'): continue
        if relative.parts[0]=='sketches' and any(part in ('Windows_J18_Tools','RPi_J18_Tools','Mac_J18_Tools') for part in relative.parts): continue
        if not x.resolve().is_relative_to(ROOT): raise ValueError('Archive input resolves outside repository: '+str(x))
        files.append(x)
    hashes={x.relative_to(ROOT).as_posix():sha(x) for x in files}
    manifest=ROOT/'ARCHIVE_MANIFEST.json'
    manifest.write_text(json.dumps({'date':'2026-10-14','hardware_tested':False,
        'hardware_validation':'Limited user bench results are documented in docs/BENCH_ACCEPTANCE.md; this is not whole-board acceptance.',
        'validated_samd_builds':releases,'files_sha256':hashes},indent=2)+'\n')
    files.append(manifest)
    hashes[manifest.name]=sha(manifest)
    dest=ROOT/'dist/Jolteon_Bringup.zip'; dest.parent.mkdir(exist_ok=True)
    staged=dest.with_suffix('.zip.tmp')
    with ZipFile(staged,'w',ZIP_DEFLATED) as z:
        for x in files:
            relative=x.relative_to(ROOT).as_posix()
            data=x.read_bytes()
            if hashlib.sha256(data).hexdigest()!=hashes[relative]: raise ValueError('Archive input changed: '+relative)
            info=ZipInfo(ROOT.name+'/'+relative); info.create_system=3
            info.external_attr=(0o100755 if x.suffix in ('.py','.sh','.command','.desktop') else 0o100644)<<16
            info.compress_type=ZIP_DEFLATED; z.writestr(info,data)
    with ZipFile(staged) as z:
        if z.testzip(): raise ValueError('ZIP CRC check failed.')
        for x in files:
            relative=x.relative_to(ROOT).as_posix()
            if hashlib.sha256(z.read(ROOT.name+'/'+relative)).hexdigest()!=hashes[relative]: raise ValueError('ZIP mismatch: '+str(x))
            if sha(x)!=hashes[relative]: raise ValueError('Archive input changed during packaging: '+relative)
    staged.replace(dest)
    print(f'Validated {len(releases)} SAMD builds and both bridge cores; packaged {len(files)} files.')
    print(f'{dest} ({dest.stat().st_size} bytes)')

if __name__=='__main__':
    try: main()
    except (OSError,ValueError,KeyError) as e:
        print('FAILED: '+str(e),file=sys.stderr); sys.exit(1)
