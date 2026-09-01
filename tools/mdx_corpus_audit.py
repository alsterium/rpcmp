#!/usr/bin/env python3
"""Aggregate, privacy-preserving structural coverage for an MDX corpus."""

from __future__ import annotations

import argparse
import json
import os
from collections import Counter
from pathlib import Path
from typing import Any

TITLE_MARKER = b"\x0d\x0a\x1a"
DEFAULT_MAX_FILE_BYTES = 1_048_576
MAX_TITLE_BYTES = 4_096
MAX_PDX_REFERENCE_BYTES = 255
MAX_TRACKS = 16


def _new_report() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "privacy": "aggregate_only",
        "files_seen": 0,
        "total_bytes": 0,
        "minimum_file_bytes": None,
        "maximum_file_bytes": 0,
        "status_counts": Counter(),
        "layout_counts": Counter(),
        "track_target_counts": Counter(),
        "opcode_counts": Counter(),
        "linear_decode_counts": Counter(),
        "pdx_reference_counts": Counter(),
        "maximum_title_bytes": 0,
        "maximum_pdx_reference_bytes": 0,
    }


def _increment(report: dict[str, Any], section: str, key: str) -> None:
    report[section][key] += 1


def _u16be(data: bytes, offset: int) -> int:
    return (data[offset] << 8) | data[offset + 1]


def _command_length(data: bytes, position: int, end: int) -> tuple[int, str]:
    opcode = data[position]
    if opcode <= 0x7F:
        return 1, "known"
    if opcode <= 0xDF:
        return 2, "known"
    if opcode in (0xEA, 0xEB, 0xEC):
        if position + 2 > end:
            return 0, "truncated"
        return (2 if data[position + 1] in (0x80, 0x81) else 6), "known"
    if opcode in (0xE6, 0xE7):
        return 0, "variable_extension"
    if 0xE0 <= opcode <= 0xE5:
        return 0, "undefined"
    lengths = {
        0xE8: 1,
        0xE9: 2,
        0xED: 2,
        0xEE: 1,
        0xEF: 2,
        0xF0: 2,
        0xF1: 0,
        0xF2: 3,
        0xF3: 3,
        0xF4: 3,
        0xF5: 3,
        0xF6: 3,
        0xF7: 1,
        0xF8: 2,
        0xF9: 1,
        0xFA: 1,
        0xFB: 2,
        0xFC: 2,
        0xFD: 2,
        0xFE: 3,
        0xFF: 2,
    }
    if opcode == 0xF1:
        if position + 2 > end:
            return 0, "truncated"
        return (2 if data[position + 1] == 0 else 3), "known"
    length = lengths.get(opcode)
    return (length, "known") if length is not None else (0, "undefined")


def _scan_track(data: bytes, start: int, end: int, report: dict[str, Any]) -> None:
    position = start
    while position < end:
        opcode = data[position]
        length, classification = _command_length(data, position, end)
        report["opcode_counts"][f"{opcode:02x}"] += 1
        if classification != "known":
            _increment(report, "linear_decode_counts", classification)
            return
        if length == 0 or position + length > end:
            _increment(report, "linear_decode_counts", "truncated")
            return
        position += length
        if opcode == 0xF1:
            _increment(report, "linear_decode_counts", "terminated")
            return
    _increment(report, "linear_decode_counts", "region_exhausted")


def _record_file_size(report: dict[str, Any], size: int) -> None:
    report["files_seen"] += 1
    report["total_bytes"] += size
    current_minimum = report["minimum_file_bytes"]
    report["minimum_file_bytes"] = size if current_minimum is None else min(current_minimum, size)
    report["maximum_file_bytes"] = max(report["maximum_file_bytes"], size)


