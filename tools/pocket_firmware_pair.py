#!/usr/bin/env python3
"""Verify that a boot MIF and OS image were produced by the same ELF32 link."""

import argparse
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path


def bounded_file(path, maximum):
    if path.stat().st_size > maximum:
        raise ValueError("firmware artifact exceeds inspection limit")
    return path.read_bytes()


def elf_sections(data):
    def span(offset, size):
        if offset > len(data) or size > len(data) - offset:
            raise ValueError("ELF range outside file")
        return data[offset:offset + size]

    header = struct.unpack("<16sHHIIIIIHHHHHH", span(0, 52))
    ident, kind, machine, version = header[:4]
    if ident[:7] != b"\x7fELF\x01\x01\x01" or (kind, machine, version) != (2, 243, 1):
        raise ValueError("expected little-endian ELF32 RISC-V executable")
    shoff, shsize, count, names_index = header[6], header[11], header[12], header[13]
    if shsize != 40 or not 1 <= count <= 256 or names_index >= count:
        raise ValueError("invalid ELF section directory")
    headers = [struct.unpack("<10I", span(shoff + i * 40, 40)) for i in range(count)]
    names = headers[names_index]
    strings = span(names[4], names[5])

    def string(table, offset):
        end = table.find(b"\0", offset)
        if offset >= len(table) or end < 0:
            raise ValueError("invalid ELF string")
        return table[offset:end].decode("ascii")

    sections = {}
    symbols = {}
    for h in headers:
        name = string(strings, h[0])
        content = b"" if h[1] == 8 else span(h[4], h[5])
        if name in sections:
            raise ValueError("duplicate ELF section")
        sections[name] = (h[3], h[5], h[1], content)
        if h[1] == 2:
            if h[9] != 16 or h[5] % 16 or h[6] >= count:
                raise ValueError("invalid ELF symbol table")
            table_header = headers[h[6]]
            table = span(table_header[4], table_header[5])
            for off in range(0, len(content), 16):
                n, value, _, info, _, section = struct.unpack_from("<IIIBBH", content, off)
                if n and section and info >> 4 in (1, 2):
                    symbol = string(table, n)
                    if symbol in symbols and symbols[symbol] != value:
                        raise ValueError("ambiguous ELF symbol")
                    symbols[symbol] = value
    return sections, symbols


def mif_bytes(text):
    text = re.sub(r"--[^\n]*", "", text)
    match = re.fullmatch(
        r"\s*WIDTH\s*=\s*32;\s*DEPTH\s*=\s*8192;\s*ADDRESS_RADIX\s*=\s*DEC;"
        r"\s*DATA_RADIX\s*=\s*HEX;\s*CONTENT\s+BEGIN(.*?)END;\s*", text, re.S
    )
    if not match:
        raise ValueError("expected complete 8192-word boot MIF")
    words = [None] * 8192
    for entry in match[1].split(";"):
        if not entry.strip():
            continue
        m = re.fullmatch(r"\s*(?:(\d+)|\[(\d+)\.\.(\d+)\])\s*:\s*([0-9A-Fa-f]{8})\s*", entry)
        if not m:
            raise ValueError("invalid boot MIF entry")
        lo, hi = (int(m[1]), int(m[1])) if m[1] else (int(m[2]), int(m[3]))
        if not 0 <= lo <= hi < 8192:
            raise ValueError("MIF address outside BRAM")
        for i in range(lo, hi + 1):
            if words[i] is not None:
                raise ValueError("overlapping boot MIF entries")
            words[i] = int(m[4], 16)
    if any(w is None for w in words):
        raise ValueError("incomplete boot MIF")
    return b"".join(struct.pack("<I", w) for w in words)


def verify(elf_path, mif_path, os_path):
    elf = bounded_file(elf_path, 16 * 1024 * 1024)
    mif = bounded_file(mif_path, 1024 * 1024)
    os_image = bounded_file(os_path, 768 * 1024)
    sections, symbols = elf_sections(elf)
    boot = bytearray(0x4000)
    occupied = bytearray(0x4000)
    boot_end = 0
    for name in (".boot", ".boot_data", ".boot_bss", ".fasttext", ".fastdata"):
        if name not in sections:
            continue
        address, size, kind, content = sections[name]
        if address > len(boot) or size > len(boot) - address:
            raise ValueError("OS BRAM section outside reserved region")
        if any(occupied[address:address + size]):
            raise ValueError("overlapping OS BRAM sections")
        occupied[address:address + size] = b"\1" * size
        if size and kind != 8:
            boot[address:address + size] = content
            boot_end = max(boot_end, address + size)
    if not boot_end or boot_end % 4 or ".boot" not in sections:
        raise ValueError("missing or unaligned boot image")
    expected = bytes(boot[:boot_end]) + b"\x13\0\0\0" * ((0x8000 - boot_end) // 4)
    if mif_bytes(mif.decode("ascii")) != expected:
        raise ValueError("boot MIF does not match linked ELF (including OS references)")
    if len(os_image) < 32:
        raise ValueError("missing OS footer")
    abi_magic, abi, meta_magic, entry, lo, hi, crc_magic, crc = struct.unpack("<8I", os_image[-32:])
    if (abi_magic, meta_magic, crc_magic) != (0x4942414F, 0x3245534F, 0x3143464F):
        raise ValueError("invalid OS footer markers")
    if abi != 0x4F2EE14D or zlib.crc32(os_image[:-8]) != crc:
        raise ValueError("OS ABI or CRC mismatch")
    address, size, kind, content = sections[".osdata"]
    if address != 0x10320000 or kind != 1 or size > 768 * 1024 - 32:
        raise ValueError("unexpected OS load section")
    if os_image[:-32] != content:
        raise ValueError("OS payload does not match linked ELF")
    if (entry, lo, hi) != tuple(symbols[n] for n in ("os_main", "__os_bss_start", "__os_bss_end")):
        raise ValueError("OS footer does not match linked ELF symbols")
    if not address <= entry < address + size or not address + len(os_image) <= lo <= hi <= 0x103E0000:
        raise ValueError("OS entry/BSS outside reserved memory or overlaps loaded image")
    return {"result": "PASS", "boot_bytes": boot_end,
            "boot_sha256": hashlib.sha256(boot[:boot_end]).hexdigest(),
            "elf_sha256": hashlib.sha256(elf).hexdigest(),
            "mif_sha256": hashlib.sha256(mif).hexdigest(),
            "os_sha256": hashlib.sha256(os_image).hexdigest(),
            "os_entry": entry, "os_bss_start": lo, "os_bss_end": hi,
            "irq_handler": symbols["irq_handler"], "syscall_dispatch": symbols["syscall_dispatch"]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("elf", "mif", "os"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(verify(args.elf, args.mif, args.os), indent=2, sort_keys=True))
