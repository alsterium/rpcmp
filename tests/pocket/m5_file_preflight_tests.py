"""Run authored real-file preflight through the production ingestion CLI."""

import pathlib
import subprocess
import sys
import tempfile


def main():
    pack, preflight, fixture = map(pathlib.Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="rpcmp-preflight-") as temporary:
        root = pathlib.Path(temporary)
        source = root / "authored.mdx"
        library = root / "authored.rpcmlib"
        source.write_bytes(bytes.fromhex(fixture.read_text(encoding="ascii")))
        subprocess.run([str(pack), str(source), str(library), "Authored"], check=True)
        result = subprocess.run([str(preflight), str(source), str(library), "Authored"],
                                check=True, capture_output=True, text=True)
        assert "result=PASS" in result.stdout
        wrong_id = subprocess.run([str(preflight), str(source), str(library), "Wrong"],
                                  capture_output=True, check=False)
        assert wrong_id.returncode != 0
        damaged = bytearray(library.read_bytes())
        damaged[100] ^= 1
        library.write_bytes(damaged)
        corrupt = subprocess.run([str(preflight), str(source), str(library), "Authored"],
                                 capture_output=True, check=False)
        assert corrupt.returncode != 0
    print("m5_file_preflight: PASS")


if __name__ == "__main__":
    main()
