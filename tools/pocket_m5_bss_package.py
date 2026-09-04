#!/usr/bin/env python3
"""Build the isolated local-only M5 BSS placement diagnostic package."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

import pocket_package as shared

CORE_ID = "RPCMP.M5BssProbe"
CORE_SHORTNAME = "M5BssProbe"
PLATFORM_ID = "rpcmp_m5bss"
VARIANT = "os25"
VERSION = "0.8.0-m5-bss"
RELEASE_DATE = "2026-09-05"
EXPECTED_OUTPUT = Path("out/build/pocket-m5-bss-package")
EXPECTED_ARCHIVE = Path("out/build/rpcmp-m5-bss-probe.zip")


def definitions() -> dict[str, object]:
    return {
        "audio.json": {"audio": {"magic": shared.MAGIC}},
        "core.json": {
            "core": {
                "magic": shared.MAGIC,
                "metadata": {
                    "platform_ids": [PLATFORM_ID],
                    "shortname": CORE_SHORTNAME,
                    "description": "RPCMP M5 BSS placement probe",
                    "author": "RPCMP",
                    "url": "",
                    "version": VERSION,
                    "date_release": RELEASE_DATE,
                },
                "framework": {
                    "target_product": "Analogue Pocket",
                    "version_required": "2.2",
                    "sleep_supported": False,
                    "dock": {"supported": True, "analog_output": False},
                    "hardware": {"link_port": True, "cartridge_adapter": 0},
                },
                "cores": [
                    {
                        "name": VARIANT,
                        "id": 0,
                        "filename": f"{VARIANT}.rbf_r",
                        "chip32_vm": "loader.bin",
                    }
                ],
            }
        },
        "data.json": {
            "data": {
                "magic": shared.MAGIC,
                "data_slots": [
                    {
                        "id": 0,
                        "name": "M5 BSS Probe",
                        "required": True,
                        "parameters": 275,
                        "extensions": ["json"],
                    },
                    {
                        "id": 1,
                        "name": "OS Binary",
                        "required": False,
                        "parameters": 0,
                        "extensions": ["bin"],
                        "deferload": True,
                    },
                    {
                        "id": 2,
                        "name": "OS Config",
                        "required": False,
                        "parameters": 0,
                        "extensions": ["ini"],
                        "deferload": True,
                    },
                    {
                        "id": 3,
                        "name": "Application",
                        "required": False,
                        "parameters": 0,
                        "extensions": ["elf"],
                        "deferload": True,
                    },
                ],
            }
        },
        "input.json": {"input": {"magic": shared.MAGIC, "controllers": []}},
        "interact.json": {
            "interact": {"magic": shared.MAGIC, "variables": [], "messages": []}
        },
        "variants.json": {"variants": {"magic": shared.MAGIC, "variant_list": []}},
        "video.json": {
            "video": {
                "magic": shared.MAGIC,
                "scaler_modes": [
                    {
                        "width": 320,
                        "height": 240,
                        "aspect_w": 4,
                        "aspect_h": 3,
                        "rotation": 0,
                        "mirror": 0,
                    }
                ],
            }
        },
    }


def expected_paths() -> set[PurePosixPath]:
    core = PurePosixPath("Cores") / CORE_ID
    common = PurePosixPath("Assets") / PLATFORM_ID / "common"
    instance = PurePosixPath("Assets") / PLATFORM_ID / CORE_ID
    return {
        *(core / name for name in definitions()),
        core / "loader.bin",
        core / f"{VARIANT}.rbf_r",
        common / "os.bin",
        common / "m5-bss.ini",
        common / "m5-bss.elf",
        instance / "m5-bss.json",
        PurePosixPath("Platforms") / f"{PLATFORM_ID}.json",
    }


def verify_tree(root: Path) -> list[Path]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    actual = {PurePosixPath(path.relative_to(root).as_posix()) for path in files}
    if actual != expected_paths():
        raise ValueError("M5 BSS package allowlist mismatch")
    for path in files:
        if path.suffix.lower() in shared.PROHIBITED_SUFFIXES:
            raise ValueError(f"prohibited media in package: {path}")
        if path.suffix.lower() == ".json":
            value = json.loads(path.read_text(encoding="ascii"))
            if len(value) != 1:
                raise ValueError(f"JSON must have one root: {path}")
            root_value = next(iter(value.values()))
            if path.name != f"{PLATFORM_ID}.json" and root_value.get("magic") != shared.MAGIC:
                raise ValueError(f"invalid APF magic: {path}")
            if path.name == "core.json":
                metadata = root_value.get("metadata", {})
                if metadata.get("shortname") != CORE_SHORTNAME or path.parent.name != CORE_ID:
                    raise ValueError("M5 BSS core identity mismatch")
            if path.name == "data.json":
                slots = root_value.get("data_slots", [])
                if [slot.get("id") for slot in slots] != [0, 1, 2, 3]:
                    raise ValueError("M5 BSS package must contain only slots 0 through 3")
                if any(slot.get("nonvolatile") for slot in slots):
                    raise ValueError("M5 BSS package prohibits nonvolatile slots")
                if any(slot.get("required") or not slot.get("deferload") for slot in slots[1:]):
                    raise ValueError("M5 BSS runtime slots must be optional and deferred")
            if path.name == "m5-bss.json":
                slots = root_value.get("data_slots", [])
                if [slot.get("id") for slot in slots] != [1, 2, 3]:
                    raise ValueError("M5 BSS instance must bind only runtime slots 1 through 3")
    return files


def build(repo: Path, sdk: Path, elf: Path, output: Path, archive: Path) -> dict[str, object]:
    if output.resolve() != (repo / EXPECTED_OUTPUT).resolve() or archive.resolve() != (
        repo / EXPECTED_ARCHIVE
    ).resolve():
        raise ValueError("M5 BSS outputs must use their dedicated out/build paths")
    if not elf.is_file():
        raise ValueError(f"M5 ELF does not exist: {elf}")

    revision = subprocess.run(
        ["git", "-C", str(sdk), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != shared.SDK_REVISION:
        raise ValueError(f"SDK revision mismatch: {revision}")

    manifest = shared.parse_manifest(sdk / "runtime" / "MANIFEST")
    loader = shared.verify_runtime_file(sdk / "runtime", manifest, "pocket/loader.bin")
    safe_os = (repo / shared.SAFE_MEMSET_OS).resolve()
    safe_rbf = (repo / shared.SAFE_MEMSET_RBF).resolve()
    if (
        safe_os.stat().st_size != shared.SAFE_MEMSET_OS_SIZE
        or shared.sha256(safe_os) != shared.SAFE_MEMSET_OS_SHA256
    ):
        raise ValueError("safe-layout OS identity mismatch")
    if (
        safe_rbf.stat().st_size != shared.SAFE_MEMSET_RBF_SIZE
        or shared.sha256(safe_rbf) != shared.SAFE_MEMSET_RBF_SHA256
    ):
        raise ValueError("safe-layout RBF identity mismatch")

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent, prefix="rpcmp-m5-bss-") as temporary:
        staging = Path(temporary)
        core = PurePosixPath("Cores") / CORE_ID
        for name, value in definitions().items():
            shared.write_file(staging, core / name, shared.json_bytes(value))
        shared.write_file(staging, core / "loader.bin", loader.read_bytes())
        shared.write_file(
            staging, core / f"{VARIANT}.rbf_r", shared.reverse_rbf_bits(safe_rbf.read_bytes())
        )
        common = PurePosixPath("Assets") / PLATFORM_ID / "common"
        shared.write_file(staging, common / "os.bin", safe_os.read_bytes())
        shared.write_file(staging, common / "m5-bss.elf", elf.read_bytes())
        shared.write_file(
            staging,
            common / "m5-bss.ini",
            b"[os]\nELF=m5-bss.elf\nARGS=\nVARIANT=os25\n",
        )
        instance = {
            "instance": {
                "magic": shared.MAGIC,
                "data_slots": [
                    {"id": 1, "filename": "os.bin"},
                    {"id": 2, "filename": "m5-bss.ini"},
                    {"id": 3, "filename": "m5-bss.elf"},
                ],
            }
        }
        shared.write_file(
            staging,
            PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "m5-bss.json",
            shared.json_bytes(instance),
        )
        platform = {
            "platform": {
                "category": "Computer",
                "name": "RPCMP M5 BSS Probe",
                "year": 2026,
                "manufacturer": "RPCMP",
            }
        }
        shared.write_file(
            staging,
            PurePosixPath("Platforms") / f"{PLATFORM_ID}.json",
            shared.json_bytes(platform),
        )
        verify_tree(staging)
        if output.exists():
            if output.is_symlink():
                raise ValueError("refusing to replace a symlinked output")
            shutil.rmtree(output)
        shutil.copytree(staging, output)

    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for source in verify_tree(output):
            info = zipfile.ZipInfo(source.relative_to(output).as_posix(), (2026, 9, 5, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, source.read_bytes())

    evidence = {
        "schema_version": 1,
        "distribution_status": "local diagnostic only",
        "core_id": CORE_ID,
        "version": VERSION,
        "safe_layout_os_sha256": shared.sha256(safe_os),
        "safe_layout_native_rbf_sha256": shared.sha256(safe_rbf),
        "artifacts": {
            path.relative_to(output).as_posix(): {
                "bytes": path.stat().st_size,
                "sha256": shared.sha256(path),
            }
            for path in verify_tree(output)
        },
        "zip": {"bytes": archive.stat().st_size, "sha256": shared.sha256(archive)},
    }
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--zip", dest="archive", type=Path, required=True)
    args = parser.parse_args()
    evidence = build(
        args.repo.resolve(), args.sdk.resolve(), args.elf.resolve(), args.output, args.archive
    )
    print(f"zip_sha256={evidence['zip']['sha256']}")
    print("result=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
