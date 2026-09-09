#!/usr/bin/env python3
"""Recover the accepted M5 control from retained objects; run inside firmware GCC image."""

import argparse
import json
import re
import shlex
import subprocess
from pathlib import Path

import pocket_firmware_pair as pair
import pocket_m5_audio_prepare as audio
import pocket_package as shared


def references(text):
    """Keep linked BRAM-to-OS relocations, excluding debug-section offsets."""
    section = ""
    result = []
    for line in text.splitlines():
        if line.startswith("Relocation section '"):
            section = line.split("'")[1]
        match = re.fullmatch(
            r"([0-9a-f]{8})\s+\S+\s+(R_RISCV_\S+|unrecognized: \S+)\s+"
            r"([0-9a-f]{8})\s+(.+)", line)
        if section not in (".rela.boot", ".rela.boot_data", ".rela.fasttext", ".rela.fastdata") or not match:
            continue
        offset, target = int(match[1], 16), int(match[3], 16)
        if offset < 0x4000 and 0x10320000 <= target < 0x103E0000:
            result.append({"section": section, "offset": offset, "type": match[2],
                           "symbol_value": target, "expression": match[4]})
    return result


def recover(repo, output):
    repo, output = repo.resolve(), output.resolve()
    if output.parent != repo / "out/build" or output.exists():
        raise ValueError("recovery requires a fresh direct child of out/build")
    historical = repo / "out/research/openfpgaCore-618a3eb-lf/src/firmware/os"
    normal = repo / "out/build/m5-firmware-control-v3/src/firmware/os"
    mif = repo / "out/build/openfpgaos-m5-audio-bootfix/src/fpga/targets/pocket/firmware.mif"
    os_image = repo / shared.SAFE_MEMSET_OS
    audio.verify_boot_mif(mif)
    if shared.sha256(os_image) != shared.SAFE_MEMSET_OS_SHA256:
        raise ValueError("accepted OS identity mismatch")
    dry = subprocess.check_output(["make", "-n", "-W", "os.ld", "bld/pocket/firmware.elf"],
                                  cwd=historical, text=True)
    lines = [line for line in dry.splitlines() if line.startswith("riscv64-unknown-elf-gcc ")
             and " -o bld/pocket/firmware.elf " in line]
    if len(lines) != 1:
        raise ValueError("cannot identify retained-object link command")
    argv = shlex.split(lines[0])
    for name in ("kernel/main.o", "hal/memtest.o"):
        argv[argv.index("bld/pocket/" + name)] = str(normal / "bld/pocket" / name)
    # Hash every input object/archive and the linker script before relinking.
    paths = [historical / "os.ld"] + [historical / p for p in argv if p.endswith((".o", ".a"))]
    inputs = {str(p.resolve()): shared.sha256(p) for p in paths}
    argv[argv.index("-o") + 1] = str(output / "firmware.elf")
    argv = [f"-Wl,-Map={output / 'firmware.map'}" if p.startswith("-Wl,-Map=") else p for p in argv]
    argv.append("-Wl,--emit-relocs")
    output.mkdir()
    subprocess.run(argv, cwd=historical, check=True)
    verified = pair.verify(output / "firmware.elf", mif, os_image)
    relocs = subprocess.check_output(["riscv64-unknown-elf-readelf", "-rW", str(output / "firmware.elf")], text=True)
    (output / "relocations.txt").write_text(relocs, encoding="utf-8")
    report = {"result": "PASS", "scope": "retained-object recovery; not clean-source reproduction",
              "pair": verified, "inputs": inputs, "command": argv,
              "compiler": subprocess.check_output([argv[0], "--version"], text=True).splitlines()[0],
              "map_sha256": shared.sha256(output / "firmware.map"),
              "bram_to_os_relocations": references(relocs)}
    (output / "evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = recover(args.repo, args.output)
    print(f"result={report['result']} bram_to_os_relocations={len(report['bram_to_os_relocations'])}")
