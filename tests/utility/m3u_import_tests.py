"""M3U -> actual HPL1 reader, using authored music and explicit byte expectations."""
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import m3u_import as ingest

CHECKER = None
if "--checker" in sys.argv:
    at = sys.argv.index("--checker")
    CHECKER = sys.argv[at + 1]
    del sys.argv[at:at + 2]


def body(channels=9):
    first = 2 + channels * 2
    return (struct.pack(">H", first + channels * 2) +
            b"".join(struct.pack(">H", first + i * 2) for i in range(channels)) +
            b"\xf1\0" * channels + bytes(27))


def mdx(title="試験曲", reference="", channels=9):
    return title.encode("cp932") + b"\r\n\x1a" + reference.encode("cp932") + b"\0" + body(channels)


def pdx():
    return struct.pack(">II", 768, 4) + bytes(760) + b"\x12\x34\x56\x78"


def lzx(stream, size):
    header = bytearray(38)
    header[4:8] = b"LZX "
    header[18:22] = size.to_bytes(4, "big")
    return bytes(header) + b"\x7f\xff\xffL" + stream


def literal_lzx(data):
    # Encode only literal groups and a terminal long token; no match search.
    full = len(data) // 8 * 8
    stream = b''.join(b'\xff' + data[i:i+8] for i in range(0, full, 8))
    tail = data[full:]
    if len(tail) == 7:
        stream += b'\xfe' + tail + b'\x80'
    else:
        control = ((255 << (8-len(tail))) & 255) | (1 << (6-len(tail)))
        stream += bytes([control]) + tail
    return lzx(stream + b'\xff\xf8\0', len(data))


