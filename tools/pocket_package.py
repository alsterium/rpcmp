#!/usr/bin/env python3
"""Build the allowlisted local-only openfpgaOS Pocket probe package."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path, PurePosixPath

SDK_REVISION = "a408ddc12aed0dfaa4aa22c06af82f829db77126"
RUNTIME_REVISION = "618a3eb"
INTEGRATED_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/rpcmpjt/output_files/ap_core.rbf"
)
INTEGRATED_RBF_SHA256 = "f532dfe96f8563a14a0860fcc83a89b67cd03527d82c5a1a71c22a73091cc190"
BOOT_ISOLATION_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/rpcmp90/output_files/ap_core.rbf"
)
BOOT_ISOLATION_RBF_SHA256 = "f131a5677389e1557de863838c1289e20b8456b57da5a93722d8e69977697d91"
CLOCK_ISOLATION_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/rpcmp90fc/output_files/ap_core.rbf"
)
CLOCK_ISOLATION_RBF_SHA256 = "d8fcc136e09d21c62505330570f9d1d16011024799625aabfa94ac20a2d016f5"
EARLY_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-entry-marker.bin"
)
EARLY_MARKER_OS_SHA256 = "560ffe0ed43e85fd0ff2cf0c40eb73be731a0639c0ae97181250940f8fd131c2"
EARLY_INIT_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-early-init-marker.bin"
)
EARLY_INIT_MARKER_OS_SHA256 = "8f286d3f3aec754277420333f6583bf80fa6de008ee5ad90b6605f8e6accab2b"
PRE_EARLY_INIT_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-pre-early-init-marker.bin"
)
PRE_EARLY_INIT_MARKER_OS_SHA256 = "492485233d45a641f5fa7956ff214ab8680a230e4d013ac7ae6feeed1b38a0e4"
PRE_VIDEO_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-pre-video-marker.bin"
)
PRE_VIDEO_MARKER_OS_SHA256 = "9dd374f0f022fb84f48dc3a2fa8af0ef19ab755d696ec33b172bf1b49269d431"
POST_VIDEO_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-post-video-marker.bin"
)
POST_VIDEO_MARKER_OS_SHA256 = "338c8459dcd9f6723cd6226138761978425ff656eac3d1f9128ab7c7f7724c12"
POST_PALETTE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-post-palette-marker.bin"
)
POST_PALETTE_MARKER_OS_SHA256 = "46fdf101ebb64e838c2785d62bc7fd1e3ac9654bdb4d9a24b5dbe0ad4267a2be"
FIRST_PALETTE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-first-palette-marker.bin"
)
FIRST_PALETTE_MARKER_OS_SHA256 = "148607342e593d8cb7246a40f44d278f6c6186754e5066833fbd1ab1986f960a"
POST_TERMINAL_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-post-terminal-marker.bin"
)
POST_TERMINAL_MARKER_OS_SHA256 = "9350174f08cd1f1e37e54461c707da68983f6573612b946ca8f1ad1d20b69662"
SYNCED_PALETTE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-synced-palette-marker.bin"
)
SYNCED_PALETTE_MARKER_OS_SHA256 = "9bef56fe8cb53b60cbb632e9c179e31521d3e0416bc5603df6f0a3f1a3415451"
TERMINAL_MODE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-terminal-mode-marker.bin"
)
TERMINAL_MODE_MARKER_OS_SHA256 = "5831e94db77386ba1c76d1865bfa18c81db710470eebce38a2b60370be7bab9b"
TERM_CLEAR_NO_FLUSH_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-clear-no-flush-marker.bin"
)
TERM_CLEAR_NO_FLUSH_MARKER_OS_SHA256 = "25ade5881f2fba8a883e7a94b33ee844461ef2803306cbf27357b4595f5637d3"
TERM_CLEAR_INTERNAL_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-clear-internal-marker.bin"
)
TERM_CLEAR_INTERNAL_MARKER_OS_SHA256 = "13a74751906c520c2cdb5d7d97ccb72df2a1eaeefbf44ad953f5b23e54146cdb"
TERM_STATE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-state-marker.bin"
)
TERM_STATE_MARKER_OS_SHA256 = "d73a272457189b37b2c1ebb2b70ec880aa9fef9993b27c478f69eb79934b6cab"
TERM_CHARS_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-chars-marker.bin"
)
TERM_CHARS_MARKER_OS_SHA256 = "c762ad7e45d6394b042806f55d4b42e094777a76a6e449ff07b61a961cb0b664"
TERM_CHARS_VOLATILE_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-chars-volatile-marker.bin"
)
TERM_CHARS_VOLATILE_MARKER_OS_SHA256 = "f78a94ff4fc05c479ec6951734905d664db954d96fdc8d35d2d2670056df25f2"
TERM_CHARS_WORD_LOOP_MARKER_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-term-chars-word-loop-marker.bin"
)
TERM_CHARS_WORD_LOOP_MARKER_OS_SHA256 = "1ceb9f4291e8037aa2f09461be9522c56965bf79561953a0854f2474a9f25493"
SAFE_MEMSET_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-safe-memset.bin"
)
SAFE_MEMSET_OS_SHA256 = "3bb812a1b320c7350046097d361dbf8567662218c9d8ba2f0457e0325f2826a9"
MEMOPS_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-selftest.bin"
)
MEMOPS_SELFTEST_OS_SHA256 = "58453b17ff872a5f715b8e56e856605cefee143b096ddbe98f28efd633c00a84"
MEMCPY_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memcpy-selftest.bin"
)
MEMCPY_SELFTEST_OS_SHA256 = "c42369c8b78bb09f34e69c62e9334f12839c1d573810c095cbea844544ee53d1"
MEMCPY_SMALL_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memcpy-small-selftest.bin"
)
MEMCPY_SMALL_SELFTEST_OS_SHA256 = "baf29ae95cf5833d84141bc6b6d23077c31007f7cdd1f84928c269a505eee699"
MEMCPY_HARNESS_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memcpy-harness-selftest.bin"
)
MEMCPY_HARNESS_SELFTEST_OS_SHA256 = "44b581123cf53fe8c1b6077308709f0081df3fa2d227a20b22283ee735a88ad9"
MEMOPS_LAYOUT_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-layout-selftest.bin"
)
MEMOPS_LAYOUT_SELFTEST_OS_SHA256 = "0f7db3529ef6fe9ac15f1777799e009936a22504aafd055af58b54483a2ce222"
MEMOPS_ONE_BUFFER_LAYOUT_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-one-buffer-layout-selftest.bin"
)
MEMOPS_ONE_BUFFER_LAYOUT_SELFTEST_OS_SHA256 = "d9513734c0eb06fd37f7785a33fb1d264b8444c5704027f101c874fb0637b7ce"
MEMOPS_NO_BUFFER_LAYOUT_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-no-buffer-layout-selftest.bin"
)
MEMOPS_NO_BUFFER_LAYOUT_SELFTEST_OS_SHA256 = "169f20e337e0e77c3b87c81a38c1576fe845451885c09c7fc9e10c07901577f9"
MEMOPS_SILENT_CALL_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-silent-call-selftest.bin"
)
MEMOPS_SILENT_CALL_SELFTEST_OS_SHA256 = "b2388a933173f3f4f24ac78117a143a6221bbdece6a00add17d3f8dcd4732f54"
MEMOPS_LABEL_ONLY_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-label-only-selftest.bin"
)
MEMOPS_LABEL_ONLY_SELFTEST_OS_SHA256 = "00c5bab243a56af2f709caf9f7ceb4e89d5800ff9732d42e00fced97193f038a"
MEMOPS_STATUS_NO_NEWLINE_SELFTEST_OS = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/firmware/os/bld/pocket/os-memops-status-no-newline-selftest.bin"
)
MEMOPS_STATUS_NO_NEWLINE_SELFTEST_OS_SHA256 = "173cf50c8594b3c1a762a0b5a79030a7a0a9772117fdce511250c1a4d75f3344"
SAFE_MEMSET_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/rpcmp90fcmem/output_files/ap_core.rbf"
)
SAFE_MEMSET_RBF_SHA256 = "fa75e3cf3fe465090924d28df5616170cd2f4eefd9f4d68c89a72cb2cd93dbd5"
FREQUENCY_CONTROL_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/rpcmp100fc/output_files/ap_core.rbf"
)
FREQUENCY_CONTROL_RBF_SHA256 = "e5728c288c5b013e4c0467f7d6a16cbf9c0a4a989cec2deed443f48d23e6fc8a"
LOCAL_STOCK_RBF = Path(
    "out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/"
    "bld/os25/output_files/ap_core.rbf"
)
LOCAL_STOCK_RBF_SHA256 = "502b60de887cc48600275d3516a8abf633cd77753be86a844f8900527a6bef6f"
CORE_ID = "RPCMP.openfpgaOSProbe"
CORE_SHORTNAME = "openfpgaOSProbe"
PLATFORM_ID = "rpcmp_probe"
VARIANT = "os25"
MAGIC = "APF_VER_1"
PROHIBITED_SUFFIXES = {".mid", ".mod", ".ofsf"}


def json_bytes(value: object) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=True) + "\n").encode("ascii")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_manifest(path: Path) -> dict[str, str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not any(f"source: {RUNTIME_REVISION}" in line for line in lines):
        raise ValueError(f"runtime manifest does not identify source {RUNTIME_REVISION}")
    entries: dict[str, str] = {}
    for line in lines:
        if not line or line.startswith("#"):
            continue
        digest, relative = line.split(maxsplit=1)
        entries[relative.removeprefix("./")] = digest
    return entries


def verify_runtime_file(runtime: Path, manifest: dict[str, str], relative: str) -> Path:
    source = runtime / relative
    expected = manifest.get(relative)
    if expected is None or not source.is_file():
        raise ValueError(f"runtime artifact is absent from its manifest: {relative}")
    actual = hashlib.md5(source.read_bytes()).hexdigest()  # noqa: S324 - upstream manifest format
    if actual != expected:
        raise ValueError(f"runtime artifact checksum mismatch: {relative}")
    return source


def definitions() -> dict[str, object]:
    core = {
        "core": {
            "magic": MAGIC,
            "metadata": {
                "platform_ids": [PLATFORM_ID],
                "shortname": CORE_SHORTNAME,
                "description": "RPCMP M0 openfpgaOS runtime workload probe",
                "author": "RPCMP",
                "url": "",
                "version": "0.5.33-spike",
                "date_release": "2026-08-31",
            },
            "framework": {
                "target_product": "Analogue Pocket",
                "version_required": "2.2",
                "sleep_supported": False,
                "dock": {"supported": True, "analog_output": False},
                "hardware": {"link_port": True, "cartridge_adapter": 0},
            },
            "cores": [
                {
                    "name": VARIANT,
                    "id": 0,
                    "filename": f"{VARIANT}.rbf_r",
                    "chip32_vm": "loader.bin",
                }
            ],
        }
    }
    slots = [
        {"id": 0, "name": "Probe", "required": True, "parameters": 275, "extensions": ["json"]},
        {"id": 1, "name": "OS Binary", "required": False, "parameters": 0, "extensions": ["bin"], "deferload": True},
        {"id": 2, "name": "OS Config", "required": False, "parameters": 0, "extensions": ["ini"], "deferload": True},
        {"id": 3, "name": "Application", "required": False, "parameters": 0, "extensions": ["elf"], "deferload": True},
        {"id": 4, "name": "Synthetic Data", "required": False, "parameters": 0, "extensions": ["bin"], "deferload": True},
    ]
    return {
        "audio.json": {"audio": {"magic": MAGIC}},
        "core.json": core,
        "data.json": {"data": {"magic": MAGIC, "data_slots": slots}},
        "input.json": {
            "input": {
                "magic": MAGIC,
                "controllers": [
                    {
                        "type": "default",
                        "mappings": [
                            {"id": 0, "name": "A", "key": "pad_btn_a"},
                            {"id": 1, "name": "B", "key": "pad_btn_b"},
                            {"id": 2, "name": "X", "key": "pad_btn_x"},
                            {"id": 3, "name": "Y", "key": "pad_btn_y"},
                            {"id": 10, "name": "L", "key": "pad_trig_l"},
                            {"id": 11, "name": "R", "key": "pad_trig_r"},
                            {"id": 20, "name": "Start", "key": "pad_btn_start"},
                            {"id": 21, "name": "Select", "key": "pad_btn_select"},
                        ],
                    }
                ],
            }
        },
        "interact.json": {"interact": {"magic": MAGIC, "variables": [], "messages": []}},
        "variants.json": {"variants": {"magic": MAGIC, "variant_list": []}},
        "video.json": {
            "video": {
                "magic": MAGIC,
                "scaler_modes": [
                    {"width": 320, "height": 240, "aspect_w": 4, "aspect_h": 3, "rotation": 0, "mirror": 0}
                ],
            }
        },
    }


def expected_paths() -> set[PurePosixPath]:
    core_root = PurePosixPath("Cores") / CORE_ID
    common_root = PurePosixPath("Assets") / PLATFORM_ID / "common"
    instance_root = PurePosixPath("Assets") / PLATFORM_ID / CORE_ID
    names = set(definitions()) | {"loader.bin", f"{VARIANT}.rbf_r"}
    paths = {core_root / name for name in names}
    paths |= {common_root / name for name in ("os.bin", "rpcmp-probe.ini", "rpcmp-probe.elf", "synthetic.bin")}
    paths |= {instance_root / "rpcmp-probe.json", PurePosixPath("Platforms") / f"{PLATFORM_ID}.json"}
    return paths


def validate_json(path: Path) -> None:
    value = json.loads(path.read_text(encoding="ascii"))
    if len(value) != 1:
        raise ValueError(f"JSON must have exactly one root object: {path}")
    root = next(iter(value.values()))
    if path.name != f"{PLATFORM_ID}.json" and root.get("magic") != MAGIC:
        raise ValueError(f"invalid or missing APF magic: {path}")
    if path.name == "data.json":
        slots = root.get("data_slots", [])
        if [slot.get("id") for slot in slots] != [0, 1, 2, 3, 4]:
            raise ValueError("data.json must contain only slots 0 through 4")
        if any(slot.get("nonvolatile") for slot in slots):
            raise ValueError("nonvolatile slots are prohibited in this spike")
        if any(slot.get("required") or not slot.get("deferload") for slot in slots[1:]):
            raise ValueError("openfpgaOS slots 1 through 4 must be optional and deferred")
    if path.name == "core.json":
        metadata = root.get("metadata", {})
        expected_core_id = f"{metadata.get('author')}.{metadata.get('shortname')}"
        if path.parent.name != expected_core_id:
            raise ValueError("core folder must match metadata author and shortname")
    if path.name == "input.json":
        controllers = root.get("controllers", [])
        if not controllers or any(not controller.get("mappings") for controller in controllers):
            raise ValueError("input.json controllers must contain mappings")
    if path.name == "rpcmp-probe.json" and "variant_select" in root:
        raise ValueError("the single-bitstream instance must not use variant_select")


def verify_tree(root: Path) -> list[Path]:
    if not root.is_dir():
        raise ValueError(f"package tree does not exist: {root}")
    files = sorted(path for path in root.rglob("*") if path.is_file())
    actual = {PurePosixPath(path.relative_to(root).as_posix()) for path in files}
    expected = expected_paths()
    if actual != expected:
        missing = sorted(str(path) for path in expected - actual)
        extra = sorted(str(path) for path in actual - expected)
        raise ValueError(f"package allowlist mismatch; missing={missing}, extra={extra}")
    for path in files:
        if path.suffix.lower() in PROHIBITED_SUFFIXES:
            raise ValueError(f"prohibited media in package: {path}")
        if path.suffix.lower() == ".json":
            validate_json(path)
    return files


def write_file(root: Path, relative: PurePosixPath, data: bytes) -> None:
    destination = root.joinpath(*relative.parts)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)


def reverse_rbf_bits(data: bytes) -> bytes:
    table = bytes(int(f"{value:08b}"[::-1], 2) for value in range(256))
    return data.translate(table)


def build(
    repo: Path,
    sdk: Path,
    elf: Path,
    output: Path,
    archive: Path,
    integrated_rbf: bool = False,
    boot_isolation_rbf: bool = False,
    clock_isolation_rbf: bool = False,
    frequency_control_rbf: bool = False,
    local_stock_rbf: bool = False,
    early_os_marker: bool = False,
    early_init_marker: bool = False,
    pre_early_init_marker: bool = False,
    pre_video_marker: bool = False,
    post_video_marker: bool = False,
    post_palette_marker: bool = False,
    first_palette_marker: bool = False,
    post_terminal_marker: bool = False,
    synced_palette_marker: bool = False,
    terminal_mode_marker: bool = False,
    term_clear_no_flush_marker: bool = False,
    term_clear_internal_marker: bool = False,
    term_state_marker: bool = False,
    term_chars_marker: bool = False,
    term_chars_volatile_marker: bool = False,
    term_chars_word_loop_marker: bool = False,
    memset_fix_candidate: bool = False,
    memops_selftest: bool = False,
    memcpy_selftest: bool = False,
    memcpy_small_selftest: bool = False,
    memcpy_harness_selftest: bool = False,
    memops_layout_selftest: bool = False,
    memops_one_buffer_layout_selftest: bool = False,
    memops_no_buffer_layout_selftest: bool = False,
    memops_silent_call_selftest: bool = False,
    memops_label_only_selftest: bool = False,
    memops_status_no_newline_selftest: bool = False,
) -> None:
    revision = subprocess.run(
        ["git", "-C", str(sdk), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != SDK_REVISION:
        raise ValueError(f"SDK revision mismatch: {revision}")
    runtime = sdk / "runtime"
    manifest = parse_manifest(runtime / "MANIFEST")
    loader = verify_runtime_file(runtime, manifest, "pocket/loader.bin")
    os_binary = verify_runtime_file(runtime, manifest, "pocket/os.bin")
    os_binary_data = os_binary.read_bytes()
    os_profile = "manifest os25"
    if early_os_marker:
        marker_os = (repo / EARLY_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"early-marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != EARLY_MARKER_OS_SHA256:
            raise ValueError(
                "early-marker OS checksum mismatch: "
                f"expected {EARLY_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = "os_main writes a 320x16 white framebuffer marker and halts"
    elif early_init_marker:
        marker_os = (repo / EARLY_INIT_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"early-init marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != EARLY_INIT_MARKER_OS_SHA256:
            raise ValueError(
                "early-init marker OS checksum mismatch: "
                f"expected {EARLY_INIT_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = "of_init_early completes, then os_main writes a 320x16 white marker and halts"
    elif pre_early_init_marker:
        marker_os = (repo / PRE_EARLY_INIT_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"pre-early-init marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != PRE_EARLY_INIT_MARKER_OS_SHA256:
            raise ValueError(
                "pre-early-init marker OS checksum mismatch: "
                f"expected {PRE_EARLY_INIT_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = "IRQ reset and textguard baseline complete, then a 320x16 white marker halts"
    elif pre_video_marker:
        marker_os = (repo / PRE_VIDEO_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"pre-video marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != PRE_VIDEO_MARKER_OS_SHA256:
            raise ValueError(
                "pre-video marker OS checksum mismatch: "
                f"expected {PRE_VIDEO_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = "clock register adoption completes, then a 320x16 white marker halts before video init"
    elif post_video_marker:
        marker_os = (repo / POST_VIDEO_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"post-video marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != POST_VIDEO_MARKER_OS_SHA256:
            raise ValueError(
                "post-video marker OS checksum mismatch: "
                f"expected {POST_VIDEO_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = "video init completes, then all three uncached app framebuffers receive a white band"
    elif post_palette_marker:
        marker_os = (repo / POST_PALETTE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"post-palette marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != POST_PALETTE_MARKER_OS_SHA256:
            raise ValueError(
                "post-palette marker OS checksum mismatch: "
                f"expected {POST_PALETTE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "video init and the terminal 16-color palette loop complete, then all three "
            "uncached app framebuffers receive a white band"
        )
    elif first_palette_marker:
        marker_os = (repo / FIRST_PALETTE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"first-palette marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != FIRST_PALETTE_MARKER_OS_SHA256:
            raise ValueError(
                "first-palette marker OS checksum mismatch: "
                f"expected {FIRST_PALETTE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "video init and the first terminal palette entry complete, then all three "
            "uncached app framebuffers receive a white band"
        )
    elif post_terminal_marker:
        marker_os = (repo / POST_TERMINAL_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"post-terminal marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != POST_TERMINAL_MARKER_OS_SHA256:
            raise ValueError(
                "post-terminal marker OS checksum mismatch: "
                f"expected {POST_TERMINAL_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal init and synchronized final palette commit complete, then the "
            "uncached terminal framebuffer receives a persistent white band"
        )
    elif synced_palette_marker:
        marker_os = (repo / SYNCED_PALETTE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"synced-palette marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != SYNCED_PALETTE_MARKER_OS_SHA256:
            raise ValueError(
                "synced-palette marker OS checksum mismatch: "
                f"expected {SYNCED_PALETTE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "the 16-color palette loop and synchronized final commit complete in app mode, "
            "then all three uncached app framebuffers receive a persistent white band"
        )
    elif terminal_mode_marker:
        marker_os = (repo / TERMINAL_MODE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-mode marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERMINAL_MODE_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-mode marker OS checksum mismatch: "
                f"expected {TERMINAL_MODE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "the synchronized 16-color palette and terminal display-mode switch complete, "
            "then the uncached terminal framebuffer receives a persistent white band"
        )
    elif term_clear_no_flush_marker:
        marker_os = (repo / TERM_CLEAR_NO_FLUSH_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-clear-no-flush marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_CLEAR_NO_FLUSH_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-clear-no-flush marker OS checksum mismatch: "
                f"expected {TERM_CLEAR_NO_FLUSH_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal clear state, internal buffers, and cached framebuffer memset complete "
            "without cache flush, then the uncached terminal framebuffer receives a white band"
        )
    elif term_clear_internal_marker:
        marker_os = (repo / TERM_CLEAR_INTERNAL_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-clear-internal marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_CLEAR_INTERNAL_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-clear-internal marker OS checksum mismatch: "
                f"expected {TERM_CLEAR_INTERNAL_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal state and internal character/color buffer clears complete without any "
            "cached framebuffer access, then the uncached terminal framebuffer receives a white band"
        )
    elif term_state_marker:
        marker_os = (repo / TERM_STATE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-state marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_STATE_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-state marker OS checksum mismatch: "
                f"expected {TERM_STATE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal state stores complete without internal buffer or cached framebuffer clears, "
            "then the uncached terminal framebuffer receives a white band"
        )
    elif term_chars_marker:
        marker_os = (repo / TERM_CHARS_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-chars marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_CHARS_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-chars marker OS checksum mismatch: "
                f"expected {TERM_CHARS_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal state stores and the character-buffer clear complete without color-buffer "
            "or framebuffer clears, then the uncached terminal framebuffer receives a white band"
        )
    elif term_chars_volatile_marker:
        marker_os = (repo / TERM_CHARS_VOLATILE_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-chars-volatile marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_CHARS_VOLATILE_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-chars-volatile marker OS checksum mismatch: "
                f"expected {TERM_CHARS_VOLATILE_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal state stores and a volatile byte loop over the character buffer complete "
            "without calling memset, then the uncached terminal framebuffer receives a white band"
        )
    elif term_chars_word_loop_marker:
        marker_os = (repo / TERM_CHARS_WORD_LOOP_MARKER_OS).resolve()
        if not marker_os.is_file():
            raise ValueError(f"terminal-chars-word-loop marker OS does not exist: {marker_os}")
        marker_os_sha256 = sha256(marker_os)
        if marker_os_sha256 != TERM_CHARS_WORD_LOOP_MARKER_OS_SHA256:
            raise ValueError(
                "terminal-chars-word-loop marker OS checksum mismatch: "
                f"expected {TERM_CHARS_WORD_LOOP_MARKER_OS_SHA256}, got {marker_os_sha256}"
            )
        os_binary_data = marker_os.read_bytes()
        os_profile = (
            "terminal state stores and a volatile one-word-per-iteration loop over the character "
            "buffer complete, then the uncached terminal framebuffer receives a white band"
        )
    elif memset_fix_candidate:
        fixed_os = (repo / SAFE_MEMSET_OS).resolve()
        if not fixed_os.is_file():
            raise ValueError(f"safe-memset OS does not exist: {fixed_os}")
        fixed_os_sha256 = sha256(fixed_os)
        if fixed_os_sha256 != SAFE_MEMSET_OS_SHA256:
            raise ValueError(
                "safe-memset OS checksum mismatch: "
                f"expected {SAFE_MEMSET_OS_SHA256}, got {fixed_os_sha256}"
            )
        os_binary_data = fixed_os.read_bytes()
        os_profile = "normal openfpgaOS with a one-word-per-iteration memset implementation"
    elif memops_selftest:
        test_os = (repo / MEMOPS_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"memory-operations self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_SELFTEST_OS_SHA256:
            raise ValueError(
                "memory-operations self-test OS checksum mismatch: "
                f"expected {MEMOPS_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with cached-SDRAM memcpy/memmove boot self-test"
    elif memcpy_selftest:
        test_os = (repo / MEMCPY_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"memcpy self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMCPY_SELFTEST_OS_SHA256:
            raise ValueError(
                "memcpy self-test OS checksum mismatch: "
                f"expected {MEMCPY_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with cached-SDRAM memcpy-only boot self-test"
    elif memcpy_small_selftest:
        test_os = (repo / MEMCPY_SMALL_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"small memcpy self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMCPY_SMALL_SELFTEST_OS_SHA256:
            raise ValueError(
                "small memcpy self-test OS checksum mismatch: "
                f"expected {MEMCPY_SMALL_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with cached-SDRAM memcpy 0..31-byte boot self-test"
    elif memcpy_harness_selftest:
        test_os = (repo / MEMCPY_HARNESS_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"memcpy harness self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMCPY_HARNESS_SELFTEST_OS_SHA256:
            raise ValueError(
                "memcpy harness self-test OS checksum mismatch: "
                f"expected {MEMCPY_HARNESS_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with memcpy test harness and no memcpy calls"
    elif memops_layout_selftest:
        test_os = (repo / MEMOPS_LAYOUT_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"memory-ops layout self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_LAYOUT_SELFTEST_OS_SHA256:
            raise ValueError(
                "memory-ops layout self-test OS checksum mismatch: "
                f"expected {MEMOPS_LAYOUT_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with retained memory-test BSS and no buffer operations"
    elif memops_one_buffer_layout_selftest:
        test_os = (repo / MEMOPS_ONE_BUFFER_LAYOUT_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"one-buffer layout self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_ONE_BUFFER_LAYOUT_SELFTEST_OS_SHA256:
            raise ValueError(
                "one-buffer layout self-test OS checksum mismatch: "
                f"expected {MEMOPS_ONE_BUFFER_LAYOUT_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with one retained memory-test BSS buffer"
    elif memops_no_buffer_layout_selftest:
        test_os = (repo / MEMOPS_NO_BUFFER_LAYOUT_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"no-buffer layout self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_NO_BUFFER_LAYOUT_SELFTEST_OS_SHA256:
            raise ValueError(
                "no-buffer layout self-test OS checksum mismatch: "
                f"expected {MEMOPS_NO_BUFFER_LAYOUT_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with memory-test call path and no extra BSS buffers"
    elif memops_status_no_newline_selftest:
        test_os = (repo / MEMOPS_STATUS_NO_NEWLINE_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"status-no-newline self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_STATUS_NO_NEWLINE_SELFTEST_OS_SHA256:
            raise ValueError(
                "status-no-newline self-test OS checksum mismatch: "
                f"expected {MEMOPS_STATUS_NO_NEWLINE_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with memory-test label and colored OK but no newline"
    elif memops_label_only_selftest:
        test_os = (repo / MEMOPS_LABEL_ONLY_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"label-only self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_LABEL_ONLY_SELFTEST_OS_SHA256:
            raise ValueError(
                "label-only self-test OS checksum mismatch: "
                f"expected {MEMOPS_LABEL_ONLY_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with forced memory-test call and label-only terminal output"
    elif memops_silent_call_selftest:
        test_os = (repo / MEMOPS_SILENT_CALL_SELFTEST_OS).resolve()
        if not test_os.is_file():
            raise ValueError(f"silent-call self-test OS does not exist: {test_os}")
        test_os_sha256 = sha256(test_os)
        if test_os_sha256 != MEMOPS_SILENT_CALL_SELFTEST_OS_SHA256:
            raise ValueError(
                "silent-call self-test OS checksum mismatch: "
                f"expected {MEMOPS_SILENT_CALL_SELFTEST_OS_SHA256}, got {test_os_sha256}"
            )
        os_binary_data = test_os.read_bytes()
        os_profile = "safe-memset OS with one forced silent memory-test call"
    runtime_bitstream = verify_runtime_file(runtime, manifest, f"pocket/{VARIANT}.rbf_r")
    bitstream_data = runtime_bitstream.read_bytes()
    bitstream_profile = "manifest os25"
    native_bitstream_sha256 = None
    if (memset_fix_candidate or memops_selftest or memcpy_selftest or
            memcpy_small_selftest or memcpy_harness_selftest or
            memops_layout_selftest or memops_one_buffer_layout_selftest or
            memops_no_buffer_layout_selftest or memops_silent_call_selftest or
            memops_label_only_selftest or memops_status_no_newline_selftest):
        native_bitstream = (repo / SAFE_MEMSET_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"safe-memset native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != SAFE_MEMSET_RBF_SHA256:
            raise ValueError(
                "safe-memset native RBF checksum mismatch: "
                f"expected {SAFE_MEMSET_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = (
            "rpcmp stock 32KiB/128KiB caches + safe-memset boot ROM at 90MHz; "
            "no JT51/MMIO overlay"
        )
    elif integrated_rbf:
        native_bitstream = (repo / INTEGRATED_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"integrated native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != INTEGRATED_RBF_SHA256:
            raise ValueError(
                "integrated native RBF checksum mismatch: "
                f"expected {INTEGRATED_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = "rpcmp 16KiB/32KiB + queue + JT51 + boot ROM at 90MHz"
    elif boot_isolation_rbf:
        native_bitstream = (repo / BOOT_ISOLATION_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"boot-isolation native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != BOOT_ISOLATION_RBF_SHA256:
            raise ValueError(
                "boot-isolation native RBF checksum mismatch: "
                f"expected {BOOT_ISOLATION_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = "rpcmp 16KiB/32KiB + boot ROM at 90MHz; no JT51/MMIO overlay"
    elif (
        clock_isolation_rbf
        or early_os_marker
        or early_init_marker
        or pre_early_init_marker
        or pre_video_marker
        or post_video_marker
        or post_palette_marker
        or first_palette_marker
        or post_terminal_marker
        or synced_palette_marker
        or terminal_mode_marker
        or term_clear_no_flush_marker
        or term_clear_internal_marker
        or term_state_marker
        or term_chars_marker
        or term_chars_volatile_marker
        or term_chars_word_loop_marker
    ):
        native_bitstream = (repo / CLOCK_ISOLATION_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"clock-isolation native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != CLOCK_ISOLATION_RBF_SHA256:
            raise ValueError(
                "clock-isolation native RBF checksum mismatch: "
                f"expected {CLOCK_ISOLATION_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = "rpcmp stock 32KiB/128KiB caches + boot ROM at 90MHz; no JT51/MMIO overlay"
    elif frequency_control_rbf:
        native_bitstream = (repo / FREQUENCY_CONTROL_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"frequency-control native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != FREQUENCY_CONTROL_RBF_SHA256:
            raise ValueError(
                "frequency-control native RBF checksum mismatch: "
                f"expected {FREQUENCY_CONTROL_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = "rpcmp stock 32KiB/128KiB caches + boot ROM at 100MHz; no JT51/MMIO overlay"
    elif local_stock_rbf:
        native_bitstream = (repo / LOCAL_STOCK_RBF).resolve()
        if not native_bitstream.is_file():
            raise ValueError(f"local-stock native RBF does not exist: {native_bitstream}")
        native_bitstream_sha256 = sha256(native_bitstream)
        if native_bitstream_sha256 != LOCAL_STOCK_RBF_SHA256:
            raise ValueError(
                "local-stock native RBF checksum mismatch: "
                f"expected {LOCAL_STOCK_RBF_SHA256}, got {native_bitstream_sha256}"
            )
        bitstream_data = reverse_rbf_bits(native_bitstream.read_bytes())
        bitstream_profile = "locally rebuilt manifest-revision os25 with repaired boot ROM"
    if not elf.is_file():
        raise ValueError(f"probe ELF does not exist: {elf}")

    build_root = (repo / "out" / "build").resolve()
    output = output.resolve()
    archive = archive.resolve()
    expected_output = build_root / "pocket-openfpgaos-package"
    expected_archive = build_root / "rpcmp-openfpgaos-probe.zip"
    if output != expected_output or archive != expected_archive:
        raise ValueError("package outputs must use the dedicated paths under out/build")

    with tempfile.TemporaryDirectory(dir=build_root, prefix="rpcmp-package-") as temporary:
        staging = Path(temporary)
        core_root = PurePosixPath("Cores") / CORE_ID
        for name, value in definitions().items():
            write_file(staging, core_root / name, json_bytes(value))
        write_file(staging, core_root / "loader.bin", loader.read_bytes())
        write_file(staging, core_root / f"{VARIANT}.rbf_r", bitstream_data)

        common_root = PurePosixPath("Assets") / PLATFORM_ID / "common"
        write_file(staging, common_root / "os.bin", os_binary_data)
        write_file(staging, common_root / "rpcmp-probe.elf", elf.read_bytes())
        write_file(staging, common_root / "rpcmp-probe.ini", b"[os]\nELF=rpcmp-probe.elf\nARGS=\nVARIANT=os25\n")
        synthetic = bytes((offset * 37 + 11) % 256 for offset in range(4096))
        write_file(staging, common_root / "synthetic.bin", synthetic)

        instance = {
            "instance": {
                "magic": MAGIC,
                "data_slots": [
                    {"id": 1, "filename": "os.bin"},
                    {"id": 2, "filename": "rpcmp-probe.ini"},
                    {"id": 3, "filename": "rpcmp-probe.elf"},
                    {"id": 4, "filename": "synthetic.bin"},
                ],
            }
        }
        write_file(staging, PurePosixPath("Assets") / PLATFORM_ID / CORE_ID / "rpcmp-probe.json", json_bytes(instance))
        platform = {"platform": {"category": "Computer", "name": "RPCMP Probe", "year": 2026, "manufacturer": "RPCMP"}}
        write_file(staging, PurePosixPath("Platforms") / f"{PLATFORM_ID}.json", json_bytes(platform))
        files = verify_tree(staging)

        if output.exists():
            if output.is_symlink():
                raise ValueError("refusing to replace a symlinked output directory")
            shutil.rmtree(output)
        shutil.copytree(staging, output)

    archive.parent.mkdir(parents=True, exist_ok=True)
    archive.unlink(missing_ok=True)
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for source in verify_tree(output):
            relative = source.relative_to(output).as_posix()
            info = zipfile.ZipInfo(relative, date_time=(2026, 8, 26, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, source.read_bytes())

    evidence = {
        "schema_version": 1,
        "distribution_status": "local experiment only; redistribution not approved",
        "sdk_revision": SDK_REVISION,
        "runtime_revision": RUNTIME_REVISION,
        "bitstream_profile": bitstream_profile,
        "os_profile": os_profile,
        "native_bitstream_sha256": native_bitstream_sha256,
        "artifacts": {
            path.relative_to(output).as_posix(): {"bytes": path.stat().st_size, "sha256": sha256(path)}
            for path in verify_tree(output)
        },
        "zip": {"bytes": archive.stat().st_size, "sha256": sha256(archive)},
    }
    evidence_path = archive.with_suffix(".evidence.json")
    evidence_path.write_bytes(json_bytes(evidence))
    print(f"package={output}")
    print(f"zip={archive}")
    print(f"zip_sha256={evidence['zip']['sha256']}")
    print("result=PASS")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--zip", dest="archive", type=Path, required=True)
    profiles = parser.add_mutually_exclusive_group()
    profiles.add_argument(
        "--integrated-rbf",
        action="store_true",
        help="package the checksum-pinned 90 MHz reduced-cache JT51 research fit",
    )
    profiles.add_argument(
        "--boot-isolation-rbf",
        action="store_true",
        help="package the checksum-pinned 90 MHz reduced-cache fit without JT51",
    )
    profiles.add_argument(
        "--clock-isolation-rbf",
        action="store_true",
        help="package the checksum-pinned 90 MHz stock-cache fit without JT51",
    )
    profiles.add_argument(
        "--frequency-control-rbf",
        action="store_true",
        help="package the checksum-pinned 100 MHz stock-cache fit without JT51",
    )
    profiles.add_argument(
        "--local-stock-rbf",
        action="store_true",
        help="package the checksum-pinned local rebuild of the stock os25 fit",
    )
    profiles.add_argument(
        "--early-os-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a checksum-pinned early OS marker",
    )
    profiles.add_argument(
        "--early-init-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker after early HAL initialization",
    )
    profiles.add_argument(
        "--pre-early-init-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker before early HAL initialization",
    )
    profiles.add_argument(
        "--pre-video-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker before video initialization",
    )
    profiles.add_argument(
        "--post-video-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a three-buffer marker after video initialization",
    )
    profiles.add_argument(
        "--post-palette-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker after terminal palette initialization",
    )
    profiles.add_argument(
        "--first-palette-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker after the first terminal palette entry",
    )
    profiles.add_argument(
        "--post-terminal-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a synchronized marker after terminal initialization",
    )
    profiles.add_argument(
        "--synced-palette-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a synchronized marker after the 16-color palette loop",
    )
    profiles.add_argument(
        "--terminal-mode-marker",
        action="store_true",
        help="package the 90 MHz stock-cache fit with a marker after terminal display-mode switching",
    )
    profiles.add_argument(
        "--term-clear-no-flush-marker",
        action="store_true",
        help="package the terminal-clear diagnostic that omits only the framebuffer cache flush",
    )
    profiles.add_argument(
        "--term-clear-internal-marker",
        action="store_true",
        help="package the terminal-clear diagnostic that touches only internal terminal buffers",
    )
    profiles.add_argument(
        "--term-state-marker",
        action="store_true",
        help="package the terminal-clear diagnostic that performs only terminal state stores",
    )
    profiles.add_argument(
        "--term-chars-marker",
        action="store_true",
        help="package the terminal-clear diagnostic that clears only the character buffer",
    )
    profiles.add_argument(
        "--term-chars-volatile-marker",
        action="store_true",
        help="package the character-buffer diagnostic that uses a volatile byte loop",
    )
    profiles.add_argument(
        "--term-chars-word-loop-marker",
        action="store_true",
        help="package the character-buffer diagnostic that uses a volatile word loop",
    )
    profiles.add_argument(
        "--memset-fix-candidate",
        action="store_true",
        help="package the normal OS and matching RBF with the safe memset implementation",
    )
    profiles.add_argument(
        "--memops-selftest",
        action="store_true",
        help="package the safe-memset OS with the cached-SDRAM memory-operations self-test",
    )
    profiles.add_argument(
        "--memcpy-selftest",
        action="store_true",
        help="package the safe-memset OS with the cached-SDRAM memcpy-only self-test",
    )
    profiles.add_argument(
        "--memcpy-small-selftest",
        action="store_true",
        help="package the safe-memset OS with the cached-SDRAM memcpy 0..31-byte self-test",
    )
    profiles.add_argument(
        "--memcpy-harness-selftest",
        action="store_true",
        help="package the memcpy test harness without invoking memcpy",
    )
    profiles.add_argument(
        "--memops-layout-selftest",
        action="store_true",
        help="package retained memory-test BSS without running buffer operations",
    )
    profiles.add_argument(
        "--memops-one-buffer-layout-selftest",
        action="store_true",
        help="package one retained memory-test BSS buffer without buffer operations",
    )
    profiles.add_argument(
        "--memops-no-buffer-layout-selftest",
        action="store_true",
        help="package the memory-test call path without extra BSS buffers",
    )
    profiles.add_argument(
        "--memops-silent-call-selftest",
        action="store_true",
        help="package one forced memory-test call without terminal/status output",
    )
    profiles.add_argument(
        "--memops-label-only-selftest",
        action="store_true",
        help="package one forced memory-test call with its terminal label only",
    )
    profiles.add_argument(
        "--memops-status-no-newline-selftest",
        action="store_true",
        help="package the memory-test label and colored OK without its newline",
    )
    args = parser.parse_args()
    build(
        args.repo.resolve(),
        args.sdk.resolve(),
        args.elf.resolve(),
        args.output,
        args.archive,
        args.integrated_rbf,
        args.boot_isolation_rbf,
        args.clock_isolation_rbf,
        args.frequency_control_rbf,
        args.local_stock_rbf,
        args.early_os_marker,
        args.early_init_marker,
        args.pre_early_init_marker,
        args.pre_video_marker,
        args.post_video_marker,
        args.post_palette_marker,
        args.first_palette_marker,
        args.post_terminal_marker,
        args.synced_palette_marker,
        args.terminal_mode_marker,
        args.term_clear_no_flush_marker,
        args.term_clear_internal_marker,
        args.term_state_marker,
        args.term_chars_marker,
        args.term_chars_volatile_marker,
        args.term_chars_word_loop_marker,
        args.memset_fix_candidate,
        args.memops_selftest,
        args.memcpy_selftest,
        args.memcpy_small_selftest,
        args.memcpy_harness_selftest,
        args.memops_layout_selftest,
        args.memops_one_buffer_layout_selftest,
        args.memops_no_buffer_layout_selftest,
        args.memops_silent_call_selftest,
        args.memops_label_only_selftest,
        args.memops_status_no_newline_selftest,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
