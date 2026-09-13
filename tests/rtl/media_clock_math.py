"""Integer proof for the native sample window in pocket-enveloped-audio-v1.

This is independent of the RTL recurrence. The companion native phase bench
checks the source-edge equation against JT51, including its first sample.
"""
from math import gcd
import unittest


AUDIO_HZ = 12_288_000
CHIP_HZ = 3_579_545
SELECTION_STEP = 48_000 * 64


def ceil_div(numerator: int, denominator: int) -> int:
    return (numerator + denominator - 1) // denominator


class MediaClockMath(unittest.TestCase):
    def test_initial_state_bounds(self):
        # After reset cur=0/zero=0. At first retained edge, A is the clock
        # accumulator, C its carry phase, and p its stored half-rate enable.
        # p=1 implies C=0 and A<CHIP_HZ. Include all A for p=0; this safely
        # over-approximates reset/hold phases without requiring a warmup length.
        # S_j = ceil((64*j*AUDIO_HZ + offset)/CHIP_HZ).
        for pulse, phase, max_accumulator in (
            (0, 0, AUDIO_HZ - 1),
            (0, 1, AUDIO_HZ - 1),
            (1, 0, CHIP_HZ - 1),
        ):
            offset = (2 - 2 * pulse - phase) * AUDIO_HZ
            self.assertGreaterEqual(offset - max_accumulator, -(CHIP_HZ - 1))
            self.assertLessEqual(offset, 2 * AUDIO_HZ)

    def test_complete_selection_period(self):
        common = gcd(CHIP_HZ, SELECTION_STEP)
        frames = SELECTION_STEP // common
        native_samples = CHIP_HZ // common
        self.assertEqual((frames, native_samples), (614_400, 715_909))
        # Advancing this complete period translates both sample bounds by
        # exactly this many retained edges, so the finite check covers all k.
        self.assertEqual(64 * native_samples * AUDIO_HZ, 256 * frames * CHIP_HZ)
        for k in range(1, frames + 1):
            j = ceil_div(k * CHIP_HZ, SELECTION_STEP)
            earliest = ceil_div(64 * j * AUDIO_HZ - (CHIP_HZ - 1), CHIP_HZ)
            latest = ceil_div((64 * j + 2) * AUDIO_HZ, CHIP_HZ)
            self.assertLessEqual(256 * k, earliest)
            self.assertLessEqual(earliest, latest)
            self.assertLessEqual(latest, 256 * (k + 1) - 29)
            self.assertEqual(ceil_div((k + frames) * CHIP_HZ, SELECTION_STEP),
                             j + native_samples)


if __name__ == "__main__":
    unittest.main()
