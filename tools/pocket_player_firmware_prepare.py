#!/usr/bin/env python3
"""Prepare the pinned, fail-closed ROM/OS for the actual M6 player binding."""
import argparse
import json
import shutil
import subprocess
from pathlib import Path

import pocket_m5_firmware_prepare as baseline
from pocket_m5_audio_prepare import sha256


def prepare(repo, upstream, musl, output):
    repo, upstream, musl, output = (p.resolve() for p in (repo, upstream, musl, output))
    baseline.prepare(repo, upstream, musl, output, fail_closed=True)
    overlay = repo / "overlays/openfpgaos"
    sound = overlay / "boot_m6_sound_reset.inc"
    destination = output / "src/firmware/os/targets/pocket/boot/boot_sound_reset.inc"
    shutil.copyfile(sound, destination)
    # The existing CPU-video patch contains both FPGA and OS hunks. Select only
    # the OS hunk here; the verified FPGA preparation owns the other half.
    relative = output.relative_to(repo).as_posix()
    patch = overlay / "player-cpu-video.patch"
    command = ["git", "apply", "--unidiff-zero", "--unsafe-paths",
               f"--directory={relative}",
               f"--include={relative}/src/firmware/os/kernel/caps_table.c", str(patch)]
    subprocess.run([*command[:2], "--check", *command[2:]], cwd=repo, check=True)
    subprocess.run(command, cwd=repo, check=True)
    caps = output / "src/firmware/os/kernel/caps_table.c"
    if "caps->gpu_base            = (features & HW_FEAT_GPU_SPAN) ? platform->gpu_base : 0u;" not in caps.read_text(encoding="utf-8"):
        raise ValueError("CPU-video firmware hunk was not applied")
    # Normalize the copied include too, matching the pinned Linux firmware ABI.
    destination.write_bytes(destination.read_bytes().replace(b"\r\n", b"\n"))
    manifest = output / "rpcmp-firmware-inputs.json"
    inputs = json.loads(manifest.read_text(encoding="utf-8"))
    inputs["profile"] = "m6-player"
    inputs["boot_overlay"]["boot_sound_reset.inc"] = sha256(sound)
    inputs["m6_sound_source"] = sound.relative_to(repo).as_posix()
    inputs["cpu_video_patch_sha256"] = sha256(patch)
    inputs["prepared_caps_sha256"] = sha256(caps)
    manifest.write_text(json.dumps(inputs, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "upstream", "musl", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.repo, args.upstream, args.musl, args.output))
