"""Package a local HYB1 candidate with selectable, previously verified real songs."""
import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath as P
import re
import wave
import zipfile

import pocket_firmware_pair as pair
import pocket_package as shared
from pocket_player_package import definitions as player_definitions

ROOT = Path(__file__).resolve().parents[1]
CORE, PLATFORM = "RPCMP.HybridProbe", "rpcmp_hybrid"


def track_files(manifest):
    if manifest.stat().st_size > 65536:
        raise ValueError("track manifest too large")
    tracks = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(tracks, list) or not 1 <= len(tracks) <= 16:
        raise ValueError("choose 1 to 16 prepared tracks")
    common = P("Assets") / PLATFORM / "common"
    files, ids = {}, set()
    catalog = ["# 実機確認の曲目一覧", "", "各JSONが1曲です。Pocketで選択後、Aで再生してください。",
               "曲を変えるときはBで停止し、Pocketのメニューからコアを終了して選び直してください。",
               "STARTは合成テストへの切り替えです。実曲の選曲には使いません。", "",
               "| Pocketで選ぶJSON | 曲名 | 音源 | PCで聴く比較用音声 |",
               "| --- | --- | --- | --- |"]

    def read(name):
        path = (manifest.parent / name).resolve()
        if not path.is_relative_to(manifest.parent.resolve()):
            raise ValueError("track file leaves prepared directory")
        if not 10 <= path.stat().st_size <= 16 * 1024 * 1024:
            raise ValueError("prepared input outside fixture bounds")
        return path.read_bytes()

    for track in tracks:
        name, title = track["id"], track["title"]
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,19}", name) or name.casefold() in ids:
            raise ValueError("invalid or duplicate track id")
        ids.add(name.casefold())
        if not isinstance(title, str) or not 1 <= len(title) <= 255 or any(ord(c) < 32 for c in title):
            raise ValueError("invalid track title")
        mdx = read(track["mdx"])
        has_pdx = mdx[2:4] == b"\0\0"
        expected = bytes.fromhex("00000000000a00080000" if has_pdx else "0000ffff000a00080000")
        if mdx[:10] != expected:
            raise ValueError("invalid prepared MDX wrapper")
        if has_pdx != bool(track.get("pdx")):
            raise ValueError("MDX/PDX dependency mismatch")
        slots = [dict(id=i, filename=f) for i, f in enumerate(("os.bin", "hybrid.ini", "hybrid.elf"), 1)]
        files[common / (name + ".mdx.bin")] = mdx
        slots.append(dict(id=4, filename=name + ".mdx.bin"))
        if has_pdx:
            pdx = read(track["pdx"])
            if pdx[:10] != bytes.fromhex("00000000000a00020000"):
                raise ValueError("invalid prepared PDX wrapper")
            files[common / (name + ".pdx.bin")] = pdx
            slots.append(dict(id=5, filename=name + ".pdx.bin"))
        files[P("Assets") / PLATFORM / CORE / (name + ".json")] = shared.json_bytes(
            dict(instance=dict(magic=shared.MAGIC, data_slots=slots)))
        audio = read(track["reference"])
        try:
            with wave.open(io.BytesIO(audio), "rb") as wav:
                if (wav.getnchannels(), wav.getsampwidth(), wav.getframerate()) != (2, 2, 48000):
                    raise ValueError("reference WAV must be stereo 16-bit 48 kHz")
                if not 0 < wav.getnframes() <= 48000 * 60 or len(wav.readframes(wav.getnframes())) != wav.getnframes() * 4:
                    raise ValueError("invalid reference WAV length")
        except (wave.Error, EOFError) as error:
            raise ValueError("invalid reference WAV") from error
        files[P("試聴用") / (name + ".wav")] = audio
        safe_title = title.replace("|", "／").replace("<", "＜").replace(">", "＞")
        catalog.append(f"| {name}.json | {safe_title} | {'FM＋PCM' if has_pdx else 'FMのみ'} | "
                       f"[試聴](試聴用/{name}.wav) |")
    catalog += ["", "比較用WAVはMDXPlayer由来のPC参照エンジンによる冒頭部分です。",
                "Pocketの録音ではありません。旋律・テンポ・打楽器などを聴き比べてください。",
                "FM音色や音量の完全一致を保証するものではありません。"]
    files[P("曲目一覧.md")] = ("\n".join(catalog) + "\n").encode("utf-8")
    return files


def package(args):
    output = args.output.resolve()
    archive = output.with_suffix(".zip")
    if output.parent != ROOT / "out/build" or output.exists() or archive.exists():
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
    if "PASS HYB1 synchronizers and all 48 Gray-pointer bits in four corners" not in audit.read_text(encoding="utf-8"):
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
    values["core.json"]["core"]["metadata"].update(platform_ids=[PLATFORM], shortname="HybridProbe",
        description="RPCMP HYB1 real-song probe", version="0.12.0-hybrid-r3", date_release="2026-09-21")
    slots = values["data.json"]["data"]["data_slots"]
    slots[0]["name"] = "Hybrid Probe"
    slots[4:] = [dict(id=i, name=name, required=False, parameters=8, extensions=["bin"],
                      deferload=True, size_maximum=16*1024*1024) for i, name in ((4, "Prepared MDX"), (5, "Prepared PDX"))]
    values["input.json"]["input"]["controllers"] = [dict(type="default", mappings=[
        dict(id=i, name=name, **{key: True}) for i, name, key in
        ((0, "Play", "pad_btn_a"), (1, "Stop", "pad_btn_b"), (6, "Select test", "pad_btn_start"))])]
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
    files[P("確認手順.md")] = (ROOT / "docs/development/pocket-hybrid-hardware.md").read_bytes()
    files[P("Platforms") / (PLATFORM + ".json")] = shared.json_bytes(dict(platform=dict(
        category="Computer", name="RPCMP Hybrid Probe", year=2026, manufacturer="RPCMP")))
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
        "initialize its pre-accumulators on reset. Sources remain in the local workspace.\n"
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
    evidence = dict(status="local hardware check pending", firmware_pair=pairing, app_budget=budget,
                    reports=reports, native_rbf_sha256=shared.sha256(rbf), artifacts=entries,
                    zip_sha256=shared.sha256(archive))
    archive.with_suffix(".evidence.json").write_bytes(shared.json_bytes(evidence))
    print(archive)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("shell", "cpu", "firmware", "tracks", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    package(parser.parse_args())
