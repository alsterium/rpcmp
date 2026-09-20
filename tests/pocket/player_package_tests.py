"""Candidate packaging: slots, scaler ABI, exact bytes, and identity rejection."""

import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path, PurePosixPath

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import pocket_player_package as package


def files():
    return package.candidate_files(b'loader', b'\x01\x80\x42', b'OS', b'ELF', b'library',
                                   {n: n.encode('ascii') for n in package.FONT_NOTICES})


class PlayerPackageTests(unittest.TestCase):
    def test_slot_graph_and_no_persistence(self):
        data = files()
        self.assertEqual(set(data), package.expected_paths())
        core = PurePosixPath('Cores/RPCMP.AlbumPlayer')
        common = PurePosixPath('Assets/rpcmp_player/common')
        definitions = json.loads(data[core / 'data.json'])['data']['data_slots']
        self.assertEqual([s['id'] for s in definitions], [0, 1, 2, 3, 4])
        self.assertEqual(definitions[0]['parameters'], 275)
        self.assertEqual(definitions[4]['parameters'], 8)
        self.assertEqual(definitions[4]['size_maximum'], 32 * 1024 * 1024)
        self.assertTrue(all(s['deferload'] for s in definitions[1:]))
        self.assertTrue(all(not s.get('nonvolatile') for s in definitions))
        instance_path = PurePosixPath('Assets/rpcmp_player/RPCMP.AlbumPlayer/player.json')
        instance = json.loads(data[instance_path])['instance']
        self.assertNotIn('memory_writes', instance)
        self.assertEqual([s['id'] for s in instance['data_slots']], [1, 2, 3, 4])
        for slot in instance['data_slots']:
            self.assertIn(common / slot['filename'], data)
        self.assertEqual(data[common / 'player.ini'], b'[os]\nELF=player.elf\nVARIANT=rpcmp\n')
        self.assertFalse(any(p.parts[0] == 'Saves' for p in data))

    def test_physical_scaler_slot_and_controls(self):
        defs = package.definitions()
        self.assertEqual(defs['core.json']['core']['framework']['version_required'], '2.2')
        modes = defs['video.json']['video']['scaler_modes']
        self.assertEqual([(m['width'], m['height']) for m in modes],
                         [(320, 240), (320, 200), (320, 224), (320, 256),
                          (320, 288), (400, 300), (256, 240), (640, 480)])
        mappings = defs['input.json']['input']['controllers'][0]['mappings']
        self.assertEqual([m['id'] for m in mappings], [0, 1, 4, 5])
        self.assertTrue(all(len(m['name']) <= 19 for m in mappings))

    def test_fresh_output_roundtrip_and_bit_reversal(self):
        data = files()
        self.assertEqual(data[PurePosixPath('Cores/RPCMP.AlbumPlayer/rpcmp.rbf_r')], b'\x80\x01\x42')
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            output, archive = root / 'candidate', root / 'candidate.zip'
            evidence = package.write_package(data, output, archive)
            self.assertEqual(len(evidence), len(data))
            with zipfile.ZipFile(archive) as zipped:
                for path, contents in data.items():
                    self.assertEqual(zipped.read(path.as_posix()), contents)
            original = archive.read_bytes()
            with self.assertRaisesRegex(ValueError, 'preserve'):
                package.write_package(data, output, archive)
            self.assertEqual(archive.read_bytes(), original)

    def test_unlisted_or_missing_payload_is_rejected_before_writing(self):
        for extra in (False, True):
            data = files()
            if extra:
                data[PurePosixPath('../escape')] = b'bad'
            else:
                del data[PurePosixPath('Assets/rpcmp_player/common/os.bin')]
            with tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                with self.assertRaisesRegex(ValueError, 'allowlist'):
                    package.write_package(data, root / 'candidate', root / 'candidate.zip')
                self.assertEqual(list(root.iterdir()), [])

    def test_unverified_app_and_bitstream_cannot_be_packaged(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            app = root / 'app.elf'
            app.write_bytes(b'unverified')
            with self.assertRaisesRegex(ValueError, 'application identity'):
                package.verify_app(app)
            (root / 'output_files').mkdir()
            (root / 'output_files/ap_core.rbf').write_bytes(b'unverified')
            with self.assertRaisesRegex(ValueError, 'RBF identity'):
                package.verify_image(root, root)


if __name__ == '__main__':
    unittest.main()
