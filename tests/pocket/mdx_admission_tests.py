"""Exercise local admission diagnostics with authored, non-ASCII-path fixtures."""

import pathlib
import subprocess
import sys
import tempfile


def main():
    executable = pathlib.Path(sys.argv[1]).resolve()
    fixture = bytes.fromhex(pathlib.Path(sys.argv[2]).read_text(encoding="ascii"))
    with tempfile.TemporaryDirectory(prefix="rpcmp-admission-") as temporary:
        root = pathlib.Path(temporary)
        corpus = root / "corpus"
        corpus.mkdir()
        (corpus / "自作.MDX").write_bytes(fixture)
        (corpus / "壊れた.mdx").write_bytes(b"invalid")
        (corpus / "対象外.日本語").write_bytes(b"ignored")
        output = root / "candidate.mdx"
        result = subprocess.run([str(executable), str(corpus), str(output)],
                                capture_output=True, text=True, check=True)
        assert "files=2 accepted=1" in result.stdout, result.stdout
        assert output.read_bytes() == fixture
        assert "自作" not in result.stdout and str(corpus) not in result.stdout
        again = subprocess.run([str(executable), str(corpus), str(output)],
                               capture_output=True, text=True, check=False)
        assert again.returncode == 1
        assert output.read_bytes() == fixture
        missing = subprocess.run([str(executable), str(root / "missing")],
                                 capture_output=True, text=True, check=False)
        assert missing.returncode == 1
    print("mdx_admission: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
