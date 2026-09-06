# M5 — Real MDX Library Playback

Status: active on 2026-09-02. M4 and the M4S stereo mapping gate are complete.

## Objective

Build a host-produced `.rpcmlib` from a locally supplied, fully admitted
FM-only MDX file, read that library through an APF deferred data slot, and play
its deterministic YM2151 stream through the hardware-proven queue, JT51, and
48 kHz stereo path on Pocket firmware 2.6.

This milestone is the first real-file playback vertical slice. It does not
make raw source paths or container offsets visible to the engine, UI, or RTL.

## Contract decisions

- The production input on Pocket is `.rpcmlib`, as required by the PRD. Raw
  `.mdx` selection on Pocket is not introduced.
- The host ingestion layer reads an MDX as untrusted bytes, applies the complete
  `specs/mdx-v1.md` structural and FM-only admission gates, then writes the
  immutable MDX bytes as the primary logical blob of one track.
- A nonempty historical PDX reference remains inside the MDX blob. Active P
  tracks and PCM8 layouts fail before a library is emitted. A later ingestion
  contract may resolve PDX to a dependency BlobId without changing the primary
  MDX blob or FM trace.
- Pocket resolves a TrackId to a primary BlobId through `LogicalLibrary`; the
  MDX engine receives only that bounded BlobView. It never sees the APF slot,
  source filename, filesystem path, or container offset.
- Storage reads and UI rendering remain outside sequencing/audio timing. The
  full selected MDX blob is admitted before playback starts in this one-track
  slice. Streaming and multi-track browsing are later milestones.
- M4S proved that logical left/right already maps to Pocket physical
  left/right. M5 must not swap channels.
- Future PCM support remains mandatory. Device routing must preserve the typed
  `ym2151`, `legacy_adpcm`, and `pcm8` targets; PCM must never be encoded as a
  fake YM2151 write. A future mixer adds a second source at the existing stereo
  audio boundary.

## Execution-substrate decision

ADR-0008 supersedes ADR-0004 and conditionally selects the stripped openfpgaOS
profile for the bounded M5 hardware experiment. It authorizes slice 4
integration but does not declare a production Pocket substrate. Promotion
requires resolving or rigorously bounding the placement-sensitive blackout and
passing ADR-0008's memory-map, perturbation, APF lifecycle, silent-failure,
integrated timing/CDC, and future-PCM-headroom gates.

## Implementation slices

1. Freeze this milestone and add deterministic one-file MDX ingestion. Test
   successful FM admission and transactional rejection of malformed, PCM, and
   unsupported input before `.rpcmlib` output exists.
2. Add a headless library-to-engine session that selects the sole TrackId,
   resolves its primary BlobId, prepares the MDX engine, and emits through the
   bounded scheduler without filesystem or UI dependencies.
3. Exercise the same session in the openfpgaOS desktop shim and cross-target
   build; measure code/data/stack budgets and preserve the safe-layout boot
   invariant.
4. Under ADR-0008's conditionally approved stripped openfpgaOS target,
   integrate the CPU/device MMIO endpoint with the hardware-proven
   JT51/Pocket AUDIO path and gather its production-promotion evidence.
5. Package one user-built `.rpcmlib` in a deferred APF slot and run Pocket
   acceptance. No copyrighted source file or generated library is committed.

## Acceptance

1. Repeated ingestion of identical MDX bytes and UTF-8 metadata produces
   byte-identical `.rpcmlib` output; malformed or unsupported input produces no
   output file.
2. The generated library passes all v1 header, directory, checksum, index,
   TrackId, and BlobId validation. Its primary blob exactly equals the source
   MDX bytes.
3. Library-to-engine event output is byte-identical to direct engine output and
   is unchanged by delayed or absent rendering.
4. Pocket reads only bounded deferred-slot ranges, admits the complete logical
   blob before playback, and performs no storage access on the scheduler/audio
   critical path.
5. Queue backpressure never reorders or drops writes. Reset, stop, parse error,
   unsupported PCM, and device fault leave JT51 in a bounded reset/silent state.
