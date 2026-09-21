"""Optional hybrid tool/ROM checks; run with a native C compiler available."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import hybrid_renderer_prepare as renderer
import pocket_hybrid_firmware_prepare as firmware
import pocket_hybrid_images as images
from pocket_firmware_pair import mif_bytes


class HybridTools(unittest.TestCase):
    def test_reference_scope_and_pin(self):
        with patch.object(renderer, "run") as run:
            with self.assertRaises(ValueError):
                renderer.prepare(ROOT / "source", ROOT / "core/generated")
            run.assert_not_called()
        for revision, dirty in (("wrong", ""), (renderer.REFERENCE, " M file")):
            with patch.object(renderer, "run", side_effect=[SimpleNamespace(stdout=revision),
                                                           SimpleNamespace(stdout=dirty)]):
                with self.assertRaises(ValueError):
                    renderer.prepare(ROOT / "source", ROOT / "out/missing-reference-test")

    def test_firmware_profile_preserves_pair_provenance(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "out") as temp:
            root = Path(temp)
            overlay = root / "overlays/openfpgaos"
            overlay.mkdir(parents=True)
            shutil.copyfile(ROOT / "overlays/openfpgaos/boot_hybrid_sound_reset.inc",
                            overlay / "boot_hybrid_sound_reset.inc")
            output = root / "out/build/probe"
            def baseline(*args):
                (output / "src/firmware/os/targets/pocket/boot").mkdir(parents=True)
                (output / "rpcmp-firmware-inputs.json").write_text(json.dumps({
                    "profile": "m6-player", "m6_sound_source": "old", "prepared_caps_sha256": "caps",
                    "source_revision": "verified", "boot_overlay": {"boot_sound_reset.inc": "old"}}))
                return output
            with patch.object(firmware.player, "prepare", side_effect=baseline):
                firmware.prepare(root, root, root, output)
            data = json.loads((output / "rpcmp-firmware-inputs.json").read_text())
            self.assertEqual(data["profile"], "hybrid-hyb2")
            self.assertEqual(data["source_revision"], "verified")
            self.assertEqual(data["prepared_caps_sha256"], "caps")
            self.assertNotIn("m6_sound_source", data)
            self.assertEqual(data["boot_overlay"]["boot_sound_reset.inc"],
                             firmware.sha256(overlay / "boot_hybrid_sound_reset.inc"))
        with patch.object(firmware.player, "prepare", side_effect=ValueError("pin")):
            with self.assertRaisesRegex(ValueError, "pin"):
                firmware.prepare(ROOT, ROOT, ROOT, ROOT / "out/missing-firmware-test")

    def test_mif_contains_boot_and_guard(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "out") as temp:
            folder = Path(temp)
            boot = folder / "boot.bin"
            boot.write_bytes(bytes.fromhex("1300000011223344"))
            with patch.object(images, "verify", return_value="checked") as verify:
                self.assertEqual(images.images(folder), "checked")
                verify.assert_called_once()
            self.assertEqual(mif_bytes((folder / "firmware.mif").read_text()),
                             boot.read_bytes() + bytes.fromhex("13000000") * 8190)
            for invalid in (b"", b"x", bytes(0x4004)):
                boot.write_bytes(invalid)
                with self.assertRaises(ValueError):
                    images.images(folder)
            boot.write_bytes(bytes(4))
            with patch.object(images, "verify", side_effect=ValueError("pair mismatch")):
                with self.assertRaisesRegex(ValueError, "pair mismatch"):
                    images.images(folder)

    def test_rom_clear_success_timeout_and_mismatch(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "out") as temp:
            folder = Path(temp)
            source = folder / "rom.c"
            source.write_text(r'''
#include <stdint.h>
#include <assert.h>
static uint32_t id=0x48594232, polls, writes, before=3, after=5, bad;
static uint32_t rpcmp_boot_read32(uint32_t address) {
    if(address==0x40000400) return id;
    assert(address==0x40000404); ++polls;
    uint32_t *wait=writes?&after:&before;
    if(*wait) { --*wait; return 0; }
    return writes?1|bad:7;
}
static void rpcmp_boot_write32(uint32_t address,uint32_t value) {
    assert(address==0x40000408 && value==1 && before==0); ++writes;
}
#include "boot_hybrid_sound_reset.inc"
int main(void) {
    assert(rpcmp_boot_reset_sound(20)==1 && writes==1);
    polls=writes=0; before=100; after=5;
    assert(rpcmp_boot_reset_sound(4)==0 && writes==0 && polls==5);
    polls=writes=0; before=0; after=100;
    assert(rpcmp_boot_reset_sound(4)==0 && writes==1 && polls==6);
    polls=writes=0; before=after=0; bad=16;
    assert(rpcmp_boot_reset_sound(4)==0 && writes==1);
    id=0; polls=writes=0;
    assert(rpcmp_boot_reset_sound(4)==0 && polls==0 && writes==0);
    return 0;
}
''')
            binary = folder / "rom"
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(ROOT / "overlays/openfpgaos"), str(source), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
