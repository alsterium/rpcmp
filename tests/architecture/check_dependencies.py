#!/usr/bin/env python3
"""Check the RPCMP source dependency boundary and active-milestone scope."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".v", ".sv"}


def source_files(path: Path):
    if not path.exists():
        return
    for candidate in path.rglob("*"):
        if candidate.is_file() and candidate.suffix.lower() in SOURCE_SUFFIXES:
            yield candidate


def find_violations(root: Path) -> list[str]:
    violations: list[str] = []

    runtime_root = root / "core" / "runtime"
    for path in source_files(runtime_root):
        text = path.read_text(encoding="utf-8")
        if "rpcmp/ui/" in text or "core/ui" in text.replace("\\", "/"):
            violations.append(f"runtime depends on UI: {path.relative_to(root)}")
        if "rpcmp/spike/" in text or "spikes/" in text.replace("\\", "/"):
            violations.append(f"runtime depends on feasibility spike: {path.relative_to(root)}")

    ui_root = root / "core" / "ui"
    for path in source_files(ui_root):
        text = path.read_text(encoding="utf-8")
        if "rpcmp/runtime/" in text or "core/runtime" in text.replace("\\", "/"):
            violations.append(f"UI depends on runtime: {path.relative_to(root)}")
        if "rpcmp/spike/" in text or "spikes/" in text.replace("\\", "/"):
            violations.append(f"UI depends on feasibility spike: {path.relative_to(root)}")

    contracts_root = root / "core" / "contracts"
    for path in source_files(contracts_root):
        text = path.read_text(encoding="utf-8")
        if "rpcmp/spike/" in text or "spikes/" in text.replace("\\", "/"):
            violations.append(f"contracts depend on feasibility spike: {path.relative_to(root)}")

    catalog_boundaries = {
        "player": ("ui",),
        "ui": ("player", "library"),
        "contracts": ("player", "library", "runtime", "ui"),
        "pocket_canvas": ("player", "library", "runtime", "pocket_sound"),
    }
    for owner, forbidden in catalog_boundaries.items():
        for path in source_files(root / "core" / owner):
            text = path.read_text(encoding="utf-8").replace("\\", "/")
            for dependency in forbidden:
                if f"rpcmp/{dependency}/" in text or f"core/{dependency}" in text:
                    violations.append(f"{owner} depends on {dependency}: {path.relative_to(root)}")

    library_root = root / "core" / "library"
    for path in source_files(library_root):
        text = path.read_text(encoding="utf-8")
        normalized = text.replace("\\", "/")
        if "rpcmp/ui/" in text or "core/ui" in normalized:
            violations.append(f"library depends on UI: {path.relative_to(root)}")
        if "rpcmp/runtime/" in text or "core/runtime" in normalized:
            violations.append(f"library depends on runtime: {path.relative_to(root)}")
        if "rpcmp/spike/" in text or "spikes/" in normalized:
            violations.append(f"library depends on feasibility spike: {path.relative_to(root)}")

    cmake = root / "CMakeLists.txt"
    if cmake.exists():
        text = cmake.read_text(encoding="utf-8")
        runtime_link = re.search(
            r"target_link_libraries\s*\(\s*rpcmp_runtime\b(?P<body>.*?)\)",
            text,
            flags=re.DOTALL,
        )
        if runtime_link and "rpcmp_ui" in runtime_link.group("body"):
            violations.append("rpcmp_runtime links rpcmp_ui")
        ui_link = re.search(
            r"target_link_libraries\s*\(\s*rpcmp_ui\b(?P<body>.*?)\)",
            text,
            flags=re.DOTALL,
        )
        if ui_link and "rpcmp_runtime" in ui_link.group("body"):
            violations.append("rpcmp_ui links rpcmp_runtime")

        for owner, forbidden in catalog_boundaries.items():
            links = re.finditer(
                rf"target_link_libraries\s*\(\s*rpcmp_{owner}\b(?P<body>.*?)\)",
                text, flags=re.DOTALL,
            )
            for link in links:
                for dependency in forbidden:
                    if re.search(rf"\brpcmp_{dependency}\b", link.group("body")):
                        violations.append(f"rpcmp_{owner} links rpcmp_{dependency}")

    allowed_rtl = {
        Path("core/rtl/pocket/rpcmp_spike_regs.sv"),
        Path("core/rtl/pocket/rpcmp_device_queue.sv"),
        Path("core/rtl/pocket/rpcmp_sound_reset.sv"),
        Path("core/rtl/pocket/rpcmp_openfpgaos_sound.sv"),
        Path("core/rtl/pocket/rpcmp_pocket_audio.sv"),
        Path("core/rtl/pocket/rpcmp_jt51_audio.sv"),
        Path("core/rtl/pocket/rpcmp_pocket_media_audio.sv"),
        Path("core/rtl/pocket/rpcmp_jt51_media_audio.sv"),
        Path("core/rtl/pocket/rpcmp_media_envelope.sv"),
        Path("core/rtl/pocket/rpcmp_media_output.sv"),
        Path("core/rtl/pocket/rpcmp_jt51_media_source.sv"),
        Path("core/rtl/pocket/rpcmp_jt51_enveloped_audio.sv"),
        Path("core/rtl/pocket/rpcmp_native_completion.sv"),
        Path("core/rtl/pocket/rpcmp_media_source_queue.sv"),
        Path("core/rtl/pocket/rpcmp_jt51_progress_audio.sv"),
        Path("core/rtl/pocket/rpcmp_sound_session.sv"),
        Path("core/rtl/pocket/rpcmp_sound_mailbox.sv"),
        Path("core/rtl/pocket/rpcmp_sound_mmio.sv"),
        Path("core/rtl/pocket/rpcmp_hybrid_mixer.sv"),
        Path("core/rtl/pocket/rpcmp_hybrid_audio.sv"),
        Path("core/rtl/pocket/rpcmp_hybrid_mmio.sv"),
        Path("core/rtl/pocket/rpcmp_output_journal.sv"),
        Path("core/rtl/pocket/rpcmp_settings_ram.sv"),
        Path("core/rtl/pocket/rpcmp_m2_fixed_core.sv"),
        Path("core/rtl/pocket/rpcmp_m4_mdx_core.sv"),
        Path("core/rtl/pocket/rpcmp_stereo_probe_core.sv"),
    }
    for path in source_files(root / "core" / "rtl"):
        relative_path = path.relative_to(root)
        if relative_path not in allowed_rtl:
            violations.append(f"out-of-scope RTL implementation: {relative_path}")

    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--expect-violation", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    violations = find_violations(root)
    for violation in violations:
        print(violation)

    if args.expect_violation:
        if violations:
            print("expected architecture violation detected")
            return 0
        print("expected an architecture violation, but none was detected", file=sys.stderr)
        return 1

    if violations:
        return 1
    print("architecture and milestone scope checks: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
