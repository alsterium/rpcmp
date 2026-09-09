"""Compare linked ELF32 images, allowing only padding and RISC-V relocation bits."""

import struct
from pathlib import Path

import pocket_firmware_pair as pair

# psABI relocation encodings: permit immediate fields, never opcodes/registers.
MASKS = {
    1: (0xFFFFFFFF, 4), 16: (0xFE000F80, 4), 17: (0xFFFFF000, 4),
    18: (0xFFF00000FFFFF000, 8), 19: (0xFFF00000FFFFF000, 8),
    23: (0xFFFFF000, 4), 24: (0xFFF00000, 4), 25: (0xFE000F80, 4),
    26: (0xFFFFF000, 4), 27: (0xFFF00000, 4), 28: (0xFE000F80, 4),
    33: (0xFF, 1), 34: (0xFFFF, 2), 35: (0xFFFFFFFF, 4),
    37: (0xFF, 1), 38: (0xFFFF, 2), 39: (0xFFFFFFFF, 4),
    44: (0x1C7C, 2), 45: (0x1FFC, 2),
    51: (0, 0), 56: (0xFFFFFFFF, 4), 57: (0xFFFFFFFF, 4),
}


def layout(path):
    data = pair.bounded_file(Path(path), 16 * 1024 * 1024)
    sections, symbols = pair.elf_sections(data)
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    headers = [struct.unpack_from("<10I", data, header[6] + i * 40) for i in range(header[12])]
    names_header = headers[header[13]]
    names = data[names_header[4]:names_header[4] + names_header[5]]
    section_names = [names[h[0]:names.find(b"\0", h[0])].decode("ascii") for h in headers]
    allocated = {n: sections[n] for n, h in zip(section_names, headers) if h[2] & 2 and h[5]}
    masks = {n: bytearray(s[1]) for n, s in allocated.items() if s[2] != 8}
    counts = {}
    for h in headers:
        if h[1] != 4:
            continue
        if h[7] >= len(headers) or h[9] != 12 or h[5] % 12:
            raise ValueError("invalid relocation section")
        name = section_names[h[7]]
        if name not in masks:
            continue
        for position in range(h[4], h[4] + h[5], 12):
            address, info, _ = struct.unpack_from("<IIi", data, position)
            kind = info & 255
            counts[kind] = counts.get(kind, 0) + 1
            # Unknown relocation types get no permission to change bits.
            mask, width = MASKS.get(kind, (0, 0))
            offset = address - allocated[name][0]
            if not 0 <= offset <= len(masks[name]) or width > len(masks[name]) - offset:
                raise ValueError("relocation outside allocated section")
            for i in range(width):
                masks[name][offset + i] |= (mask >> (8 * i)) & 255
    return allocated, symbols, masks, counts


def compare(base_path, variant_path, pad_section, pad_address, pad_size=64):
    base, before, old_masks, old_counts = layout(base_path)
    new, after, new_masks, new_counts = layout(variant_path)
    if set(base) != set(new):
        raise ValueError("allocated section set changed")
    if pad_section not in base or pad_size <= 0:
        raise ValueError("padding requires an allocated section and positive size")
    if not old_counts or not new_counts:
        raise ValueError("both images must retain relocation metadata")
    if not set(before) <= set(after):
        raise ValueError("defined symbols disappeared")
    changes = {}
    section_layout = {}
    for name, (address, size, kind, contents) in base.items():
        new_address, new_size, new_kind, new_contents = new[name]
        extra = pad_size if name == pad_section else 0
        boundary = base[pad_section][0] + base[pad_section][1]
        shift = pad_size if address >= boundary else 0
        if new_address != address + shift:
            raise ValueError(f"section address changed unexpectedly: {name}")
        if kind != new_kind or new_size != size + extra:
            raise ValueError(f"section size/type changed unexpectedly: {name}")
        section_layout[name] = {"before_address": address, "after_address": new_address,
                                "before_bytes": size, "after_bytes": new_size}
        if extra:
            offset = pad_address - new_address
            if not 0 <= offset <= size:
                raise ValueError("padding outside selected section")
        if kind == 8:
            continue
        permissions = new_masks[name]
        if extra:
            if any(new_contents[offset:offset + extra]):
                raise ValueError("padding must contain only zero fill")
            new_contents = new_contents[:offset] + new_contents[offset + extra:]
            permissions = permissions[:offset] + permissions[offset + extra:]
        changed = 0
        for i, (a, b) in enumerate(zip(contents, new_contents)):
            # A changed byte must be relocation-controlled in both images.
            mask = old_masks[name][i] & permissions[i]
            if (a ^ b) & ~mask:
                raise ValueError(f"non-relocation bits changed: {name}+0x{i:x}")
            changed += a != b
        changes[name] = changed
    return {"result": "PASS", "changed_bytes_by_section": changes, "sections": section_layout,
            "symbol_deltas": {n: after[n] - v for n, v in before.items() if n in after and after[n] != v},
            "base_relocation_types": old_counts, "variant_relocation_types": new_counts}
