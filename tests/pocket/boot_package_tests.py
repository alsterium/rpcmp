"""Probe packaging preserves application bytes and isolates the injected CRC fault."""

import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path, PurePosixPath

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_m5_boot_package as package
import pocket_m5_file_package as control
import pocket_package as shared


def source_files():
    core = PurePosixPath("Cores") / control.CORE_ID
    common = PurePosixPath("Assets") / control.PLATFORM_ID / "common"
    files = {p: b"authored dummy payload" for p in control.expected_paths()}
    files.update({core / n: shared.json_bytes(v) for n, v in control.definitions().items()})
    files[PurePosixPath("Platforms") / f"{control.PLATFORM_ID}.json"] = shared.json_bytes(
        {"platform": {"name": "Control"}})
    files[PurePosixPath("Assets") / control.PLATFORM_ID / control.CORE_ID / "m5-file.json"] = (
        shared.json_bytes({"instance": {"magic": shared.MAGIC, "data_slots": [
            {"id": i, "filename": name} for i, name in enumerate(
                ("os.bin", "m5-file.ini", "m5-file.elf", "music.rpcmlib"), 1)]}}))
    return files, common


class BootPackageTests(unittest.TestCase):
    def test_valid_probe_preserves_control_assets(self):
        files, common = source_files()
        before = dict(files)
        result = package.probe_files(files, b"\x01\x80", bytes(range(64)), False)
        for name in ("m5-file.elf", "m5-file.ini", "music.rpcmlib"):
            self.assertEqual(result[PurePosixPath("Assets/rpcmp_m5boot/common") / name], files[common / name])
        self.assertEqual(result[PurePosixPath("Cores/RPCMP.M5BootProbe/rpcmp.rbf_r")], b"\x80\x01")
        self.assertEqual(files, before)
        self.assertEqual(len(result), len(files))

    def test_fault_probe_has_only_os_slot_and_one_changed_bit(self):
        source, _ = source_files()
        original = bytes(range(64))
        result = package.probe_files(source, b"rbf", original, True)
        self.assertEqual(result[PurePosixPath("Assets/rpcmp_m5crcstop/common/os.bin")],
                         original[:-1] + b"\x3e")
        self.assertFalse(any(p.suffix in (".elf", ".ini", ".rpcmlib") for p in result))
        instance = json.loads(result[PurePosixPath("Assets/rpcmp_m5crcstop/RPCMP.M5CrcStopProbe/m5-file.json")])
        self.assertEqual(instance["instance"]["data_slots"], [{"id": 1, "filename": "os.bin"}])
        self.assertEqual(len(result), len(source) - 3)

    def test_control_zip_identity_required(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder) / "control.zip"
            archive.write_bytes(b"changed archive")
            archive.with_suffix(".evidence.json").write_text(json.dumps({"zip": {"sha256": "0" * 64}}))
            with self.assertRaisesRegex(ValueError, "identity"):
                package.read_control(archive)

    def test_unexpected_control_members_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            archive = Path(folder) / "control.zip"
            with zipfile.ZipFile(archive, "w") as zipped:
                zipped.writestr("../escape", b"unexpected")
            archive.with_suffix(".evidence.json").write_text(json.dumps(
                {"zip": {"sha256": shared.sha256(archive)}}))
            with self.assertRaisesRegex(ValueError, "allowlist"):
                package.read_control(archive)


if __name__ == "__main__":
    unittest.main()
