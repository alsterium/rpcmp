"""Relocation audit must not mistake debug offsets for executable references."""
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import pocket_m5_control_relink as relink


class RelinkTests(unittest.TestCase):
    def test_only_retained_bram_to_os_references(self):
        row = "00000126  003ee717 R_RISCV_PCREL_HI20     10336980   irq_handler + 0"
        text = "Relocation section '.rela.boot' at offset 0x100 contains 2 entries:\n" + row
        text += "\n00000130  003ee717 R_RISCV_HI20           40000200   device + 0"
        text += "\nRelocation section '.rela.debug_info' at offset 0x200 contains 1 entries:\n" + row
        refs = relink.references(text)
        self.assertEqual(len(refs), 1)
        self.assertEqual((refs[0]["offset"], refs[0]["symbol_value"]), (0x126, 0x10336980))

    def test_recovery_preserves_existing_output(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            output = root / "out/build/accepted"
            output.mkdir(parents=True)
            sentinel = output / "sentinel"
            sentinel.write_text("preserve")
            for target in (output, root / "outside"):
                with self.assertRaisesRegex(ValueError, "fresh direct child"):
                    relink.recover(root, target)
            self.assertEqual(sentinel.read_text(), "preserve")


if __name__ == "__main__":
    unittest.main()
