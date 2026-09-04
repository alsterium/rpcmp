#!/usr/bin/env python3
"""Write deterministic code, static-memory, and stack-frame evidence for an ELF."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--size-tool", required=True)
    parser.add_argument("--stack-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--static-limit", type=int, required=True)
    parser.add_argument("--data-limit", type=int)
    parser.add_argument("--stack-limit", type=int, required=True)
    args = parser.parse_args()

    output = subprocess.run(
        [args.size_tool, str(args.elf)], check=True, capture_output=True, text=True
    ).stdout.splitlines()
    columns = output[-1].split()
    text, data, bss = (int(columns[index]) for index in range(3))

    frames = []
    dynamic = []
    pattern = re.compile(r"^(.*)\t(\d+)\t(.+)$")
    for usage in sorted(args.stack_root.rglob("*.su")):
        for line in usage.read_text(encoding="utf-8").splitlines():
            match = pattern.match(line)
            if not match:
                continue
            function, size, qualifier = match.groups()
            record = {"bytes": int(size), "function": function, "qualifier": qualifier}
            frames.append(record)
            if "dynamic" in qualifier:
                dynamic.append(record)

    if not frames:
        raise SystemExit("no stack-usage records found")
    largest = max(frames, key=lambda record: (record["bytes"], record["function"]))
    conservative_stack = sum(record["bytes"] for record in frames)
    static_bytes = text + data + bss
    evidence = {
        "schema": 2,
        "elf": args.elf.name,
        "text_bytes": text,
        "data_bytes": data,
        "data_limit_bytes": args.data_limit,
        "data_headroom_bytes": None if args.data_limit is None else args.data_limit - data,
        "bss_bytes": bss,
        "static_bytes": static_bytes,
        "static_limit_bytes": args.static_limit,
        "static_headroom_bytes": args.static_limit - static_bytes,
        "largest_stack_frame": largest,
        "conservative_stack_bound_bytes": conservative_stack,
        "stack_limit_bytes": args.stack_limit,
        "stack_headroom_bytes": args.stack_limit - conservative_stack,
        "dynamic_stack_frames": dynamic,
        "stack_record_count": len(frames),
    }
    data_exceeded = args.data_limit is not None and data > args.data_limit
    if (
        evidence["static_headroom_bytes"] < 0
        or evidence["stack_headroom_bytes"] < 0
        or data_exceeded
        or dynamic
    ):
        failures = []
        if evidence["static_headroom_bytes"] < 0:
            failures.append("static memory limit")
        if evidence["stack_headroom_bytes"] < 0:
            failures.append("stack limit")
        if data_exceeded:
            failures.append("initialized-data limit")
        if dynamic:
            failures.append("dynamic stack usage")
        raise SystemExit("ELF budget failure: " + ", ".join(failures))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(evidence, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
