"""Build the pinned split renderer and minimal Pocket timing fixture in Docker."""
import argparse
from pathlib import Path
import subprocess

from hybrid_renderer_prepare import prepare
from mdx_hybrid_probe import ROOT, UNITS, run


def build(output):
    output = output.resolve()
    prepare(ROOT / "out/research/mdxplayer-reference-20260921", output)
    source = ROOT / "spikes/pocket/openfpgaos"
    jni = output / "jni"
    units = [source / "hybrid_renderer.cpp"] + [jni / path for path in UNITS]
    common = ["-std=c++17", "-O3", "-flto", "-fsigned-char", "-fno-exceptions", "-fno-rtti",
              "-include", "cstdint", "-ffunction-sections", "-fdata-sections", "-fstack-usage",
              "-isystem", str(jni)]
    run(["g++", *common, "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-fPIC",
         "-c", units[0], "-o", output / "renderer-native.o"])
    run(["g++", *common, "-fPIC", "-shared", output / "renderer-native.o", *units[1:],
         "-o", output / "renderer.so"])
    arch = ["-march=rv32imafc_zicsr", "-mabi=ilp32f"]
    sdk = ROOT / "out/research/openfpgaSDK-a408ddc/src/sdk"
    compiler = "riscv-none-elf-g++"
    def library(name):
        return subprocess.check_output([compiler, *arch, "-print-file-name=" + name], text=True).strip()
    search = run([compiler, *arch, "-x", "c++", "-E", "-Wp,-v", "-"],
                 input="", text=True, capture_output=True).stderr
    includes = ["-nostdinc"]
    for line in search.splitlines():
        if line.startswith(" /") and "c++" in line:
            includes += ["-isystem", line.strip()]
    includes += ["-isystem", str(sdk / "musl/include"), "-isystem", library("include")]
    objects = []
    for i, unit in enumerate([*units, source / "hybrid_main.cpp"]):
        obj = output / f"sdk-{i}.o"
        warnings = ["-Wall", "-Wextra", "-Wpedantic", "-Werror"] if unit.parent == source else []
        run([compiler, *arch, *common, *includes, *warnings, "-c", unit, "-o", obj])
        objects.append(obj)
    for name, unit in (("compat", source / "newlib_musl_compat.c"),
                       ("pocket", source / "pocket_sdk_adapter.c"),
                       ("video", source / "player_sdk_adapter.c"), ("init", sdk / "of_init.c")):
        obj = output / f"sdk-{name}.o"
        run(["riscv-none-elf-gcc", *arch, "-O3", "-flto", "-fstack-usage",
             "-ffunction-sections", "-fdata-sections", "-fno-tree-loop-distribute-patterns",
             "-I" + str(sdk / "musl/include"), "-I" + str(sdk / "include"), "-c", unit, "-o", obj])
        objects.append(obj)
    elf = output / "hybrid-player.elf"
    run([compiler, *arch, "-O3", "-flto", "-fstack-usage", "-nostdlib", "-static",
         "-T", sdk / "app.ld", "-Wl,--gc-sections", "-Wl,--no-warn-rwx-segments",
         "-Wl,-Map," + str(output / "hybrid-player.map"), "-o", elf,
         *[sdk / "musl/lib" / p for p in ("crt1.o", "crti.o", "crtn.o")], *objects,
         "-Wl,--start-group", sdk / "musl/lib/libc.a", library("libstdc++.a"),
         library("libsupc++.a"), library("libgcc.a"), "-Wl,--end-group"])
    undefined = run(["riscv-none-elf-nm", "--undefined-only", elf], capture_output=True, text=True).stdout
    if undefined.strip():
        raise ValueError(undefined)
    run(["python3", ROOT / "tools/elf_budget.py", "--elf", elf, "--size-tool", "riscv-none-elf-size",
         "--stack-root", output, "--static-limit", 56623104, "--data-limit", 131072,
         "--stack-limit", 524288, "--output", output / "budget.json"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    build(parser.parse_args().output)
