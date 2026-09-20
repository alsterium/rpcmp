"""Create and verify HYB1 ROM MIF without the upstream hexdump dependency."""
import argparse
from pathlib import Path
import struct

from mdx_hybrid_probe import ROOT
from pocket_firmware_pair import verify


def images(folder):
    folder = folder.resolve()
    if not folder.is_relative_to(ROOT / "out"):
        raise ValueError("generated firmware must stay under out")
    boot = (folder / "boot.bin").read_bytes()
    if not boot or len(boot) % 4 or len(boot) > 0x4000:
        raise ValueError("invalid reserved-ROM size/alignment")
    words = [word for word, in struct.iter_unpack("<I", boot)]
    text = "WIDTH=32;\nDEPTH=8192;\nADDRESS_RADIX=DEC;\nDATA_RADIX=HEX;\nCONTENT BEGIN\n"
    text += "".join(f"{i} : {word:08X};\n" for i, word in enumerate(words))
    text += f"[{len(words)}..8191] : 00000013;\nEND;\n"
    mif = folder / "firmware.mif"
    mif.write_text(text, encoding="ascii")
    return verify(folder / "firmware.elf", mif, folder / "os.bin")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path)
    print(images(parser.parse_args().folder))