6. The integrated fit has nonnegative timing slack, reviewed CDCs, no
   unconstrained external ports under the approved constraint policy, and
   sufficient recorded memory/resource headroom.
7. On firmware 2.6, a locally supplied FM-only MDX library produces audible
   stereo playback, no red/fault state, one warm relaunch pass, and one full
   power-off cold-start pass.
8. Host tests, formatting, static analysis, architecture checks, behavioral
   RTL, and pinned real-JT51 simulation remain green.

## Local corpus policy

`C:\Users\new03\Documents\mdx` may be read locally for aggregate compatibility
analysis and user-run package construction. Filenames, titles, source bytes,
per-file hashes, generated user libraries, and proprietary driver binaries are
not committed. Repository tests continue to use only project-authored fixtures.

## Progress evidence

Slices 1 through 3 are complete as of 2026-09-02. The Docker-pinned xPack GCC
14.2.0-3 cross-build links the logical-library session as an ELF32 RISC-V
application with no undefined symbols. Its project-authored 752-byte library
fixture is also executed by the openfpgaOS desktop shim for 32 driver ticks:
33 YM2151 writes and digest `f1f04f5a8695a112`.

The integrated Pocket diagnostic budget is `text=59,172`, `data=968`, and
`bss=899,800` bytes: 959,940 static bytes total and 55,663,164 bytes headroom
in the SDK's
56,623,104-byte application SDRAM window. GCC reports no dynamic stack frames.
The largest individual frame is 7,408 bytes; summing every emitted static
frame gives a deliberately conservative 17,856-byte bound and 506,432 bytes of
headroom against the Pocket application's 524,288-byte stack. Large engine and
batch workspaces are static and explicitly placed in zero-backed storage in the
probe. The initialized-data build gate is 4,096 bytes; the loader's file-backed
`PT_LOAD` range is `0x0de18` after adding the Pocket terminal result adapter,
still 733,640 bytes below the original `0x0c0fe0`, without changing the public
MDX defaults or desktop trace.

On Pocket firmware 2.6, the isolated `0.8.0-m5-bss` diagnostic reached its
exact 33-write/digest PASS result on the initial launch, three warm relaunches,
and one full power-off cold start. This closes the hardware check for the
current zero-backed workspace placement. The diagnostic intentionally retains
the safe-layout RBF without JT51/MMIO, so slice 4 integration and the broader
ADR-0008 placement-perturbation gate remain open.

This compatibility and resource evidence supported ADR-0008's conditional M5
selection. It still does not authorize treating a changed M5 package as a
production replacement for the hardware-proven safe-layout control.

Slice 4 now has a reproducible source overlay against pinned openfpgaOS commit
`618a3eb985759a4154115109c2c8036271252888` and JT51 commit
`985a573`. The endpoint decodes only `0x40000200..0x4000025f`, routes queue v1
to JT51, and drives Pocket AUDIO directly at 48 kHz. A clean overlay build with
Quartus 25.1std.0 completed with zero errors at 13,822/18,480 ALMs (75%),
171/308 RAM blocks (56%), and 13/66 DSP blocks (20%). All analyzed corners have
nonnegative timing: the worst setup slack is +0.770 ns and the worst hold slack
is +0.047 ns. The report still lists unconstrained APF bridge, cartridge,
debug, scaler video, and scaler audio physical I/O. A baseline comparison and
the approved external-I/O constraint policy remain open. RPCMP request/acknowledge CDCs are
two-register toggle synchronizers marked for Quartus asynchronous recognition;
the associated command bundles remain stable from request until acknowledge.

The local-only `0.9.0-m5-audio` hardware package contains the safe-layout OS,
the integrated RBF, and the 33-write project fixture probe. Its purpose is the
slice-4 integration gate, not real-file acceptance. Hardware must next show its
green PASS state and audible output on firmware 2.6 for an initial launch, one
warm relaunch, and one power-off cold start. Slice 5 then replaces the fixture
with a locally generated `.rpcmlib` supplied through a deferred APF slot.

