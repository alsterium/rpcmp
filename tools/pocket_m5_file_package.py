#!/usr/bin/env python3
"""Package the local-only M5 deferred-library application; never overwrite controls."""

import argparse
import json
import re
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

import pocket_m5_audio_package as audio
import pocket_package as shared

CORE_ID = "RPCMP.M5FileProbe"
PLATFORM_ID = "rpcmp_m5file"
VERSION = "0.10.0-m5-file"
OUTPUT = Path("out/build/pocket-m5-file-package")
ARCHIVE = Path("out/build/rpcmp-m5-file.zip")


def definitions():
    values = audio.definitions()
    metadata = values["core.json"]["core"]["metadata"]
    metadata.update(platform_ids=[PLATFORM_ID], shortname="M5FileProbe",
                    description="RPCMP M5 local MDX library playback", version=VERSION)
    values["core.json"]["core"]["framework"]["version_required"] = "2.6"
    slots = values["data.json"]["data"]["data_slots"]
    slots[0]["name"] = "M5 File Probe"
    slots.append({"id": 4, "name": "Music Library", "required": False,
                  "parameters": 8, "extensions": ["rpcmlib"], "deferload": True,
                  "size_maximum": 2 * 1024 * 1024})
    return values


def expected_paths():
    core = PurePosixPath("Cores") / CORE_ID
    common = PurePosixPath("Assets") / PLATFORM_ID / "common"
    return {*(core / name for name in definitions()), core / "loader.bin", core / "rpcmp.rbf_r",
            common / "os.bin", common / "m5-file.elf", common / "m5-file.ini",
            common / "music.rpcmlib",
            PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "m5-file.json",
            PurePosixPath("Platforms") / f"{PLATFORM_ID}.json"}


def build(repo, sdk, elf, rbf, library, source, preflight, title):
    output = repo / OUTPUT
    archive = repo / ARCHIVE
    # Fresh outputs only: no recursive deletion, replacement, or control mutation.
    if output.exists() or output.is_symlink() or archive.exists() or archive.is_symlink():
        raise ValueError("M5 file package outputs already exist; preserve them")
    if shared.sha256(rbf) != audio.VERIFIED_RBF_SHA256:
        raise ValueError("RBF is not the hardware-accepted boot-ROM repair")
    build_evidence = audio.verify_build(repo, rbf)
    revision = subprocess.run(["git", "-C", str(sdk), "rev-parse", "HEAD"],
                              capture_output=True, text=True, check=True).stdout.strip()
    if revision != shared.SDK_REVISION:
        raise ValueError("SDK revision mismatch")
    manifest = shared.parse_manifest(sdk / "runtime/MANIFEST")
    loader = shared.verify_runtime_file(sdk / "runtime", manifest, "pocket/loader.bin")
    safe_os = repo / shared.SAFE_MEMSET_OS
    if (safe_os.stat().st_size != shared.SAFE_MEMSET_OS_SIZE or
            shared.sha256(safe_os) != shared.SAFE_MEMSET_OS_SHA256):
        raise ValueError("safe OS identity mismatch")
    if not elf.is_file() or not 80 <= library.stat().st_size <= 2 * 1024 * 1024:
        raise ValueError("application missing or library outside Pocket limits")
    budget = json.loads(elf.with_name("budget.json").read_text(encoding="utf-8"))
    if (budget["static_bytes"] > 56623104 or budget["data_bytes"] > 4096 or
            budget["conservative_stack_bound_bytes"] > 524288 or budget["dynamic_stack_frames"]):
        raise ValueError("application budget failed")
    checked = subprocess.run([str(preflight), str(source), str(library), title],
                             capture_output=True, text=True, check=True)
    match = re.search(r"^track_id=([0-9a-f]{16})$", checked.stdout, re.MULTILINE)
    if not match or int(match[1], 16) == 0 or "result=PASS" not in checked.stdout:
        raise ValueError("real-file preflight did not pass")

    core = PurePosixPath("Cores") / CORE_ID
    common = PurePosixPath("Assets") / PLATFORM_ID / "common"
    files = {core / name: shared.json_bytes(value) for name, value in definitions().items()}
    files.update({core / "loader.bin": loader.read_bytes(),
                  core / "rpcmp.rbf_r": shared.reverse_rbf_bits(rbf.read_bytes()),
                  common / "os.bin": safe_os.read_bytes(),
                  common / "m5-file.elf": elf.read_bytes(),
                  common / "music.rpcmlib": library.read_bytes(),
                  common / "m5-file.ini": (
                      f"[os]\nELF=m5-file.elf\nARGS={match[1]}\nVARIANT=rpcmp\n").encode("ascii")})
    instance = {"instance": {"magic": shared.MAGIC, "data_slots": [
        {"id": 1, "filename": "os.bin"}, {"id": 2, "filename": "m5-file.ini"},
        {"id": 3, "filename": "m5-file.elf"}, {"id": 4, "filename": "music.rpcmlib"}]}}
    files[PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "m5-file.json"] = shared.json_bytes(instance)
    platform = {"platform": {"category": "Computer", "name": "RPCMP M5 File Probe",
                              "year": 2026, "manufacturer": "RPCMP"}}
    files[PurePosixPath("Platforms") / f"{PLATFORM_ID}.json"] = shared.json_bytes(platform)
    if set(files) != expected_paths():
        raise ValueError("package allowlist mismatch")
    for path, contents in files.items():
        if path.suffix == ".json":
            value = json.loads(contents)
            if len(value) != 1 or (path.parts[0] != "Platforms" and
                                  next(iter(value.values())).get("magic") != shared.MAGIC):
                raise ValueError("invalid APF JSON root/magic")
        shared.write_file(output, path, contents)
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for path, contents in sorted(files.items()):
            info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 5, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, contents)
    evidence = {"schema_version": 1, "distribution_status": "PRIVATE USER MUSIC: DO NOT DISTRIBUTE",
                "core_id": CORE_ID, "version": VERSION, "build_evidence": build_evidence,
                "budget": budget, "preflight": checked.stdout.splitlines(),
                "inputs": {"elf_sha256": shared.sha256(elf), "native_rbf_sha256": shared.sha256(rbf)},
                "artifacts": {path.as_posix(): {"bytes": len(contents), "sha256": shared.sha256(output / path)}
                              for path, contents in sorted(files.items())},
                "zip": {"bytes": archive.stat().st_size, "sha256": shared.sha256(archive)}}
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    return evidence


def main():
    parser = argparse.ArgumentParser()
    for name in ("repo", "sdk", "elf", "rbf", "library", "source", "preflight"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--title", required=True)
    args = parser.parse_args()
    evidence = build(*(getattr(args, name).resolve() for name in
                       ("repo", "sdk", "elf", "rbf", "library", "source", "preflight")), args.title)
    print(f"zip_sha256={evidence['zip']['sha256']}\nresult=PASS")


if __name__ == "__main__":
    main()
