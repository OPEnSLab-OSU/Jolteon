import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from build_receipt import source_hashes,verify_inputs,verify_receipt,sha


class ReceiptTests(unittest.TestCase):
    def setUp(self):
        fixtures=ROOT/'.build/test-fixtures'
        fixtures.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='receipt-',dir=fixtures)
        self.root=Path(self.temp.name).resolve()
        self.assertTrue(self.root.is_relative_to(fixtures.resolve()))
        self.addCleanup(self.temp.cleanup) # Cleanup target was resolved/checked.
        self.source=self.root/'bridge'; self.source.mkdir()
        (self.source/'main.c').write_text('int value=1;\n')
        self.build=self.root/'.build/bridge'; self.build.mkdir(parents=True)

    def test_input_changes_and_missing_files_are_rejected(self):
        receipt={'source_sha256':source_hashes(self.source)}
        verify_inputs(receipt,self.source)
        (self.source/'main.c').write_text('int value=2;\n')
        with self.assertRaises(ValueError): verify_inputs(receipt,self.source)
        (self.source/'main.c').write_text('int value=1;\n')
        (self.source/'new.h').write_text('#pragma once\n')
        with self.assertRaises(ValueError): verify_inputs(receipt,self.source)

    def test_pristine_keeps_input_snapshot_and_output_tampering_fails(self):
        tool=ROOT/'tools/build_receipt.py'
        args=['--source',str(self.source),'--build',str(self.build),
              '--helper',str(ROOT/'tools/build_bridge.ps1'),
              '--sdk-root',str(self.root/'sdk'),'--toolchain-root',str(self.root/'toolchain')]
        subprocess.run([sys.executable,str(tool),'begin',*args],check=True,capture_output=True)
        pending=self.build.parent/'bridge-inputs.pending.json'
        self.assertTrue(pending.is_file())
        self.assertTrue(self.build.resolve().is_relative_to(self.root))
        shutil.rmtree(self.build) # Simulate west --pristine, within checked fixture.
        self.build.mkdir()
        self.assertTrue(pending.is_file())
        (self.build/'domains.yaml').write_text('default: bridge\n')
        files={'application.hex':self.build/'merged.hex','network_core.hex':self.build/'merged_CPUNET.hex',
               'application.bin':self.build/'bridge/zephyr/zephyr.bin',
               'network_core.bin':self.build/'hci_ipc/zephyr/zephyr.bin'}
        for i,path in enumerate(files.values()):
            path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(bytes([i+1]))
        subprocess.run([sys.executable,str(tool),'finish',*args],check=True,capture_output=True)
        receipt=json.loads((self.build/'build-receipt.json').read_text())
        self.assertEqual(verify_receipt(receipt,self.source,self.build),files)
        files['network_core.bin'].write_bytes(b'changed')
        with self.assertRaises(ValueError): verify_receipt(receipt,self.source,self.build)


if __name__=='__main__': unittest.main()