### Slice 4 hardware failure — 2026-09-05

The user reported a persistent black screen with the `0.9.0-m5-audio`
candidate. Whether `Loading...` appeared was not reported. Hardware acceptance
has failed; the previously recorded `0.8.0-m5-bss` passes remain separate.
The failed ZIP SHA-256 is
`c789849d5d5776d20482bfdf1d55d43987917b370636ce331b0933061d2eb3a5`.

Inspection of the preserved `rpcmp-m5-repro/output_files/ap_core.map.rpt`
found Critical Warning 127003 for both `firmware.mif` and
`./apf/build_id.mif`: Quartus explicitly substituted zero initial contents.
The preparation script extracts the upstream source and copies the CPU
netlist, but does not supply the boot-ROM initialization files. The zero-filled
boot ROM is a confirmed build defect sufficient to prevent normal CPU boot;
this result cannot establish whether the separate placement-sensitive failure
still affects a correctly initialized integrated build. This repeats the
boot-ROM packaging defect documented on 2026-08-29 in
`docs/design/pocket-openfpgaos-spike.md`.

The next repair must supply and verify the boot ROM and build-ID MIFs at the
paths used by Quartus, perform a clean compile, and reject missing-memory-file
warnings before packaging. Record their identities with the replacement RBF.
Do not request another hardware run of this unchanged candidate. The reported
positive timing slack does not make its zero-filled boot ROM usable.

### Boot-ROM repair candidate — 2026-09-05

`0.9.1-m5-audio` supplies the safe-memset boot ROM and a deterministic build-ID
MIF before synthesis. The preserved safe-control job's `firmware.mif` has
SHA-256 `a84afd867a4cdb2cc6bb4319f7be4252a5ab2d39cb857a9735c942d7c2ead567`.
Decoding its initialized words yields exactly 15,652 bytes with SHA-256
`bc904414d8188d4ff8cb38b8b08202f507b3d30d04a5a48bcb09c7d42d4f43f4`,
matching the previously hardware-accepted safe-memset boot binary. Preparation
checks both identities. It deliberately does not use the mutable latest
firmware build directory's `boot.bin`, whose hash differs from that control.

A new `openfpgaos-m5-audio-bootfix` tree was prepared and compiled from scratch
as job `rpcmp-m5-bootfix`. The synthesis input report resolves both MIFs;
Critical Warning 127003 is absent. The fit uses 13,822 ALMs, 171 RAM blocks,
and 13 DSPs. Worst analyzed setup/hold slack is +0.770/+0.047 ns, all TNS zero.
The external-I/O/CDC production-promotion gates above remain open.

The 1,775,532-byte native RBF has SHA-256
`2444e999c0c7af162b26e4f3590f911ad78c01eb8102a5057d78adc37da7ec4a`.
The packager pins this reviewed RBF, checks the boot/build-ID files and the
map/fit/STA/assembler/flow completion reports, and rejects missing-memory
initialization warnings. It records MIF and report hashes in the evidence file.
The OS, application ELF, loader, queue/audio RTL, and APF slot behavior are
unchanged from the failed candidate; no application rebuild is needed.

The separate local ZIP is `out/build/rpcmp-m5-audio-bootfix.zip`, SHA-256
`1b3a3082274fa2120795beb33917306fffda4b8c14637335bba29b8a0c0a1a4e`.
All 14 ZIP members match the evidence file's sizes and hashes. The failed ZIP
and accepted BSS control remain available at their original paths.

Validation: 37/37 host tests, including format/tidy and architecture checks;
9 behavioral RTL simulations; 5 real-JT51 simulations; 3 new Python regression
tests for missing/zero ROM and missing/failed reports. After adjusting the
flow-report completion parser, the affected Python suite passed again.

