"""Authored ELF/MIF fixtures exercise pairing without third-party firmware."""

import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import pocket_firmware_pair as pair
import pocket_m5_firmware_prepare as prepare


def fixture(boot_word=0x10336980):
    base = 0x10320000
    boot = struct.pack("<I", boot_word)
    payload = b"\x13\0\0\0" * 8
    names = b"\0.boot\0.osdata\0.os_bss\0.symtab\0.strtab\0.shstrtab\0"
    symbol_names = b"\0os_main\0__os_bss_start\0__os_bss_end\0irq_handler\0syscall_dispatch\0"
    symbols = bytes(16)
    for name, value in (("os_main", base), ("__os_bss_start", base + 64),
                        ("__os_bss_end", base + 80), ("irq_handler", base + 4),
                        ("syscall_dispatch", base + 8)):
        symbols += struct.pack("<IIIBBH", symbol_names.index(name.encode()), value, 0, 0x10, 0, 2)
    image = bytearray(52)
    headers = [bytes(40)]
    for name, kind, address, data, link, entry_size in (
        (".boot", 1, 0, boot, 0, 0), (".osdata", 1, base, payload, 0, 0),
        (".os_bss", 8, base + 64, bytes(16), 0, 0),
        (".symtab", 2, 0, symbols, 5, 16), (".strtab", 3, 0, symbol_names, 0, 0),
        (".shstrtab", 3, 0, names, 0, 0),
    ):
        offset = len(image)
        if kind != 8:
            image.extend(data)
        headers.append(struct.pack("<10I", names.index(name.encode()), kind, 2 if kind in (1, 8) else 0,
                                   address, offset, len(data), link, 0, 4, entry_size))
    shoff = len(image)
    image.extend(b"".join(headers))
    image[:52] = struct.pack("<16sHHIIIIIHHHHHH", b"\x7fELF\x01\x01\x01" + bytes(9),
                             2, 243, 1, 0, 0, shoff, 0, 52, 0, 0, 40, 7, 6)
    os_image = payload + struct.pack("<6I", 0x4942414F, 0x4F2EE14D, 0x3245534F,
                                     base, base + 64, base + 80)
    os_image += struct.pack("<II", 0x3143464F, zlib.crc32(os_image))
    mif = ("WIDTH=32; DEPTH=8192; ADDRESS_RADIX=DEC; DATA_RADIX=HEX; CONTENT BEGIN\n"
           f"0 : {boot_word:08X};\n[1..8191] : 00000013;\nEND;\n").encode()
    return bytes(image), mif, os_image


class PairTests(unittest.TestCase):
    def run_pair(self, artifacts):
        with tempfile.TemporaryDirectory() as directory:
            paths = [Path(directory) / n for n in ("firmware.elf", "firmware.mif", "os.bin")]
            for p, data in zip(paths, artifacts):
                p.write_bytes(data)
            return pair.verify(*paths)

    def test_same_link(self):
        self.assertEqual(self.run_pair(fixture())["result"], "PASS")

    def test_shifted_rom_reference_with_valid_metadata_is_rejected(self):
        elf, _, os_image = fixture(0x103369C0)
        _, old_mif, _ = fixture(0x10336980)
        with self.assertRaisesRegex(ValueError, "boot MIF does not match"):
            self.run_pair((elf, old_mif, os_image))

    def test_different_os_payload_even_with_updated_crc(self):
        elf, mif, os_image = fixture()
        changed = bytearray(os_image)
        changed[0] ^= 1
        struct.pack_into("<I", changed, len(changed) - 4, zlib.crc32(changed[:-8]))
        with self.assertRaisesRegex(ValueError, "OS payload"):
            self.run_pair((elf, mif, changed))

    def test_crc_and_metadata_corruption(self):
        elf, mif, original = fixture()
        for recalculate in (False, True):
            changed = bytearray(original)
            struct.pack_into("<I", changed, len(changed) - 16, 0x10320048)
            if recalculate:
                struct.pack_into("<I", changed, len(changed) - 4, zlib.crc32(changed[:-8]))
            with self.assertRaisesRegex(ValueError, "CRC mismatch|footer does not match"):
                self.run_pair((elf, mif, changed))

    def test_truncation_and_directory_bounds(self):
        elf, mif, os_image = fixture()
        for length in (0, 51, len(elf) - 1):
            with self.assertRaises(ValueError):
                self.run_pair((elf[:length], mif, os_image))
        changed = bytearray(elf)
        struct.pack_into("<I", changed, 32, 0xFFFFFFF0)
        with self.assertRaises(ValueError):
            self.run_pair((changed, mif, os_image))

    def test_mif_requires_complete_nonoverlapping_bounded_addresses(self):
        elf, mif, os_image = fixture()
        for bad in (mif.replace(b"1..8191", b"2..8191"),
                    mif.replace(b"1..8191", b"0..8191"),
                    mif.replace(b"1..8191", b"1..8192"),
                    mif.replace(b"00000013", b"00000000")):
            with self.assertRaises(ValueError):
                self.run_pair((elf, bad, os_image))

    def test_prepare_never_replaces_output_or_escapes_build_root(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            existing = root / "out/build/accepted"
            existing.mkdir(parents=True)
            sentinel = existing / "control"
            sentinel.write_bytes(b"preserve")
            for output in (existing, root / "elsewhere"):
                with self.assertRaisesRegex(ValueError, "new direct child"):
                    prepare.prepare(root, root, root, output)
                self.assertEqual(sentinel.read_bytes(), b"preserve")


if __name__ == "__main__":
    unittest.main()