def _audit_bytes(data: bytes, report: dict[str, Any], max_file_bytes: int) -> None:
    size = len(data)
    _record_file_size(report, size)

    if size > max_file_bytes:
        _increment(report, "status_counts", "input_too_large")
        return

    marker = data.find(TITLE_MARKER, 0, min(size, MAX_TITLE_BYTES + len(TITLE_MARKER)))
    if marker < 0:
        _increment(report, "status_counts", "missing_title_terminator")
        return
    report["maximum_title_bytes"] = max(report["maximum_title_bytes"], marker)

    pdx_start = marker + len(TITLE_MARKER)
    pdx_limit = min(size, pdx_start + MAX_PDX_REFERENCE_BYTES + 1)
    pdx_end = data.find(b"\x00", pdx_start, pdx_limit)
    if pdx_end < 0:
        _increment(report, "status_counts", "missing_or_long_pdx_terminator")
        return
    pdx_length = pdx_end - pdx_start
    report["maximum_pdx_reference_bytes"] = max(
        report["maximum_pdx_reference_bytes"], pdx_length
    )
    _increment(
        report,
        "pdx_reference_counts",
        "nonempty" if pdx_length else "empty",
    )

    base = pdx_end + 1
    if base + 7 <= size and data[base + 4 : base + 7] == b"LZX":
        _increment(report, "status_counts", "unsupported_compression")
        return
    if base + 4 > size:
        _increment(report, "status_counts", "truncated_offset_table")
        return

    first_track_offset = _u16be(data, base + 2)
    if first_track_offset < 2 or (first_track_offset - 2) % 2 != 0:
        _increment(report, "status_counts", "invalid_track_table")
        return
    track_count = (first_track_offset - 2) // 2
    report["layout_counts"][str(track_count)] += 1
    if track_count < 1 or track_count > MAX_TRACKS:
        _increment(report, "status_counts", "invalid_track_table")
        return

    table_bytes = 2 + 2 * track_count
    if base + table_bytes > size:
        _increment(report, "status_counts", "truncated_offset_table")
        return
    offsets = [_u16be(data, base + index * 2) for index in range(track_count + 1)]
    if any(offset < table_bytes or base + offset >= size for offset in offsets):
        _increment(report, "status_counts", "range_outside_input")
        return
    if len(set(offsets)) != len(offsets):
        _increment(report, "status_counts", "overlapping_regions")
        return

    sorted_offsets = sorted(offsets)
    region_end = {
        offset: (base + sorted_offsets[index + 1] if index + 1 < len(sorted_offsets) else size)
        for index, offset in enumerate(sorted_offsets)
    }
    for track_index, offset in enumerate(offsets[1:]):
        target = "ym2151" if track_index < 8 else ("legacy_adpcm" if track_index == 8 else "pcm8")
        report["track_target_counts"][target] += 1
        _scan_track(data, base + offset, region_end[offset], report)

    _increment(report, "status_counts", "bounded_layout")


def audit_corpus(root: Path, max_file_bytes: int = DEFAULT_MAX_FILE_BYTES) -> dict[str, Any]:
    if max_file_bytes < 1 or max_file_bytes > DEFAULT_MAX_FILE_BYTES:
        raise ValueError("max_file_bytes must be between 1 and 1048576")
    if not root.is_dir():
        raise ValueError("root must be an existing directory")

    report = _new_report()
    for directory, directory_names, file_names in os.walk(root, followlinks=False):
        directory_names.sort(key=str.casefold)
        file_names.sort(key=str.casefold)
        for file_name in file_names:
            if Path(file_name).suffix.casefold() != ".mdx":
                continue
            path = Path(directory) / file_name
            try:
                with path.open("rb") as stream:
                    size = os.fstat(stream.fileno()).st_size
                    if size > max_file_bytes:
                        _record_file_size(report, size)
                        _increment(report, "status_counts", "input_too_large")
                        continue
                    _audit_bytes(stream.read(max_file_bytes + 1), report, max_file_bytes)
            except OSError:
                report["files_seen"] += 1
                _increment(report, "status_counts", "io_error")

    for key in (
        "status_counts",
        "layout_counts",
        "track_target_counts",
        "opcode_counts",
        "linear_decode_counts",
        "pdx_reference_counts",
    ):
        report[key] = dict(sorted(report[key].items()))
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="directory scanned recursively for .mdx files")
    parser.add_argument("--max-file-bytes", type=int, default=DEFAULT_MAX_FILE_BYTES)
    arguments = parser.parse_args()
    try:
        report = audit_corpus(arguments.root, arguments.max_file_bytes)
    except ValueError as error:
        parser.error(str(error))
    print(json.dumps(report, indent=2, sort_keys=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
