"""Prepared real-song instances must resolve independent, complete input pairs."""
import json
from pathlib import Path, PurePosixPath as P
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_hybrid_package as package


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
        manifest.write_text(json.dumps(self.tracks), encoding="utf-8")
        return package.track_files(manifest)

    def test_independent_slot_graph_and_japanese_catalog(self):
        files = self.files()
        common = P("Assets/rpcmp_hybrid/common")
        for track in self.tracks:
            path = P("Assets/rpcmp_hybrid/RPCMP.HybridProbe") / (track["id"] + ".json")
            slots = json.loads(files[path])["instance"]["data_slots"]
            self.assertEqual([s["id"] for s in slots], [1, 2, 3, 4] + ([5] if track["pdx"] else []))
            self.assertEqual([s["filename"] for s in slots[:3]], ["os.bin", "hybrid.ini", "hybrid.elf"])
            self.assertEqual(files[common / slots[3]["filename"]], (self.root / track["mdx"]).read_bytes())
            if track["pdx"]:
                self.assertEqual(files[common / slots[4]["filename"]], (self.root / "pdx.bin").read_bytes())
            self.assertIn(P("試聴用") / (track["id"] + ".wav"), files)
        catalog = files[P("曲目一覧.md")].decode("utf-8")
        self.assertIn("試験曲 FM", catalog)
        self.assertIn("試験曲 PCM", catalog)
        self.assertIn("02-mixed.json", catalog)

    def test_required_pdx_cannot_be_silently_omitted_or_added(self):
        self.tracks[1]["pdx"] = None
        with self.assertRaisesRegex(ValueError, "PDX"):
            self.files()
        self.tracks = self.tracks[:1]
        self.tracks[0]["pdx"] = "pdx.bin"
        with self.assertRaisesRegex(ValueError, "PDX"):
            self.files()

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

    def test_ids_counts_and_paths(self):
        original = self.tracks
        for bad_id in ("../escape", "hybrid/one", "", "a" * 21):
            self.tracks = [dict(original[0], id=bad_id)]
            with self.assertRaises(ValueError):
                self.files()
        for tracks in ([], original * 9, [original[0], dict(original[1], id="01-FM")]):
            self.tracks = tracks
            with self.assertRaises(ValueError):
                self.files()
        self.tracks = [dict(original[0], mdx="../outside.bin")]
        with self.assertRaises(ValueError):
            self.files()


if __name__ == "__main__":
    unittest.main()
