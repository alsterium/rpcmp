#!/usr/bin/env python3
"""Package coherent OS placement probes using the accepted local library payload."""

import argparse
import hashlib
import json
import re
import zipfile
from pathlib import Path, PurePosixPath

import pocket_firmware_pair as pair
import pocket_m5_audio_package as audio
import pocket_m5_boot_package as boot
import pocket_m5_file_package as control
import pocket_package as shared
import pocket_placement_verify as placement

VERSION = "0.10.2-m5-placement"
REVIEWED_OS_RBF = {
    "code": "5ea69316b681f7f942604d490ba3c9ba74381b31681dff52a4f03e90fc927ca8",
    "bss": "bb68f81c6814d3191ff6c6b88007dd3e2ca60715cf758b90eb8cdb2d6b78a055",
}


def probe_files(source, rbf, os_image, profile, target="os", app=None):
    if profile not in ("code", "bss") or target not in ("os", "app"):
        raise ValueError("unknown placement profile")
    if (target == "app") != (app is not None):
        raise ValueError("app bytes required only for app placement")
    short = f"M5{target.title()}{profile.title()}Probe"
    core_id, platform_id = f"RPCMP.{short}", f"rpcmp_m5{target}{profile}"
    files = {}
    for path, data in source.items():
        renamed = PurePosixPath(*(part.replace(control.CORE_ID, core_id).replace(
            control.PLATFORM_ID, platform_id) for part in path.parts))
        if path.suffix == ".json":
            data = data.replace(control.CORE_ID.encode(), core_id.encode()).replace(
                control.PLATFORM_ID.encode(), platform_id.encode())
        files[renamed] = data
    core = PurePosixPath("Cores") / core_id
    common = PurePosixPath("Assets") / platform_id / "common"
    metadata = json.loads(files[core / "core.json"])
    metadata["core"]["metadata"].update(shortname=short, version=VERSION,
        date_release="2026-09-10", description=f"RPCMP M5 {target} {profile} placement")
    files[core / "core.json"] = shared.json_bytes(metadata)
    platform = PurePosixPath("Platforms") / f"{platform_id}.json"
    value = json.loads(files[platform])
    value["platform"]["name"] = f"RPCMP M5 {target.upper()} {profile.upper()}"
    files[platform] = shared.json_bytes(value)
    files[core / "rpcmp.rbf_r"] = shared.reverse_rbf_bits(rbf)
    files[common / "os.bin"] = os_image
    if app is not None:
        files[common / "m5-file.elf"] = app
    return files


def timing(summary):
    text = summary.read_text()
    rows = re.findall(r"Type\s*:\s*(.+)\nSlack\s*:\s*(-?[\d.]+)\nTNS\s*:\s*(-?[\d.]+)", text)
    if not rows or any(float(slack) < 0 or float(tns) != 0 for _, slack, tns in rows):
        raise ValueError("timing summary missing or contains violations")
    result = {}
    for kind in ("Setup", "Hold"):
        values = [float(s) for name, s, _ in rows if f" {kind} " in name]
        if not values:
            raise ValueError("setup/hold timing coverage missing")
        result[kind.lower() + "_min_ns"] = min(values)
    result.update(checked_rows=len(rows), all_tns_zero=True, sha256=shared.sha256(summary))
    return result


def build(repo, firmware, rbf, profile, target="os"):
    expected_rbf = audio.VERIFIED_RBF_SHA256 if target == "app" else REVIEWED_OS_RBF[profile]
    if shared.sha256(rbf) != expected_rbf:
        raise ValueError("RBF differs from reviewed placement build")
    evidence = json.loads((firmware / "evidence.json").read_text())
    if evidence["placement"]["result"] != "PASS":
        raise ValueError("placement verification missing")
    is_app = target == "app"
    elf = firmware / ("app.elf" if is_app else "firmware.elf")
    os_image = repo / shared.SAFE_MEMSET_OS if is_app else firmware / "os.bin"
    if shared.sha256(elf) != evidence["elf_sha256"] or (not is_app and
            shared.sha256(os_image) != evidence["os_sha256"]):
        raise ValueError("firmware changed since placement verification")
    evidence["placement"] = placement.compare(firmware.parent / "control" / elf.name, elf,
        (".text" if is_app else ".osdata") if profile == "code" else (".bss" if is_app else ".os_bss"),
        evidence["pad_start"])
    deltas = evidence["placement"]["symbol_deltas"]
    expected_symbol = "main" if is_app else "irq_handler"
    if deltas.get(expected_symbol, 0) != (64 if profile == "code" else 0):
        raise ValueError("placement profile does not match requested package")
    if is_app:
        if shared.sha256(rbf) != audio.VERIFIED_RBF_SHA256 or shared.sha256(os_image) != shared.SAFE_MEMSET_OS_SHA256:
            raise ValueError("app probe must retain accepted ROM/OS/RBF")
        paired = {"scope": "accepted fixed ROM/OS baseline", "os_sha256": shared.sha256(os_image)}
        reports = audio.verify_build(repo, rbf)
    else:
        paired = pair.verify(elf, rbf.parent.parent.parent.parent / "firmware.mif", os_image)
        reports = audio.verify_build(repo, rbf, firmware_elf=elf, os_image=os_image)
    timing_result = timing(rbf.with_suffix(".sta.summary"))
    archive = repo / "out/build" / f"rpcmp-m5-{target}-{profile}.zip"
    if archive.exists() or archive.with_suffix(".evidence.json").exists():
        raise ValueError("output exists; preserve previous artifacts")
    source = boot.read_control(repo / control.ARCHIVE)
    files = probe_files(source, rbf.read_bytes(), os_image.read_bytes(), profile, target,
                        elf.read_bytes() if is_app else None)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as zipped:
        for path, data in sorted(files.items()):
            info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 10, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, data)
    with zipfile.ZipFile(archive) as zipped:
        if len(zipped.infolist()) != len(files) or set(zipped.namelist()) != {p.as_posix() for p in files}:
            raise ValueError("written archive allowlist mismatch")
        if any(zipped.read(p.as_posix()) != data for p, data in files.items()):
            raise ValueError("written archive byte mismatch")
    report = {"version": VERSION, "profile": profile, "target": target, "hardware_status": "pending",
              "distribution_status": "LOCAL ONLY; contains private user music",
              "paired_firmware": paired, "placement": evidence, "build_reports": reports, "timing": timing_result,
              "rbf_sha256": shared.sha256(rbf), "zip_sha256": shared.sha256(archive),
              "artifacts": {p.as_posix(): {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                            for p, data in files.items()}}
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(report))
    return archive


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "firmware", "rbf"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--profile", choices=("code", "bss"), required=True)
    parser.add_argument("--target", choices=("os", "app"), default="os")
    args = parser.parse_args()
    print(build(args.repo.resolve(), args.firmware.resolve(), args.rbf.resolve(), args.profile, args.target))
