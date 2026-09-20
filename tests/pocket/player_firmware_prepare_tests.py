"""The M6 overlay changes only ROM sound handling and the no-GPU OS capability."""

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import pocket_player_firmware_prepare as prepare


class FirmwarePrepareTests(unittest.TestCase):
    def test_filtered_patch_and_provenance(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder)
            overlay = repo / "overlays/openfpgaos"
            overlay.mkdir(parents=True)
            for name in ("player-cpu-video.patch", "boot_m6_sound_reset.inc"):
                shutil.copyfile(ROOT / "overlays/openfpgaos" / name, overlay / name)
            output = repo / "out/build/player"

            def baseline(*args, **kwargs):
                self.assertTrue(kwargs["fail_closed"])
                boot = output / "src/firmware/os/targets/pocket/boot"
                boot.mkdir(parents=True)
                (boot / "boot_sound_reset.inc").write_text("old sound", encoding="utf-8")
                caps = output / "src/firmware/os/kernel/caps_table.c"
                caps.parent.mkdir(parents=True)
                # Authored context exercises the pinned line-83 hunk, including
                # UTF-8 which must not be decoded with the Windows ANSI page.
                caps.write_text("// 音声\n" * 82 +
                                "    caps->gpu_base            = platform->gpu_base;\n",
                                encoding="utf-8")
                fpga = output / "src/fpga/common/axi_periph_slave.v"
                fpga.parent.mkdir(parents=True)
                fpga.write_bytes(b"FPGA preparation owns this file\n")
                (output / "rpcmp-firmware-inputs.json").write_text(json.dumps({
                    "profile": "fail-closed", "boot_overlay": {
                        "boot_sound_reset.inc": "old", "boot_crc_retry.inc": "unchanged"},
                    "source_revision": "baseline-verified"}), encoding="utf-8")

            with patch.object(prepare.baseline, "prepare", side_effect=baseline):
                self.assertEqual(prepare.prepare(repo, repo / "source", repo / "musl", output), output)
            self.assertEqual((output / "src/fpga/common/axi_periph_slave.v").read_bytes(),
                             b"FPGA preparation owns this file\n")
            sound = output / "src/firmware/os/targets/pocket/boot/boot_sound_reset.inc"
            self.assertEqual(sound.read_bytes(),
                             (overlay / "boot_m6_sound_reset.inc").read_bytes().replace(b"\r\n", b"\n"))
            manifest = json.loads((output / "rpcmp-firmware-inputs.json").read_text(encoding="utf-8"))
            self.assertEqual(manifest["profile"], "m6-player")
            self.assertEqual(manifest["source_revision"], "baseline-verified")
            self.assertEqual(manifest["boot_overlay"]["boot_crc_retry.inc"], "unchanged")
            self.assertEqual(manifest["boot_overlay"]["boot_sound_reset.inc"],
                             prepare.sha256(overlay / "boot_m6_sound_reset.inc"))
            caps = output / "src/firmware/os/kernel/caps_table.c"
            self.assertEqual(manifest["prepared_caps_sha256"], prepare.sha256(caps))
            self.assertIn("? platform->gpu_base : 0u;", caps.read_text(encoding="utf-8"))

    def test_baseline_rejection_is_not_bypassed(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            with patch.object(prepare.baseline, "prepare", side_effect=ValueError("pinned input")):
                with self.assertRaisesRegex(ValueError, "pinned input"):
                    prepare.prepare(root, root, root, root / "out/build/player")
            self.assertFalse((root / "out").exists())


if __name__ == "__main__":
    unittest.main()
