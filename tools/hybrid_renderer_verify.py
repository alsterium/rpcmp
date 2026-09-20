"""Compare the bounded CPU renderer with separately captured reference streams."""
import argparse
import ctypes as c
import json
from pathlib import Path
import struct

from mdx_hybrid_probe import unpack


class Event(c.Structure):
    _fields_ = [("sample", c.c_uint32), ("operation", c.c_uint32)]


class Block(c.Structure):
    _fields_ = [(name, c.c_uint32) for name in ("first_frame", "frame_count", "event_count", "ended")]
    _fields_ += [("events", Event * 1024), ("pcm", c.c_int32 * 3072)]


def wrapped(case, oracle):
    raw = Path(case["mdx"]).read_bytes()
    end = raw.index(b"\0", raw.index(b"\x1a", raw.index(b"\r\n")))
    pdx = unpack(Path(case["pdx"]).read_bytes(), oracle) if case.get("pdx") else None
    mdx = bytes.fromhex("00000000000a00080000" if pdx is not None else "0000ffff000a00080000")
    mdx += unpack(raw[end + 1:], oracle)
    pdx = bytes.fromhex("00000000000a00020000") + pdx if pdx is not None else b""
    return mdx, pdx


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--oracle", type=Path, required=True)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    lib = c.CDLL(str(args.library.resolve()))
    lib.rpcmp_hybrid_open.argtypes = [c.c_void_p, c.c_uint32, c.c_void_p, c.c_uint32]
    lib.rpcmp_hybrid_authored.argtypes = [c.c_uint]
    lib.rpcmp_hybrid_render.restype = c.POINTER(Block)
    cases = [{"id": name, "kind": kind, "blocks": 32} for name, kind in
             (("mixed8", 4), ("pcm16x8", 5), ("pcm8x8", 6))]
    if args.manifest:
        cases += [dict(item, id=f"private-{i:03d}") for i, item in enumerate(json.loads(args.manifest.read_text()))]
    for case in cases:
        if "kind" in case:
            assert lib.rpcmp_hybrid_authored(case["kind"]) == 0
        else:
            mdx, pdx = wrapped(case, args.oracle)
            a, b = c.create_string_buffer(mdx + bytes(16)), c.create_string_buffer(pdx + bytes(16))
            assert lib.rpcmp_hybrid_open(a, len(mdx), b, len(pdx)) == 0
        pcm, events, chunks = bytearray(), [], []
        peak_events = 0
        for index in range(case.get("blocks", 480)):
            result = lib.rpcmp_hybrid_render()
            assert result, f"render failed at {index}"
            block = result.contents
            assert block.first_frame == len(pcm) // 8
            assert 0 < block.frame_count <= 1536 and block.event_count <= 1024
            # 125/96 native-to-output ratio with carried fractional phase.
            assert block.first_frame + block.frame_count == (index + 1) * 1024 * 125 // 96
            events += [(e.sample, e.operation >> 8, e.operation & 255) for e in block.events[:block.event_count]]
            assert all(block.first_frame <= e.sample <= block.first_frame + block.frame_count
                       for e in block.events[:block.event_count])
            pcm.extend(bytes(block.pcm)[:block.frame_count * 8])
            chunks.append(block.frame_count)
            peak_events = max(peak_events, block.event_count)
        prefix = args.oracle / (case["id"] + "-split")
        assert pcm == prefix.with_suffix(".pcm").read_bytes()[:len(pcm)], "wide PCM differs"
        expected = list(struct.iter_unpack("<QII", prefix.with_suffix(".events").read_bytes()))
        if "kind" in case:
            # PCM-only reference cases have the same eight PCM voices; check FM
            # initialization against the separately captured eight-FM mixed case.
            expected = list(struct.iter_unpack("<QII", (args.oracle / "mixed8-split.events").read_bytes()))
            expected = [event for event in expected if event[0] < len(pcm) // 8]
        if events != expected:
            first = next((i for i, pair in enumerate(zip(events, expected)) if pair[0] != pair[1]),
                         min(len(events), len(expected)))
            raise AssertionError(f"timed FM events differ: counts {len(events)}/{len(expected)}, "
                                 f"first {first}: {events[first:first+3]} / {expected[first:first+3]}")
        expected_chunks = []
        inner_total = outer_total = 0
        for inner, outer in struct.iter_unpack("<II", prefix.with_suffix(".chunks").read_bytes()):
            inner_total += inner
            outer_total += outer
            assert outer_total <= 1024
            if outer_total == 1024:
                expected_chunks.append(inner_total)
                inner_total = outer_total = 0
        assert inner_total == outer_total == 0
        assert chunks == expected_chunks[:len(chunks)], "chunk positions differ"
        lib.rpcmp_hybrid_close()
        assert not lib.rpcmp_hybrid_render(), "closed renderer produced a block"
        print(json.dumps(dict(case=case["id"], blocks=len(chunks), frames=len(pcm)//8,
                              events=len(events), max_block_events=peak_events, identical=True)), flush=True)
    for kind in (0, 3, 7, 0xffffffff):
        assert lib.rpcmp_hybrid_authored(kind) != 0
    for mdx_size, pdx_size in ((0, 0), (9, 0), (16777217, 0), (10, 1), (10, 16777217)):
        assert lib.rpcmp_hybrid_open(None, mdx_size, None, pdx_size) != 0
    print("PASS bounded renderer/reference streams and closed/invalid input cases")


if __name__ == "__main__":
    main()
