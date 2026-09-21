"""Offline MXDRV/PCM8 + native JT51 experiment. Linux, little endian, local inputs only."""
import argparse
import array
import ctypes as c
import hashlib
import json
import math
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import wave


ROOT = Path(__file__).resolve().parents[1]
REFERENCE = "4076b91c7ced57bf6047f69b87c12a34bd99a438"
JT51 = "985a573dcfc1ff135553a39f7eae21d18ba57cbe"
UNITS = ("fmgen/fmgen.cpp", "fmgen/fmtimer.cpp", "fmgen/opm.cpp",
         "pcm8/pcm8.cpp", "pcm8/x68pcm8.cpp", "downsample/downsample.cpp")

HOOKS = r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
static FILE *events, *pcm, *fm, *chunks;
static uint64_t pcm_count;
static void save(FILE *file, const void *p, size_t n) {
    if (!file || fwrite(p, 1, n, file) != n) abort();
}
extern "C" void probe_open(const char *prefix) {
    char name[4096];
    FILE **files[] = {&events, &pcm, &fm, &chunks};
    const char *suffix[] = {"events", "pcm", "fm", "chunks"};
    for (int i=0; i<4; ++i) {
        if (snprintf(name, sizeof(name), "%s.%s", prefix, suffix[i]) >= sizeof(name)) abort();
        *files[i] = fopen(name, "wb");
        if (!*files[i]) abort();
    }
}
static void probe_event(uint32_t address, uint32_t value) {
    uint64_t at = pcm_count / 2;
    save(events, &at, 8); save(events, &address, 4); save(events, &value, 4);
}
extern "C" void probe_value(int32_t value, int16_t original_fm) {
    save(pcm, &value, 4); save(fm, &original_fm, 2); ++pcm_count;
}
static void probe_chunk(uint32_t inner, uint32_t outer) {
    save(chunks, &inner, 4); save(chunks, &outer, 4);
}
extern "C" void probe_close() {
    if (fclose(events) || fclose(pcm) || fclose(fm) || fclose(chunks)) abort();
}
#define MXDRVG_EXPORT
#define MXDRVG_CALLBACK
volatile unsigned char OpmReg1B;
#include "jni/mxdrvg/mxdrvg_core.h"
extern "C" unsigned probe_error() { return G.FATALERROR; }
extern "C" unsigned probe_loaded() { return (G.L002230 ? 1 : 0) | (G.L002231 ? 2 : 0); }
static X68K::DOWNSAMPLE converter;
extern "C" void probe_convert_reset() { converter.Init(62500, 48000, true); }
extern "C" void probe_convert(int16_t *in, int count, int16_t *out) {
    converter.DownSample(in, count, out);
}
static unsigned char authored_pcm[65536];
static void reg(int a, int v) { probe_event(a, v); OPM.SetReg(a, v); }
extern "C" void probe_keys(int channels, int on) {
    for (int ch=0; ch<channels; ++ch) reg(8, (on ? 0x78 : 0) | ch);
}
extern "C" void probe_authored(int fm_channels, int pcm_channels, int kind, int level) {
    for (int ch=0; ch<fm_channels; ++ch) {
        reg(0x20+ch, 0xc7); reg(0x28+ch, 0x30+ch);
        for (int op=0; op<4; ++op) {
            int a = op*8+ch;
            reg(0x40+a, 1+op); reg(0x60+a, level); reg(0x80+a, 31);
            reg(0xa0+a, 0); reg(0xc0+a, 0); reg(0xe0+a, 15);
        }
    }
    probe_keys(fm_channels, 1);
    for (unsigned i=0; i<sizeof(authored_pcm); ++i) {
        if (kind == 4) authored_pcm[i] = (i & 32) ? 0x99 : 0x11;
        else if (kind == 5) {
            int v = (int((i/2)%64)-32)*64;
            authored_pcm[i] = (i & 1) ? v & 255 : (v >> 8) & 255;
        } else authored_pcm[i] = (i%64)-32;
    }
    for (int ch=0; ch<pcm_channels; ++ch)
        PCM8.Out(ch, authored_pcm, (2<<16) | (kind<<8) | 3, sizeof(authored_pcm));
}
'''


def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)


def checked_replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError("reference instrumentation site changed")
    return text.replace(old, new)


def build(args):
    for path, revision in ((args.reference, REFERENCE), (args.jt51, JT51)):
        actual = run(["git", "-C", path, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        # Windows checkouts may have CRLF text on the Linux bind mount. Apply
        # Git's normal text conversion for this read-only check, not a diff waiver.
        dirty = run(["git", "-c", "core.autocrlf=true", "-C", path, "status", "--porcelain"],
                    capture_output=True, text=True).stdout
        if actual != revision or dirty:
            raise ValueError("reference checkout is unpinned or modified")
    src = args.out / "jni"
    shutil.copytree(args.reference / "gamdx/jni", src, dirs_exist_ok=True)
    path = src / "mxdrvg/mxdrvg_core.h"
    text = path.read_text()
    line = "\tOPM.SetReg( (UBYTE)D1, (UBYTE)D2 );"
    text = checked_replace(text, line, "\tprobe_event((UBYTE)D1, (UBYTE)D2);\n" + line)
    line = "\t\t\tOPM.Mix(innerbuf, create_len2);"
    text = checked_replace(text, line, "#ifndef PROBE_SKIP_FM\n" + line + "\n#endif")
    line = "\t\t\tDS.DownSample(innerbuf, create_len, outerbuf);"
    text = checked_replace(text, line, "\t\t\tprobe_chunk(create_len2, create_len);\n" + line)
    path.write_text(text)
    path = src / "pcm8/x68pcm8.h"
    line = "\tinline void StoreSample(Sample& dest, ISample data)\n\t{"
    text = checked_replace(path.read_text(), line, line + "\n\t\t::probe_value(data, dest);")
    path.write_text('#include <cstdint>\nextern "C" void probe_value(int32_t, int16_t);\n' + text)
    wrapper = args.out / "capture.cpp"
    wrapper.write_text(HOOKS)
    run(["g++", "-std=c++17", "-O2", "-fPIC", "-shared", "-fsigned-char", "-include", "cstdint",
         wrapper, *[src / p for p in UNITS], "-o", args.out / "capture.so"])
    run(["g++", "-std=c++17", "-O2", "-fPIC", "-shared", "-fsigned-char", "-include", "cstdint",
         "-DPROBE_SKIP_FM", wrapper, *[src / p for p in UNITS], "-o", args.out / "split.so"])
    run(["gcc", "-O2", "-fPIC", "-shared", args.reference / "classes/objc/lzx042.c",
         "-o", args.out / "lzx.so"])
    sources = prepare_jt51_wide(args.jt51, args.out / "jt51-wide")
    # Upstream width warnings remain visible; own C++ warnings are errors.
    run(["verilator", "--cc", "--exe", "--build", "-j", "2", "-Wno-fatal", "--top-module", "jt51",
         "--Mdir", args.out / "obj", *sources, ROOT / "tools/mdx_hybrid_replay.cpp",
         "-CFLAGS", "-O2 -std=c++17 -Wall -Wextra -Werror"])


def prepare_jt51_wide(source, wide):
    """Add pre-limit output taps and deterministic initial accumulator state."""
    source, wide = Path(source).resolve(), Path(wide).resolve()
    if not wide.is_relative_to(ROOT / "out"):
        raise ValueError("generated JT51 must stay under out")
    revision = run(["git", "-C", source, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = run(["git", "-c", "core.autocrlf=true", "-C", source, "status", "--porcelain"],
                capture_output=True, text=True).stdout
    if revision != JT51 or dirty:
        raise ValueError("JT51 checkout is unpinned or modified")
    wide.mkdir(parents=True, exist_ok=True)
    for name in ("jt51.v", "jt51_acc.v"):
        shutil.copyfile(source / "hdl" / name, wide / name)
    widen_jt51(wide)
    return [wide / name if name in ("jt51.v", "jt51_acc.v") else source / "hdl" / name
            for name in (source / "hdl/jt51.f").read_text().split()]


def prepare_jt51_paused(source, output):
    """Compose the reviewed hold recipe and wide taps for the HYB3 sound owner."""
    from jt51_hold_prepare import prepare
    source, output = Path(source).resolve(), Path(output).resolve()
    if not output.is_relative_to(ROOT / "out") or output == ROOT / "out":
        raise ValueError("generated JT51 must stay under an out subdirectory")
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output) as temp:
        staged = Path(temp) / "hdl"
        manifest = prepare(source, staged)
        widen_jt51(staged)
        manifest["purpose"] = "HYB3 hold and wide mixed-audio output"
        manifest["wide_recipe_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        for record in manifest["files"]:
            path = staged / record["file"]
            record["generated_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            shutil.copyfile(path, output / path.name)
        shutil.copyfile(staged / "LICENSE", output / "LICENSE")
        (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    prepared = {record["file"] for record in manifest["files"]}
    return [output / name if name in prepared else source / "hdl" / name
            for name in (source / "hdl/jt51.f").read_text().split()]


def widen_jt51(wide):
    path = wide / "jt51.v"
    text = checked_replace(path.read_text(), "    output  signed  [15:0] xright\n",
                           "    output  signed  [15:0] xright,\n"
                           "    output signed [18:0] wide_left, wide_right\n")
    text = checked_replace(text, "    .xright     ( xright        )",
                           "    .xright     ( xright        ),\n"
                           "    .wide_left(wide_left), .wide_right(wide_right)")
    path.write_text(text)
    path = wide / "jt51_acc.v"
    text = checked_replace(path.read_text(), "    output  reg signed  [15:0]  xright\n",
                           "    output  reg signed  [15:0]  xright,\n"
                           "    output reg signed [18:0] wide_left, wide_right\n")
    text = checked_replace(text, "    if( rst ) begin\n        sum_all <= 1'b0;",
                           "    if( rst ) begin\n        sum_all <= 1'b0;\n"
                           "        pre_left <= 0; pre_right <= 0;\n"
                           "        wide_left <= 0; wide_right <= 0;")
    text = checked_replace(text, "            xleft  <= lim16(pre_left);",
                           "            wide_left <= pre_left; wide_right <= pre_right;\n"
                           "            xleft  <= lim16(pre_left);")
    path.write_text(text)


def load_library(out, split=False):
    lib = c.CDLL(str(out / ("split.so" if split else "capture.so")))
    signatures = {
        "probe_open": ([c.c_char_p], None), "probe_close": ([], None),
        "probe_convert_reset": ([], None),
        "probe_convert": ([c.c_void_p, c.c_int, c.c_void_p], None),
        "probe_authored": ([c.c_int] * 4, None), "probe_keys": ([c.c_int] * 2, None),
        "probe_loaded": ([], c.c_uint32), "probe_error": ([], c.c_uint32),
        "MXDRVG_Start": ([c.c_int] * 4, c.c_int), "MXDRVG_TotalVolume": ([c.c_int], None),
        "MXDRVG_SetData": ([c.c_void_p, c.c_uint32, c.c_void_p, c.c_uint32], None),
        "MXDRVG_PlayAt": ([c.c_uint32, c.c_int, c.c_int], None),
        "MXDRVG_GetPCM": ([c.c_void_p, c.c_int], c.c_int), "MXDRVG_End": ([], None),
    }
    for name, (parameters, result) in signatures.items():
        function = getattr(lib, name)
        function.argtypes, function.restype = parameters, result
    return lib


def unpack(data, out):
    if data[4:8] != b"LZX ":
        return data
    if len(data) < 46:
        raise ValueError("short LZX header")
    size = int.from_bytes(data[18:22], "big")
    if not 0 < size <= 16 * 1024 * 1024:
        raise ValueError("LZX size outside probe bound")
    decoder = c.CDLL(str(out / "lzx.so"))
    decoder.lzx042decode.argtypes = [c.c_void_p, c.c_uint32, c.c_void_p, c.c_uint32]
    decoder.lzx042decode.restype = c.c_uint32
    source, target = c.create_string_buffer(data + bytes(16)), c.create_string_buffer(size)
    if decoder.lzx042decode(target, size, source, len(data)) != size:
        raise ValueError("LZX decode size mismatch")
    return target.raw


def capture(args, case):
    # Legacy reference executes in its own time/memory-bounded child process.
    import resource
    resource.setrlimit(resource.RLIMIT_CPU, (30, 30))
    resource.setrlimit(resource.RLIMIT_AS, (512 * 1024 * 1024,) * 2)
    lib = load_library(args.out, args.split)
    prefix = args.out / (case["id"] + ("-split" if args.split else ""))
    lib.probe_open(str(prefix).encode())
    if lib.MXDRVG_Start(48000, 0, 65536, 1048576):
        raise ValueError("reference start failed")
    lib.MXDRVG_TotalVolume(256)
    if "authored" in case:
        lib.probe_authored(*case["authored"], case.get("level", 48))
    else:
        raw = Path(case["mdx"]).read_bytes()
        end = raw.index(b"\0", raw.index(b"\x1a", raw.index(b"\r\n")))
        pdx = unpack(Path(case["pdx"]).read_bytes(), args.out) if case.get("pdx") else None
        mdx = bytes.fromhex("00000000000a00080000" if pdx is not None else "0000ffff000a00080000")
        mdx += unpack(raw[end + 1:], args.out)
        pdx = bytes.fromhex("00000000000a00020000") + pdx if pdx is not None else None
        if len(mdx) > 65536 - 16 or len(pdx or b"") > 1048576 - 16:
            raise ValueError("file outside reference allocation/guard")
        a = c.create_string_buffer(mdx + bytes(16))
        b = c.create_string_buffer(pdx + bytes(16)) if pdx is not None else None
        lib.MXDRVG_SetData(a, len(mdx), b, len(pdx or b""))
        if lib.probe_loaded() != (3 if pdx is not None else 1):
            raise ValueError("reference load flags")
        lib.MXDRVG_PlayAt(0, 2, 1)
    output = (c.c_int16 * (2048 + 64))(*([23130] * (2048 + 64)))
    ptr = c.cast(c.byref(output, 64), c.c_void_p)
    with prefix.with_suffix(".reference").open("wb") as file:
        for block in range(case["blocks"]):
            if "authored" in case and block in (32, 64):
                lib.probe_keys(case["authored"][0], int(block == 64))
            if lib.MXDRVG_GetPCM(ptr, 1024) != 1024 or lib.probe_error():
                raise ValueError("reference render failure")
            if any(x != 23130 for x in [*output[:32], *output[-32:]]):
                raise ValueError("reference output canary changed")
            file.write(c.string_at(ptr, 4096))
    lib.MXDRVG_End()
    lib.probe_close()


def samples(path, kind):
    data = array.array(kind)
    data.frombytes(path.read_bytes())
    return data


def clip(value):
    return min(32767, max(-32768, value))


def convert(lib, values, chunks):
    lib.probe_convert_reset()
    result = bytearray()
    offset = 0
    phase = 0
    for inner, outer in chunks:
        if (not 0 < inner <= 2048 or not 0 < outer <= 1024 or
                inner != (phase + outer * 62500) // 48000 or offset + inner * 2 > len(values)):
            raise ValueError("invalid chunk")
        phase = (phase + outer * 62500) % 48000
        source = (c.c_int16 * (inner * 2))(*values[offset:offset + inner * 2])
        target = (c.c_int16 * (outer * 2))()
        lib.probe_convert(source, outer, target)
        result.extend(bytes(target))
        offset += inner * 2
    if offset != len(values):
        raise ValueError("chunk length mismatch")
    return bytes(result)


def analyze(args, case):
    prefix = args.out / case["id"]
    pcm, fm = samples(prefix.with_suffix(".pcm"), "i"), samples(prefix.with_suffix(".fm"), "h")
    candidate = args.out / (case["id"] + "-split")
    for suffix in (".events", ".pcm", ".chunks"):
        if prefix.with_suffix(suffix).read_bytes() != candidate.with_suffix(suffix).read_bytes():
            raise ValueError("omitting FM synthesis changed timed events/PCM/chunks")
    if len(pcm) != len(fm) or len(pcm) % 2:
        raise ValueError("capture length mismatch")
    replay = run([args.out / "obj/Vjt51", prefix.with_suffix(".events"), len(pcm) // 2,
                  prefix.with_suffix(".jt"), prefix.with_suffix(".wide")],
                 capture_output=True, text=True, timeout=180)
    metrics = json.loads(replay.stdout)
    jt = samples(prefix.with_suffix(".jt"), "h")
    wide = samples(prefix.with_suffix(".wide"), "i")
    events = struct.iter_unpack("<QII", prefix.with_suffix(".events").read_bytes())
    expected_writes = sum(at < len(pcm) // 2 for at, _, _ in events)
    if len(jt) != len(pcm) or len(wide) != len(pcm) or metrics["writes"] != expected_writes:
        raise ValueError("native replay lost samples or writes")
    chunks = list(struct.iter_unpack("<II", prefix.with_suffix(".chunks").read_bytes()))
    lib = load_library(args.out)
    rebuilt = convert(lib, [clip(a + b) for a, b in zip(fm, pcm)], chunks)
    reference = prefix.with_suffix(".reference").read_bytes()
    if rebuilt != reference:
        raise ValueError("captured contributions do not reconstruct software oracle")
    if case.get("reference_sha256") and hashlib.sha256(reference).hexdigest() != case["reference_sha256"]:
        raise ValueError("instrumented reference differs from prior unmodified oracle")
    pcm = samples(candidate.with_suffix(".pcm"), "i")
    # Follow FMGEN's 17-bit limiter, gain, int16 FM sample, then wide PCM sum.
    scaled_fm = [clip((min(65535, max(-65536, a)) * args.fm_gain) // 16384) for a in wide]
    mixed = [clip(a + b) for a, b in zip(scaled_fm, pcm)]
    audio = convert(lib, mixed, chunks)
    with wave.open(str(prefix.with_suffix(".wav")), "wb") as file:
        file.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        file.writeframes(audio)
    metrics.update(id=case["id"], oracle_reconstructed=True, split_streams_identical=True,
                   fm_gain_q14=args.fm_gain,
                   pcm_peak=max(map(abs, pcm)), fm_reference_peak=max(map(abs, fm)),
                   jt51_peak=max(map(abs, jt)), wide_fm_peak=max(map(abs, wide)),
                   scaled_fm_peak=max(map(abs, scaled_fm)), mixed_peak=max(map(abs, mixed)),
                   clipped_samples=sum(not -32768 <= a + b <= 32767 for a, b in zip(scaled_fm, pcm)),
                   fm_reference_rms=math.sqrt(sum(x*x for x in fm) / len(fm)),
                   jt51_rms=math.sqrt(sum(x*x for x in jt) / len(jt)),
                   scaled_fm_rms=math.sqrt(sum(x*x for x in scaled_fm) / len(scaled_fm)),
                   early_pcm_clip_changes=sum(clip(a + b) != clip(a + clip(b))
                                              for a, b in zip(scaled_fm, pcm)),
                   audio_sha256=hashlib.sha256(audio).hexdigest())
    if "authored" in case:
        f, p, _ = case["authored"]
        if not f and (any(jt) or any(wide) or audio != reference):
            raise ValueError("silence/PCM-only control differs from reference")
        if f and not any(jt):
            raise ValueError("FM control is silent")
        if p and not any(pcm):
            raise ValueError("PCM control is silent")
        if f:
            # Authored key-off at 0.683 s, key-on at 1.365 s, fastest release.
            if any(jt[50000*2:81250*2]) or not any(jt[93750*2:]):
                raise ValueError("authored key-off/re-key-on control failed")
            ratio = metrics["scaled_fm_rms"] / metrics["fm_reference_rms"]
            metrics["calibrated_fm_rms_ratio"] = ratio
            if not 0.95 <= ratio <= 1.05:
                raise ValueError("authored FM RMS differs by more than 5 percent")
    return metrics


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=ROOT / "out/research/mdxplayer-reference-20260921")
    parser.add_argument("--jt51", type=Path, default=ROOT / "out/research/jt51-985a573")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, help="private JSON list of mdx/pdx paths and blocks")
    # FMGEN SetVolume uses pow(10, parameter/40), not parameter/20. Authored
    # one/eight-channel measurements also place raw JT51 at about 2x reference.
    parser.add_argument("--fm-gain", type=int, default=8211, help="experimental Q14 gain")
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--capture", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--split", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    if sys.byteorder != "little" or array.array("i").itemsize != 4 or array.array("h").itemsize != 2:
        raise ValueError("probe requires little-endian int32/int16 host")
    if not args.out.resolve().is_relative_to((ROOT / "out").resolve()):
        raise ValueError("generated/private output must stay under repository out/")
    if not 0 <= args.fm_gain <= 65536:
        raise ValueError("FM gain outside probe bound")
    args.out.mkdir(parents=True, exist_ok=True)
    if args.capture:
        capture(args, json.load(sys.stdin))
        return
    if args.build:
        build(args)
    cases = [{"id": name, "authored": channels, "blocks": 96} for name, channels in (
        ("silence", [0, 0, 4]), ("fm1", [1, 0, 4]), ("fm8", [8, 0, 4]),
        ("adpcm8", [0, 8, 4]), ("pcm16x8", [0, 8, 5]), ("pcm8x8", [0, 8, 6]),
        ("mixed8", [8, 8, 4]))]
    cases.append({"id": "fm8-loud", "authored": [8, 0, 4], "level": 0, "blocks": 96})
    if args.manifest:
        for index, item in enumerate(json.loads(args.manifest.read_text())):
            cases.append({"id": f"private-{index:03d}", "mdx": item["mdx"],
                          "pdx": item.get("pdx"), "blocks": int(item.get("blocks", 480)),
                          "reference_sha256": item.get("reference_sha256")})
    results = []
    for case in cases:
        if not 1 <= case["blocks"] <= 960:
            raise ValueError("prefix outside probe bound")
        run([sys.executable, "-B", __file__, "--out", args.out, "--capture"],
            input=json.dumps(case), text=True, timeout=40)
        run([sys.executable, "-B", __file__, "--out", args.out, "--capture", "--split"],
            input=json.dumps(case), text=True, timeout=40)
        result = analyze(args, case)
        results.append(result)
        # Private identities and audio hashes remain local, not public evidence.
        print(json.dumps({k: v for k, v in result.items() if k != "audio_sha256"}), flush=True)
    (args.out / "results.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
