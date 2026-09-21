"""Authored loop/end witnesses for the optional built reference renderer (Linux)."""
import argparse
import ctypes as c
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from hybrid_renderer_verify import Block


def song(loop):
    # Nine streams. Channel A: tempo, one intro marker + 10 ticks, then
    # a body marker + 30 ticks. The signed F1 back edge spans exactly 7 bytes.
    first = bytes.fromhex("ffc8fe1b1109fe1b221d")
    first += bytes.fromhex("f1fff9" if loop else "f100")
    streams = [first] + [bytes.fromhex("f100")] * 8
    offsets, payload = [], b""
    for stream in streams:
        offsets.append(20 + len(payload))
        payload += stream
    return bytes.fromhex("0000ffff000a00080000") + struct.pack(">10H", 20 + len(payload), *offsets) + payload + bytes(27)


class RendererTests(unittest.TestCase):
    def render(self, data, continuous=False):
        buf = c.create_string_buffer(data + bytes(16))
        self.assertEqual(LIB.rpcmp_hybrid_open(buf, len(data), None, 0), 0)
        frames, events, chunks = 0, [], 0
        for _ in range(400):
            pointer = LIB.rpcmp_hybrid_render()
            self.assertTrue(pointer)
            block = pointer.contents
            self.assertEqual(block.first_frame, frames)
            self.assertLessEqual(block.frame_count, 1536)
            self.assertLessEqual(block.event_count, 1024)
            self.assertTrue(block.frame_count or block.ended)
            events += [(e.sample, e.operation) for e in block.events[:block.event_count]]
            frames += block.frame_count
            chunks += 1
            if block.ended:
                self.assertFalse(continuous, "loop policy must not truncate the renderer")
                break
        else:
            self.assertTrue(continuous, "bounded song did not terminate")
        if not continuous:
            self.assertFalse(LIB.rpcmp_hybrid_render())
        LIB.rpcmp_hybrid_close()
        self.assertEqual(events, sorted(events, key=lambda e: e[0]))
        self.assertTrue(all(at <= frames for at, _ in events))
        return frames, events, chunks

    def test_each_complete_body_is_marked_without_an_automatic_fade_or_end(self):
        frames, events, _ = self.render(song(True), continuous=True)
        intro = [at for at, op in events if op == 0x1b11]
        bodies = [at for at, op in events if op == 0x1b22]
        loops = [at for at, op in events if op == 0x10001]
        self.assertEqual(len(intro), 1)
        self.assertGreaterEqual(len(bodies), 8)
        self.assertEqual(loops, bodies[1:])  # Each following body starts after a complete loop.
        self.assertGreater(bodies[0], intro[0])
        self.assertGreater(bodies[1], bodies[0])
        self.assertGreater(frames, loops[1] + 5 * 62500)

    def test_natural_end_once_without_fade(self):
        frames, events, _ = self.render(song(False))
        self.assertEqual(sum(op == 0x1b11 for _, op in events), 1)
        self.assertEqual(sum(op == 0x1b22 for _, op in events), 1)
        self.assertNotIn(0x10001, [op for _, op in events])
        self.assertLess(frames, 62500)

    def test_reopening_clears_loop_and_completed_state(self):
        before = self.render(song(True), continuous=True)
        self.render(song(False))
        self.assertEqual(self.render(song(True), continuous=True), before)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    args, rest = parser.parse_known_args()
    LIB = c.CDLL(str(args.library.resolve()))
    LIB.rpcmp_hybrid_open.argtypes = [c.c_void_p, c.c_uint32, c.c_void_p, c.c_uint32]
    LIB.rpcmp_hybrid_render.restype = c.POINTER(Block)
    unittest.main(argv=[__file__, *rest])
