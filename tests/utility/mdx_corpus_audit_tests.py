import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    "mdx_corpus_audit", ROOT / "tools" / "mdx_corpus_audit.py"
)
assert SPEC is not None and SPEC.loader is not None
AUDIT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


def make_mdx(title: bytes = b"private title", pdx: bytes = b"private.pdx") -> bytes:
    table_bytes = 20
    track_offsets = [table_bytes + index * 2 for index in range(9)]
    voice_offset = table_bytes + 18
    table = bytearray()
    for offset in [voice_offset, *track_offsets]:
        table.extend(offset.to_bytes(2, "big"))
    body = b"\xf1\x00" * 9 + b"\x00"
    return title + b"\x0d\x0a\x1a" + pdx + b"\x00" + bytes(table) + body


class MdxCorpusAuditTests(unittest.TestCase):
    def test_valid_nine_track_file_reports_aggregate_structure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "track.MDX").write_bytes(make_mdx())
            report = AUDIT.audit_corpus(root)

        self.assertEqual(report["files_seen"], 1)
        self.assertEqual(report["layout_counts"], {"9": 1})
        self.assertEqual(report["track_target_counts"], {"legacy_adpcm": 1, "ym2151": 8})
        self.assertEqual(report["opcode_counts"], {"f1": 9})
        self.assertEqual(report["linear_decode_counts"], {"terminated": 9})
        self.assertEqual(report["status_counts"], {"bounded_layout": 1})

    def test_variable_extension_stops_without_guessing_length(self) -> None:
        data = bytearray(make_mdx())
        first_track = data.index(b"\xf1\x00")
        data[first_track : first_track + 2] = b"\xe7\x99"
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "extension.mdx").write_bytes(data)
            report = AUDIT.audit_corpus(root)

        self.assertEqual(report["opcode_counts"]["e7"], 1)
        self.assertEqual(report["linear_decode_counts"]["variable_extension"], 1)

    def test_report_and_cli_do_not_disclose_names_or_metadata(self) -> None:
        secrets = ("secret-file-name", "secret title", "secret-bank.pdx")
        with tempfile.TemporaryDirectory(prefix=secrets[0]) as temporary:
            root = Path(temporary)
            (root / f"{secrets[0]}.mdx").write_bytes(
                make_mdx(secrets[1].encode("ascii"), secrets[2].encode("ascii"))
            )
            completed = subprocess.run(
                [sys.executable, str(ROOT / "tools" / "mdx_corpus_audit.py"), str(root)],
                check=True,
                capture_output=True,
                text=True,
            )
            json.loads(completed.stdout)

        for secret in secrets:
            self.assertNotIn(secret, completed.stdout)
        self.assertNotIn(str(root), completed.stdout)

    def test_malformed_inputs_have_bounded_aggregate_results(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "missing-marker.mdx").write_bytes(b"not mdx")
            (root / "missing-pdx.mdx").write_bytes(b"t\x0d\x0a\x1a" + b"x" * 256)
            (root / "too-large.mdx").write_bytes(b"x" * 65)
            report = AUDIT.audit_corpus(root, max_file_bytes=64)

        self.assertEqual(report["files_seen"], 3)
        self.assertEqual(report["status_counts"]["input_too_large"], 2)
        self.assertEqual(report["status_counts"]["missing_title_terminator"], 1)

    def test_invalid_limit_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaisesRegex(ValueError, "max_file_bytes"):
                AUDIT.audit_corpus(Path(temporary), 0)


if __name__ == "__main__":
    unittest.main()
