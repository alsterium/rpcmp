"""APF fault packages must change metadata/configuration, never music or firmware."""
import json
import sys
import unittest
from pathlib import Path, PurePosixPath

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_m5_apf_package as package
from failure_package_tests import normal_source


class ApfPackageTests(unittest.TestCase):
    def test_three_modes_preserve_payloads(self):
        source = normal_source()
        before = dict(source)
        for name, (short, platform, mode) in package.PROFILES.items():
            self.assertLessEqual(len(platform), 15)
            files = package.probe_files(source, name)
            self.assertEqual(len(files), len(source))
            common = PurePosixPath("Assets") / platform / "common"
            for filename in ("os.bin", "m5-file.elf", "m5-file.ini", "music.rpcmlib"):
                self.assertEqual(files[common / filename],
                                 source[PurePosixPath("Assets/rpcmp_m5boot/common") / filename])
            for filename in ("loader.bin", "rpcmp.rbf_r"):
                self.assertEqual(files[PurePosixPath("Cores") / ("RPCMP." + short) / filename],
                                 source[PurePosixPath("Cores/RPCMP.M5BootProbe") / filename])
            path = PurePosixPath("Assets") / platform / ("RPCMP." + short) / "m5-file.json"
            value = json.loads(files[path])["instance"]
            self.assertEqual(value["memory_writes"],
                             [{"address": "0xF7000020", "data": f"0x{mode * 0x01010101:08X}"}])
            for path, data in files.items():
                if path.suffix == ".json":
                    root = json.loads(data)
                    self.assertEqual(len(root), 1)
                    if "platform" in root:
                        # Platform metadata has its own schema, without magic.
                        self.assertEqual(root["platform"]["name"], f"RPCMP M5 APF2 {name.upper()}")
                    else:
                        self.assertEqual(next(iter(root.values()))["magic"], "APF_VER_1")
        self.assertEqual(source, before)

    def test_existing_memory_writes_rejected(self):
        source = normal_source()
        path = PurePosixPath("Assets/rpcmp_m5boot/RPCMP.M5BootProbe/m5-file.json")
        value = json.loads(source[path])
        value["instance"]["memory_writes"] = [{"address": 0, "data": 1}]
        source[path] = json.dumps(value).encode()
        with self.assertRaises(ValueError):
            package.probe_files(source, "normal")

    def test_m5_size_limits_include_single_chunk(self):
        path = PurePosixPath("Assets/rpcmp_m5boot/common/music.rpcmlib")
        for size in (0, 4, 79, 2 * 1024 * 1024 + 1):
            source = normal_source()
            source[path] = bytes(size)
            with self.assertRaisesRegex(ValueError, "size limits"):
                package.probe_files(source, "timeout")
        for size in (80, 4096, 4097):
            source = normal_source()
            source[path] = bytes(size)
            package.probe_files(source, "timeout")


if __name__ == "__main__":
    unittest.main()
