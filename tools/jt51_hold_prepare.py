#!/usr/bin/env python3
"""Create the pinned local JT51 hold experiment; generated GPL HDL stays in out/."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

REVISION = "985a573dcfc1ff135553a39f7eae21d18ba57cbe"
# Reviewed clocked processes, including two disabled debug processes. This is
# an exact-revision edit recipe, not a Verilog frontend or arbitrary-core tool.
PROCESSES = {
    "jt51_acc.v": 2, "jt51_eg.v": 8, "jt51_exp2lin.v": 0,
    "jt51_exprom.v": 1, "jt51_kon.v": 1, "jt51_lfo.v": 3,
    "jt51_lin2exp.v": 0, "jt51_mmr.v": 3, "jt51_mod.v": 0,
    "jt51_noise_lfsr.v": 1, "jt51_noise.v": 3, "jt51_op.v": 4,
    "jt51_pg.v": 9, "jt51_phinc_rom.v": 0, "jt51_phrom.v": 1,
    "jt51_pm.v": 0, "jt51_reg_ch.v": 2, "jt51_csr_op.v": 0,
    "jt51_reg.v": 2, "jt51_sh.v": 1, "jt51_timers.v": 3, "jt51.v": 0,
}
# These four active processes can update independently of cen. All other active
# processes already stop with their original cen, gated once at the top boundary.
EXPLICIT_HOLD = {"jt51_lfo.v": 0, "jt51_mmr.v": 0, "jt51_reg_ch.v": 1, "jt51_timers.v": 0}
HOLD_FILES = {"jt51.v", "jt51_lfo.v", "jt51_mmr.v", "jt51_reg.v", "jt51_reg_ch.v", "jt51_timers.v"}


def git(source: Path, *args: str) -> bytes:
    return subprocess.run(["git", "-C", str(source), *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def code_mask(text: str) -> str:
    """Preserve offsets and license/comments while locating known code tokens."""
    return re.sub(r'/\*.*?\*/|//[^\n]*',
                  lambda m: ''.join('\n' if c == '\n' else ' ' for c in m[0]),
                  text, flags=re.DOTALL)


def edit(text: str, pattern: str, replacement) -> tuple[str, int]:
    matches = list(re.finditer(pattern, code_mask(text)))
    for match in reversed(matches):
        text = text[:match.start()] + replacement(match) + text[match.end():]
    return text, len(matches)


def guard_processes(text: str, has_reset: bool, explicit: int | None) -> tuple[str, int, int]:
    mask = code_mask(text)
    processes = list(re.finditer(r'always\s*@\s*\(\s*posedge\s+clk\s*'
                                r'(?:,\s*posedge\s+rst\s*)?\)', mask))
    asynchronous = 0
    for index, process in reversed(list(enumerate(processes))):
        if 'posedge rst' in process[0]:
            asynchronous += 1
        if index != explicit:
            continue
        if 'posedge rst' in process[0]:
            # Keep the asynchronous reset as the leading condition for synthesis.
            # Every pinned reset arm is a simple statement/block (or reset loop)
            # without a nested if/else. Refuse a changed shape instead of guessing.
            prefix = re.match(r'\s*(?:begin(?:\s*:\s*\w+)?\s*)?if\s*\(\s*rst\s*\)',
                              mask[process.end():])
            if not prefix:
                raise ValueError('unexpected asynchronous reset prefix')
            start = process.end() + prefix.end()
            arm = re.search(r'\b(?:else|if|always|endmodule)\b', mask[start:])
            if not arm or arm[0] != 'else':
                raise ValueError('unexpected asynchronous reset arm')
            offset = start + arm.end()
            text = text[:offset] + ' if (!rpcmp_hold)' + text[offset:]
        else:
            condition = ' if (rst || !rpcmp_hold)' if has_reset else ' if (!rpcmp_hold)'
            text = text[:process.end()] + condition + text[process.end():]
    return text, len(processes), asynchronous


def prepare(source: Path, output: Path) -> dict:
    root = Path(__file__).resolve().parents[1]
    output = output.resolve()
    if not output.is_relative_to(root / "out") or output == root / "out":
        raise ValueError("generated HDL must stay in a new repository out/ subdirectory")
    if output.exists():
        raise ValueError("output already exists; choose a new directory")
    if git(source, "rev-parse", "HEAD").decode().strip() != REVISION:
        raise ValueError("JT51 revision mismatch")
    if git(source, "status", "--porcelain=v1").strip():
        raise ValueError("JT51 checkout has local changes")
    files = {name: git(source, "show", f"HEAD:hdl/{name}").decode("utf-8")
             for name in PROCESSES}
    modules = [m[1] for text in files.values()
               for m in re.finditer(r'\bmodule\s+(\w+)', code_mask(text))]
    if len(modules) != 23 or len(set(modules)) != 23:
        raise ValueError("unexpected JT51 module inventory")
    names = r'\b(?:' + '|'.join(sorted(modules, key=len, reverse=True)) + r')\b'
    generated = {}
    records = []
    for name, original in files.items():
        text, _ = edit(original, names, lambda m: "rpcmp_hold_" + m[0])
        has_reset = bool(re.search(r'\binput\s+rst\b', code_mask(text)))
        ports = 0
        if name in HOLD_FILES:
            text, ports = edit(text, r'\binput\s+clk\b',
                               lambda m: 'input rpcmp_hold,\n    ' + m[0])
        text, children = edit(text, r'\brpcmp_hold_jt51_(?:lfo|mmr|reg|reg_ch|timers|timer)\s+'
                              r'(?:#\s*\([^;]+?\)\s*)?\w+\s*\(',
                              lambda m: m[0] + '.rpcmp_hold(rpcmp_hold && !rst), ')
        enables = 0
        if name == 'jt51.v':
            text, enables = edit(text, r'\.cen\s*\(\s*cen_p1\s*\)',
                                 lambda m: '.cen(cen_p1 && (!rpcmp_hold || rst))')
            if enables != 8:
                raise ValueError('unexpected top-level enable boundary')
        text, processes, asynchronous = guard_processes(text, has_reset, EXPLICIT_HOLD.get(name))
        if processes != PROCESSES[name] or (children and not ports):
            raise ValueError(f"unexpected clock/hold structure: {name}")
        expected_ports = 0 if name not in HOLD_FILES else 2 if name == "jt51_timers.v" else 1
        if ports != expected_ports:
            raise ValueError(f"unexpected clock port count: {name}")
        generated[name] = text.encode("utf-8")
        records.append({"file": name, "source_sha256": hashlib.sha256(original.encode()).hexdigest(),
                        "generated_sha256": hashlib.sha256(generated[name]).hexdigest(),
                        "hold_ports": ports, "hold_children": children, "clocked_processes": processes,
                        "async_reset_processes": asynchronous, "explicit_hold_process": EXPLICIT_HOLD.get(name),
                        "gated_enable_connections": enables})
    if sum(item['async_reset_processes'] for item in records) != 14:
        raise ValueError('unexpected asynchronous reset process count')
    if sum(item['hold_children'] for item in records) != 7:
        raise ValueError('unexpected hold propagation graph')
    manifest = {"schema": 1, "revision": REVISION, "purpose": "local M6 hold feasibility",
                "recipe_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "top": "rpcmp_hold_jt51", "license": "GPL-3.0-or-later",
                "debug_defines": False, "files": records}
    license_bytes = git(source, "show", "HEAD:LICENSE")
    output.mkdir(parents=True)
    for name, data in generated.items():
        (output / name).write_bytes(data)
    (output / "LICENSE").write_bytes(license_bytes)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + '\n', encoding="utf-8")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jt51", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = prepare(args.jt51.resolve(), args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"JT51 hold preparation failed: {error}\n")
    print(f"JT51 hold prepared: {len(manifest['files'])} files, 8 enable connections, 4 explicit process guards")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
