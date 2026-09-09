#!/usr/bin/env python3
"""Relink identical M5 app objects with independent text/BSS padding."""

import argparse
import json
import shlex
import struct
import subprocess
from pathlib import Path

import pocket_package as shared
import pocket_placement_verify as placement

APP_SHA = "237b16d8f8abaf67471518cc3bb8c33e76d70f5e11d729c1fe8ae2f46a01fb85"


def linker_script(source, profile):
    if profile == "control":
        return source
    if profile == "code":
        old = "        *(.text*)"
        new = ("        KEEP(*crt1.o(.text .text._start_c))\n"
               "        __rpcmp_pad_start = .;\n        . += 64;\n" + old)
    elif profile == "bss":
        old = "        __bss_start = .;"
        new = "        __rpcmp_pad_start = .;\n        . += 64;\n" + old
    else:
        raise ValueError("unknown placement profile")
    if source.count(old) != 1:
        raise ValueError("linker insertion point is not unique")
    return source.replace(old, new)


def memory(path):
    data = path.read_bytes()
    sections, symbols, _, _ = placement.layout(path)
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    if header[9] != 32 or header[10] != 2 or header[5] + 64 > len(data):
        raise ValueError("expected bounded LOAD and GNU_STACK headers")
    segment = struct.unpack_from("<8I", data, header[5])
    stack = struct.unpack_from("<8I", data, header[5] + 32)
    if stack != (0x6474E551, 0, 0, 0, 0, 0, 6, 16):
        raise ValueError("GNU_STACK differs from accepted app")
    kind, offset, vma, paddr, filesz, memsz, flags, alignment = segment
    if (kind != 1 or vma != 0x10400000 or paddr != vma or header[4] != vma or
            offset + filesz > len(data) or filesz > memsz or vma + memsz > 0x13A00000 or
            alignment != 0x1000 or flags != 7):
        raise ValueError("invalid M5 app load segment")
    return {"entry": header[4], "load": list(segment), "heap_start": (vma + memsz + 15) & ~15,
            "gp": symbols["__global_pointer$"],
            "sections": {n: {"address": s[0], "bytes": s[1], "type": s[2]} for n, s in sections.items()}}


def build(repo, output):
    repo, output = repo.resolve(), output.resolve()
    if output.parent != repo / "out/build" or output.exists():
        raise ValueError("requires a fresh direct child of out/build")
    sdk = repo / "out/research/openfpgaSDK-a408ddc/src/sdk"
    base = repo / "out/build/pocket-openfpgaos-m5-file"
    accepted = base / "rpcmp-m5-file.elf"
    if shared.sha256(accepted) != APP_SHA:
        raise ValueError("accepted app identity mismatch")
    dry = subprocess.check_output(["make", "-f", "spikes/pocket/openfpgaos/Makefile", "-n", "-W",
                                   str(sdk / "app.ld"), str(accepted)], cwd=repo, text=True)
    lines = [line for line in dry.replace("\\\n", " ").splitlines()
             if line.startswith("riscv-none-elf-g++ ") and " -o " in line]
    if len(lines) != 1:
        raise ValueError("cannot identify retained-object app link")
    command = shlex.split(lines[0])
    inputs = [sdk / "app.ld", sdk / "musl/lib/libc.a"] + [Path(p) for p in command if p.endswith((".o", ".a"))]
    hashes = {str(p): shared.sha256(p) for p in inputs}
    output.mkdir()
    results = {}
    for profile in ("control", "code", "bss"):
        folder = output / profile
        folder.mkdir()
        (folder / "app.ld").write_text(linker_script((sdk / "app.ld").read_text(), profile))
        argv = list(command)
        argv[argv.index("-T") + 1] = str(folder / "app.ld")
        argv[argv.index("-o") + 1] = str(folder / "app.elf")
        argv = [f"-Wl,-Map,{folder}/app.map" if a.startswith("-Wl,-Map,") else a for a in argv]
        argv.append("-Wl,--emit-relocs")
        subprocess.run(argv, cwd=repo, check=True)
        allocated, symbols, _, _ = placement.layout(folder / "app.elf")
        record = {"profile": profile, "elf_sha256": shared.sha256(folder / "app.elf"),
                  "map_sha256": shared.sha256(folder / "app.map"), "command": argv,
                  "linker_sha256": shared.sha256(folder / "app.ld"), "memory": memory(folder / "app.elf")}
        if profile == "control":
            if allocated != placement.layout(accepted)[0] or record["memory"] != memory(accepted):
                raise ValueError("control differs from accepted allocated image or load layout")
        else:
            record["placement"] = placement.compare(output / "control/app.elf", folder / "app.elf",
                ".text" if profile == "code" else ".bss", symbols["__rpcmp_pad_start"])
            deltas = record["placement"]["symbol_deltas"]
            if deltas.get("_start", 0) or deltas.get("_start_c", 0) or any(d != 64 for d in deltas.values()):
                raise ValueError("unexpected app symbol displacement")
            if deltas.get("__bss_start") != 64 or deltas.get("__bss_end") != 64:
                raise ValueError("app BSS did not move by 64 bytes")
            record["pad_start"] = symbols["__rpcmp_pad_start"]
        subprocess.run(["python3", "-B", str(repo / "tools/elf_budget.py"), "--elf", str(folder / "app.elf"),
                        "--size-tool", "riscv-none-elf-size", "--stack-root", str(base),
                        "--static-limit", "56623104", "--data-limit", "4096", "--stack-limit", "524288",
                        "--output", str(folder / "budget.json")], check=True, stdout=subprocess.DEVNULL)
        record["budget"] = json.loads((folder / "budget.json").read_text())
        (folder / "evidence.json").write_bytes(shared.json_bytes(record))
        results[profile] = record
    if hashes != {str(p): shared.sha256(p) for p in inputs}:
        raise ValueError("app inputs changed during build")
    report = {"result": "PASS", "inputs": hashes, "profiles": results,
              "compiler": subprocess.check_output([command[0], "--version"], text=True).splitlines()[0]}
    (output / "evidence.json").write_bytes(shared.json_bytes(report))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(build(args.repo, args.output)["result"])
