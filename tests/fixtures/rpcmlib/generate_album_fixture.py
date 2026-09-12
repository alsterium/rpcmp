"""Independent specification encoder for the authored 2-album/4-track fixture.

No RPCMP implementation is imported or executed. Run only for reviewed fixture
changes, never as part of the test oracle's build.
"""

import hashlib
from pathlib import Path
import struct
import zlib


def identity(domain, data):
    return int.from_bytes(hashlib.sha256(domain + b'\0' + data).digest()[:8], 'little')


def aligned(data):
    return data + bytes((-len(data)) % 8)


def generate():
    blob = bytes([1, 2, 3])
    blob_id = identity(b'blob', b'MDX ' + blob)
    values = ['', 'Alpha', 'Beta', 'A1', 'A2', 'B1', 'B2', '1/Alpha', '2/Beta']
    strings = {s: identity(b'string', s.encode()) for s in values}
    string_records = b''
    string_data = b''
    for s in sorted(values, key=strings.get):
        raw = s.encode()
        string_records += struct.pack('<QII', strings[s], len(string_data), len(raw))
        string_data += raw
    strs = struct.pack('<IIQ', len(values), 16, 16 + 16 * len(values)) + string_records + string_data
    tracks = {}
    for title, album in [('A1', 'Alpha'), ('A2', 'Alpha'), ('B1', 'Beta'), ('B2', 'Beta')]:
        refs = [strings[title], strings[''], strings[album], 0, 0]
        tid = identity(b'track', b'MDX ' + struct.pack('<Q5Q', blob_id, *refs))
        tracks[title] = (tid, b'MDX ' + struct.pack('<IQ5Q2QIIHB5x', 0, blob_id, *refs,
                                                  2**64-1, 2**64-1, 0, 0, 0, 0))
    ordered = sorted(tracks.values())
    trak = struct.pack('<II', 4, 96) + b''.join(struct.pack('<Q', tid) + body for tid, body in ordered)
    blobs = struct.pack('<IIQ', 1, 48, 64) + struct.pack('<Q4sHHQQQII', blob_id, b'MDX ', 0, 0,
                                                      3, 3, 64, 8, zlib.crc32(blob)) + blob
    deps = struct.pack('<II', 0, 24)
    index = struct.pack('<II', 5, 24)
    for ordinal, (tid, _) in enumerate(ordered):
        index += struct.pack('<IIQII', 1, 0, tid, ordinal, 0)
    index += struct.pack('<IIQII', 2, 0, blob_id, 0, 0)
    csum = struct.pack('<IIIIQQ', 1, 24, 2, zlib.crc32(blob), blob_id, 3)
    # Display Beta before Alpha, and 2 before 1: intentionally not ID order.
    albums = [('2/Beta', 'Beta', ['B2', 'B1']), ('1/Alpha', 'Alpha', ['A2', 'A1'])]
    album_records = []
    for ordinal, (key, name, members) in enumerate(albums):
        aid = identity(b'album', struct.pack('<I', len(key.encode())) + key.encode())
        album_records.append((aid, ordinal, key, name, members))
    albm = struct.pack('<6HIIIQ', 1, 0, 32, 40, 16, 0, 2, 4, 0, 112)
    members_data = b''
    for aid, ordinal, key, name, members in sorted(album_records):
        albm += struct.pack('<QQQIIII', aid, strings[name], strings[key], ordinal,
                            len(members_data) // 16, len(members), 0)
        for track_ordinal, title in enumerate(members):
            members_data += struct.pack('<QII', tracks[title][0], track_ordinal, 0)
    albm += members_data
    assert len(albm) == 176
    payloads = [trak, blobs, strs, deps, index, csum, albm]
    build_id = hashlib.sha256(b''.join(payloads[:6])).digest()[:16]
    file = bytes(80)
    directory = b''
    for tag, payload in zip([b'TRAK', b'BLOB', b'STRS', b'DEPS', b'INDX', b'CSUM', b'ALBM'], payloads):
        file = aligned(file)
        directory += struct.pack('<4sIQQIIQ', tag, int(tag != b'ALBM'), len(file), len(payload),
                                  zlib.crc32(payload), 8, 0)
        file += payload
    file = aligned(file)
    directory_offset = len(file)
    file += directory
    header = bytearray(struct.pack('<8sHHIQQQQII16sII', b'RPCMLIB\0', 1, 0, 80, 0, 0,
                                   len(file), directory_offset, 7, 40, build_id, 0, 0))
    struct.pack_into('<I', header, 72, zlib.crc32(header))
    file = bytes(header) + file[80:]
    return file


if __name__ == '__main__':
    data = generate()
    target = Path(__file__).with_name('album.rpcmlib.hex')
    target.write_text('\n'.join(data[i:i+32].hex() for i in range(0, len(data), 32)) + '\n',
                      encoding='ascii', newline='\n')
    print(f'authored album fixture: {len(data)} bytes, SHA-256 {hashlib.sha256(data).hexdigest()}')
