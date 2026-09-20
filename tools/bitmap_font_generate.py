#!/usr/bin/env python3
"""Build RPCMP Bitmap JP from pinned glyphs and ingestion's pinned NFC helper."""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
from pathlib import Path
import subprocess

FONT_HASH = "5ba84e901b9f7fad3bce0571c7e4b4b0aef4ce0acc4ee6622ef0cfa2eef38a6f"
CP932_HASH = "c9bc0b0cd42e0fbcb82a09635bb5abed86afbdd4abc9e76fa5716638217cb59f"
EXTRA = {0x2026, 0x2190, 0x2191, 0x2192, 0x2193, 0x221E, 0x25A1, 0xFFFD}


def checked(path: Path, digest: str) -> bytes:
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError(f"source hash mismatch: {path}")
    return data


def build(repo: Path, normalizer: Path, output: Path) -> dict:
    mapping = checked(repo / "third_party/cp932/CP932.TXT", CP932_HASH)
    base = set()
    for line in mapping.decode("ascii").splitlines():
        fields = line.split("#", 1)[0].split()
        if len(fields) >= 2:
            point = int(fields[1], 16)
            if point >= 0x20 and not 0x7F <= point <= 0x9F:
                base.add(point)
    result = subprocess.run([str(normalizer.resolve())], check=True, text=True,
                            input="".join(f"{point}\n" for point in sorted(base)),
                            capture_output=True)
    normalized = {int(line) for line in result.stdout.splitlines()}
    selected = base | normalized | EXTRA
    if not selected or len(selected) > 8192:
        raise ValueError("glyph count outside budget")
    source = checked(repo / "third_party/unifont/unifont_jp-16.0.04.hex.gz", FONT_HASH)
    glyphs = {}
    for line in gzip.decompress(source).decode("ascii").splitlines():
        code, data = line.split(":")
        point = int(code, 16)
        if point in glyphs:
            raise ValueError("duplicate source glyph")
        glyphs[point] = bytes.fromhex(data)
    bits = bytearray()
    index = []
    for point in sorted(selected):
        data = glyphs[point]  # Missing glyph is a build failure, never a blank.
        if len(data) not in (16, 32):
            raise ValueError("unsupported glyph dimensions")
        index.append((point, len(bits), len(data) // 2))
        bits.extend(data)
    # Native index has three uint32_t fields; no packed struct assumptions.
    if len(index) * 12 + len(bits) > 512 * 1024:
        raise ValueError("font exceeds read-only budget")
    lines = ["// Generated RPCMP Bitmap JP. SIL OFL 1.1; see third_party/unifont.",
             f"constexpr std::array<FontEntry, {len(index)}> kFontIndex{{{{"]
    lines += [f"  {{{point}, {offset}, {width}}}," for point, offset, width in index]
    lines += ["}};", f"constexpr std::array<std::uint8_t, {len(bits)}> kFontBits{{{{"]
    lines += ["  " + ", ".join(f"0x{byte:02x}" for byte in bits[i:i + 16]) + ","
              for i in range(0, len(bits), 16)]
    lines += ["}};", ""]
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines), encoding="ascii", newline="\n")
    manifest = {"font": "RPCMP Bitmap JP", "license": "OFL-1.1",
                "source_sha256": FONT_HASH, "cp932_sha256": CP932_HASH,
                "nfc": "utf8proc 2.11.3 / Unicode 17.0.0 / STABLE COMPOSE",
                "glyphs": len(index), "nfc_added": sorted(normalized - base),
                "bitmap_bytes": len(bits), "index_bytes": len(index) * 12,
                "generated_sha256": hashlib.sha256(output.read_bytes()).hexdigest()}
    output.with_suffix(".json").write_text(json.dumps(manifest, indent=2) + "\n",
                                          encoding="ascii", newline="\n")
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--normalizer", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.repo, args.normalizer, args.output), indent=2))
