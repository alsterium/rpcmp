"""Pack bounded, predecoded MDX/PDX inputs for the minimal Pocket player (HPL1)."""
import json
from pathlib import Path
import struct
import zlib

PAIR_LIMIT = 16 * 1024 * 1024
FILE_LIMIT = 512 * 1024 * 1024


def read_local(manifest, name, maximum=PAIR_LIMIT):
    if not isinstance(name, str) or not name:
        raise ValueError("missing prepared filename")
    path = (manifest.parent / name).resolve()
    if not path.is_relative_to(manifest.parent.resolve()):
        raise ValueError("track file leaves prepared directory")
    if not 10 <= path.stat().st_size <= maximum:
        raise ValueError("prepared input outside bounds")
    data = path.read_bytes()
    if not 10 <= len(data) <= maximum:
        raise ValueError("prepared input changed size")
    return data


def pack(manifest):
    manifest = Path(manifest)
    if manifest.stat().st_size > 1024 * 1024:
        raise ValueError("track manifest too large")
    tracks = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(tracks, list) or not 1 <= len(tracks) <= 300:
        raise ValueError("choose 1 to 300 prepared tracks")
    index, payload = bytearray(), bytearray()
    base = 32 + len(tracks) * 128
    for track in tracks:
        title = track["title"]
        if not isinstance(title, str) or not 1 <= len(title) <= 255 or any(ord(c) < 32 or ord(c) == 127 for c in title):
            raise ValueError("invalid track title")
        title = title.encode("utf-8")
        if len(title) > 96:
            title = title[:93].decode("utf-8", errors="ignore").encode("utf-8") + "…".encode("utf-8")
        mdx = read_local(manifest, track["mdx"])
        pdx = read_local(manifest, track["pdx"]) if track.get("pdx") else b""
        if len(mdx) + len(pdx) > PAIR_LIMIT:
            raise ValueError("MDX/PDX pair exceeds 16 MiB")
        expected = bytes.fromhex("00000000000a00080000" if pdx else "0000ffff000a00080000")
        if mdx[:10] != expected:
            raise ValueError("MDX/PDX wrapper or dependency mismatch")
        if pdx and pdx[:10] != bytes.fromhex("00000000000a00020000"):
            raise ValueError("invalid prepared PDX wrapper")
        offset = base + len(payload)
        if offset + len(mdx) + len(pdx) > FILE_LIMIT:
            raise ValueError("playlist exceeds 512 MiB")
        index.extend(struct.pack("<7I", offset, len(mdx), zlib.crc32(mdx),
                                 offset + len(mdx) if pdx else 0, len(pdx), zlib.crc32(pdx), len(title)))
        index.extend(title.ljust(96, b"\0") + bytes(4))
        payload.extend(mdx)
        payload.extend(pdx)
    header = struct.pack("<4s5I", b"HPL1", 1, 128, len(tracks), base + len(payload), zlib.crc32(index))
    return header + struct.pack("<2I", zlib.crc32(header), 0) + index + payload, tracks
