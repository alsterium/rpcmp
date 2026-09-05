"""Regression checks for the zero-initialized boot-ROM package failure."""

import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "tools"))
import pocket_m5_audio_package as package
import pocket_m5_audio_prepare as prepare


class M5AudioBootTests(unittest.TestCase):
    def test_missing_rom_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileNotFoundError):
                prepare.verify_boot_mif(Path(directory) / "missing.mif")

    def test_zero_rom_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            rom = Path(directory) / "firmware.mif"
            rom.write_text("WIDTH=32; DEPTH=8192; CONTENT BEGIN [0..8191]:0; END;")
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                prepare.verify_boot_mif(rom)

    def test_reports_are_required_and_missing_initialization_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pocket = root / "pocket"
            rbf = pocket / "bld/job/output_files/ap_core.rbf"
            rbf.parent.mkdir(parents=True)
            (pocket / "apf").mkdir()
            (pocket / "firmware.mif").write_bytes(b"synthetic ROM")
            (pocket / "apf/build_id.mif").write_bytes(
                (ROOT / "overlays/openfpgaos/build_id.mif").read_bytes()
            )
            # ROM identity has its own tests; no third-party ROM enters fixtures.
            with patch.object(package, "verify_boot_mif"):
                with self.assertRaises(FileNotFoundError):
                    package.verify_build(ROOT, rbf)
                for suffix in ("map.rpt", "fit.rpt", "sta.rpt", "asm.rpt", "flow.rpt"):
                    content = (
                        "; Flow Status ; Successful - test ;\n"
                        if suffix == "flow.rpt"
                        else "successful. 0 errors\n"
                    )
                    rbf.with_suffix(f".{suffix}").write_text(content)
                self.assertEqual(len(package.verify_build(ROOT, rbf)), 7)
                for warning in (
                    "Critical Warning (127003): Can't find Memory Initialization File",
                    "setting all initial values to 0",
                ):
                    rbf.with_suffix(".map.rpt").write_text("0 errors\n" + warning)
                    with self.assertRaisesRegex(ValueError, "missing memory initialization"):
                        package.verify_build(ROOT, rbf)
                rbf.with_suffix(".map.rpt").write_text("compilation incomplete")
                with self.assertRaisesRegex(ValueError, "zero-error completion"):
                    package.verify_build(ROOT, rbf)


if __name__ == "__main__":
    unittest.main()
