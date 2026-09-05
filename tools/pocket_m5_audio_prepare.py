#!/usr/bin/env python3
"""Prepare a reproducible, isolated openfpgaOS M5 audio integration tree."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path


OPENFPGAOS_REVISION = "618a3eb985759a4154115109c2c8036271252888"
JT51_REVISION = "985a573dcfc1ff135553a39f7eae21d18ba57cbe"
VEXII_NETLIST_SHA256 = "266242a20fb65d91b674920db869201d100fbc31721e8df9b4dc383d24cc019b"


def run(*args: str, cwd: Path | None = None) -> str:
    result = subprocess.run(args, cwd=cwd, check=True, text=True, capture_output=True)
    return result.stdout.strip()


def verify_checkout(path: Path, revision: str, name: str) -> None:
    if run("git", "rev-parse", "HEAD", cwd=path) != revision:
        raise ValueError(f"{name} revision mismatch")
    if run("git", "status", "--porcelain=v1", cwd=path):
        raise ValueError(f"{name} checkout has local changes")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--openfpgaos", type=Path, required=True)
    parser.add_argument("--jt51", type=Path, required=True)
    parser.add_argument("--vexii-netlist", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    repo = args.repo.resolve()
    upstream = args.openfpgaos.resolve()
    jt51 = args.jt51.resolve()
    netlist = args.vexii_netlist.resolve()
    output = args.output.resolve()
    build_root = (repo / "out" / "build").resolve()
    if output.parent != build_root or output.exists():
        raise ValueError("output must be a new direct child of repo/out/build")

    verify_checkout(upstream, OPENFPGAOS_REVISION, "openfpgaOS")
    verify_checkout(jt51, JT51_REVISION, "JT51")
    if sha256(netlist) != VEXII_NETLIST_SHA256:
        raise ValueError("VexiiRiscv rpcmp netlist identity mismatch")

    overlay = repo / "overlays" / "openfpgaos"
    with tempfile.TemporaryDirectory(prefix="rpcmp-m5-audio-") as temporary:
        archive = Path(temporary) / "openfpgaos.zip"
        subprocess.run(
            ["git", "archive", "--format=zip", f"--output={archive}", OPENFPGAOS_REVISION],
            cwd=upstream,
            check=True,
        )
        output.mkdir(parents=True)
        with zipfile.ZipFile(archive) as source:
            source.extractall(output)

    output_relative = output.relative_to(repo).as_posix()
    apply_args = [
        "git",
        "apply",
        "--unsafe-paths",
        f"--directory={output_relative}",
        str(overlay / "m5-audio.patch"),
    ]
    subprocess.run([*apply_args[:2], "--check", *apply_args[2:]], cwd=repo, check=True)
    subprocess.run(apply_args, cwd=repo, check=True)

    pocket = output / "src" / "fpga" / "targets" / "pocket"
    configs = output / "src" / "fpga" / "vendor" / "vexriscv" / "configs"
    vexii = output / "src" / "fpga" / "vendor" / "vexriscv" / "VexiiRiscv"
    (pocket / "variants").mkdir(parents=True, exist_ok=True)
    (pocket / "seeds").mkdir(parents=True, exist_ok=True)
    configs.mkdir(parents=True, exist_ok=True)
    vexii.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(overlay / "rpcmp.mk", pocket / "variants" / "rpcmp.mk")
    shutil.copyfile(overlay / "rpcmp.seed", pocket / "seeds" / "rpcmp.seed")
    shutil.copyfile(overlay / "rpcmp.cfg", configs / "rpcmp.cfg")
    shutil.copyfile(netlist, vexii / "VexiiRiscv_rpcmp.v")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
