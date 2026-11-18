import json
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build import ROOT, load_board
from combine import combine
from board_profile import validate_routes, pin_type


class ImageTests(unittest.TestCase):
    def test_reserved_pin_metadata_preserves_startup_mux_without_changing_alias_indices(self):
        for pin in ('PA00','PA01','PA30','PA31'):
            self.assertEqual(pin_type(pin, {}), 'PIO_NOT_A_PIN')
        for pin in ('PA20','PA24','PA25','PB30','PB31'):
            self.assertEqual(pin_type(pin, {}), 'PIO_DIGITAL')
        self.assertEqual(pin_type('PA22', {'PA22':'PIO_SERCOM'}), 'PIO_SERCOM')
    def test_combined_layout_and_invalid_inputs(self):
        boot = struct.pack('<II', 0x20007c00, 0x11) + bytes(64)
        app = struct.pack('<II', 0x20008000, 0x2011) + bytes(64)
        out = combine(boot, app)
        self.assertEqual(out[:len(boot)], boot)
        self.assertEqual(out[len(boot):8192], b'\xff' * (8192-len(boot)))
        self.assertEqual(out[8192:], app)
        official=(ROOT/'tests/data/samd21_sam_ba_official.bin').read_bytes()
        self.assertEqual(combine(official,app)[8192:],app)
        for wrong in (out, boot, bytes(64), app[:7], struct.pack('<II',0x20008000,0x2010)+bytes(64),
                      struct.pack('<II',0x20007ffc,0x2011)+bytes(64)):
            with self.assertRaises(ValueError):
                combine(boot,wrong)

    def test_board_states_match_current_pcb(self):
        b=load_board()
        self.assertEqual(b['safe_outputs'], {'NRF_EN':0,'LTE_PWR':0,'LTE_RST':0,'RADIO_RST':0,'RADIO_CS':0,'SD_CS':0})
        self.assertTrue(b['crystalless'])
        self.assertFalse(b['boot_indicator'])
        self.assertEqual(b,json.loads((ROOT/'configs/jolteon-j18-crystalless-tested.json').read_text()))
        self.assertEqual(b['pins']['LED'],'PA05')
        self.assertEqual(b['pins']['LTE_RX'],'PA17')
        self.assertEqual(b['pins']['SPI0_SCK'],'PB10') # Still an invalid hardware SPI route.

    def test_release_manifest(self):
        import hashlib
        import zlib
        data=(ROOT/'firmware/samd21_sam_ba.bin').read_bytes()
        m=json.loads((ROOT/'firmware/manifest.json').read_text())
        self.assertEqual(len(data),m['size'])
        self.assertEqual(hashlib.sha256(data).hexdigest(),m['sha256'])
        self.assertEqual(f'{zlib.crc32(data):08x}',m['crc32'])
        self.assertTrue(m['repeat_build_verified'])
        self.assertLessEqual(len(data),8192)
        for name, sha in m['source_sha256'].items():
            self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),sha,name)
        for name, sha in m['build_inputs_sha256'].items():
            self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),sha,name)
        self.assertTrue(all('\\' not in name for name in m['source_sha256']))
        profile=ROOT/'arduino-profile/hardware/opens/samd'
        for name, sha in m['profile_sha256'].items():
            self.assertEqual(hashlib.sha256((profile/name).read_bytes()).hexdigest(),sha,name)

    def test_invalid_board_states_rejected(self):
        import copy
        from unittest.mock import patch
        original=load_board()
        cases=[]
        b=copy.deepcopy(original); b['pins']['LED']='PA24'; cases.append(b)
        b=copy.deepcopy(original); b['pins']['LED']='PB18'; cases.append(b)
        b=copy.deepcopy(original); b['pins']['NRF_TX']=b['pins']['LTE_RX']; cases.append(b)
        b=copy.deepcopy(original); b['safe_outputs']['NRF_EN']=1; cases.append(b)
        b=copy.deepcopy(original); b['high_impedance'].append('NRF_EN'); cases.append(b)
        b=copy.deepcopy(original); b['safe_outputs']['LTE_PWR']=True; cases.append(b)
        b=copy.deepcopy(original); b['usb']['vid']=9025.0; cases.append(b)
        b=copy.deepcopy(original); b['pins']['EXTRA']='PB04'; cases.append(b)
        for b in cases:
            with patch.object(Path, 'read_text', return_value=json.dumps(b)):
                with self.assertRaises(ValueError):
                    load_board()

    def test_supported_and_unreviewed_routes(self):
        b=load_board()
        self.assertEqual(validate_routes(b),(False,False))
        b['pins'].update(SPI0_SCK='PB13',RADIO_CS='PB10')
        self.assertEqual(validate_routes(b),(True,False))
        b['pins'].update(SPI1_SCK='PB17',SPI1_MISO='PB22',SPI1_MOSI='PB16')
        self.assertEqual(validate_routes(b),(True,True))
        b['pins'].update(SPI0_SCK='PB10',RADIO_CS='PB13')
        with self.assertRaises(ValueError): validate_routes(b)
        b=load_board(); b['pins']['NRF_TX']='PA04'
        with self.assertRaises(ValueError): validate_routes(b)


if __name__=='__main__':
    unittest.main()
