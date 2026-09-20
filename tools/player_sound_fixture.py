#!/usr/bin/env python3
"""Extract the actual Pocket AXI/sound/reset binding for authored bus tests."""
import argparse
import re
from pathlib import Path

from apf_flush_fixture import section


def generate(tree: Path, output: Path) -> None:
    top = (tree / "src/fpga/targets/pocket/core_top.v").read_text(encoding="utf-8")
    axi = (tree / "src/fpga/common/axi_periph_slave.v").read_text(encoding="utf-8")
    ports = re.findall(
        r"^[ \t]*,?[ \t]*(input|output)[ \t]+(?:wire|reg)[ \t]*(\[[^\]]+\])?[ \t]*(\w+)",
        section(axi, ") (", "\n);"), re.MULTILINE,
    )
    names = {port[2] for port in ports}
    if len(ports) < 100 or len(names) != len(ports) or not {"rpcmp_mmio_be", "rpcmp_mmio_error"} <= names:
        raise ValueError("unexpected player peripheral interface")
    actual = section(top, ") periph (", "\n    );")
    connections = []
    for direction, _, name in ports:
        if name.startswith("s_axi_"):
            value = name
        elif name.startswith("rpcmp_") or name in ("clk", "reset_n"):
            matches = re.findall(rf"\.{name}\(([^)]+)\)", actual)
            if len(matches) != 1:
                raise ValueError(f"ambiguous/missing connection: {name}")
            value = matches[0]
        else:
            value = "'0" if direction == "input" else ""
        connections.append(f"    .{name}({value})")
    reset = "reg [1:0] reset_cpu_core_sync;" + section(
        top, "reg [1:0] reset_cpu_core_sync;", "// Synchronize reset deassertion to clk_vid domain",
    )
    sound = "rpcmp_sound_mmio rpcmp_sound (" + section(top, "rpcmp_sound_mmio rpcmp_sound (", "\n);") + "\n);\n"
    signals = (
        "wire rpcmp_mmio_rd, rpcmp_mmio_wr, rpcmp_mmio_error, rpcmp_sound_fault;\n"
        "wire [31:0] rpcmp_mmio_addr, rpcmp_mmio_wdata, rpcmp_mmio_rdata;\n"
        "wire [3:0] rpcmp_mmio_be;\n"
    )
    output.mkdir(parents=True, exist_ok=True)
    (output / "player_bindings.svh").write_text(
        signals + reset + "axi_periph_slave periph (\n" + ",\n".join(connections) + "\n);\n" + sound,
        encoding="utf-8",
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tree", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.tree, args.output)
