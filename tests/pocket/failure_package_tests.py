"""Input fault probes must preserve the accepted executable and audio substrate."""

import re
import struct
import sys
import unittest
from pathlib import Path, PurePosixPath

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_m5_failure_package as package
import pocket_m5_boot_package as boot
from boot_package_tests import source_files


def normal_source():
    files, old = source_files()
    fixture = Path(__file__).resolve().parents[1] / "fixtures/rpcmlib/minimal.rpcmlib.hex"
    files[old / "music.rpcmlib"] = bytes.fromhex(fixture.read_text())
    files[old / "m5-file.ini"] = b"[os]\nELF=m5-file.elf\nARGS=123456789abcdef0\nVARIANT=rpcmp\n"
    return boot.probe_files(files, b"\x01\x80", b"accepted os bytes", False)


class FailurePackageTests(unittest.TestCase):
    def test_only_selected_input_changes(self):
        source = normal_source()
        before = dict(source)
        common = PurePosixPath("Assets/rpcmp_m5boot/common")
        for profile, (short, platform, _) in package.PROFILES.items():
            with self.subTest(profile=profile):
                result = package.probe_files(source, profile)
                target = PurePosixPath("Assets") / platform / "common"
                self.assertEqual(result[target / "os.bin"], source[common / "os.bin"])
                self.assertEqual(result[target / "m5-file.elf"], source[common / "m5-file.elf"])
                for name in ("loader.bin", "rpcmp.rbf_r"):
                    self.assertEqual(result[PurePosixPath("Cores") / ("RPCMP." + short) / name],
                                     source[PurePosixPath("Cores/RPCMP.M5BootProbe") / name])
                self.assertEqual(len(result), len(source))
                self.assertRegex(platform, r"^[a-z0-9][a-z0-9_]{0,14}$")
                library = result[target / "music.rpcmlib"]
                config = result[target / "m5-file.ini"]
                if profile == "library":
                    original = source[common / "music.rpcmlib"]
                    self.assertEqual(library[1:], original[1:])
                    self.assertEqual(library[0] ^ original[0], 1)
                    self.assertEqual(config, source[common / "m5-file.ini"])
                else:
                    self.assertEqual(library, source[common / "music.rpcmlib"])
                    selected = int(re.search(rb"ARGS=([0-9a-f]+)", config)[1], 16)
                    self.assertEqual(selected, 0 if profile == "args" else package.absent_track(library))
        self.assertEqual(source, before)

    def test_absent_track_handles_id_one(self):
        source = normal_source()
        data = bytearray(source[PurePosixPath("Assets/rpcmp_m5boot/common/music.rpcmlib")])
        directory = struct.unpack_from("<Q", data, 40)[0]
        offset = struct.unpack_from("<Q", data, directory + 8)[0]
        self.assertEqual(data[directory:directory + 4], b"TRAK")
        struct.pack_into("<Q", data, offset + 8, 1)
        self.assertEqual(package.absent_track(data), 2)

    def test_truncated_or_oversized_directory_rejected(self):
        source = normal_source()
        data = bytearray(source[PurePosixPath("Assets/rpcmp_m5boot/common/music.rpcmlib")])
        for bad in (data[:79], data[:100]):
            with self.assertRaises(ValueError):
                package.absent_track(bad)
        struct.pack_into("<I", data, 48, 0xFFFFFFFF)
        with self.assertRaises(ValueError):
            package.absent_track(data)

    def test_ambiguous_config_rejected(self):
        source = normal_source()
        source[PurePosixPath("Assets/rpcmp_m5boot/common/m5-file.ini")] = b"ARGS=1\nARGS=2\n"
        with self.assertRaisesRegex(ValueError, "one hex"):
            package.probe_files(source, "args")


if __name__ == "__main__":
    unittest.main()
