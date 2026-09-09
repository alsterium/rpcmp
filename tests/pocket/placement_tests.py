"""Authored instruction fields test padding and relocation-only comparison."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from pathlib import PurePosixPath

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_placement_verify as verify
import pocket_m5_os_placement as build
import pocket_m5_app_placement as app_build
import pocket_m5_placement_package as package
from boot_package_tests import source_files


def fixture(padding=0, word=0x103202B7):
    names = b"\0.text\0.bss\0.rela.text\0.symtab\0.strtab\0.shstrtab\0"
    base = 0x10320000
    text = struct.pack("<I", 0x00000013) + bytes(padding) + struct.pack("<I", word)
    symbols = bytes(16) + struct.pack("<IIIBBH", 1, base + 4 + padding, 4, 0x12, 0, 1)
    strings = b"\0target\0"
    image = bytearray(52)
    headers = [bytes(40)]
    for name, kind, flags, address, data, link, info, entsize in (
        (".text", 1, 6, base, text, 0, 0, 0),
        (".bss", 8, 3, base + 256 + padding, bytes(4), 0, 0, 0),
        (".rela.text", 4, 0, 0, struct.pack("<IIi", base + 4 + padding, (1 << 8) | 26, 0), 4, 1, 12),
        (".symtab", 2, 0, 0, symbols, 5, 0, 16),
        (".strtab", 3, 0, 0, strings, 0, 0, 0),
        (".shstrtab", 3, 0, 0, names, 0, 0, 0),
    ):
        offset = len(image)
        if kind != 8:
            image.extend(data)
        headers.append(struct.pack("<10I", names.index(name.encode()), kind, flags, address,
                                   offset, len(data), link, info, 4, entsize))
    shoff = len(image)
    image.extend(b"".join(headers))
    image[:52] = struct.pack("<16sHHIIIIIHHHHHH", b"\x7fELF\x01\x01\x01" + bytes(9),
                             2, 243, 1, base, 0, shoff, 0, 52, 0, 0, 40, 7, 6)
    return image


class PlacementTests(unittest.TestCase):
    def test_package_preserves_fixed_assets_and_isolates_targets(self):
        source, common = source_files()
        original = dict(source)
        for target in ("os", "app"):
            for profile in ("code", "bss"):
                with self.subTest(target=target, profile=profile):
                    result = package.probe_files(source, b"\x01\x80", b"os", profile, target,
                                                 b"app" if target == "app" else None)
                    folder = PurePosixPath(f"Assets/rpcmp_m5{target}{profile}/common")
                    for name in ("m5-file.ini", "music.rpcmlib"):
                        self.assertEqual(result[folder / name], source[common / name])
                    self.assertEqual(result[folder / "m5-file.elf"],
                                     b"app" if target == "app" else source[common / "m5-file.elf"])
                    self.assertEqual(len(result), len(source))
                    self.assertEqual(result[folder / "os.bin"], b"os")
        self.assertEqual(source, original)

    def test_app_linker_requires_unique_marker(self):
        with self.assertRaisesRegex(ValueError, "not unique"):
            app_build.linker_script("", "bss")

    def test_timing_rejects_negative_slack_or_missing_hold(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "sta.summary"
            path.write_text("Type : Model Setup 'clk'\nSlack : 0.7\nTNS : 0.000\n")
            with self.assertRaisesRegex(ValueError, "coverage missing"):
                package.timing(path)
            with path.open("a") as output:
                output.write("Type : Model Hold 'clk'\nSlack : -0.1\nTNS : -0.1\n")
            with self.assertRaisesRegex(ValueError, "violations"):
                package.timing(path)

    def test_linker_insertion_requires_unique_marker(self):
        for source in ("", "        KEEP(*(.text*))\n" * 2):
            with self.assertRaisesRegex(ValueError, "not unique"):
                build.linker_script(source, "code")

    def test_unknown_profile_rejected(self):
        with self.assertRaisesRegex(ValueError, "unknown"):
            build.linker_script("", "other")

    def compare(self, candidate):
        with tempfile.TemporaryDirectory() as folder:
            before, after = (Path(folder) / n for n in ("before.elf", "after.elf"))
            before.write_bytes(fixture())
            after.write_bytes(candidate)
            return verify.compare(before, after, ".text", 0x10320004)

    def test_padding_and_relocated_immediate(self):
        result = self.compare(fixture(64, 0x103212B7))
        self.assertEqual(result["result"], "PASS")
        self.assertEqual(result["symbol_deltas"]["target"], 64)

    def test_register_change_is_not_relocation(self):
        with self.assertRaisesRegex(ValueError, "non-relocation bits"):
            self.compare(fixture(64, 0x10321337))

    def test_section_address_change_rejected(self):
        candidate = fixture(64, 0x103212B7)
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", candidate)
        struct.pack_into("<I", candidate, header[6] + 2 * 40 + 12, 0x10320200)
        with self.assertRaisesRegex(ValueError, "section address"):
            self.compare(candidate)

    def test_nonzero_padding_rejected(self):
        candidate = fixture(64, 0x103212B7)
        candidate[52 + 4] = 1
        with self.assertRaisesRegex(ValueError, "zero fill"):
            self.compare(candidate)

    def test_wrong_padding_size_rejected(self):
        with self.assertRaisesRegex(ValueError, "size/type"):
            self.compare(fixture(60))

    def test_unknown_padding_section_cannot_pass_an_unchanged_image(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "control.elf"
            path.write_bytes(fixture())
            with self.assertRaisesRegex(ValueError, "allocated section"):
                verify.compare(path, path, ".missing", 0x10320000)


if __name__ == "__main__":
    unittest.main()
