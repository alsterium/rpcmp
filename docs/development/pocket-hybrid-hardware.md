# HYB1 local hardware check

This is the first prepared-input CPU PCM8 + FPGA FM timing fixture, not M6 or
whole-corpus acceptance. The project charter and smaller select/play/stop MVP
remain current. Rich UI, M3U browsing and arbitrary-file admission are deferred.

## Candidate

- Local ZIP: `out/build/hybrid-hardware-r1.zip`
- SHA-256: `6a897604979f14b2a0ebc51cf8baf76ffc4fc3681dd7d1d2e3d647c2b8573bd7`
- Core: `RPCMP.HybridProbe`; platform: `rpcmp_hybrid`; version: `0.12.0-hybrid-r1`.
- Framework minimum: `2.2`, preserving the user's Firmware 2.6 loading result.
- The matching ROM/OS/FPGA/app and one locally prepared MDX/PDX pair are included.
  This private local ZIP contains user music; it is not a public distribution.
- Complete member lengths/hashes, ROM/OS pairing, app budget and Quartus report
  identities are in `out/build/hybrid-hardware-r1.evidence.json`.

Extract the ZIP and copy its `Cores`, `Assets` and `Platforms` folders to the
SD card root. Start **HybridProbe** and select **hybrid.json**. The terminal
starts silent. **START** cycles between Loaded MDX/PDX, 8 FM + 8 ADPCM,
8 FM + 8 PCM16 and 8 FM + 8 PCM8 while stopped. **A** starts the selected test;
**B** clears/stops it. This fixture has no pause or live selection while playing.

The screen intentionally stops updating during playback. B or natural end
shows maximum render time, maximum feed time and the lowest observed PCM queue
occupancy (including observations after rendering). These are sampled diagnostics,
not an exhaustive hardware watermark. The authored PCM buffers are short; FM
continues after their PCM portion finishes. Repeat stop/start to repeat the PCM
stress, rather than interpreting the later FM-only sustain as a dropped channel.

## Report on Firmware 2.6

```text
HYB1 r1
Boot / silent before A:
Loaded MDX: music / left+right / 1 minute / clicks:
Loaded MDX after B: Render max / Feed max / Queue minimum:
8 FM + 8 ADPCM: sound / B stops / A restarts / metrics:
8 FM + 8 PCM16: sound / B stops / A restarts / metrics:
8 FM + 8 PCM8: sound / B stops / A restarts / metrics:
Normal restart / power-off restart:
Error text or status (if any):
```

Do not infer hardware success from the CPU simulation or a fitted bitstream.
In particular, the CPU experiment has ideal AXI memories and a modeled peripheral
delay; it does not include SDRAM/video/OS interrupt contention. Inherited shell
external-pin constraints also remain incomplete. The new HYB1 internal timing,
48 Gray-pointer crossing bits and later control synchronization stages were
audited separately; that does not establish whole-platform sign-off.

## Reproduction inputs

Use the pinned checkouts and tool images recorded in the
[compatibility evidence](../research/mdxplayer-compatibility.md#cpu-renderer-and-actual-shell).
`tools/hybrid_renderer_build.py` prepares and builds the reference-backed renderer
and SDK app under `out/build/hybrid-cpu-20260921`. Run
`tools/hybrid_renderer_verify.py` against the independently captured split streams,
and `tools/hybrid-renderer-tidy.ps1` against the generated header.

Prepare ROM/OS using `tools/pocket_hybrid_firmware_prepare.py`. In its
`src/firmware/os`, build with `make -j4 TARGET=pocket CROSS=riscv-none-elf-
EXTRA_CFLAGS=-march=rv32imafc_zicsr_zifencei` in the pinned firmware image.
The upstream recipe does not fail when `hexdump` is absent; do not use its empty
MIF. `tools/pocket_hybrid_images.py` converts `boot.bin` and requires the resulting
MIF and OS image to match the linked ELF before shell preparation.

`tools/pocket_hybrid_prepare.py` creates the no-GPU HYB1 shell and a Windows
Quartus project. Supply that verified MIF, firmware ELF and OS image together.
Compile the generated `hybrid-fit/ap_core`, then run
`quartus_sta -t <repo>/tools/hybrid_cdc_audit.tcl` in that project directory.
Run `tools/rtl-hybrid-verify.ps1 -PreparedTree <prepared-shell>` as well as the
relevant host/tooling checks. `tools/pocket_hybrid_package.py` checks the matched
artifacts and writes a fresh package with complete directory/ZIP readback.

Component notices are retained in the local package. Public redistribution
still requires the component-specific review described in the reference
evidence; the application README's BSD badge is not a license for the complete
MXDRV/PCM8/FMGEN dependency set.
