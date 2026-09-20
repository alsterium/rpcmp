#!/usr/bin/env python3
"""Independent source/normalization and bounds checks for the generated font."""
import argparse
import gzip
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from bitmap_font_generate import checked, FONT_HASH

parser = argparse.ArgumentParser()
parser.add_argument("--normalizer", required=True)
parser.add_argument("--generated", type=Path, required=True)
args = parser.parse_args()
# Canonical single-scalar normalization: OHM SIGN -> OMEGA, KELVIN -> K,
# ANGSTROM -> A RING. CP932 glyphs themselves are retained as well.
result = subprocess.run([args.normalizer], input="8486\n8490\n8491\n12354\n",
                        text=True, capture_output=True, check=True)
assert set(map(int, result.stdout.splitlines())) == {937, 75, 197, 12354}
assert subprocess.run([args.normalizer], input="55296\n", text=True,
                      capture_output=True).returncode != 0
generated = args.generated.read_text(encoding="ascii")
index_part, bitmap_part = generated.split("kFontBits", 1)
entries = [tuple(map(int, match)) for match in re.findall(r"\{(\d+), (\d+), (\d+)\}", index_part)]
bitmap = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", bitmap_part))
source = dict(line.split(":") for line in gzip.decompress(checked(
    ROOT / "third_party/unifont/unifont_jp-16.0.04.hex.gz", FONT_HASH)).decode("ascii").splitlines())
previous = -1
end = 0
for point, offset, width in entries:
    assert point > previous and offset == end and width in (8, 16)
    end = offset + width * 2
    assert end <= len(bitmap)
    assert bitmap[offset:end] == bytes.fromhex(source[f"{point:04X}"])
    previous = point
assert end == len(bitmap)
assert len(entries) <= 8192 and len(entries) * 12 + len(bitmap) <= 512 * 1024
points = {entry[0] for entry in entries}
assert {0x41, 0x3042, 0xFF76, 0xFF5E, 0x2026, 0x221E, 0xFFFD} <= points
manifest = json.loads(args.generated.with_suffix(".json").read_text())
assert manifest["glyphs"] == len(entries) and manifest["bitmap_bytes"] == len(bitmap)
with tempfile.TemporaryDirectory() as temp:
    damaged = Path(temp) / "bad.gz"
    damaged.write_bytes(b"not the pinned font")
    try:
        checked(damaged, FONT_HASH)
    except ValueError:
        pass
    else:
        raise AssertionError("source drift accepted")
print(f"bitmap font source: PASS ({len(entries)} glyphs, {len(bitmap)} bitmap bytes)")
