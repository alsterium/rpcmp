import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("pocket_package", ROOT / "tools" / "pocket_package.py")
assert SPEC is not None and SPEC.loader is not None
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageBuilderTests(unittest.TestCase):
    def test_probe_metadata_is_versioned(self) -> None:
        metadata = PACKAGE.definitions()["core.json"]["core"]["metadata"]
        self.assertEqual(metadata["shortname"], PACKAGE.CORE_SHORTNAME)
        self.assertEqual(metadata["version"], "0.5.15-spike")
        self.assertEqual(metadata["date_release"], "2026-08-29")

    def test_native_rbf_bits_are_reversed_per_byte(self) -> None:
        self.assertEqual(PACKAGE.reverse_rbf_bits(bytes([0x00, 0x01, 0x96, 0xFF])),
                         bytes([0x00, 0x80, 0x69, 0xFF]))

    def make_valid_tree(self, root: Path) -> None:
        for relative in PACKAGE.expected_paths():
            destination = root.joinpath(*relative.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.suffix == ".json":
                if destination.name == f"{PACKAGE.PLATFORM_ID}.json":
                    value = {"platform": {}}
                elif destination.name == "data.json":
                    value = PACKAGE.definitions()["data.json"]
                elif destination.name in PACKAGE.definitions():
                    value = PACKAGE.definitions()[destination.name]
                else:
                    value = {"instance": {"magic": PACKAGE.MAGIC}}
                destination.write_bytes(PACKAGE.json_bytes(value))
            else:
                destination.write_bytes(b"fixture")

    def test_valid_allowlisted_tree_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            self.assertEqual(len(PACKAGE.verify_tree(root)), len(PACKAGE.expected_paths()))

    def test_prohibited_media_is_rejected_as_unexpected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            prohibited = root.joinpath(*PurePosixPath("Assets/rpcmp_probe/common/bank.ofsf").parts)
            prohibited.write_bytes(b"prohibited")
            with self.assertRaisesRegex(ValueError, "allowlist mismatch"):
                PACKAGE.verify_tree(root)

    def test_slot_seven_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            data_path = root / "Cores" / PACKAGE.CORE_ID / "data.json"
            value = json.loads(data_path.read_text(encoding="ascii"))
            value["data"]["data_slots"].append({"id": 7})
            data_path.write_bytes(PACKAGE.json_bytes(value))
            with self.assertRaisesRegex(ValueError, "only slots 0 through 4"):
                PACKAGE.verify_tree(root)

    def test_core_folder_metadata_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            core_path = root / "Cores" / PACKAGE.CORE_ID / "core.json"
            value = json.loads(core_path.read_text(encoding="ascii"))
            value["core"]["metadata"]["shortname"] = "RPCMP Probe"
            core_path.write_bytes(PACKAGE.json_bytes(value))
            with self.assertRaisesRegex(ValueError, "core folder must match"):
                PACKAGE.verify_tree(root)

    def test_non_deferred_os_config_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            data_path = root / "Cores" / PACKAGE.CORE_ID / "data.json"
            value = json.loads(data_path.read_text(encoding="ascii"))
            del value["data"]["data_slots"][2]["deferload"]
            data_path.write_bytes(PACKAGE.json_bytes(value))
            with self.assertRaisesRegex(ValueError, "optional and deferred"):
                PACKAGE.verify_tree(root)


if __name__ == "__main__":
    unittest.main()