class LzxTests(unittest.TestCase):
    def test_hand_encoded_literals_and_matches(self):
        # Independent control-bit vectors, MSB first: 111 01(end),
        # 11 00 10(short length 4) 01(end), 111 01(long length 3) 01(end).
        cases = [(b"\xe8ABC\xff\xf8\0", b"ABC"),
                 (b"\xc9AB\xfe\xff\xf8\0", b"ABABAB"),
                 (b"\xeaABC\xff\xe9\xff\xf8\0", b"ABCABC"),
                 (b"\xa8Z\xff\xf8\x09\xff\xf8\0", b"Z" * 11)]
        for stream, expected in cases:
            self.assertEqual(ingest.unpack(lzx(stream, len(expected))), expected)
        self.assertEqual(ingest.unpack(b"plain"), b"plain")

    def test_bad_headers_distances_sizes_and_truncation(self):
        good = lzx(b"\xe8ABC\xff\xf8\0", 3)
        for bad in (good[:8], good[:38], good[:45], good[:-1],
                    lzx(b"\xe8ABC\xff\xf8\0", 2), lzx(b"\xe8ABC\xff\xf8\0", 4),
                    lzx(b"\x40\xff\xf9", 3), lzx(b"\xe8ABC\xff\xf8\0", 0),
                    lzx(b"\xe8ABC\xff\xf8\0", ingest.MUSIC_LIMIT + 1)):
            with self.subTest(data=bad[:24]), self.assertRaises(ValueError):
                ingest.unpack(bad)


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        self.music = self.root / "音楽"
        self.music.mkdir()
        (self.music / "FM.MDX").write_bytes(mdx())
        (self.music / "PCM.MDX").write_bytes(mdx("PCM試験", "SAMPLES", 16))
        (self.music / "Samples.PdX").write_bytes(pdx())
        self.playlist = self.root / "曲集.m3u8"
        self.output = self.root / "output"

    def run_import(self, text, **kwargs):
        self.playlist.write_text(text, encoding="utf-8-sig")
        return ingest.import_playlist(self.playlist, self.output, **kwargs)

    def unpack_result(self):
        data = (self.output / ingest.ASSET).read_bytes()
        magic, version, width, count, length, crc, header_crc, reserved = struct.unpack_from("<4s7I", data)
        self.assertEqual((magic, version, width, length, reserved), (b"HPL1", 1, 128, len(data), 0))
        self.assertEqual(header_crc, zlib.crc32(data[:24]))
        self.assertEqual(crc, zlib.crc32(data[32:32 + count * 128]))
        result = []
        for i in range(count):
            a, n, ac, b, m, bc, t = struct.unpack_from("<7I", data, 32 + i * 128)
            self.assertEqual(zlib.crc32(data[a:a+n]), ac)
            self.assertEqual(zlib.crc32(data[b:b+m]), bc)
            result.append((data[60+i*128:60+i*128+t].decode("utf-8"), data[a:a+n], data[b:b+m]))
        if CHECKER:
            subprocess.run([CHECKER, "--playlist", str(self.output / ingest.ASSET)], check=True)
        return result

    def test_order_duplicates_16_tracks_pcm_titles_and_zip(self):
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in self.music.iterdir()}
        report = self.run_import("#EXTM3U\n\n#EXTINF:0,ignored\n音楽/pcm.mdx\n音楽\\fm.mdx\n音楽/PCM.MDX\n")
        self.assertEqual((report['imported'], report['excluded']), (3, 0))
        actual = self.unpack_result()
        self.assertEqual([row[0] for row in actual], ["PCM試験", "試験曲", "PCM試験"])
        self.assertEqual(actual[0][1], bytes.fromhex("00000000000a00080000") + body(16))
        self.assertEqual(actual[0][2], bytes.fromhex("00000000000a00020000") + pdx())
        self.assertEqual(actual[1][1], bytes.fromhex("0000ffff000a00080000") + body())
        self.assertEqual(actual[1][2], b"")
        self.assertEqual(actual[0], actual[2])
        self.assertEqual(before, {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in self.music.iterdir()})
        with zipfile.ZipFile(self.output.with_suffix('.zip')) as archive:
            for name in archive.namelist():
                self.assertEqual(archive.read(name), (self.output / name).read_bytes())
            self.assertIn(ingest.ASSET.as_posix(), archive.namelist())
            self.assertFalse(any(name.startswith('Cores/') for name in archive.namelist()))
        with self.assertRaisesRegex(ValueError, "既に"):
            ingest.import_playlist(self.playlist, self.output)

    def test_exclusions_are_visible_and_other_songs_survive(self):
        (self.music / "broken.mdx").write_bytes(b"invalid")
        (self.music / "missing.mdx").write_bytes(mdx(reference="absent"))
        report = self.run_import("音楽/broken.mdx\n音楽/missing.mdx\nhttps://host/track.mdx\n音楽/Samples.PdX\n音楽/FM.MDX\n")
        self.assertEqual((report['imported'], report['excluded']), (1, 4))
        self.assertEqual([r['line'] for r in report['entries']], [1, 2, 3, 4, 5])
        self.assertTrue(all(r.get('reason') for r in report['entries'][:4]))
        self.assertEqual(json.loads((self.output / "取り込み結果.json").read_text(encoding="utf-8")), report)
        self.assertEqual(len(self.unpack_result()), 1)

    def test_no_tracks_means_no_installable_output(self):
        report = self.run_import("音楽/absent.mdx\n../escape.mdx\n")
        self.assertEqual(report['imported'], 0)
        self.assertFalse((self.output / ingest.ASSET).exists())
        self.assertFalse(self.output.with_suffix('.zip').exists())
        self.assertTrue((self.output / "取り込み結果.txt").exists())

    def test_root_parent_paths_and_shared_pdx(self):
        shared = self.root / "共通"
        shared.mkdir()
        (self.music / "Samples.PdX").rename(shared / "Samples.PdX")
        self.playlist = self.music / "local.m3u8"
        result = self.run_import("../音楽/PCM.MDX", root=self.root, pdx_dirs=["../共通"])
        self.assertEqual(result['imported'], 1)
        self.assertTrue(self.unpack_result()[0][2])

    def test_compressed_mdx_and_pdx_use_identical_wrappers(self):
        (self.music / 'PCM.MDX').write_bytes(b'Compressed\r\n\x1aSAMPLES\0' + literal_lzx(body(16)))
        (self.music / 'Samples.PdX').write_bytes(literal_lzx(pdx()))
        self.assertEqual(self.run_import('音楽/PCM.MDX')['imported'], 1)
        actual = self.unpack_result()[0]
        self.assertEqual(actual[1], bytes.fromhex('00000000000a00080000') + body(16))
        self.assertEqual(actual[2], bytes.fromhex('00000000000a00020000') + pdx())

    def test_ambiguous_shared_pdx_is_not_guessed(self):
        (self.music / "Samples.PdX").unlink()
        for name in ('a', 'b'):
            (self.root / name).mkdir()
            (self.root / name / "samples.pdx").write_bytes(pdx())
        result = self.run_import("音楽/PCM.MDX", pdx_dirs=['a', 'b'])
        self.assertEqual(result['excluded'], 1)
        self.assertIn("複数", result['entries'][0]['reason'])

    def test_cp932_m3u_and_title_fallback(self):
        (self.music / "FM.MDX").write_bytes(b"\x81\r\n\x1a\0" + body())
        self.playlist.write_bytes("音楽/FM.MDX\n".encode('cp932'))
        with self.assertRaises(UnicodeDecodeError):
            ingest.import_playlist(self.playlist, self.output)
        self.assertFalse(self.output.exists())
        result = ingest.import_playlist(self.playlist, self.output, encoding='cp932')
        self.assertEqual(result['imported'], 1)
        self.assertEqual(self.unpack_result()[0][0], 'FM.MDX')

    def test_list_limits_before_any_music_read(self):
        for text in ('# nothing\n', '音楽/FM.MDX\n' * 301, 'x' * (ingest.LIST_LIMIT + 1)):
            with self.assertRaises(ValueError):
                self.run_import(text)
            self.assertFalse(self.output.exists())

    def test_300_entries_roundtrip(self):
        result = self.run_import('音楽/FM.MDX\n' * 300)
        self.assertEqual(result['imported'], 300)
        self.assertEqual(len(self.unpack_result()), 300)

    def test_local_pdx_wins_and_output_name_with_dot(self):
        shared = self.root / 'shared'
        shared.mkdir()
        (shared / 'samples.pdx').write_bytes(b'invalid')
        self.output = self.root / 'collection.r1'
        result = self.run_import('音楽/PCM.MDX', pdx_dirs=['shared'])
        self.assertEqual(result['imported'], 1)
        self.assertTrue((self.root / 'collection.r1.zip').is_file())
        self.assertTrue(self.unpack_result()[0][2])

    def test_empty_title_and_absolute_local_entry(self):
        path = self.music / 'FM.MDX'
        path.write_bytes(mdx(title=''))
        result = self.run_import(path.as_posix())
        self.assertEqual(result['imported'], 1)
        self.assertEqual(self.unpack_result()[0][0], 'FM.MDX')

    def test_expanded_pair_and_collection_limits(self):
        # Both input files fit individually; their wrapped pair exceeds 800.
        with patch.object(ingest, 'MUSIC_LIMIT', 800):
            result = self.run_import('音楽/PCM.MDX')
        self.assertEqual(result['excluded'], 1)
        self.output = self.root / 'small-collection'
        with patch.object(ingest.prepared_playlist, 'FILE_LIMIT', 200):
            result = self.run_import('音楽/FM.MDX')
        self.assertEqual(result['excluded'], 1)

    def test_structure_and_pair_bounds(self):
        for invalid in (b'', b'\0' * 25, body()[:21]):
            with self.assertRaises(ValueError):
                ingest.validate_mdx(invalid)
        for invalid in (bytes(767), struct.pack('>II', 700, 4) + bytes(764),
                        struct.pack('>II', 768, 50) + bytes(764)):
            with self.assertRaises(ValueError):
                ingest.validate_pdx(invalid)
        with patch.object(ingest, 'MUSIC_LIMIT', 100):
            self.assertEqual(self.run_import('音楽/PCM.MDX')['excluded'], 1)

    def test_cli_failure_and_output_io_failure(self):
        self.playlist.write_text('absent.mdx', encoding='utf-8')
        run = subprocess.run([sys.executable, '-B', str(ROOT/'tools/m3u_import.py'),
                              str(self.playlist), '--output', str(self.output)], capture_output=True)
        self.assertEqual(run.returncode, 1)
        self.assertFalse(self.output.with_suffix('.zip').exists())
        other = self.root / 'other'
        self.playlist.write_text('音楽/FM.MDX', encoding='utf-8')
        with patch.object(Path, 'write_bytes', side_effect=OSError('disk failure')):
            with self.assertRaisesRegex(OSError, 'disk failure'):
                ingest.import_playlist(self.playlist, other)

    @unittest.skipIf(os.name == 'nt', 'Case collisions and symlinks are exercised on Linux')
    def test_case_collision_and_symlink_escape(self):
        (self.music / 'fm.mdx').write_bytes(mdx())
        self.assertEqual(self.run_import('音楽/FM.MDX')['excluded'], 1)
        (self.root/'outside.mdx').write_bytes(mdx())
        (self.music/'link.mdx').symlink_to(self.root/'outside.mdx')
        self.playlist = self.music / 'list.m3u8'
        self.output = self.root / 'links'
        self.assertEqual(self.run_import('link.mdx')['excluded'], 1)


if __name__ == '__main__':
    unittest.main()
