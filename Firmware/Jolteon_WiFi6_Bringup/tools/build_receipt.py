"""Record bridge inputs before compilation and bind successful output to them."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

SUFFIXES={'.c','.h','.cpp','.overlay','.conf','.dts','.dtsi','.yaml','.yml'}
SKIP={'.git','.build','build','firmware','tests','node_modules','__pycache__'}


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def source_hashes(source):
    return {x.relative_to(source).as_posix():sha(x) for x in sorted(source.rglob('*'))
        if x.is_file() and not any(p in SKIP for p in x.relative_to(source).parts)
        and (x.suffix in SUFFIXES or x.name in ('Kconfig','CMakeLists.txt'))}


def verify_inputs(receipt,source):
    if source_hashes(source)!=receipt['source_sha256']:
        raise ValueError('Bridge inputs changed since compilation started; rebuild before packaging.')


def outputs(build):
    text=(build/'domains.yaml').read_text()
    match=re.search(r'^default:\s*([A-Za-z0-9_-]+)\s*$',text,re.M)
    if not match: raise ValueError('Cannot identify application domain in domains.yaml.')
    return {'application.hex':build/'merged.hex','network_core.hex':build/'merged_CPUNET.hex',
        'application.bin':build/match[1]/'zephyr/zephyr.bin',
        'network_core.bin':build/'hci_ipc/zephyr/zephyr.bin'}


def verify_receipt(receipt,source,build):
    if not receipt.get('build_succeeded'):
        raise ValueError('Bridge has no successful build receipt; run tools/build_bridge.ps1.')
    verify_inputs(receipt,source)
    files=outputs(build)
    if set(files)!=set(receipt['images_sha256']): raise ValueError('Bridge output set changed.')
    for name,path in files.items():
        if sha(path)!=receipt['images_sha256'][name]:
            raise ValueError('Bridge output changed since build: '+name)
    return files


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode',choices=('begin','finish'))
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--build',type=Path,required=True)
    p.add_argument('--helper',type=Path,required=True)
    p.add_argument('--sdk-root',type=Path,required=True)
    p.add_argument('--toolchain-root',type=Path,required=True)
    a=p.parse_args()
    if not a.build.resolve().is_relative_to(a.source.resolve().parent):
        raise ValueError('Build directory resolves outside the repository; inspect symlinks/junctions.')
    a.build.mkdir(parents=True,exist_ok=True)
    # west --pristine may remove the build directory, so keep the input snapshot
    # beside it until the successful build is recorded.
    pending=a.build.parent/(a.build.name+'-inputs.pending.json')
    if a.mode=='begin':
        version=a.sdk_root/'nrf/VERSION'
        receipt={'build_succeeded':False,'source_sha256':source_hashes(a.source),
            'build_helper_sha256':sha(a.helper),'receipt_tool_sha256':sha(Path(__file__)),
            'sdk_root':str(a.sdk_root),'toolchain_root':str(a.toolchain_root),
            'sdk_version_file':version.read_text().strip() if version.is_file() else None,
            'board':'nrf7002dk/nrf5340/cpuapp','extra_overlay':'boards/jolteon_uart.overlay'}
        pending.write_text(json.dumps(receipt,indent=2)+'\n')
    else:
        receipt=json.loads(pending.read_text())
        verify_inputs(receipt,a.source)
        if sha(a.helper)!=receipt['build_helper_sha256'] or sha(Path(__file__))!=receipt['receipt_tool_sha256']:
            raise ValueError('Build/receipt helper changed during compilation.')
        files=outputs(a.build)
        receipt['images_sha256']={name:sha(path) for name,path in files.items()}
        compiler=a.toolchain_root/'opt/zephyr-sdk/arm-zephyr-eabi/bin'/('arm-zephyr-eabi-gcc.exe' if sys.platform=='win32' else 'arm-zephyr-eabi-gcc')
        receipt['compiler']=subprocess.run([str(compiler),'--version'],check=True,capture_output=True,text=True).stdout.splitlines()[0] if compiler.is_file() else None
        receipt['build_succeeded']=True
        destination=a.build/'build-receipt.json'
        temp=a.build/'build-receipt.json.tmp'
        temp.write_text(json.dumps(receipt,indent=2)+'\n')
        temp.replace(destination)
        print('Recorded successful bridge build; source and image hashes agree.')


if __name__=='__main__':
    try: main()
    except (OSError,ValueError,KeyError,subprocess.CalledProcessError) as e:
        print('FAILED: '+str(e),file=sys.stderr); sys.exit(1)
