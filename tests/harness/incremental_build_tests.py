"""Verify the actual host compiler discovers changed headers in an incremental build."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args: str) -> bytes:
    return subprocess.run(args, check=True, capture_output=True).stdout


with tempfile.TemporaryDirectory(prefix="rpcmp-incremental-") as temporary:
    root = Path(temporary)
    (root / "CMakeLists.txt").write_text(
        'cmake_minimum_required(VERSION 3.25)\nproject(probe LANGUAGES CXX)\n'
        'add_executable(probe main.cpp)\n', encoding="ascii")
    header = root / "value.hpp"
    header.write_text("constexpr int value = 1;\n", encoding="ascii")
    (root / "main.cpp").write_text(
        '#include "value.hpp"\n#include <cstdio>\nint main() { std::printf("%d", value); }\n',
        encoding="ascii")
    build = root / "build"
    run(sys.argv[1], "-S", str(root), "-B", str(build), "-G", "Ninja")
    first_build = run(sys.argv[1], "--build", str(build))
    executable = build / ("probe.exe" if os.name == "nt" else "probe")
    assert run(str(executable)) == b"1"
    # Ensure a newer timestamp even on coarse-resolution filesystems.
    stamp = max(header.stat().st_mtime, executable.stat().st_mtime) + 2
    header.write_text("constexpr int value = 2;\n", encoding="ascii")
    os.utime(header, (stamp, stamp))
    second_build = run(sys.argv[1], "--build", str(build))
    prefixes = [line for line in (build / "CMakeFiles/rules.ninja").read_text(encoding="utf-8").splitlines()
                if "msvc_deps_prefix" in line]
    assert run(str(executable)) == b"2", (
        f"changed header did not rebuild its consumer; VSLANG={os.environ.get('VSLANG')!r}; "
        f"prefixes={prefixes!r}; first={first_build!r}; second={second_build!r}")
print("incremental header dependency: PASS")
