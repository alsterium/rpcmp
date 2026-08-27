#!/usr/bin/env python3
"""Build the allowlisted local-only openfpgaOS Pocket probe package."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

SDK_REVISION = "a408ddc12aed0dfaa4aa22c06af82f829db77126"
RUNTIME_REVISION = "618a3eb"
CORE_ID = "RPCMP.openfpgaOSProbe"
CORE_SHORTNAME = "openfpgaOSProbe"
PLATFORM_ID = "rpcmp_probe"
VARIANT = "os25"
MAGIC = "APF_VER_1"
PROHIBITED_SUFFIXES = {".mid", ".mod", ".ofsf"}


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("ascii")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_manifest(path: Path) -> dict[str, str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not any(f"source: {RUNTIME_REVISION}" in line for line in lines):
        raise ValueError(f"runtime manifest does not identify source {RUNTIME_REVISION}")
    entries: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith("#"):
            continue
        digest, relative = line.split(maxsplit=1)
        entries[relative.removeprefix("./")] = digest
    return entries


def verify_runtime_file(runtime: Path, manifest: dict[str, str], relative: str) -> Path:
    source = runtime / relative
    expected = manifest.get(relative)
    if expected is None or not source.is_file():
        raise ValueError(f"runtime artifact is absent from its manifest: {relative}")
    actual = hashlib.md5(source.read_bytes()).hexdigest()  # noqa: S324 - upstream manifest format
    if actual != expected:
        raise ValueError(f"runtime artifact checksum mismatch: {relative}")
    return source


def definitions() -> dict[str, object]:
    core = {
        "core": {
            "magic": MAGIC,
            "metadata": {
                "platform_ids": [PLATFORM_ID],
                "shortname": CORE_SHORTNAME,
                "description": "RPCMP M0 openfpgaOS compatibility probe",
                "author": "RPCMP",
                "url": "",
                "version": "0.1.0-spike",
                "date_release": "2026-08-26",
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
    }
    slots = [
        {"id": 0, "name": "Probe", "required": True, "parameters": 275, "extensions": ["json"]},
        {"id": 1, "name": "OS Binary", "required": False, "parameters": 0, "extensions": ["bin"], "deferload": True},
        {"id": 2, "name": "OS Config", "required": False, "parameters": 0, "extensions": ["ini"], "deferload": True},
        {"id": 3, "name": "Application", "required": False, "parameters": 0, "extensions": ["elf"], "deferload": True},
        {"id": 4, "name": "Synthetic Data", "required": False, "parameters": 0, "extensions": ["bin"], "deferload": True},
    ]
    return {
        "audio.json": {"audio": {"magic": MAGIC}},
        "core.json": core,
        "data.json": {"data": {"magic": MAGIC, "data_slots": slots}},
        "input.json": {
            "input": {
                "magic": MAGIC,
                "controllers": [
                    {
                        "type": "default",
                        "mappings": [
                            {"id": 0, "name": "A", "key": "pad_btn_a"},
                            {"id": 1, "name": "B", "key": "pad_btn_b"},
                            {"id": 2, "name": "X", "key": "pad_btn_x"},
                            {"id": 3, "name": "Y", "key": "pad_btn_y"},
                            {"id": 10, "name": "L", "key": "pad_trig_l"},
                            {"id": 11, "name": "R", "key": "pad_trig_r"},
                            {"id": 20, "name": "Start", "key": "pad_btn_start"},
                            {"id": 21, "name": "Select", "key": "pad_btn_select"},
                        ],
                    }
                ],
            }
        },
        "interact.json": {"interact": {"magic": MAGIC, "variables": [], "messages": []}},
        "variants.json": {"variants": {"magic": MAGIC, "variant_list": []}},
        "video.json": {
            "video": {
                "magic": MAGIC,
                "scaler_modes": [
                    {"width": 320, "height": 240, "aspect_w": 4, "aspect_h": 3, "rotation": 0, "mirror": 0}
                ],
            }
        },
    }


def expected_paths() -> set[PurePosixPath]:
    core_root = PurePosixPath("Cores") / CORE_ID
    common_root = PurePosixPath("Assets") / PLATFORM_ID / "common"
    instance_root = PurePosixPath("Assets") / PLATFORM_ID / CORE_ID
    names = set(definitions()) | {"loader.bin", f"{VARIANT}.rbf_r"}
    paths = {core_root / name for name in names}
    paths |= {common_root / name for name in ("os.bin", "rpcmp-probe.ini", "rpcmp-probe.elf", "synthetic.bin")}
    paths |= {instance_root / "rpcmp-probe.json", PurePosixPath("Platforms") / f"{PLATFORM_ID}.json"}
    return paths


def validate_json(path: Path) -> None:
    value = json.loads(path.read_text(encoding="ascii"))
    if len(value) != 1:
        raise ValueError(f"JSON must have exactly one root object: {path}")
    root = next(iter(value.values()))
    if path.name != f"{PLATFORM_ID}.json" and root.get("magic") != MAGIC:
        raise ValueError(f"invalid or missing APF magic: {path}")
    if path.name == "data.json":
        slots = root.get("data_slots", [])
        if [slot.get("id") for slot in slots] != [0, 1, 2, 3, 4]:
            raise ValueError("data.json must contain only slots 0 through 4")
        if any(slot.get("nonvolatile") for slot in slots):
            raise ValueError("nonvolatile slots are prohibited in this spike")
        if any(slot.get("required") or not slot.get("deferload") for slot in slots[1:]):
            raise ValueError("openfpgaOS slots 1 through 4 must be optional and deferred")
    if path.name == "core.json":
        metadata = root.get("metadata", {})
        expected_core_id = f"{metadata.get('author')}.{metadata.get('shortname')}"
        if path.parent.name != expected_core_id:
            raise ValueError("core folder must match metadata author and shortname")
    if path.name == "input.json":
        controllers = root.get("controllers", [])
        if not controllers or any(not controller.get("mappings") for controller in controllers):
            raise ValueError("input.json controllers must contain mappings")
    if path.name == "rpcmp-probe.json" and "variant_select" in root:
        raise ValueError("the single-bitstream instance must not use variant_select")


def verify_tree(root: Path) -> list[Path]:
    if not root.is_dir():
        raise ValueError(f"package tree does not exist: {root}")
    files = sorted(path for path in root.rglob("*") if path.is_file())
    actual = {PurePosixPath(path.relative_to(root).as_posix()) for path in files}
    expected = expected_paths()
    if actual != expected:
        missing = sorted(str(path) for path in expected - actual)
        extra = sorted(str(path) for path in actual - expected)
        raise ValueError(f"package allowlist mismatch; missing={missing}, extra={extra}")
    for path in files:
        if path.suffix.lower() in PROHIBITED_SUFFIXES:
            raise ValueError(f"prohibited media in package: {path}")
        if path.suffix.lower() == ".json":
            validate_json(path)
    return files


def write_file(root: Path, relative: PurePosixPath, data: bytes) -> None:
    destination = root.joinpath(*relative.parts)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def build(repo: Path, sdk: Path, elf: Path, output: Path, archive: Path) -> None:
    revision = subprocess.run(
        ["git", "-C", str(sdk), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != SDK_REVISION:
        raise ValueError(f"SDK revision mismatch: {revision}")
    runtime = sdk / "runtime"
    manifest = parse_manifest(runtime / "MANIFEST")
    loader = verify_runtime_file(runtime, manifest, "pocket/loader.bin")
    os_binary = verify_runtime_file(runtime, manifest, "pocket/os.bin")
    bitstream = verify_runtime_file(runtime, manifest, f"pocket/{VARIANT}.rbf_r")
    if not elf.is_file():
        raise ValueError(f"probe ELF does not exist: {elf}")

    build_root = (repo / "out" / "build").resolve()
    output = output.resolve()
    archive = archive.resolve()
    expected_output = build_root / "pocket-openfpgaos-package"
    expected_archive = build_root / "rpcmp-openfpgaos-probe.zip"
    if output != expected_output or archive != expected_archive:
        raise ValueError("package outputs must use the dedicated paths under out/build")

    with tempfile.TemporaryDirectory(dir=build_root, prefix="rpcmp-package-") as temporary:
        staging = Path(temporary)
        core_root = PurePosixPath("Cores") / CORE_ID
        for name, value in definitions().items():
            write_file(staging, core_root / name, json_bytes(value))
        write_file(staging, core_root / "loader.bin", loader.read_bytes())
        write_file(staging, core_root / f"{VARIANT}.rbf_r", bitstream.read_bytes())

        common_root = PurePosixPath("Assets") / PLATFORM_ID / "common"
        write_file(staging, common_root / "os.bin", os_binary.read_bytes())
        write_file(staging, common_root / "rpcmp-probe.elf", elf.read_bytes())
        write_file(staging, common_root / "rpcmp-probe.ini", b"[os]\nELF=rpcmp-probe.elf\nARGS=\nVARIANT=os25\n")
        synthetic = bytes((offset * 37 + 11) % 256 for offset in range(4096))
        write_file(staging, common_root / "synthetic.bin", synthetic)

        instance = {
            "instance": {
                "magic": MAGIC,
                "data_slots": [
                    {"id": 1, "filename": "os.bin"},
                    {"id": 2, "filename": "rpcmp-probe.ini"},
                    {"id": 3, "filename": "rpcmp-probe.elf"},
                    {"id": 4, "filename": "synthetic.bin"},
                ],
            }
        }
        write_file(staging, PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "rpcmp-probe.json", json_bytes(instance))
        platform = {"platform": {"category": "Computer", "name": "RPCMP Probe", "year": 2026, "manufacturer": "RPCMP"}}
        write_file(staging, PurePosixPath("Platforms") / f"{PLATFORM_ID}.json", json_bytes(platform))
        files = verify_tree(staging)

        if output.exists():
            if output.is_symlink():
                raise ValueError("refusing to replace a symlinked output directory")
            shutil.rmtree(output)
        shutil.copytree(staging, output)

    archive.parent.mkdir(parents=True, exist_ok=True)
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for source in verify_tree(output):
            relative = source.relative_to(output).as_posix()
            info = zipfile.ZipInfo(relative, date_time=(2026, 8, 26, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, source.read_bytes())

    evidence = {
        "schema_version": 1,
        "distribution_status": "local experiment only; redistribution not approved",
        "sdk_revision": SDK_REVISION,
        "runtime_revision": RUNTIME_REVISION,
        "artifacts": {
            path.relative_to(output).as_posix(): {"bytes": path.stat().st_size, "sha256": sha256(path)}
            for path in verify_tree(output)
        },
        "zip": {"bytes": archive.stat().st_size, "sha256": sha256(archive)},
    }
    evidence_path = archive.with_suffix(".evidence.json")
    evidence_path.write_bytes(json_bytes(evidence))
    print(f"package={output}")
    print(f"zip={archive}")
    print(f"zip_sha256={evidence['zip']['sha256']}")
    print("result=PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--zip", dest="archive", type=Path, required=True)
    args = parser.parse_args()
    build(args.repo.resolve(), args.sdk.resolve(), args.elf.resolve(), args.output, args.archive)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
