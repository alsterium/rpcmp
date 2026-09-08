#!/usr/bin/env python3
"""Prepare isolated, pinned M5 firmware sources without changing accepted controls."""

import argparse
import io
import json
import shutil
import subprocess
import zipfile
from pathlib import Path

from pocket_m5_audio_prepare import OPENFPGAOS_REVISION, sha256, verify_checkout

MUSL_SHA256 = "de6bb084326aefa2f1a2ca8d82780f972162f018764b05f366c44849960adaf0"
FIRMWARE_IMAGE = "sha256:98771990c464c0b3231d685de02579a25c6b5b0f952b68d5e573679664070e58"


def prepare(repo, upstream, musl, output, fail_closed=False):
    repo, upstream, musl, output = (p.resolve() for p in (repo, upstream, musl, output))
    if output.parent != repo / "out/build" or output.exists():
        raise ValueError("firmware output must be a new direct child of out/build")
    verify_checkout(upstream, OPENFPGAOS_REVISION, "openfpgaOS")
    if sha256(musl / "lib/libc.a") != MUSL_SHA256:
        raise ValueError("firmware musl archive identity mismatch")
    archive = subprocess.check_output(
        ["git", "-C", str(upstream), "archive", "--format=zip", OPENFPGAOS_REVISION]
    )
    output.mkdir(parents=True)
    with zipfile.ZipFile(io.BytesIO(archive)) as source:
        source.extractall(output)
    # This pinned repository stores CRLF text. Its Linux shell scripts and
    # memory-map ABI hash require the same LF normalization as the safe build.
    for path in output.rglob("*"):
        if path.is_file():
            content = path.read_bytes()
            if b"\0" not in content and b"\r\n" in content:
                path.write_bytes(content.replace(b"\r\n", b"\n"))
    patch = repo / "spikes/pocket/openfpgaos/safe-memset.patch"
    args = ["git", "apply", "--unidiff-zero", "--unsafe-paths", f"--directory={output.relative_to(repo).as_posix()}", str(patch)]
    subprocess.run([*args[:2], "--check", *args[2:]], cwd=repo, check=True)
    subprocess.run(args, cwd=repo, check=True)
    if fail_closed:
        overlay = repo / "overlays/openfpgaos"
        args[-1] = str(overlay / "boot-fail-closed.patch")
        subprocess.run([*args[:2], "--check", *args[2:]], cwd=repo, check=True)
        subprocess.run(args, cwd=repo, check=True)
        for name in ("boot_crc_retry.inc", "boot_sound_reset.inc"):
            shutil.copyfile(overlay / name, output / "src/firmware/os/targets/pocket/boot" / name)
    for name in ("include", "lib"):
        shutil.copytree(musl / name, output / "src/firmware/musl" / name)
    evidence = {
        "source_revision": OPENFPGAOS_REVISION,
        "firmware_image": FIRMWARE_IMAGE,
        "safe_memset_patch_sha256": sha256(patch),
        "profile": "fail-closed" if fail_closed else "control-reproduction",
        "boot_overlay": ({n: sha256(repo / "overlays/openfpgaos" / n)
                          for n in ("boot-fail-closed.patch", "boot_crc_retry.inc", "boot_sound_reset.inc")}
                         if fail_closed else {}),
        "musl_inputs": {p.relative_to(musl).as_posix(): sha256(p)
                        for name in ("include", "lib")
                        for p in sorted((musl / name).rglob("*")) if p.is_file()},
    }
    (output / "rpcmp-firmware-inputs.json").write_text(
        json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "upstream", "musl", "output"):
        parser.add_argument(f"--{name}", type=Path, required=True)
    parser.add_argument("--fail-closed", action="store_true")
    args = parser.parse_args()
    print(prepare(args.repo, args.upstream, args.musl, args.output, args.fail_closed))
