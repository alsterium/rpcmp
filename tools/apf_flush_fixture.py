#!/usr/bin/env python3
"""Generate test bindings around the actual prepared Pocket APF transport.

Extract the exact CDC/drain blocks and named port connections from core_top,
so the test does not maintain a second implementation of those crossings.
Only unrelated peripheral inputs are tied to zero. No behavioral oracle is
generated here; APF transactions and assertions are authored in the testbench.
"""
import argparse
import re
from pathlib import Path


def section(text, start, end):
    if text.count(start) != 1:
        raise ValueError(f"ambiguous/missing source boundary: {start}")
    tail = text.split(start, 1)[1]
    if end not in tail:
        raise ValueError(f"missing source boundary: {end}")
    return tail.split(end, 1)[0]


def generate(tree, output):
    top = (tree / "src/fpga/targets/pocket/core_top.v").read_text(encoding="utf-8")
    axi = (tree / "src/fpga/common/axi_periph_slave.v").read_text(encoding="utf-8")
    header = section(axi, ") (", "\n);")
    # The optional legacy sound ports are present in the test compilation too.
    ports = re.findall(r"^[ \t]*,?[ \t]*(input|output)[ \t]+(?:wire|reg)[ \t]*(\[[^\]]+\])?[ \t]*(\w+)",
                       header, re.MULTILINE)
    if len(ports) < 100 or len({p[2] for p in ports}) != len(ports):
        raise ValueError("unexpected peripheral port declaration")
    connections = []
    names = {p[2] for p in ports}
    required = {"target_dataslot_flush", "target_dataslot_id", "target_dataslot_done"}
    if not required <= names:
        raise ValueError("flush transport missing")
    for direction, width, name in ports:
        if name.startswith("target_"):
            matches = re.findall(rf"\.{name}\(([^)]+)\)", top)
            if len(matches) != 1:
                raise ValueError(f"ambiguous peripheral connection: {name}")
            value = matches[0]
        elif name.startswith("s_axi_") or name in ("clk", "reset_n"):
            value = {"clk": "clk_cpu", "reset_n": "reset_n_apf"}.get(name, name)
        elif name == "bridge_wr_idle":
            value = name
        elif direction == "input":
            value = "'0"
        else:
            value = ""
        connections.append(f"    .{name}({value})")
    instance = "axi_periph_slave #(.INCLUDE_APF_FLUSH(FLUSH_ENABLE)) periph (\n"
    instance += ",\n".join(connections) + "\n);\n"
    wiring = section(top, "// CPU-side signals (in clk_ram_controller domain)",
                     "// F2i: per-DS-command bridge word counters (clk_74a)")
    drain = section(top, "// Bridge DMA active tracking", "// Bridge SDRAM reads are not used.")
    # All the payload and completion connections must come from production.
    if ".INCLUDE_APF_FLUSH(1)" not in top or "cpu_ds_flush_start" not in drain:
        raise ValueError("Pocket flush integration missing")
    output.mkdir(parents=True, exist_ok=True)
    (output / "apf_flush_bindings.svh").write_text(wiring + drain + instance, encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tree", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.tree, args.output)
