"""The host guard tests need no downloaded/vendor HDL; RTL proves the actual edit recipe."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import jt51_hold_prepare as prepare


class PreparationGuards(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='jt51-hold-guard-', dir=ROOT / 'out')
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.output = self.directory / 'generated'

    def test_outside_output_never_reads_or_writes_source(self):
        outside = ROOT / 'core' / self.directory.name
        with patch.object(prepare, 'git') as git:
            with self.assertRaises(ValueError):
                prepare.prepare(self.directory, outside)
            git.assert_not_called()
        self.assertFalse(outside.exists())

    def test_existing_output_preserves_unrelated_content(self):
        self.output.mkdir()
        sentinel = self.output / 'sentinel'
        sentinel.write_bytes(b'preserve this output')
        with patch.object(prepare, 'git') as git:
            with self.assertRaises(ValueError):
                prepare.prepare(self.directory, self.output)
            git.assert_not_called()
        self.assertEqual(sentinel.read_bytes(), b'preserve this output')

    def test_wrong_revision_publishes_nothing(self):
        with patch.object(prepare, 'git', return_value=b'0' * 40 + b'\n'):
            with self.assertRaisesRegex(ValueError, 'revision mismatch'):
                prepare.prepare(self.directory, self.output)
        self.assertFalse(self.output.exists())

    def test_dirty_pinned_source_publishes_nothing(self):
        with patch.object(prepare, 'git', side_effect=[prepare.REVISION.encode(), b' M hdl/jt51.v']):
            with self.assertRaisesRegex(ValueError, 'local changes'):
                prepare.prepare(self.directory, self.output)
        self.assertFalse(self.output.exists())

    def test_comments_keep_offsets_and_cannot_add_a_clock_port(self):
        authored = '/* input clk,\n license */\n// input clk,\ninput clk,\n'
        mask = prepare.code_mask(authored)
        self.assertEqual(len(mask), len(authored))
        self.assertEqual([i for i, value in enumerate(mask) if value == '\n'],
                         [i for i, value in enumerate(authored) if value == '\n'])
        self.assertEqual(mask.index('input clk'), authored.rindex('input clk'))


if __name__ == '__main__':
    unittest.main()
