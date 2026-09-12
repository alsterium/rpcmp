"""Native folder/transaction checks with an independent ALBM byte inspector."""

import os
import pathlib
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import zlib


def inspect(data):
    assert data[:8] == b'RPCMLIB\0' and len(data) == struct.unpack_from('<Q', data, 32)[0]
    directory, count, stride = struct.unpack_from('<QII', data, 40)
    assert count == 7 and stride == 40
    sections = {}
    for i in range(count):
        tag, flags, offset, length, crc = struct.unpack_from('<4sIQQI', data, directory + i * stride)
        payload = data[offset:offset + length]
        assert len(payload) == length and zlib.crc32(payload) == crc
        assert flags == (0 if tag == b'ALBM' else 1)
        sections[tag] = payload
    strings = sections[b'STRS']
    count, stride, area = struct.unpack_from('<IIQ', strings)
    assert stride == 16
    names = {}
    for i in range(count):
        identity, offset, length = struct.unpack_from('<QII', strings, 16 + i * stride)
        names[identity] = strings[area + offset:area + offset + length].decode('utf-8')
    tracks = sections[b'TRAK']
    count, stride = struct.unpack_from('<II', tracks)
    assert stride == 96
    titles = {}
    for i in range(count):
        start = 8 + i * stride
        identity = struct.unpack_from('<Q', tracks, start)[0]
        title, artist, owner = struct.unpack_from('<3Q', tracks, start + 24)
        assert names[artist] == ''
        titles[identity] = (names[title], owner)
    albm = sections[b'ALBM']
    major, minor, header, stride, member_stride, reserved, count, members, reserved2, area = struct.unpack_from('<6HIIIQ', albm)
    assert (major, minor, header, stride, member_stride) == (1, 0, 32, 40, 16)
    assert area == 32 + count * 40 and len(albm) == area + members * 16
    albums = []
    seen = set()
    for i in range(count):
        identity, name, key, display, first, size, reserved = struct.unpack_from('<QQQIIII', albm, 32 + i * 40)
        ordered = []
        for j in range(size):
            track, ordinal, reserved = struct.unpack_from('<QII', albm, area + (first + j) * 16)
            assert ordinal == j and titles[track][1] == name and track not in seen
            ordered.append(titles[track][0])
            seen.add(track)
        albums.append((display, names[name], names[key], identity, ordered))
    assert len(seen) == members == len(titles)
    return sorted(albums)


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    fixture = bytes.fromhex(pathlib.Path(sys.argv[2]).read_text(encoding='ascii'))
    body = fixture[fixture.index(b'\r\n\x1a'):]
    def mdx(title):
        return title + body

    with tempfile.TemporaryDirectory(prefix='rpcmp-album-') as temporary:
        base = pathlib.Path(temporary)
        root = base / '曲集'
        root.mkdir()
        output = base / '日本語.rpcmlib'
        def invoke(folder=root, destination=output, code=0):
            result = subprocess.run([str(executable), str(folder), str(destination)], capture_output=True)
            assert result.returncode == code, (result.returncode, result.stdout, result.stderr)
            assert not list(destination.parent.glob('*.rpcmp-tmp-*'))
            return result.stdout.decode('utf-8'), result.stderr.decode('utf-8')
        def put(relative, data):
            path = root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            return path

        sources = {}
        for name, title in [('00.mdx', b'root'), ('02-か\u3099/10.mdx', b'alpha'),
                            ('02-か\u3099/2.MdX', b'beta'), ('02-か\u3099/01.mdx', b'omega'),
                            ('10/1.mdx', b'last')]:
            path = put(name, mdx(title))
            sources[path] = mdx(title)
        put('notes.txt', b'ignored')
        invoke(folder=str(root) + os.sep)
        first = output.read_bytes()
        albums = inspect(first)
        assert [(a[1], a[2], a[4]) for a in albums] == [
            ('曲集', '.', ['root']), ('02-が', '02-が', ['omega', 'beta', 'alpha']),
            ('10', '10', ['last'])]
        invoke()
        assert output.read_bytes() == first
        moved = base / 'elsewhere' / root.name
        shutil.copytree(root, moved)
        invoke(folder=moved)
        assert output.read_bytes() == first
        assert all(path.read_bytes() == data for path, data in sources.items())

        put('broken.mdx', b'bad')
        pcm = bytearray(mdx(b'pcm'))
        # BASE is title+delimiter+empty-PDX. The authored P offset is 0x30.
        pcm[3 + 4 + 0x30] = 0xe8
        put('pcm.mdx', pcm)
        text, _ = invoke(code=2)
        assert 'exclude=malformed-mdx' in text and 'exclude=pcm-not-supported' in text
        assert 'accepted=5 excluded=2 skipped=1' in text and output.read_bytes() == first
        put('empty-title.mdx', mdx(b''))
        text, _ = invoke(code=2)
        assert 'title-fallback=empty' in text and 'accepted=6 excluded=2' in text

        outside = base / 'outside'
        outside.mkdir()
        sentinel = outside / 'outside.mdx'
        sentinel.write_bytes(mdx(b'outside'))
        link = root / 'linked'
        if os.name == 'nt':
            created = subprocess.run(['cmd', '/d', '/c', 'mklink', '/J', str(link), str(outside)],
                                     capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            assert created.returncode == 0, created.stderr
        else:
            link.symlink_to(outside, target_is_directory=True)
        try:
            text, _ = invoke(code=2)
            assert 'skip=link path="linked"' in text
            assert all('outside' not in a[4] for a in inspect(output.read_bytes()))
            invoke(folder=link, code=1)
            invoke(destination=link, code=1)
            assert sentinel.read_bytes() == mdx(b'outside')
        finally:
            if os.name == 'nt':
                os.rmdir(link)  # remove only this junction, never its target tree
            else:
                link.unlink()

        previous = output.read_bytes()
        # Naming collisions in empty folders are global, even with usable MDX.
        (root / 'が').mkdir()
        (root / 'か\u3099').mkdir()
        _, error = invoke(code=1)
        assert 'name-collision' in error and output.read_bytes() == previous
        (root / 'が').rmdir()
        (root / 'か\u3099').rmdir()
        duplicate = put('copy.mdx', sources[root / '00.mdx'])
        _, error = invoke(code=1)
        assert 'duplicate-track' in error and '00.mdx' in error and 'copy.mdx' in error
        assert output.read_bytes() == previous
        duplicate.unlink()
        oversized = root / 'oversized.mdx'
        with oversized.open('wb') as stream:
            stream.truncate(1048577)
        _, error = invoke(code=1)
        assert 'capacity' in error and output.read_bytes() == previous
        oversized.unlink()
        invoke(destination=root / '00.mdx', code=1)
        assert (root / '00.mdx').read_bytes() == sources[root / '00.mdx']
        alias = base / 'input-alias.rpcmlib'
        os.link(root / '00.mdx', alias)
        try:
            invoke(destination=alias, code=1)
            assert alias.read_bytes() == sources[root / '00.mdx']
        finally:
            alias.unlink()

        no_good = base / 'no-good'
        no_good.mkdir()
        (no_good / 'bad.mdx').write_bytes(b'bad')
        _, error = invoke(folder=no_good, code=1)
        assert 'no-accepted-tracks' in error and output.read_bytes() == previous
        directory_destination = base / 'directory-output'
        directory_destination.mkdir()
        marker = directory_destination / 'keep'
        marker.write_bytes(b'keep')
        invoke(destination=directory_destination, code=1)
        assert marker.read_bytes() == b'keep'
        if os.name == 'nt':
            os.chmod(output, stat.S_IREAD)
            try:
                invoke(code=1)
                assert output.read_bytes() == previous
            finally:
                os.chmod(output, stat.S_IWRITE | stat.S_IREAD)
        else:
            put('bad\nname.mdx', b'bad')
            text, _ = invoke(code=2)
            assert 'path="bad\\x0aname.mdx"' in text
    print('native album ingestion/publication: PASS')


if __name__ == '__main__':
    main()
