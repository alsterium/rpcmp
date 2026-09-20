"""Prepare matching HYB1 boot/OS firmware on the existing pinned substrate."""
import argparse
import json
from pathlib import Path

import pocket_player_firmware_prepare as player
from pocket_m5_audio_prepare import sha256


def prepare(repo, upstream, musl, output):
    output = player.prepare(repo, upstream, musl, output)
    source = repo.resolve() / "overlays/openfpgaos/boot_hybrid_sound_reset.inc"
    target = output / "src/firmware/os/targets/pocket/boot/boot_sound_reset.inc"
    target.write_bytes(source.read_bytes().replace(b"\r\n", b"\n"))
    manifest = output / "rpcmp-firmware-inputs.json"
    data = json.loads(manifest.read_text(encoding="utf-8"))
    data["profile"] = "hybrid-hyb1"
    del data["m6_sound_source"]
    data["hybrid_sound_source"] = source.relative_to(repo.resolve()).as_posix()
    data["boot_overlay"]["boot_sound_reset.inc"] = sha256(source)
    manifest.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("repo", "upstream", "musl", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.repo, args.upstream, args.musl, args.output))
