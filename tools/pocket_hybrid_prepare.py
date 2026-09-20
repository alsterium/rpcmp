"""Prepare a fresh HYB1 shell from the existing pinned APF/CPU substrate."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from mdx_hybrid_probe import checked_replace, prepare_jt51_wide


def prepare_fit(pocket):
    """Make the actual pinned shell directly buildable by Windows Quartus."""
    periph = (pocket / "../../common/axi_periph_slave.v").resolve()
    periph.write_text(checked_replace(periph.read_text(encoding="utf-8"),
                                     '.init_file("firmware.mif")',
                                     f'.init_file("{(pocket / "firmware.mif").resolve().as_posix()}")'),
                      encoding="utf-8")
    project = pocket.parents[3] / "hybrid-fit"
    project.mkdir(exist_ok=False)
    path_types = {"VERILOG_FILE", "SYSTEMVERILOG_FILE", "VHDL_FILE", "QIP_FILE",
                  "SDC_FILE", "MIF_FILE", "SOURCE_FILE", "SEARCH_PATH", "QSYS_FILE"}
    lines = []
    for line in (pocket / "ap_core.qsf").read_text(encoding="utf-8").splitlines():
        match = re.match(r'set_global_assignment -name (\w+) ("[^"]+"|\S+)(.*)', line)
        if match and match[1] in {"SEED", "NUM_PARALLEL_PROCESSORS"}:
            continue
        if match and match[1] in path_types:
            path = (pocket / match[2].strip('"')).resolve()
            if not path.exists():
                raise ValueError(f"missing shell source: {path}")
            line = f'set_global_assignment -name {match[1]} "{path.as_posix()}"{match[3]}'
        lines.append(line)
    variant = (pocket / "variants/rpcmp.mk").read_text(encoding="utf-8")
    defines = re.search(r"^DEFS := (.+)$", variant, re.MULTILINE)[1].split()
    if not {"INCLUDE_CLK90", "INCLUDE_RPCMP_PLAYER", "EXCLUDE_GPU"}.issubset(defines):
        raise ValueError("unexpected HYB1 CPU/video variant")
    lines.extend(f"set_global_assignment -name VERILOG_MACRO {name}" for name in defines)
    netlist = (pocket / "../../vendor/vexriscv/VexiiRiscv/VexiiRiscv_rpcmp.v").resolve()
    lines.extend([f'set_global_assignment -name VERILOG_FILE "{netlist.as_posix()}"',
                  f'set_global_assignment -name SEARCH_PATH "{pocket.as_posix()}"',
                  "set_global_assignment -name SEED 1",
                  "set_global_assignment -name NUM_PARALLEL_PROCESSORS 4"])
    (project / "ap_core.qsf").write_text("\n".join(lines) + "\n", encoding="utf-8")
    shutil.copyfile(pocket / "ap_core.qpf", project / "ap_core.qpf")
    return project


def prepare(args):
    repo = Path(__file__).resolve().parents[1]
    command = [sys.executable, "-B", str(repo / "tools/pocket_m5_audio_prepare.py"),
               "--repo", str(repo), "--apf-lifecycle", "--apf-flush", "--player-sound"]
    for field in ("openfpgaos", "jt51", "vexii_netlist", "boot_mif", "output",
                  "firmware_elf", "os_image"):
        value = getattr(args, field)
        if value is not None:
            command.extend(("--" + field.replace("_", "-"), str(value.resolve())))
    # The shared preparer validates pins, firmware pairing and a fresh output
    # directory before extracting/applying the existing platform patches.
    subprocess.run(command, check=True)
    pocket = args.output.resolve() / "src/fpga/targets/pocket"
    top = pocket / "core_top.v"
    text = top.read_text(encoding="utf-8")
    begin = "rpcmp_sound_mmio rpcmp_sound ("
    if text.count(begin) != 1:
        raise ValueError("unexpected sound binding")
    old = begin + text.split(begin)[1].split("\n);", 1)[0] + "\n);"
    replacement = """rpcmp_hybrid_mmio rpcmp_sound (
    .clk_cpu(clk_cpu), .clk_audio(clk_core_12288), .reset_n(reset_n),
    .address(rpcmp_mmio_addr[9:0]), .write_data(rpcmp_mmio_wdata),
    .byte_enable(rpcmp_mmio_rd ? 4'hf : rpcmp_mmio_be),
    .read(rpcmp_mmio_rd), .write(rpcmp_mmio_wr),
    .read_data(rpcmp_mmio_rdata), .error(rpcmp_mmio_error),
    .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac)
);"""
    top.write_text(checked_replace(text, old, replacement), encoding="utf-8", newline="\n")
    native = prepare_jt51_wide(args.jt51, args.output / "rpcmp-jt51-wide")
    qsf = pocket / "ap_core.qsf"
    lines = [line for line in qsf.read_text(encoding="utf-8").splitlines() if not any(
        marker in line for marker in ("core/rtl/pocket/", "rpcmp-jt51-hold/", "jt51.qip", "player-sound.sdc"))]
    sources = [("SYSTEMVERILOG_FILE", repo / f"core/rtl/pocket/rpcmp_hybrid_{name}.sv")
               for name in ("mixer", "audio", "mmio")]
    sources.extend(("VERILOG_FILE", path) for path in native)
    sources.append(("SDC_FILE", repo / "overlays/openfpgaos/hybrid-sound.sdc"))
    for kind, path in sources:
        relative = Path(os.path.relpath(path, pocket)).as_posix()
        lines.append(f'set_global_assignment -name {kind} "{relative}"')
    qsf.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(prepare_fit(pocket))
    print("HYB1 shell prepared; hardware validation pending" if args.firmware_elf else
          "Synthesis fixture only: hardware packaging requires matching HYB1 firmware")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for field in ("openfpgaos", "jt51", "vexii-netlist", "boot-mif", "output"):
        parser.add_argument("--" + field, type=Path, required=True)
    parser.add_argument("--firmware-elf", type=Path)
    parser.add_argument("--os-image", type=Path)
    prepare(parser.parse_args())
