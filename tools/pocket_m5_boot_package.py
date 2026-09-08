#!/usr/bin/env python3
"""Create separate local M5 valid-boot and CRC-stop probes from an accepted control."""

import argparse
import hashlib
import json
import zipfile
from pathlib import Path, PurePosixPath

import pocket_firmware_pair as pair
import pocket_m5_audio_package as audio
import pocket_m5_file_package as control
import pocket_package as shared

VERSION = "0.10.1-m5-boot"
REVIEWED_RBF_SHA256 = "f3d06c863da82254dc1078088111047d5b7ab3eee3ff29d49b060946d894c7d3"
REVIEWED_MIF_SHA256 = "a7ebbfbd7014e3f519bb8baea51e3f962e3f0c6d4448d6636e55ffe01aae5621"


def read_control(archive):
    evidence = json.loads(archive.with_suffix(".evidence.json").read_text(encoding="utf-8"))
    if shared.sha256(archive) != evidence["zip"]["sha256"]:
        raise ValueError("control archive identity mismatch")
    with zipfile.ZipFile(archive) as zipped:
        members = zipped.infolist()
        if (len(members) != len(control.expected_paths()) or
                {PurePosixPath(m.filename) for m in members} != control.expected_paths() or
                any(m.file_size > 4 * 1024 * 1024 for m in members)):
            raise ValueError("control archive allowlist/size mismatch")
        files = {PurePosixPath(m.filename): zipped.read(m) for m in members}
    for path, data in files.items():
        expected = evidence["artifacts"][path.as_posix()]
        if len(data) != expected["bytes"] or hashlib.sha256(data).hexdigest() != expected["sha256"]:
            raise ValueError("control member identity mismatch")
    old_core = PurePosixPath("Cores") / control.CORE_ID
    old_common = PurePosixPath("Assets") / control.PLATFORM_ID / "common"
    if (hashlib.sha256(shared.reverse_rbf_bits(files[old_core / "rpcmp.rbf_r"])).hexdigest()
            != audio.VERIFIED_RBF_SHA256 or
            hashlib.sha256(files[old_common / "os.bin"]).hexdigest() != shared.SAFE_MEMSET_OS_SHA256):
        raise ValueError("control is not the accepted ROM/OS build")
    return files


def probe_files(source, rbf, os_image, fault):
    short = "M5CrcStopProbe" if fault else "M5BootProbe"
    core_id = f"RPCMP.{short}"
    platform_id = "rpcmp_m5crcstop" if fault else "rpcmp_m5boot"
    files = {}
    for path, data in source.items():
        renamed = PurePosixPath(*(part.replace(control.CORE_ID, core_id)
                                 .replace(control.PLATFORM_ID, platform_id) for part in path.parts))
        if path.suffix == ".json":
            data = data.replace(control.CORE_ID.encode(), core_id.encode()).replace(
                control.PLATFORM_ID.encode(), platform_id.encode())
        files[renamed] = data
    core = PurePosixPath("Cores") / core_id
    common = PurePosixPath("Assets") / platform_id / "common"
    metadata = json.loads(files[core / "core.json"])
    metadata["core"]["metadata"].update(shortname=short, version=VERSION,
        date_release="2026-09-08", description="RPCMP M5 CRC stop" if fault else "RPCMP M5 paired boot")
    files[core / "core.json"] = shared.json_bytes(metadata)
    platform = PurePosixPath("Platforms") / f"{platform_id}.json"
    value = json.loads(files[platform])
    value["platform"]["name"] = "RPCMP M5 CRC Stop" if fault else "RPCMP M5 Boot"
    files[platform] = shared.json_bytes(value)
    files[core / "rpcmp.rbf_r"] = shared.reverse_rbf_bits(rbf)
    # Flip only a stored CRC bit, preserving all payload and entry/BSS metadata.
    files[common / "os.bin"] = os_image[:-1] + bytes([os_image[-1] ^ 1]) if fault else os_image
    if fault:
        for name in ("m5-file.elf", "m5-file.ini", "music.rpcmlib"):
            del files[common / name]
        instance_path = PurePosixPath("Assets") / platform_id / core_id / "m5-file.json"
        instance = json.loads(files[instance_path])
        instance["instance"]["data_slots"] = [{"id": 1, "filename": "os.bin"}]
        files[instance_path] = shared.json_bytes(instance)
        slots = json.loads(files[core / "data.json"])
        slots["data"]["data_slots"] = [s for s in slots["data"]["data_slots"] if s["id"] in (0, 1)]
        slots["data"]["data_slots"][0]["name"] = "M5 CRC Stop"
        files[core / "data.json"] = shared.json_bytes(slots)
    return files


def build(repo, firmware, rbf):
    if shared.sha256(rbf) != REVIEWED_RBF_SHA256:
        raise ValueError("RBF is not the reviewed fail-closed integration build")
    elf = firmware / "src/firmware/os/bld/pocket/firmware.elf"
    os_image = firmware / "src/firmware/os/bld/pocket/os.bin"
    mif = rbf.parent.parent.parent.parent / "firmware.mif"
    if shared.sha256(mif) != REVIEWED_MIF_SHA256:
        raise ValueError("boot MIF differs from the reviewed RBF input")
    inputs = json.loads((firmware / "rpcmp-firmware-inputs.json").read_text(encoding="utf-8"))
    expected_overlay = {n: shared.sha256(repo / "overlays/openfpgaos" / n) for n in
                        ("boot-fail-closed.patch", "boot_crc_retry.inc", "boot_sound_reset.inc")}
    if inputs["profile"] != "fail-closed" or inputs["boot_overlay"] != expected_overlay:
        raise ValueError("firmware overlay provenance mismatch")
    paired = pair.verify(elf, mif, os_image)
    reports = audio.verify_build(repo, rbf, firmware_elf=elf, os_image=os_image)
    source = read_control(repo / control.ARCHIVE)
    targets = [(fault, repo / "out/build" / name) for fault, name in
               ((False, "rpcmp-m5-boot.zip"), (True, "rpcmp-m5-crc-stop.zip"))]
    for _, archive in targets:
        for path in (archive, archive.with_suffix(".evidence.json")):
            if path.exists() or path.is_symlink():
                raise ValueError("probe output exists; preserve previous artifacts")
    for fault, archive in targets:
        files = probe_files(source, rbf.read_bytes(), os_image.read_bytes(), fault)
        with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as zipped:
            for path, data in sorted(files.items()):
                info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 8, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                zipped.writestr(info, data)
        with zipfile.ZipFile(archive) as zipped:
            if set(zipped.namelist()) != {p.as_posix() for p in files} or any(
                    zipped.read(p.as_posix()) != data for p, data in files.items()):
                raise ValueError("written archive verification failed")
        evidence = {"version": VERSION, "hardware_status": "pending",
                    "distribution_status": "LOCAL ONLY; valid probe contains private user music",
                    "crc_fault_injected": fault, "paired_firmware": paired,
                    "firmware_inputs": inputs, "build_reports": reports,
                    "rbf_sha256": shared.sha256(rbf),
                    "artifacts": {p.as_posix(): {"bytes": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()} for p, data in files.items()},
                    "zip_sha256": shared.sha256(archive)}
        archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return [str(path) for _, path in targets]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "firmware", "rbf"):
        parser.add_argument(f"--{name}", required=True, type=Path)
    args = parser.parse_args()
    print("\n".join(build(args.repo.resolve(), args.firmware.resolve(), args.rbf.resolve())))
    print("result=PASS")