After receiving the repair ZIP and the initial/warm/cold-start instructions,
the user reported that it worked perfectly on 2026-09-05. Record this as an
overall successful hardware report for `0.9.1-m5-audio` and resolution of the
reported zero-ROM candidate's blackout. The reply did not separately enumerate
the displayed result, audio observations, or restart cases; do not invent
individual measurements or repeat counts. Firmware 2.6 is the previously
reported environment, not a newly supplied version measurement.

The next M5 implementation task is slice 5: bounded deferred-slot loading of
a locally generated FM-only `.rpcmlib` into the existing logical-library
session. The production-promotion gates remain open, and this fixture success
does not constitute real-MDX-file acceptance.

The slice-5 application plan and explicit Pocket admission limits are in
`docs/design/pocket-m5-deferred-playback.md`. Implementation must retain the
accepted FPGA/OS artifacts and separately verify loading, long-rest pacing,
real-file preflight, and the new application/package before hardware handoff.

The first slice-5 component is `spikes/pocket/common/src/m5_library_loader.cpp`:
an injected slot reader loads bounded chunks into caller-owned storage, then
validates the complete logical library using the profile's explicit limits.
Only a validated one-track/one-blob library is published. Host regression cases
cover invalid sizes/capacity, failed and short transfers, corrupt envelopes,
multiple tracks, the exact 2 MiB transport ceiling, and decoded-size rejection.
The component is now connected to the separate real-file application described
below. Hardware acceptance remains unclaimed.

### Slice 5 hardware candidate — 2026-09-05

`0.10.0-m5-file`, core `RPCMP.M5FileProbe`, uses the unchanged accepted
`0.9.1-m5-audio` FPGA/boot/OS artifacts. A new ELF loads the one-track library
from read-only deferred slot 4, resolves its command-line TrackId logically,
admits the MDX, and services the bounded playback pump without rendering or
storage on the sequencing path. The 50 ms startup lead and 100 ms lookahead
are independent of video. Natural completion and terminal faults reset sound;
a reset failure is explicitly distinguished rather than reported as silence.

Docker/xPack 14.2.0-3 cross-build: ELF32 RISC-V, no undefined symbols,
text 67,388 bytes, data 228 bytes, BSS 2,997,016 bytes. Static total is 3,064,632
bytes, leaving 53,558,472 bytes in the application SDRAM window. Conservative
stack sum is 18,288 bytes (largest frame 7,408), with no dynamic frames;
506,000 bytes remain against the 524,288-byte stack limit. Initialized data
remains below the 4,096-byte safe-layout gate. This is application headroom,
not completion of ADR-0008's broader PCM/CDC/substrate promotion review.

A locally supplied, unmodified MDX was ingested and preflighted for up to
60 seconds with the same pump and engine. Primary-blob bytes equal the source;
the library route and direct MDX route produce matching operation count and
digest, with key-on writes and both pan output bits observed. Private track
identity and trace measurements remain in the ignored local evidence file.
This is deterministic sequencing evidence, not measured Pocket audio fidelity.

The new local-only artifact is `out/build/rpcmp-m5-file.zip`; the adjacent
`.evidence.json` records all inputs, preflight, budget, and ZIP member hashes.
All 15 ZIP members passed independent size/hash/allowlist comparison. Source
MDX is not included separately; the library contains private music and the ZIP
must not be redistributed or committed. Earlier controls remain unchanged.

Validation: 40 non-tidy CTest cases, including format and architecture checks,
passed after correcting the authored corruption test to modify checksummed
payload instead of a deliberately ignored reserved directory byte. Full C++
tidy passed on the same implementation. Behavioral RTL (9) and pinned real
JT51 simulations (5) passed sequentially. No Quartus rebuild was performed:
the packaged native RBF is hash-identical to the reviewed accepted fit.

The next required step is firmware 2.6 hardware verification following
`docs/design/pocket-m5-file-hardware-check.md`. Initial boot, audible real-file
playback, warm relaunch, cold start, and the changed ELF's placement behavior
remain unverified. Do not mark M5 or substrate promotion complete from this
software/package handoff.

See `overlays/openfpgaos/README.md` for the repair build's reproduction steps.
