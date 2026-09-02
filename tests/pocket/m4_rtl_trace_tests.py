import csv
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class M4RtlTraceTests(unittest.TestCase):
    def test_rtl_literals_match_engine_trace(self) -> None:
        with (ROOT / "specs/fixtures/mdx-fm-probe-trace-v1.csv").open(encoding="ascii") as stream:
            expected = [
                (int(row["at_tick"]), int(row["address"], 0), int(row["value"], 0))
                for row in csv.DictReader(line for line in stream if not line.startswith("#"))
            ]
        source = (ROOT / "core/rtl/pocket/rpcmp_m4_mdx_core.sv").read_text(encoding="ascii")
        pattern = re.compile(
            r"^\s*(\d+): begin operation_tick=(\d+); operation_address=8'h([0-9a-f]{2}); "
            r"operation_value=8'h([0-9a-f]{2}); end$",
            re.MULTILINE,
        )
        actual = [(int(index), int(tick), int(address, 16), int(value, 16))
                  for index, tick, address, value in pattern.findall(source)]
        self.assertEqual([(index, *entry) for index, entry in enumerate(expected)], actual)


if __name__ == "__main__":
    unittest.main()
