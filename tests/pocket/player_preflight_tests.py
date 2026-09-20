"""Exercise the M6 candidate corpus through native ingestion and Core admission."""

import subprocess
import struct
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import pocket_player_demo as demo


def main():
    pack, preflight = (str(Path(p).resolve()) for p in sys.argv[1:])
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        source, library = root / 'source', root / 'demo.rpcmlib'
        demo.create(source)
        subprocess.run([pack, str(source), str(library)], check=True, capture_output=True)
        result = subprocess.run([preflight, str(library)], check=True, capture_output=True)
        assert result.stdout.decode('ascii').splitlines() == ['albums=2 tracks=3', 'result=PASS']
        before = {p: p.read_bytes() for p in source.rglob('*.mdx')}
        try:
            demo.create(source)
            raise AssertionError('existing source replaced')
        except ValueError:
            pass
        assert all(p.read_bytes() == data for p, data in before.items())
        changed = bytearray(library.read_bytes())
        # Corrupt covered STRS bytes, not the container's trailing alignment
        # padding (which is outside every section CRC by contract).
        directory, count, stride = struct.unpack_from('<QII', changed, 40)
        for i in range(count):
            tag, _, offset, length = struct.unpack_from('<4sIQQ', changed, directory + i * stride)
            if tag == b'STRS':
                changed[offset + length - 1] ^= 1
                break
        else:
            raise AssertionError('missing string section')
        library.write_bytes(changed)
        assert subprocess.run([preflight, str(library)], capture_output=True).returncode != 0
        for length in (0, 32 * 1024 * 1024 + 1):
            with library.open('wb') as output:
                output.truncate(length)
            assert subprocess.run([preflight, str(library)], capture_output=True).returncode != 0
    print('M6 library preflight: PASS')


if __name__ == '__main__':
    main()
