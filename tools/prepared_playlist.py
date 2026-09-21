"""Write bounded HPL2 playlists, sharing identical prepared MDX/PDX payloads."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import zlib

PAIR_LIMIT = 16 * 1024 * 1024
FILE_LIMIT = 0x7fffffff
LIST_LIMIT = 100
TRACK_LIMIT = 300


def read_local(manifest, name, maximum=PAIR_LIMIT):
    if not isinstance(name, str) or not name:
        raise ValueError("missing prepared filename")
    path = (manifest.parent / name).resolve()
    if not path.is_relative_to(manifest.parent.resolve()):
        raise ValueError("track file leaves prepared directory")
    if not 10 <= path.stat().st_size <= maximum:
        raise ValueError("prepared input outside bounds")
    with path.open("rb") as source:
        data = source.read(maximum + 1)
    if not 10 <= len(data) <= maximum:
        raise ValueError("prepared input changed size")
    return data


def title_bytes(title):
    if not isinstance(title, str) or not 1 <= len(title) <= 255 or any(ord(c) < 32 or ord(c) == 127 for c in title):
        raise ValueError("invalid title")
    title = title.encode("utf-8")
    if len(title) > 96:
        title = title[:93].decode("utf-8", errors="ignore").encode("utf-8") + "…".encode("utf-8")
    return title


def write_collection(manifest, playlists, output):
    """Stream at most one bounded blob at a time; keep only the small index."""
    if not isinstance(playlists, list) or not 1 <= len(playlists) <= LIST_LIMIT:
        raise ValueError("choose 1 to 100 playlists")
    lists, tracks = bytearray(), []
    for playlist in playlists:
        if not isinstance(playlist, dict) or not isinstance(playlist.get("tracks"), list):
            raise ValueError("invalid playlist")
        entries = playlist["tracks"]
        if not 1 <= len(entries) <= TRACK_LIMIT or any(not isinstance(t, dict) for t in entries):
            raise ValueError("choose 1 to 300 tracks per playlist")
        name = title_bytes(playlist.get("name"))
        lists.extend(struct.pack("<3I", len(tracks) + 1, len(entries), len(name)) + name.ljust(96, b"\0") + bytes(4))
        tracks.extend(dict(entry, playlist=playlist["name"]) for entry in entries)
    base = 48 + len(lists) + 128 * len(tracks)
    if base > FILE_LIMIT:
        raise ValueError("曲集が容量上限を超えます (2 GiB未満)")
    index, cached_paths, blobs = bytearray(lists), {}, {}
    with Path(output).open("xb") as target:
        target.write(bytes(base))

        def blob(name):
            if not isinstance(name, str) or not name:
                raise ValueError("missing prepared filename")
            if name in cached_paths:
                return cached_paths[name]
            data = read_local(manifest, name)
            identity = hashlib.sha256(data).digest()
            if identity not in blobs:
                offset = target.tell()
                if len(data) > FILE_LIMIT - offset:
                    raise ValueError("曲集が容量上限を超えます (2 GiB未満)")
                target.write(data)
                blobs[identity] = (offset, len(data), zlib.crc32(data), data[:10])
            cached_paths[name] = blobs[identity]
            return blobs[identity]

        for track in tracks:
            title = title_bytes(track.get("title"))
            a = blob(track.get("mdx"))
            b = blob(track["pdx"]) if track.get("pdx") else (0, 0, 0, b"")
            if a[1] + b[1] > PAIR_LIMIT:
                raise ValueError("MDX/PDX pair exceeds 16 MiB")
            expected = bytes.fromhex("00000000000a00080000" if b[1] else "0000ffff000a00080000")
            if a[3] != expected:
                raise ValueError("MDX/PDX wrapper or dependency mismatch")
            if b[1] and b[3] != bytes.fromhex("00000000000a00020000"):
                raise ValueError("invalid prepared PDX wrapper")
            index.extend(struct.pack("<7I", *a[:3], *b[:3], len(title)) + title.ljust(96, b"\0") + bytes(4))
        header = struct.pack("<4s7I", b"HPL2", 2, 112, 128, len(playlists), len(tracks),
                             target.tell(), zlib.crc32(index))
        target.seek(0)
        target.write(header + struct.pack("<I", zlib.crc32(header)) + bytes(12))
        target.write(index)
    return tracks


def pack(manifest):
    """Small local hardware package; the M3U CLI uses the streaming writer."""
    manifest = Path(manifest)
    with manifest.open("rb") as source:
        encoded = source.read(16 * 1024 * 1024 + 1)
    if len(encoded) > 16 * 1024 * 1024:
        raise ValueError("track manifest too large")
    playlists = json.loads(encoded.decode("utf-8"))
    with tempfile.TemporaryDirectory(prefix="rpcmp-pack-") as temporary:
        output = Path(temporary) / "playlist.hpl"
        tracks = write_collection(manifest, playlists, output)
        return output.read_bytes(), tracks
