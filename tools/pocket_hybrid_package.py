"""Package a local HYB4 candidate and a runtime-only update preserving the user's collection."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath as P
import re
import wave
import zipfile

import prepared_playlist
import pocket_firmware_pair as pair
import pocket_package as shared
from pocket_player_package import definitions as player_definitions

ROOT = Path(__file__).resolve().parents[1]
CORE, PLATFORM = "RPCMP.MinimalPlayer", "rpcmp_minimal"


def runtime_update(files):
    return {path: data for path, data in files.items()
            if (path.parts[0] in ("Cores", "Assets", "Platforms") and path.suffix != ".hpl")
            or path == P("確認手順.md")}


def track_files(manifest):
    playlist, tracks = prepared_playlist.pack(manifest)
    common = P("Assets") / PLATFORM / "common"
    slots = [dict(id=i, filename=f) for i, f in enumerate(
        ("os.bin", "hybrid.ini", "hybrid.elf", "playlist.hpl"), 1)]
    files = {common / "playlist.hpl": playlist,
             P("Assets") / PLATFORM / CORE / "Playlist.json": shared.json_bytes(
                 dict(instance=dict(magic=shared.MAGIC, data_slots=slots)))}
    catalog = ["# 曲目一覧", "", "Pocketで Playlist.json を選ぶと、全曲を一覧から選べます。",
               "上下で選曲、左右でページ移動、Aで先頭から再生、Bで停止、Xで一時停止・再開、Yでループ設定を切り替えます。", "",
               "| 番号 | 曲名 | 音源 | PC比較音声 |", "| --- | --- | --- | --- |"]
    for number, track in enumerate(tracks, 1):
        reference = "なし"
        if track.get("reference"):
            audio = prepared_playlist.read_local(manifest, track["reference"])
            try:
                with wave.open(io.BytesIO(audio), "rb") as wav:
                    if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (2, 2, 48000):
                        raise ValueError("reference WAV must be stereo 16-bit 48 kHz")
                    if not 0 < wav.getnframes() <= 48000 * 60 or len(wav.readframes(wav.getnframes())) != wav.getnframes() * 4:
                        raise ValueError("invalid reference WAV length")
            except (wave.Error, EOFError) as error:
                raise ValueError("invalid reference WAV") from error
            name = f"{number:03d}.wav"
            files[P("試聴用") / name] = audio
            reference = f"[試聴](試聴用/{name})"
        title = track["title"].replace("|", "／").replace("<", "＜").replace(">", "＞")
        catalog.append(f"| {number} | {title} | {'FM＋PCM' if track.get('pdx') else 'FMのみ'} | {reference} |")
    files[P("曲目一覧.md")] = ("\n".join(catalog) + "\n").encode("utf-8")
    return files

def package(args):
    output = args.output.resolve()
    archive = output.with_suffix(".zip")
    update_archive = output.with_name(output.name + "-update").with_suffix(".zip")
    if output.parent != ROOT / "out/build" or output.exists() or archive.exists() or update_archive.exists():
        raise ValueError("choose a fresh package directly under out/build")
    shell, cpu, firmware = (p.resolve() for p in (args.shell, args.cpu, args.firmware))
    fit = shell / "hybrid-fit"
    mif = shell / "src/fpga/targets/pocket/firmware.mif"
    pairing = pair.verify(firmware / "firmware.elf", mif, firmware / "os.bin")
    reports = {}
    for suffix in ("map.rpt", "fit.rpt", "sta.rpt", "asm.rpt", "flow.rpt"):
        path = fit / "output_files" / ("ap_core." + suffix)
        # Quartus mixes legacy degree symbols into these reports. The tested
        # completion markers and this workspace's ROM path are all ASCII.
        text = path.read_text(encoding="utf-8", errors="replace")
        success = re.search(r"Flow Status\s*;\s*Successful", text) if suffix == "flow.rpt" else "successful. 0 errors" in text
        if not success or "Critical Warning (127003)" in text or "setting all initial values to 0" in text:
            raise ValueError("invalid Quartus completion")
        if suffix == "map.rpt" and mif.as_posix() not in text:
            raise ValueError("mapped ROM is not the paired ROM")
        reports[suffix] = shared.sha256(path)
    audit = fit / "hybrid-cdc-audit.log"
    if "PASS HYB4 repeat/pause/status synchronizers and all 48 Gray-pointer bits in four corners" not in audit.read_text(encoding="utf-8"):
        raise ValueError("candidate CDC audit missing")
    elf = cpu / "hybrid-player.elf"
    sections, _ = pair.elf_sections(elf.read_bytes())
    budget = json.loads((cpu / "budget.json").read_text())
    measured = dict(text_bytes=sum(sections[n][1] for n in (".text", ".eh_frame")),
                    data_bytes=sum(sections[n][1] for n in (".data", ".init_array")),
                    bss_bytes=sections[".bss"][1])
    if (any(budget[k] != v for k, v in measured.items()) or
            budget["static_bytes"] != sum(measured.values()) or budget["static_bytes"] > 56623104 or
            budget["data_bytes"] > 131072 or budget["dynamic_stack_frames"] or
            not 0 < budget["conservative_stack_bound_bytes"] <= 524288):
        raise ValueError("application budget does not match this ELF")
    values = player_definitions()
    values["core.json"]["core"]["metadata"].update(platform_ids=[PLATFORM], shortname="MinimalPlayer",
        description="RPCMP MDX player with repeat", version="0.16.0-player-r4", date_release="2026-09-22")
    slots = values["data.json"]["data"]["data_slots"]
    slots[0]["name"] = "MDX Player"
    slots[4:] = [dict(id=4, name="Playlist", required=True, parameters=8, extensions=["hpl"],
                      deferload=True, size_maximum=prepared_playlist.FILE_LIMIT)]
    values["input.json"]["input"]["controllers"] = [dict(type="default", mappings=[
        dict(id=i, name=name, **{key: True}) for i, name, key in
        ((0, "Play selected", "pad_btn_a"), (1, "Stop", "pad_btn_b"),
         (2, "Pause / Resume", "pad_btn_x"), (3, "Loop / Repeat", "pad_btn_y"))])]
    sdk = ROOT / "out/research/openfpgaSDK-a408ddc"
    manifest = shared.parse_manifest(sdk / "runtime/MANIFEST")
    loader = shared.verify_runtime_file(sdk / "runtime", manifest, "pocket/loader.bin")
    core, common = P("Cores") / CORE, P("Assets") / PLATFORM / "common"
    rbf = fit / "output_files/ap_core.rbf"
    files = {core / name: shared.json_bytes(value) for name, value in values.items()}
    files.update({core / "loader.bin": loader.read_bytes(), core / "rpcmp.rbf_r": shared.reverse_rbf_bits(rbf.read_bytes()),
                  common / "os.bin": (firmware / "os.bin").read_bytes(), common / "hybrid.elf": elf.read_bytes(),
                  common / "hybrid.ini": b"[os]\nELF=hybrid.elf\nVARIANT=rpcmp\n"})
    files.update(track_files(args.tracks.resolve()))
    files[P("確認手順.md")] = (ROOT / "docs/development/pocket-minimal-player.md").read_bytes()
    files[P("Platforms") / (PLATFORM + ".json")] = shared.json_bytes(dict(platform=dict(
        category="Computer", name="RPCMP MDX Player", year=2026, manufacturer="RPCMP")))
    for name in ("OFL-1.1.txt", "README.md"):
        files[core / "NOTICES" / ("font-" + name)] = (ROOT / "third_party/unifont" / name).read_bytes()
    reference = ROOT / "out/research/mdxplayer-reference-20260921"
    files[core / "NOTICES" / "MDXPlayer-README.md"] = (reference / "README.md").read_bytes()
    files[core / "NOTICES" / "FMGEN-readme.txt"] = (reference / "gamdx/jni/fmgen/readme.txt").read_bytes()
    files[core / "NOTICES" / "JT51-COPYING"] = (ROOT / "out/research/jt51-985a573/LICENSE").read_bytes()
    files[core / "NOTICES" / "LOCAL-TEST.txt"] = (
        "Local hardware experiment; not a public release. Contains private user music.\n"
        "MXDRV (c)1988-92 milk., K.MAEKAWA, Missy.M, Yatsube; fMXDRVg/GAMDX by GORRY.\n"
        "X68Sound/PCM8/downsampling by m_puusan; FMGEN by cisc; JT51 GPL-3.0-or-later.\n"
        "Reference terms: https://gorry.haun.org/android/gamdx/\n"
        "Component-specific redistribution review remains required before public release.\n"
        "Renderer changes: capture timed writes and wide PCM; omit software FM mixing;\n"
        "use signed char and integer-zero portability fix. JT51 changes: wide taps and\n"
        "initialize its pre-accumulators on reset; retain native state during pause.\n"
        "Sources remain in the local workspace.\n"
    ).encode()
    entries = {}
    for path, data in files.items():
        if path.suffix == ".json":
            value = json.loads(data)
            if len(value) != 1 or (path.parts[0] != "Platforms" and next(iter(value.values())).get("magic") != shared.MAGIC):
                raise ValueError("invalid APF JSON root/magic")
        shared.write_file(output, path, data)
        entries[path.as_posix()] = dict(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as zipped:
        for path, data in sorted(files.items()):
            zipped.writestr(path.as_posix(), data)
    with zipfile.ZipFile(archive) as zipped:
        if set(zipped.namelist()) != set(entries):
            raise ValueError("archive members differ")
        for path, data in files.items():
            if zipped.read(path.as_posix()) != data or (output / path).read_bytes() != data:
                raise ValueError("package readback differs")
    # Existing M3U collections stay installed when applying this runtime-only update.
    update_files = runtime_update(files)
    with zipfile.ZipFile(update_archive, "x", zipfile.ZIP_DEFLATED) as zipped:
        for path, data in sorted(update_files.items()):
            zipped.writestr(path.as_posix(), data)
    with zipfile.ZipFile(update_archive) as zipped:
        if set(zipped.namelist()) != {p.as_posix() for p in update_files}:
            raise ValueError("update archive members differ")
        for path, data in update_files.items():
            if zipped.read(path.as_posix()) != data:
                raise ValueError("update package readback differs")
    evidence = dict(status="local hardware check pending", firmware_pair=pairing, app_budget=budget,
                    reports=reports, native_rbf_sha256=shared.sha256(rbf), artifacts=entries,
                    zip_sha256=shared.sha256(archive), update_zip_sha256=shared.sha256(update_archive))
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    print(archive)
    print(update_archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("shell", "cpu", "firmware", "tracks", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    package(parser.parse_args())
