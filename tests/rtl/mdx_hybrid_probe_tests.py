"""Focused acceptance of the optional, built native-JT51 offline probe."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("hybrid", ROOT / "tools/mdx_hybrid_probe.py")
PROBE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROBE)


class HybridProbeTests(unittest.TestCase):
    def replay(self, data, frames=100):
        with tempfile.TemporaryDirectory(dir=OPTIONS.out) as temp:
            trace, audio = Path(temp) / "events", Path(temp) / "audio"
            trace.write_bytes(data)
            result = subprocess.run([str(OPTIONS.out / "obj/Vjt51"), str(trace), str(frames), str(audio),
                                     str(Path(temp) / "wide")],
                                    capture_output=True, text=True, timeout=45)
            return result, audio.read_bytes() if audio.exists() else None

    def test_silence_and_prefix_endpoint(self):
        # The last event is outside the audible prefix, so only the first is sent.
        result, audio = self.replay(struct.pack("<QIIQII", 0, 8, 0, 100, 8, 120))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["writes"], 1)
        self.assertEqual(audio, bytes(400))

    def test_malformed_traces(self):
        cases = [b"\0", bytes(9), struct.pack("<QII", 0, 256, 0),
                 struct.pack("<QII", 0, 0, 256), struct.pack("<QII", 101, 8, 0),
                 struct.pack("<QIIQII", 2, 8, 0, 1, 8, 0), bytes(16) * 131073]
        for data in cases:
            with self.subTest(length=len(data)):
                result, audio = self.replay(data)
                self.assertNotEqual(result.returncode, 0)
                self.assertIsNone(audio)
        for count in (0, 62500 * 21 + 1):
            result, audio = self.replay(b"", count)
            self.assertNotEqual(result.returncode, 0)
            self.assertIsNone(audio)

    def test_clean_start_repeats_native_audio(self):
        trace = (OPTIONS.out / "fm1.events").read_bytes()
        previous = (OPTIONS.out / "fm1.jt").read_bytes()
        result, audio = self.replay(trace, len(previous) // 4)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(audio, previous)

    def test_reference_resampler_analytical_positions(self):
        # 62500/48000 = 125/96; zero-order selection takes floor(i*125/96).
        # Split after 5 output frames to exercise the retained fractional phase.
        values = [value for i in range(125) for value in (i, -i)]
        actual = PROBE.convert(PROBE.load_library(OPTIONS.out), values, [(6, 5), (119, 91)])
        expected = b"".join(struct.pack("<hh", i*125//96, -(i*125//96)) for i in range(96))
        self.assertEqual(actual, expected)
        for invalid in ([(1, 1024)], [(125, 96), (1, 1)]):
            with self.assertRaises(ValueError):
                PROBE.convert(PROBE.load_library(OPTIONS.out), values, invalid)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    OPTIONS, rest = parser.parse_known_args()
    unittest.main(argv=[__file__, *rest])
