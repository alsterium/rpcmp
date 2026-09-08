#!/usr/bin/env python3
"""Build the local M5 openfpgaOS/JT51 audio integration package."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

import pocket_package as shared
from pocket_m5_audio_prepare import verify_boot_mif

CORE_ID = "RPCMP.M5AudioProbe"
PLATFORM_ID = "rpcmp_m5audio"
VARIANT = "rpcmp"
VERSION = "0.9.1-m5-audio"
EXPECTED_OUTPUT = Path("out/build/pocket-m5-audio-bootfix-package")
EXPECTED_ARCHIVE = Path("out/build/rpcmp-m5-audio-bootfix.zip")
VERIFIED_RBF_SHA256 = "2444e999c0c7af162b26e4f3590f911ad78c01eb8102a5057d78adc37da7ec4a"


def verify_build(repo: Path, rbf: Path, *, firmware_elf: Path | None = None,
                 os_image: Path | None = None) -> dict[str, str]:
    """Reject the zero-ROM regression before creating or replacing artifacts."""
    project = rbf.parent.parent
    pocket = project.parent.parent
    boot = pocket / "firmware.mif"
    build_id = pocket / "apf" / "build_id.mif"
    if (firmware_elf is None) != (os_image is None):
        raise ValueError("firmware ELF and OS image must be supplied together")
    if firmware_elf is None:
        verify_boot_mif(boot)
    else:
        import pocket_firmware_pair
        pocket_firmware_pair.verify(firmware_elf, boot, os_image)
    if shared.sha256(build_id) != shared.sha256(repo / "overlays/openfpgaos/build_id.mif"):
        raise ValueError("build-ID MIF identity mismatch")
    evidence = {}
    for suffix in ("map.rpt", "fit.rpt", "sta.rpt", "asm.rpt", "flow.rpt"):
        report = rbf.with_suffix(f".{suffix}")
        content = report.read_text(encoding="utf-8", errors="replace")
        if "Critical Warning (127003)" in content or "setting all initial values to 0" in content:
            raise ValueError("Quartus report contains missing memory initialization")
        completed = (
            re.search(r"Flow Status\s*;\s*Successful", content)
            if suffix == "flow.rpt"
            else "successful. 0 errors" in content
        )
        if not completed:
            raise ValueError(f"Quartus report lacks zero-error completion: {report.name}")
        evidence[report.name] = shared.sha256(report)
    evidence["firmware.mif"] = shared.sha256(boot)
    evidence["apf/build_id.mif"] = shared.sha256(build_id)
    return evidence


def definitions() -> dict[str, object]:
    metadata = {"platform_ids": [PLATFORM_ID], "shortname": "M5AudioProbe",
                "description": "RPCMP M5 JT51 audio integration probe", "author": "RPCMP",
                "url": "", "version": VERSION, "date_release": "2026-09-05"}
    return {
        "audio.json": {"audio": {"magic": shared.MAGIC}},
        "core.json": {"core": {"magic": shared.MAGIC, "metadata": metadata,
            "framework": {"target_product": "Analogue Pocket", "version_required": "2.2",
                "sleep_supported": False, "dock": {"supported": True, "analog_output": False},
                "hardware": {"link_port": True, "cartridge_adapter": 0}},
            "cores": [{"name": VARIANT, "id": 0, "filename": f"{VARIANT}.rbf_r",
                       "chip32_vm": "loader.bin"}]}},
        "data.json": {"data": {"magic": shared.MAGIC, "data_slots": [
            {"id": 0, "name": "M5 Audio Probe", "required": True,
             "parameters": 275, "extensions": ["json"]},
            {"id": 1, "name": "OS Binary", "required": False, "parameters": 0,
             "extensions": ["bin"], "deferload": True},
            {"id": 2, "name": "OS Config", "required": False, "parameters": 0,
             "extensions": ["ini"], "deferload": True},
            {"id": 3, "name": "Application", "required": False, "parameters": 0,
             "extensions": ["elf"], "deferload": True}]}},
        "input.json": {"input": {"magic": shared.MAGIC, "controllers": []}},
        "interact.json": {"interact": {"magic": shared.MAGIC, "variables": [], "messages": []}},
        "variants.json": {"variants": {"magic": shared.MAGIC, "variant_list": []}},
        "video.json": {"video": {"magic": shared.MAGIC, "scaler_modes": [
            {"width": 320, "height": 240, "aspect_w": 4, "aspect_h": 3,
             "rotation": 0, "mirror": 0}]}}
    }


def expected_paths() -> set[PurePosixPath]:
    core = PurePosixPath("Cores") / CORE_ID
    common = PurePosixPath("Assets") / PLATFORM_ID / "common"
    return {*(core / name for name in definitions()), core / "loader.bin", core / f"{VARIANT}.rbf_r",
            common / "os.bin", common / "m5-audio.ini", common / "m5-audio.elf",
            PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "m5-audio.json",
            PurePosixPath("Platforms") / f"{PLATFORM_ID}.json"}


def verify_tree(root: Path) -> list[Path]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    actual = {PurePosixPath(path.relative_to(root).as_posix()) for path in files}
    if actual != expected_paths():
        raise ValueError("M5 audio package allowlist mismatch")
    for path in files:
        if path.suffix.lower() in shared.PROHIBITED_SUFFIXES:
            raise ValueError(f"prohibited media in package: {path}")
        if path.suffix.lower() == ".json":
            value = json.loads(path.read_text(encoding="ascii"))
            if len(value) != 1:
                raise ValueError(f"JSON must have one root: {path}")
    return files


def build(repo: Path, sdk: Path, elf: Path, rbf: Path, output: Path, archive: Path) -> dict[str, object]:
    output = output.resolve()
    archive = archive.resolve()
    if output.resolve() != (repo / EXPECTED_OUTPUT).resolve() or archive.resolve() != (repo / EXPECTED_ARCHIVE).resolve():
        raise ValueError("M5 audio outputs must use dedicated out/build paths")
    revision = subprocess.run(["git", "-C", str(sdk), "rev-parse", "HEAD"], check=True,
                              capture_output=True, text=True).stdout.strip()
    if revision != shared.SDK_REVISION:
        raise ValueError(f"SDK revision mismatch: {revision}")
    if not elf.is_file() or not rbf.is_file():
        raise ValueError("M5 audio ELF or RBF is missing")
    if shared.sha256(rbf) != VERIFIED_RBF_SHA256:
        raise ValueError("RBF is not the reviewed boot-ROM repair build")
    build_evidence = verify_build(repo, rbf)
    manifest = shared.parse_manifest(sdk / "runtime" / "MANIFEST")
    loader = shared.verify_runtime_file(sdk / "runtime", manifest, "pocket/loader.bin")
    safe_os = (repo / shared.SAFE_MEMSET_OS).resolve()
    if safe_os.stat().st_size != shared.SAFE_MEMSET_OS_SIZE or shared.sha256(safe_os) != shared.SAFE_MEMSET_OS_SHA256:
        raise ValueError("safe-layout OS identity mismatch")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent, prefix="rpcmp-m5-audio-") as temporary:
        staging = Path(temporary); core = PurePosixPath("Cores") / CORE_ID
        for name, value in definitions().items(): shared.write_file(staging, core / name, shared.json_bytes(value))
        shared.write_file(staging, core / "loader.bin", loader.read_bytes())
        shared.write_file(staging, core / f"{VARIANT}.rbf_r", shared.reverse_rbf_bits(rbf.read_bytes()))
        common = PurePosixPath("Assets") / PLATFORM_ID / "common"
        shared.write_file(staging, common / "os.bin", safe_os.read_bytes())
        shared.write_file(staging, common / "m5-audio.elf", elf.read_bytes())
        shared.write_file(staging, common / "m5-audio.ini", b"[os]\nELF=m5-audio.elf\nARGS=\nVARIANT=rpcmp\n")
        instance = {"instance": {"magic": shared.MAGIC, "data_slots": [
            {"id": 1, "filename": "os.bin"}, {"id": 2, "filename": "m5-audio.ini"},
            {"id": 3, "filename": "m5-audio.elf"}]}}
        shared.write_file(staging, PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "m5-audio.json", shared.json_bytes(instance))
        platform = {"platform": {"category": "Computer", "name": "RPCMP M5 Audio Probe",
                                  "year": 2026, "manufacturer": "RPCMP"}}
        shared.write_file(staging, PurePosixPath("Platforms") / f"{PLATFORM_ID}.json", shared.json_bytes(platform))
        verify_tree(staging)
        if output.exists():
            if output.is_symlink(): raise ValueError("refusing to replace symlinked output")
            shutil.rmtree(output)
        shutil.copytree(staging, output)
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for source in verify_tree(output):
            info = zipfile.ZipInfo(source.relative_to(output).as_posix(), (2026, 9, 5, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED; info.external_attr = 0o100644 << 16
            zipped.writestr(info, source.read_bytes())
    evidence = {"schema_version": 1, "distribution_status": "local diagnostic only",
                "core_id": CORE_ID, "version": VERSION,
                "build_evidence": build_evidence,
                "inputs": {"elf_sha256": shared.sha256(elf), "native_rbf_sha256": shared.sha256(rbf),
                           "os_sha256": shared.sha256(safe_os)},
                "artifacts": {p.relative_to(output).as_posix(): {"bytes": p.stat().st_size,
                    "sha256": shared.sha256(p)} for p in verify_tree(output)},
                "zip": {"bytes": archive.stat().st_size, "sha256": shared.sha256(archive)}}
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser()
    for name in ("repo", "sdk", "elf", "rbf", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--zip", dest="archive", type=Path, required=True)
    args = parser.parse_args()
    evidence = build(args.repo.resolve(), args.sdk.resolve(), args.elf.resolve(), args.rbf.resolve(),
                     args.output.resolve(), args.archive.resolve())
    print(f"zip_sha256={evidence['zip']['sha256']}"); print("result=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
