"""Verify pinned host metadata sources and generate the offline CP932 table."""

import argparse
import hashlib
from pathlib import Path


def generate(root: Path, output: Path | None) -> None:
    vendor = root / "third_party"
    for line in (vendor / "metadata-sources.sha256").read_text(encoding="ascii").splitlines():
        digest, name = line.split()
        if hashlib.sha256((vendor / name).read_bytes()).hexdigest() != digest:
            raise ValueError(f"pinned metadata source changed: {name}")
    entries = []
    for line in (vendor / "cp932/CP932.TXT").read_text(encoding="ascii").splitlines():
        fields = line.split("#", 1)[0].split()
        if len(fields) == 2:
            code, scalar = (int(value, 16) for value in fields)
            if not 0 <= code <= 0xffff or not 0 <= scalar <= 0xffff:
                raise ValueError("CP932 table is outside its adopted 16-bit profile")
            entries.append((code, scalar))
    if entries != sorted(set(entries)) or len({code for code, _ in entries}) != len(entries):
        raise ValueError("CP932 codes must be unique and sorted")
    if output is not None:
        text = "// Generated from pinned Microsoft CP932 table 2.01; see third_party/cp932.\n"
        text += f"inline constexpr std::array<Cp932Entry, {len(entries)}> kCp932{{{{\n"
        text += "".join(f"  {{0x{code:04x}, 0x{scalar:04x}}},\n" for code, scalar in entries)
        text += "}};\n"
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="ascii", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    generate(args.root, args.output)
    print("pinned metadata sources: PASS")
