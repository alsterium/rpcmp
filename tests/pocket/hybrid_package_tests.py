"""Prepared real-song instances must resolve independent, complete input pairs."""
import json
from pathlib import Path, PurePosixPath as P
import sys
import struct
import zlib
import subprocess
import tempfile
import unittest
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_hybrid_package as package

CHECKER = None
if "--checker" in sys.argv:
    index = sys.argv.index("--checker")
    CHECKER = sys.argv[index + 1]
    del sys.argv[index:index + 2]


class HybridPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "fm.bin").write_bytes(bytes.fromhex("0000ffff000a00080000") + b"FM")
        (self.root / "mixed.bin").write_bytes(bytes.fromhex("00000000000a00080000") + b"FM+PCM")
        (self.root / "pdx.bin").write_bytes(bytes.fromhex("00000000000a00020000") + b"PCM")
        with wave.open(str(self.root / "reference.wav"), "wb") as wav:
            wav.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
            wav.writeframes(b"\x01\x00\x02\x00" * 100)
        self.tracks = [dict(id="01-fm", title="試験曲 FM", mdx="fm.bin", pdx=None,
                            reference="reference.wav"),
                       dict(id="02-mixed", title="試験曲 PCM", mdx="mixed.bin", pdx="pdx.bin",
                            reference="reference.wav")]

    def files(self):
        manifest = self.root / "tracks.json"
        manifest.write_text(json.dumps([dict(name="試験曲集", tracks=self.tracks)]), encoding="utf-8")
        return package.track_files(manifest)

    def test_single_playlist_index_payloads_and_japanese_catalog(self):
        files = self.files()
        common = P("Assets/rpcmp_minimal/common")
        path = P("Assets/rpcmp_minimal/RPCMP.MinimalPlayer/Playlist.json")
        slots = json.loads(files[path])["instance"]["data_slots"]
        self.assertEqual([s["id"] for s in slots], [1, 2, 3, 4])
        self.assertEqual(slots[3]["filename"], "playlist.hpl")
        packed = files[common / "playlist.hpl"]
        header = struct.unpack_from("<4s11I", packed)
        self.assertEqual(header[:7], (b"HPL2", 2, 112, 128, 1, 2, len(packed)))
        self.assertEqual(header[7], zlib.crc32(packed[48:416]))
        self.assertEqual(header[8], zlib.crc32(packed[:32]))
        self.assertEqual(header[9:], (0, 0, 0))
        for i, track in enumerate(self.tracks):
            offset, length, crc, poff, plen, pcrc, titlelen = struct.unpack_from("<7I", packed, 160 + i * 128)
            self.assertEqual(packed[offset:offset+length], (self.root / track["mdx"]).read_bytes())
            self.assertEqual(crc, zlib.crc32(packed[offset:offset+length]))
            if track["pdx"]:
                self.assertEqual(poff, offset + length)
                self.assertEqual(packed[poff:poff+plen], (self.root / track["pdx"]).read_bytes())
                self.assertEqual(pcrc, zlib.crc32(packed[poff:poff+plen]))
            else:
                self.assertEqual((poff, plen, pcrc), (0, 0, 0))
            title = packed[188 + i*128:188 + i*128 + titlelen].decode("utf-8")
            self.assertEqual(title, track["title"])
        catalog = files[P("曲目一覧.md")].decode("utf-8")
        self.assertIn("試験曲 PCM", catalog)
        self.assertIn("Playlist.json", catalog)
        self.assertIn("Xで一覧と操作パネルを切り替えます", catalog)
        self.assertIn("Bでプレイリスト一覧へ戻ります", catalog)
        self.assertNotIn("Xで一時停止", catalog)
        self.assertNotIn("X/Yには操作を割り当てていません", catalog)
        self.assertIn("L/R/Yには操作を割り当てていません", catalog)

    def test_required_pdx_cannot_be_silently_omitted_or_added(self):
        self.tracks[1]["pdx"] = None
        with self.assertRaisesRegex(ValueError, "PDX"):
            self.files()
        self.tracks = self.tracks[:1]
        self.tracks[0]["pdx"] = "pdx.bin"
        with self.assertRaisesRegex(ValueError, "PDX"):
            self.files()

    def test_oversize_manifest_is_rejected_before_parsing(self):
        manifest = self.root / "oversize.json"
        manifest.write_bytes(b"[" + b" " * (16 * 1024 * 1024))
        with self.assertRaisesRegex(ValueError, "manifest too large"):
            package.track_files(manifest)

    def test_runtime_update_preserves_installed_collection(self):
        files = self.files()
        runtime = P("Assets/rpcmp_minimal/common/hybrid.elf")
        core = P("Cores/RPCMP.MinimalPlayer/rpcmp.rbf_r")
        guide = P("確認手順.md")
        files.update({runtime: b"new application", core: b"new FPGA", guide: "日本語手順".encode()})
        playlist = P("Assets/rpcmp_minimal/common/playlist.hpl")
        installed = {playlist: b"user's existing collection"}
        update = package.runtime_update(files)
        installed.update(update)
        self.assertEqual(installed[playlist], b"user's existing collection")
        self.assertEqual(installed[runtime], b"new application")
        self.assertEqual(installed[core], b"new FPGA")
        self.assertEqual(installed[guide], "日本語手順".encode())
        self.assertNotIn(P("曲目一覧.md"), update)
        self.assertFalse(any(path.parts[0] == "試聴用" for path in update))

    def test_invalid_wrappers_audio_and_missing_files(self):
        for name, invalid in (("fm.bin", b"bad"), ("pdx.bin", bytes(10)),
                              ("reference.wav", b"not audio")):
            path = self.root / name
            before = path.read_bytes()
            path.write_bytes(invalid)
            with self.assertRaises(ValueError):
                self.files()
            path.write_bytes(before)
        (self.root / "pdx.bin").unlink()
        with self.assertRaises(FileNotFoundError):
            self.files()

    def test_counts_titles_and_paths(self):
        original = self.tracks
        for tracks in ([], original * 151):
            self.tracks = tracks
            with self.assertRaises(ValueError):
                self.files()
        self.tracks = [dict(original[0], reference=None)] * 300
        packed = self.files()[P("Assets/rpcmp_minimal/common/playlist.hpl")]
        self.assertEqual(struct.unpack_from("<I", packed, 20)[0], 300)
        if CHECKER:
            path = self.root / "all.hpl"
            path.write_bytes(packed)
            subprocess.run([CHECKER, "--playlist", str(path)], check=True)
        for title in ("", "bad\nname", "bad\0name", "x" * 256):
            self.tracks = [dict(original[0], title=title)]
            with self.assertRaises(ValueError):
                self.files()
        self.tracks = [dict(original[0], title="日本語" * 40)]
        packed = self.files()[P("Assets/rpcmp_minimal/common/playlist.hpl")]
        length = struct.unpack_from("<I", packed, 184)[0]
        self.assertLessEqual(length, 96)
        self.assertTrue(packed[188:188+length].decode("utf-8").endswith("…"))
        self.tracks = [dict(original[0], mdx="../outside.bin")]
        with self.assertRaises(ValueError):
            self.files()

    def test_package_to_runtime_and_corrupt_payload(self):
        if CHECKER is None:
            self.skipTest("pass --checker for native runtime readback")
        packed = self.files()[P("Assets/rpcmp_minimal/common/playlist.hpl")]
        path = self.root / "playlist.hpl"
        path.write_bytes(packed)
        subprocess.run([CHECKER, "--playlist", str(path)], check=True)
        path.write_bytes(packed[:-1] + bytes([packed[-1] ^ 1]))
        result = subprocess.run([CHECKER, "--playlist", str(path)], check=False)
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
