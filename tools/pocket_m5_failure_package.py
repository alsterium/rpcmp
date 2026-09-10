#!/usr/bin/env python3
"""Package input-failure probes without changing the accepted ROM/OS/application."""

import argparse
import hashlib
import json
import re
import struct
import zipfile
from pathlib import Path, PurePosixPath

import pocket_m5_boot_package as boot
import pocket_m5_file_package as control
import pocket_package as shared

VERSION = "0.10.3-m5-failure"
PROFILES = {
    "args": ("M5ArgStopProbe", "rpcmp_m5argstop", 2),
    "library": ("M5LibStopProbe", "rpcmp_m5libstop", 3),
    "track": ("M5TrackStopProbe", "rpcmp_m5track", 4),
}


def absent_track(library):
    """Choose an absent ID in the accepted single-track library, without logging it."""
    if len(library) < 80 or len(library) > 2 * 1024 * 1024 or library[:8] != b"RPCMLIB\0":
        raise ValueError("invalid source library envelope")
    directory, count, width = struct.unpack_from("<QII", library, 40)
    if width != 40 or not 1 <= count <= 64 or directory > len(library) - count * 40:
        raise ValueError("invalid source library directory")
    tracks = []
    for i in range(count):
        tag, _, offset, size = struct.unpack_from("<4sIQQ", library, directory + i * 40)
        if offset > len(library) or size > len(library) - offset:
            raise ValueError("source section outside library")
        if tag == b"TRAK":
            if size != 104 or struct.unpack_from("<II", library, offset) != (1, 96):
                raise ValueError("expected single-track source")
            tracks.append(struct.unpack_from("<Q", library, offset + 8)[0])
    if len(tracks) != 1 or tracks[0] == 0:
        raise ValueError("expected one nonzero source track")
    return 2 if tracks[0] == 1 else 1


def probe_files(source, profile):
    short, platform_id, _ = PROFILES[profile]
    core_id = "RPCMP." + short
    files = {}
    for path, data in source.items():
        renamed = PurePosixPath(*(part.replace("RPCMP.M5BootProbe", core_id).replace(
            "rpcmp_m5boot", platform_id) for part in path.parts))
        if path.suffix == ".json":
            data = data.replace(b"RPCMP.M5BootProbe", core_id.encode()).replace(
                b"rpcmp_m5boot", platform_id.encode())
        files[renamed] = data
    core = PurePosixPath("Cores") / core_id
    common = PurePosixPath("Assets") / platform_id / "common"
    meta = json.loads(files[core / "core.json"])
    meta["core"]["metadata"].update(shortname=short, version=VERSION,
        date_release="2026-09-10", description=f"RPCMP M5 {profile} failure")
    files[core / "core.json"] = shared.json_bytes(meta)
    platform = PurePosixPath("Platforms") / f"{platform_id}.json"
    value = json.loads(files[platform])
    value["platform"]["name"] = f"RPCMP M5 {profile.upper()} STOP"
    files[platform] = shared.json_bytes(value)
    if profile == "library":
        data = files[common / "music.rpcmlib"]
        if data[:8] != b"RPCMLIB\0":
            raise ValueError("source library magic mismatch")
        files[common / "music.rpcmlib"] = bytes([data[0] ^ 1]) + data[1:]
    else:
        selected = 0 if profile == "args" else absent_track(files[common / "music.rpcmlib"])
        config, count = re.subn(rb"(?m)^ARGS=[0-9a-fA-F]+$", f"ARGS={selected:016x}".encode(),
                                files[common / "m5-file.ini"])
        if count != 1:
            raise ValueError("source config must contain one hex track argument")
        files[common / "m5-file.ini"] = config
    return files


def build(repo):
    rbf = repo / "out/build/openfpgaos-m5-fail-closed/src/fpga/targets/pocket/bld/rpcmp-m5-fail-closed/output_files/ap_core.rbf"
    firmware = repo / "out/build/m5-firmware-fail-closed/src/firmware/os/bld/pocket"
    if shared.sha256(rbf) != boot.REVIEWED_RBF_SHA256:
        raise ValueError("not the accepted fail-closed RBF")
    paired = boot.pair.verify(firmware / "firmware.elf",
        rbf.parent.parent.parent.parent / "firmware.mif", firmware / "os.bin")
    reports = boot.audio.verify_build(repo, rbf, firmware_elf=firmware / "firmware.elf",
                                      os_image=firmware / "os.bin")
    source = boot.probe_files(boot.read_control(repo / control.ARCHIVE),
                              rbf.read_bytes(), (firmware / "os.bin").read_bytes(), False)
    # Prove these fixed assets are the ones in the hardware-accepted normal ZIP.
    accepted = repo / "out/build/rpcmp-m5-boot.zip"
    accepted_evidence = json.loads(accepted.with_suffix(".evidence.json").read_text())
    if shared.sha256(accepted) != accepted_evidence["zip_sha256"]:
        raise ValueError("accepted boot archive changed")
    with zipfile.ZipFile(accepted) as zipped:
        if set(zipped.namelist()) != {p.as_posix() for p in source} or any(
                zipped.read(p.as_posix()) != data for p, data in source.items()):
            raise ValueError("source differs from accepted normal probe")
    archives = {p: repo / "out/build" / f"rpcmp-m5-{p}-stop.zip" for p in PROFILES}
    if any(p.exists() or p.with_suffix(".evidence.json").exists() for p in archives.values()):
        raise ValueError("failure probe output exists; preserve it")
    for profile, archive in archives.items():
        files = probe_files(source, profile)
        with zipfile.ZipFile(archive, "x") as zipped:
            for path, data in sorted(files.items()):
                info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 10, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                zipped.writestr(info, data)
        with zipfile.ZipFile(archive) as zipped:
            if len(zipped.infolist()) != len(files) or any(
                    zipped.read(p.as_posix()) != data for p, data in files.items()):
                raise ValueError("written archive differs")
        evidence = {"version": VERSION, "profile": profile,
                    "expected_failure_code": PROFILES[profile][2], "hardware_status": "pending",
                    "distribution_status": "LOCAL ONLY; contains private user library",
                    "paired_firmware": paired, "build_reports": reports,
                    "rbf_sha256": shared.sha256(rbf), "zip_sha256": shared.sha256(archive),
                    "artifacts": {p.as_posix(): {"bytes": len(d), "sha256": hashlib.sha256(d).hexdigest()}
                                  for p, d in files.items()}}
        archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return list(archives.values())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    args = parser.parse_args()
    print("\n".join(str(p) for p in build(args.repo.resolve())))
