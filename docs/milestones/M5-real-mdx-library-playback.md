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

The cross-target budget is now `text=23,112`, `data=792`, and `bss=898,260`
bytes: 922,164 static bytes total and 55,700,940 bytes headroom in the SDK's
56,623,104-byte application SDRAM window. GCC reports no dynamic stack frames.
The largest individual frame is 7,408 bytes; summing every emitted static
frame gives a deliberately conservative 17,328-byte bound and 506,960 bytes of
headroom against the Pocket application's 524,288-byte stack. Large engine and
batch workspaces are static and explicitly placed in zero-backed storage in the
probe. The initialized-data build gate is 4,096 bytes; the loader's file-backed
`PT_LOAD` range fell from `0x0c0fe0` to `0x05d60` without changing the public
MDX defaults or desktop trace.

This compatibility and resource evidence supported ADR-0008's conditional M5
selection. It still does not authorize treating a changed M5 package as a
production replacement for the hardware-proven safe-layout control.
