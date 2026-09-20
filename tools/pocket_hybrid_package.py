"""Package a local HYB1 timing candidate with one previously verified input pair."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath as P
import re
import zipfile

import pocket_firmware_pair as pair
import pocket_package as shared
from pocket_player_package import definitions as player_definitions

ROOT = Path(__file__).resolve().parents[1]
CORE, PLATFORM = "RPCMP.HybridProbe", "rpcmp_hybrid"


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
        description="RPCMP HYB1 local timing probe", version="0.12.0-hybrid-r1", date_release="2026-09-21")
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
    for name in ("mdx.bin", "pdx.bin"):
        path = cpu / name
        if not 10 <= path.stat().st_size <= 16*1024*1024:
            raise ValueError("prepared input outside fixture bounds")
        files[common / name] = path.read_bytes()
    files[P("Assets") / PLATFORM / CORE / "hybrid.json"] = shared.json_bytes(dict(instance=dict(
        magic=shared.MAGIC, data_slots=[dict(id=i, filename=name) for i, name in enumerate(
            ("os.bin", "hybrid.ini", "hybrid.elf", "mdx.bin", "pdx.bin"), 1)])))
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
    for name in ("shell", "cpu", "firmware", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    package(parser.parse_args())
