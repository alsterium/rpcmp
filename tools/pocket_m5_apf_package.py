#!/usr/bin/env python3
"""Package the bounded APF lifecycle and transport-completion experiments."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path, PurePosixPath

import pocket_m5_boot_package as boot
import pocket_m5_file_package as control
import pocket_m5_placement_package as placement
import pocket_package as shared

VERSION = "0.10.4-m5-apf"
RBF_SHA256 = "b02972159f9dbd3ba360e27a2b7acef9ac16347aecdf1fa0b39abba80cd2e128"
PROFILES = {"normal": ("M5ApfProbe", "rpcmp_m5apf", 0),
            "error": ("M5ApfErrorProbe", "rpcmp_m5apferr", 1),
            "timeout": ("M5ApfTimeoutProbe", "rpcmp_m5apftmo", 2)}


def probe_files(source, profile):
    short, platform, mode = PROFILES[profile]
    core_id = "RPCMP." + short
    files = {}
    for path, data in source.items():
        renamed = PurePosixPath(*(part.replace("RPCMP.M5BootProbe", core_id).replace(
            "rpcmp_m5boot", platform) for part in path.parts))
        if path.suffix == ".json":
            data = data.replace(b"RPCMP.M5BootProbe", core_id.encode()).replace(
                b"rpcmp_m5boot", platform.encode())
        files[renamed] = data
    core = PurePosixPath("Cores") / core_id
    metadata = json.loads(files[core / "core.json"])
    metadata["core"]["metadata"].update(shortname=short, version=VERSION,
        date_release="2026-09-10", description=f"RPCMP M5 APF {profile}")
    files[core / "core.json"] = shared.json_bytes(metadata)
    path = PurePosixPath("Platforms") / f"{platform}.json"
    value = json.loads(files[path])
    value["platform"]["name"] = f"RPCMP M5 APF {profile.upper()}"
    files[path] = shared.json_bytes(value)
    path = PurePosixPath("Assets") / platform / core_id / "m5-file.json"
    instance = json.loads(files[path])
    if instance["instance"].get("memory_writes"):
        raise ValueError("unexpected existing instance writes")
    # Repeated-byte values make the setting independent of bridge byte order.
    instance["instance"]["memory_writes"] = [
        {"address": "0xF7000020", "data": f"0x{mode * 0x01010101:08X}"}]
    files[path] = shared.json_bytes(instance)
    return files


def build(repo):
    tree = repo / "out/build/openfpgaos-m5-apf-lifecycle-v3"
    pocket = tree / "src/fpga/targets/pocket"
    rbf = pocket / "bld/rpcmp-m5-apf-lifecycle/output_files/ap_core.rbf"
    firmware = repo / "out/build/m5-firmware-fail-closed/src/firmware/os/bld/pocket"
    if shared.sha256(rbf) != RBF_SHA256:
        raise ValueError("RBF is not the reviewed APF build")
    paired = boot.pair.verify(firmware / "firmware.elf", pocket / "firmware.mif", firmware / "os.bin")
    reports = boot.audio.verify_build(repo, rbf, firmware_elf=firmware / "firmware.elf",
                                     os_image=firmware / "os.bin")
    timing = placement.timing(rbf.with_suffix(".sta.summary"))
    source = boot.probe_files(boot.read_control(repo / control.ARCHIVE), rbf.read_bytes(),
                             (firmware / "os.bin").read_bytes(), False)
    archives = {name: repo / "out/build" / f"rpcmp-m5-apf-{name}-v2.zip" for name in PROFILES}
    if any(p.exists() or p.with_suffix(".evidence.json").exists() for p in archives.values()):
        raise ValueError("output exists; preserve previous artifacts")
    for profile, archive in archives.items():
        files = probe_files(source, profile)
        with zipfile.ZipFile(archive, "x") as zipped:
            for path, data in sorted(files.items()):
                info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 10, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                zipped.writestr(info, data)
        with zipfile.ZipFile(archive) as zipped:
            if set(zipped.namelist()) != {p.as_posix() for p in files} or any(
                    zipped.read(p.as_posix()) != data for p, data in files.items()):
                raise ValueError("archive readback mismatch")
        evidence = {"version": VERSION, "profile": profile, "hardware_status": "pending",
                    "distribution_status": "LOCAL ONLY; contains private user library",
                    "paired_firmware": paired, "build_reports": reports, "timing": timing,
                    "rbf_sha256": RBF_SHA256, "zip_sha256": shared.sha256(archive),
                    "artifacts": {p.as_posix(): {"bytes": len(d), "sha256": hashlib.sha256(d).hexdigest()}
                                  for p, d in files.items()}}
        archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return list(archives.values())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    args = parser.parse_args()
    print("\n".join(str(p) for p in build(args.repo.resolve())))
