import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
SPEC = importlib.util.spec_from_file_location(
    "pocket_m5_bss_package", ROOT / "tools" / "pocket_m5_bss_package.py"
)
assert SPEC is not None and SPEC.loader is not None
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class M5BssPackageTests(unittest.TestCase):
    def make_valid_tree(self, root: Path) -> None:
        for relative in PACKAGE.expected_paths():
            destination = root.joinpath(*relative.parts)
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.suffix != ".json":
                destination.write_bytes(b"fixture")
            elif destination.name == f"{PACKAGE.PLATFORM_ID}.json":
                destination.write_bytes(PACKAGE.shared.json_bytes({"platform": {}}))
            elif destination.name == "m5-bss.json":
                value = {
                    "instance": {
                        "magic": PACKAGE.shared.MAGIC,
                        "data_slots": [{"id": slot} for slot in (1, 2, 3)],
                    }
                }
                destination.write_bytes(PACKAGE.shared.json_bytes(value))
            else:
                destination.write_bytes(PACKAGE.shared.json_bytes(PACKAGE.definitions()[destination.name]))

    def test_identity_is_separate_from_safe_control(self) -> None:
        self.assertEqual(PACKAGE.CORE_ID, "RPCMP.M5BssProbe")
        self.assertNotEqual(PACKAGE.CORE_ID, PACKAGE.shared.CORE_ID)
        self.assertEqual(PACKAGE.definitions()["core.json"]["core"]["metadata"]["version"], "0.8.0-m5-bss")

    def test_tree_is_strictly_allowlisted(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            self.assertEqual(len(PACKAGE.verify_tree(root)), len(PACKAGE.expected_paths()))
            prohibited = root.joinpath(*PurePosixPath("Assets/rpcmp_m5bss/common/bank.ofsf").parts)
            prohibited.write_bytes(b"fixture")
            with self.assertRaisesRegex(ValueError, "allowlist mismatch"):
                PACKAGE.verify_tree(root)

    def test_non_deferred_runtime_slot_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.make_valid_tree(root)
            data_path = root / "Cores" / PACKAGE.CORE_ID / "data.json"
            value = json.loads(data_path.read_text(encoding="ascii"))
            del value["data"]["data_slots"][1]["deferload"]
            data_path.write_bytes(PACKAGE.shared.json_bytes(value))
            with self.assertRaisesRegex(ValueError, "optional and deferred"):
                PACKAGE.verify_tree(root)


if __name__ == "__main__":
    unittest.main()
