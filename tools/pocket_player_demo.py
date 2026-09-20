#!/usr/bin/env python3
"""Create an original, synthetic FM scale corpus for the M6 hardware candidate."""

import argparse
import struct
from pathlib import Path


def mdx(title, notes, loop):
    # Tempo 200, voice 0, stereo, volume 15. The only keyed carrier's
    # effective TL is 10 + 2 = 12 (previously 30 + 21 = 51). This sets the
    # authored test tone's level, without changing player gain for real music.
    track = bytearray([0xff, 200, 0xfd, 0, 0xfc, 3, 0xfb, 15])
    start = len(track)
    for note in notes:
        track.extend((0x80 + note, 47))
    if loop:
        track.append(0xf1)
        track.extend(struct.pack('>h', start - (len(track) + 2)))
    else:
        track.extend((0xf1, 0))
    offsets = [20 + len(track) + 16, 20]
    offsets.extend(20 + len(track) + i * 2 for i in range(8))
    voice = bytes([0, 7, 1] + [1] * 4 + [10, 127, 127, 127] +
                  [31] * 4 + [0] * 4 + [0] * 4 + [15] * 4)
    return (title.encode('cp932') + b'\r\n\x1a\0' + struct.pack('>10H', *offsets) +
            track + b'\xf1\0' * 8 + voice)


def create(output):
    if output.exists() or output.is_symlink():
        raise ValueError('demo output already exists; preserve it')
    for album, filename, title, notes, loop in (
        ('01-FMテスト', '01-loop.mdx', '上昇ループ', [36, 40, 43, 48, 43, 40, 36, 36], True),
        ('01-FMテスト', '02-end.mdx', '下降・終端', [48, 43, 40, 36], False),
        ('02-操作確認', '01-loop.mdx', '低音ループ', [24, 24, 31, 31, 28, 28, 24, 24], True),
    ):
        folder = output / album
        folder.mkdir(parents=True, exist_ok=True)
        (folder / filename).write_bytes(mdx(title, notes, loop))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    create(parser.parse_args().output)
