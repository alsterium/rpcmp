"""Prepare the pinned reference's CPU-only timed FM/wide PCM renderer."""
import argparse
from pathlib import Path
import shutil

from mdx_hybrid_probe import REFERENCE, ROOT, checked_replace, run


def prepare(source, output):
    source, output = source.resolve(), output.resolve()
    if not output.is_relative_to(ROOT / "out") or output.is_relative_to(source):
        raise ValueError("renderer output must be under out and outside its source")
    revision = run(["git", "-C", source, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    dirty = run(["git", "-c", "core.autocrlf=true", "-C", source, "status", "--porcelain"],
                capture_output=True, text=True).stdout
    if revision != REFERENCE or dirty:
        raise ValueError("reference checkout is unpinned or modified")
    jni = output / "jni"
    shutil.copytree(source / "gamdx/jni", jni, dirs_exist_ok=True)
    header = jni / "mxdrvg/mxdrvg_core.h"
    text = header.read_text(encoding="utf-8")
    line = "\tOPM.SetReg( (UBYTE)D1, (UBYTE)D2 );"
    text = checked_replace(text, line, "\thybrid_capture_event((UBYTE)D1, (UBYTE)D2);\n" + line)
    text = checked_replace(text, "\t\t\tOPM.Mix(innerbuf, create_len2);", "\t\t\t// HYB1 synthesizes FM in hardware.")
    text = checked_replace(text, "DisposeStack_L00122e = NULL;", "DisposeStack_L00122e = 0;")
    header.write_text(text, encoding="utf-8")
    header = jni / "pcm8/x68pcm8.h"
    line = "\tinline void StoreSample(Sample& dest, ISample data)\n\t{"
    text = checked_replace(header.read_text(encoding="utf-8"), line, line + "\n\t\t::rpcmp_hybrid_pcm(data);")
    header.write_text('#include <stdint.h>\nextern "C" void rpcmp_hybrid_pcm(int32_t);\n' + text, encoding="utf-8")
    print(jni)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, default=ROOT / "out/research/mdxplayer-reference-20260921")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    prepare(args.reference, args.output)
