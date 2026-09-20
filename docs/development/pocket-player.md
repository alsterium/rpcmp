# M6 Pocket application

This is the actual CPU application composition for M6 slice 5, using the
approved RSM1 sound binding and CPU framebuffer profile. The isolated M5 probes
and their package gates remain unchanged. The first matching ROM/OS/bitstream
candidate is now available; use the [hardware check](pocket-player-hardware.md).

## Build

From the repository root in the pinned `rpcmp-openfpgaos-toolchain:14.2.0-3`
Linux image, with the pinned SDK checkout available:

```sh
make -j4 -f spikes/pocket/openfpgaos/player.mk player-check
```

The output is `out/build/pocket-m6-player/rpcmp-player.elf`, its link map and
`budget.json`. The build generates the licensed bitmap font using native host
tools, then compiles the application for RV32IMAFC / ILP32F with GCC 14.2.0.
There are no unresolved symbols; initialized data is limited to 4096 bytes,
static memory to the SDK's 54 MiB app region, and recorded conservative stack
usage to 512 KiB. The SDK reserves its video buffers outside that app region;
the app retains only the current borrowed draw pointer. Package distribution
must include the font notices in `third_party/unifont`.

## Ownership and operation

- Before filesystem work, request RSM1 INHIBIT. This is a silence request, not
  an acknowledgement or a quiescence assertion. Load slot 4 in 64 KiB chunks
  into at most 32 MiB of BSS, then validate through the existing AlbumCatalog.
  Loading occurs before the audio mailbox owner exists. No file I/O occurs in
  the playback loop.
- `PlayerApplication` owns the sound client/backend, PlayerSession, PlayerUi
  and bitmap canvas. The library bytes and injected ports outlive it. Large
  objects are placement-constructed once in zeroed BSS; they are not stack or
  initialized-data allocations. UI still accesses only commands, snapshots
  and the read-only CatalogReader. Runtime/player have no UI dependency.
- Every iteration services sound and advances PlayerSession. At intervals of
  at least 5 ms it publishes, updates UI and samples held input. This does not
  derive audio timing from UI cadence. Preparation/control deadlines are 5 s /
  100 ms; the independent approved mailbox watchdog remains **1000 us**.
- A new frame is recorded at most every 33,333 us, then rasterized in chunks
  of at most 256 pixels with sound service between chunks. The current frame
  retains its copied text, geometry and draw buffer while UI keeps updating.
  The display port returns Busy without replacing that buffer or blocking on
  vsync. Only a complete frame is presented.
- The pinned SDK adapter checks 640x480, stride 640, indexed 8-bit mode and the
  no-GPU capability profile. It polls `FB_SWAP_CTRL.pending` before calling
  the OS triple-buffer flip. It does not call GPU or wait-for-vblank services
  during playback. The OS flip still cleans the **whole frame**; target CPU
  duration is unmeasured and may require further integration work.
- Time comes from coherent bounded high/low/high reads of the pinned CPU cycle
  registers and the OS's live CPU frequency, rather than a wrapping 32-bit
  microsecond timer. Invalid frequency, exhausted reads or reversed time stop
  this application and request INHIBIT. The target adapter requires an integer
  number of MHz, which includes this 90 MHz profile.
- APF PAD types 1–3 are connected gamepads. Buttons and type come from one raw
  key word. Replaceable bindings map A/B/L/R/directions to the existing UI
  actions; first-held and reconnect-held buttons require release. B from the
  initial empty monitor opens albums. A chooses the album, then the track.
- Each new instance has AlbumOrder / Default (two loops and five-second fade),
  shuffle off and no autoplay. No settings storage is supplied and
  `kPlaybackSettings` is absent. Policy changes remain effective in session.

The application records maximum sound-service gaps and record/pump/present
durations. Fatal app diagnostics print these values after INHIBIT. They are
measurements of CPU work, not proofs of audio delivery. Ordinary recoverable
playback errors remain in the shared UI/transport recovery path.

## Verification boundary

`mdx_backend` now also drives the real application through scripted copied
sound mailboxes and physical button samples. It checks startup defaults,
library selection, pause/resume, stop/back, in-session policy, fresh launches,
surface retention during long presentation backpressure, and fatal display /
clock / canvas failures. Separate cases cover timer rollover/retry, disconnect
and reconnect, and the full 32 MiB input bound. The scripted device is not an
RTL engine or a proof of audible playback.

The candidate now includes M6 boot-failure sound handling and coherent
ROM/OS/app packaging. Still required: actual display/input/audio checks on Pocket,
measured service gaps (including MDX admission, synchronous command recording
and OS cache maintenance), and
Japanese readability at the real screen size. No hardware acceptance follows
from host tests or the ELF budget.

## Candidate build and packaging

Prepare the pinned fail-closed firmware with
`tools/pocket_player_firmware_prepare.py --repo . --upstream <pinned-core-checkout>
--musl <pinned-firmware-musl> --output out/build/<new-firmware-directory>`.
Build `src/firmware/os` with `make -j4 TARGET=pocket` in the pinned firmware
image recorded by the preparation manifest. This preserves the existing boot
CRC/retry behavior, uses the RSM1 Reset mailbox on boot failure, and reports
`gpu_base=0` when the FPGA advertises no GPU. It does not add settings storage.

The FPGA uses the preceding slice's `--player-sound --apf-flush` preparation,
no-GPU macros, seed 1 and scoped sound SDC. For this candidate a fresh Quartus
project was copied from that configuration, with all source references absolute.
Only the copied AXI peripheral's `init_file` literal was changed to the absolute
path of this candidate's `firmware.mif`; no logic was changed. Run a full
`quartus_sh --flow compile <project>/ap_core`, then the four-corner CDC audit.
An attempted update of a copied placement database did not report processing
the firmware MIF and was **not used**. Do not infer ROM identity from a generic
successful `--update_mif` exit. Pair the mapped MIF and `os.bin` against the
same `firmware.elf` using `tools/pocket_firmware_pair.py`.

`tools/pocket_player_package.py` pins the inspected candidate app, ROM, OS and
RBF identities. It checks app section sizes against the budget, SDK loader
manifest, successful Quartus reports, mapped ROM path and firmware pairing.
It invokes `rpcmp_player_preflight` to open the ALBM catalog and admit every
track with the actual Core MDX preparation path. Fresh `out/build/m6-player-*`
outputs only are permitted. Every extracted/ZIP member is read back exactly,
and the evidence JSON records lengths and SHA-256 hashes. These checks establish
artifact identity, not hardware behavior or timing acceptance.

The package advertises all eight pinned scaler slots, including boot/terminal
320x240 at slot 0 and app 640x480 at slot 7. APF allows runtime selection among
up to eight configurations ([official video schema](https://www.analogue.co/developer/docs/core-definition-files/video-json)).
Reordering these slots would break the OS/RTL ABI. Slots 1–4 are deferred OS,
INI, ELF and read-only library assets; there is no nonvolatile slot. The input
mapping names A/B/L/R without overriding the app's replaceable focus tables.
Font copyright and OFL notices are included with the binary.

The original synthetic scale corpus is reproducible with
`python -B tools/pocket_player_demo.py <new-folder>` followed by
`rpcmp_album_pack <new-folder> <music.rpcmlib>`. It contains one looping and one
finite track in the first album, plus a different looping track in the second.
It uses FM channel A with stereo panning; it contains no copied music or ROM.
Preflight proves parse/admission only; audible output is a hardware check.
