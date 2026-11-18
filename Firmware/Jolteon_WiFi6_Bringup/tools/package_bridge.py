#!/usr/bin/env python3
"""Publish checked application/network images and a true dual-core HEX; never flash."""
import json
from pathlib import Path
import shutil
import hashlib
from intel_hex import parse,encode,checked_dual_core
from build_receipt import verify_receipt,verify_inputs

ROOT=Path(__file__).resolve().parents[1]


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    build=ROOT/'.build/bridge'
    receipt=json.loads((build/'build-receipt.json').read_text())
    files=verify_receipt(receipt,ROOT/'bridge',build)
    for name,key in (('build_bridge.ps1','build_helper_sha256'),('build_receipt.py','receipt_tool_sha256')):
        if sha(ROOT/'tools'/name)!=receipt[key]:
            raise ValueError('Build workflow changed; rebuild the bridge before packaging: '+name)
    frozen={name:path.read_bytes() for name,path in files.items()}
    for name,data in frozen.items():
        if hashlib.sha256(data).hexdigest()!=receipt['images_sha256'][name]:
            raise ValueError('Bridge output changed while packaging: '+name)
    combined=checked_dual_core(parse(frozen['application.hex'].decode('ascii')),
                               parse(frozen['network_core.hex'].decode('ascii')))
    full=encode(combined)
    if parse(full)!=combined:
        raise ValueError('Dual-core HEX round-trip mismatch.')
    dest=ROOT/'releases/bridge/repository-build'
    dest.mkdir(parents=True,exist_ok=True)
    verify_inputs(receipt,ROOT/'bridge')
    for target,data in frozen.items(): (dest/target).write_bytes(data)
    (dest/'full_dual_core.hex').write_text(full,encoding='ascii',newline='\n')
    manifest={'sdk_root':receipt['sdk_root'],'sdk_version_file':receipt['sdk_version_file'],
        'toolchain_root':receipt['toolchain_root'],'compiler':receipt['compiler'],
        'board':'nrf7002dk/nrf5340/cpuapp','sysbuild':True,'hardware_tested':False,
        'imported_bridge_commit':json.loads((ROOT/'docs/IMPORT_MANIFEST.json').read_text())['bridge_commit'],
        'core_vector_bases':{'application':0,'network':0x01000000},
        'merged_hex_roundtrip_verified':True,'both_core_vectors_verified':True,
        'source_sha256':receipt['source_sha256'],
        'build_helper_sha256':receipt['build_helper_sha256'],'build_receipt':receipt,
        'images':{x.name:{'sha256':sha(x),'bytes':x.stat().st_size} for x in dest.iterdir() if x.suffix in ('.hex','.bin')}}
    (dest/'BUILD_MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
    shutil.copy2(build/'build-receipt.json',dest/'build-receipt.json')
    (dest/'README.md').write_text('# WT02E40E repository build\n\n'
        'Both nRF5340 cores built from bridge/ with the SDK/compiler recorded in BUILD_MANIFEST.json. No hardware was flashed.\n\n'
        '- application.hex: SDK merged.hex; **application core only**.\n'
        '- network_core.hex: SDK merged_CPUNET.hex; network core at 0x01000000.\n'
        '- full_dual_core.hex: checked, conflict-free merge of those two files.\n\n'
        'Prefer sysbuild west flash with the original build directory, or the repository ST-LINK helper '
        'which explicitly programs network then application core. The single application .bin is not complete firmware.\n')
    (dest/'SHA256SUMS.txt').write_text(''.join(f'{sha(x)}  {x.name}\n' for x in sorted(dest.iterdir()) if x.suffix in ('.hex','.bin')))
    print('Packaged both cores, verified vector bases and HEX round-trip: '+str(dest))


if __name__=='__main__': main()
