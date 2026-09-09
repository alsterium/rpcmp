#!/usr/bin/env python3
"""Relink the accepted fail-closed objects into isolated OS placement probes."""

import argparse
import json
import shlex
import struct
import subprocess
import zlib
from pathlib import Path

import pocket_firmware_pair as pair
import pocket_package as shared
import pocket_placement_verify as placement

BOOT_SHA = "d666a02a0561f02e3f320e976e71f447845ef01135e017feeea026e5b18df643"
OS_SHA = "7cffaf9ba421c9ca9382f60f71f92d7eeacc9fd81448293458d5c048b0d38f9e"


def linker_script(source, profile):
    if profile == "control":
        return source
    if profile == "code":
        old = "        KEEP(*(.text*))"
        new = ("        KEEP(*ltrans0.ltrans.o(.text))\n        . = ALIGN(64);\n"
               "        __rpcmp_pad_start = .;\n        . += 64;\n" + old)
    elif profile == "bss":
        old = "        __os_bss_start = .;"
        new = "        __rpcmp_pad_start = .;\n        . += 64;\n" + old
    else:
        raise ValueError("unknown placement profile")
    if source.count(old) != 1:
        raise ValueError("linker insertion point is not unique")
    return source.replace(old, new)


def images(elf, output):
    sections, symbols = pair.elf_sections(elf.read_bytes())
    subprocess.run(["riscv64-unknown-elf-objcopy", "-O", "binary",
                    *[f"--only-section={n}" for n in
                      (".boot", ".boot_data", ".boot_bss", ".fasttext", ".fastdata")],
                    str(elf), str(output / "boot.bin")], check=True)
    boot = (output / "boot.bin").read_bytes()
    if not boot or len(boot) % 4 or len(boot) > 32768:
        raise ValueError("boot image size/alignment invalid")
    words = struct.unpack("<" + "I" * (len(boot) // 4), boot)
    mif = "WIDTH=32;\nDEPTH=8192;\nADDRESS_RADIX=DEC;\nDATA_RADIX=HEX;\nCONTENT BEGIN\n"
    mif += "".join(f"{i} : {word:08X};\n" for i, word in enumerate(words))
    if len(words) < 8192:
        mif += f"[{len(words)}..8191] : 00000013;\n"
    (output / "firmware.mif").write_text(mif + "END;\n", encoding="ascii")
    image = sections[".osdata"][3] + struct.pack(
        "<6I", 0x4942414F, 0x4F2EE14D, 0x3245534F, symbols["os_main"],
        symbols["__os_bss_start"], symbols["__os_bss_end"])
    image += struct.pack("<II", 0x3143464F, zlib.crc32(image))
    (output / "os.bin").write_bytes(image)
    return pair.verify(elf, output / "firmware.mif", output / "os.bin")


def build(repo, output):
    repo, output = repo.resolve(), output.resolve()
    if output.parent != repo / "out/build" or output.exists():
        raise ValueError("requires a fresh direct child of out/build")
    source = repo / "out/build/m5-firmware-fail-closed/src/firmware/os"
    dry = subprocess.check_output(["make", "-n", "-W", "os.ld", "bld/pocket/firmware.elf"],
                                  cwd=source, text=True)
    lines = [line for line in dry.splitlines() if line.startswith("riscv64-unknown-elf-gcc ")
             and " -o bld/pocket/firmware.elf " in line]
    if len(lines) != 1:
        raise ValueError("cannot identify retained-object link")
    command = shlex.split(lines[0])
    inputs = [source / "os.ld"] + [source / p for p in command if p.endswith((".o", ".a"))]
    hashes = {str(p.resolve()): shared.sha256(p) for p in inputs}
    output.mkdir()
    results = {}
    for profile in ("control", "code", "bss"):
        folder = output / profile
        folder.mkdir()
        (folder / "os.ld").write_text(linker_script((source / "os.ld").read_text(), profile))
        argv = list(command)
        argv[argv.index("-T") + 1] = str(folder / "os.ld")
        argv[argv.index("-o") + 1] = str(folder / "firmware.elf")
        argv = [f"-Wl,-Map={folder}/firmware.map" if a.startswith("-Wl,-Map=") else a for a in argv]
        argv.append("-Wl,--emit-relocs")
        subprocess.run(argv, cwd=source, check=True)
        verified = images(folder / "firmware.elf", folder)
        if profile == "control":
            if verified["boot_sha256"] != BOOT_SHA or verified["os_sha256"] != OS_SHA:
                raise ValueError("control does not reproduce accepted ROM/OS")
        else:
            _, symbols, _, _ = placement.layout(folder / "firmware.elf")
            verified["placement"] = placement.compare(
                output / "control/firmware.elf", folder / "firmware.elf",
                ".osdata" if profile == "code" else ".os_bss", symbols["__rpcmp_pad_start"])
            deltas = verified["placement"]["symbol_deltas"]
            for symbol in ("irq_handler", "syscall_dispatch"):
                if deltas.get(symbol, 0) != (64 if profile == "code" else 0):
                    raise ValueError("unexpected trap target displacement")
            if deltas.get("os_main", 0) or any(deltas.get(n) != 64 for n in
                                               ("__os_bss_start", "__os_bss_end")):
                raise ValueError("unexpected entry/BSS displacement")
            if any(delta != 64 for delta in deltas.values()):
                raise ValueError("unexpected symbol displacement")
            verified["pad_start"] = symbols["__rpcmp_pad_start"]
        verified.update(command=argv, map_sha256=shared.sha256(folder / "firmware.map"),
                        linker_sha256=shared.sha256(folder / "os.ld"))
        (folder / "evidence.json").write_bytes(shared.json_bytes(verified))
        results[profile] = verified
    if hashes != {str(p.resolve()): shared.sha256(p) for p in inputs}:
        raise ValueError("link inputs changed during build")
    report = {"result": "PASS", "scope": "retained-object relink; hardware pending",
              "inputs": hashes, "profiles": results,
              "compiler": subprocess.check_output([command[0], "--version"], text=True).splitlines()[0]}
    (output / "evidence.json").write_bytes(shared.json_bytes(report))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    print(build(args.repo, args.output)["result"])
