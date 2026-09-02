# ADR-0008: Conditionally select stripped openfpgaOS for M5

- Status: accepted
- Date: 2026-09-02
- Deciders: RPCMP maintainers
- Supersedes: ADR-0004

## Context

M5 must run the already host-verified logical-library-to-MDX session on Pocket,
feed its ordered YM2151 operations through the v1 CPU-to-RTL queue, and use the
hardware-proven JT51 and 48 kHz stereo path. ADR-0004 prevented production
Pocket integration until a target spike supplied execution, resource, storage,
MMIO, input/video, boot, reset, timing, and dependency evidence.

The bounded openfpgaOS investigation now supplies enough evidence to choose an
M5 experiment substrate:

- a stripped 90 MHz openfpgaOS profile with 16 KiB instruction cache and 32 KiB
  data cache fits together with the eight-entry timestamped queue and JT51;
- the integrated fit uses 13,774 of 18,480 ALMs, 172 of 308 M10Ks, and 13 of 66
  DSPs, with reported setup slack `+0.327 ns` and hold slack at least
  `+0.110 ns` across the reported corners;
- RTL tests cover reset, full rejection, sticky flags, due-time dispatch,
  ordering, wrap/reuse, the bundled-data CDC handshake, JT51 writes, and sample
  diagnostics;
- Pocket firmware 2.6 tests cover APF data-slot reads, input-to-command,
  bounded queue behavior, representative headless workloads, warm restarts,
  cold starts, and the JT51/Pocket AUDIO stereo route;
- the M5 library session cross-builds as an ELF32 RISC-V application with no
  undefined symbols and executes identically in the desktop shim.

The candidate is not yet production-stable. Diagnostics demonstrated that a
`0x40` shift in later code and BSS placement can change a normally booting
package into a repeatable blackout or reload loop. The accepted
`0.5.38-safe` OS/RBF pair is therefore a checksum- and size-pinned workaround,
not proof that arbitrary firmware layouts boot correctly. The current M5
session also contains 767,712 bytes of initialized data. This fits comfortably
in SDRAM, but its startup copy and placement effects make it relevant to the
unresolved failure.

The stock openfpgaOS profile is not a viable alternative: it uses 301 of 308
M10Ks before JT51 and misses its 100 MHz setup target. A project-owned minimal
VexRiscv SoC remains a credible fallback, supported by the reviewed HarpMudd
design, but RPCMP has not implemented its APF, SDRAM, storage, video, or C++
runtime services. An RTL-only player would discard the portable parser,
library, player, and replaceable-UI architecture.

## Decision

Select the stripped openfpgaOS profile as the execution substrate for the
bounded M5 hardware experiment, subject to all restrictions below. This is not
approval to describe it as RPCMP's production Pocket substrate or to publish a
general release.

- Keep the candidate under `spikes/pocket` until the production-promotion gates
  below pass. Production Core and public contracts must not depend on the SDK.
- Preserve the platform-neutral library, player, engine, command, snapshot, and
  device-port boundaries. Physical input, APF storage, framebuffer rendering,
  clocks, MMIO, and audio remain platform adapters.
- Use `.rpcmlib` through a deferred APF data slot. Admit the complete selected
  logical MDX blob before playback; perform no storage access on the sequencing
  or audio critical path.
- Use Core-to-RTL Device Queue v1 at its CPU-local little-endian MMIO mapping.
  Do not allocate RPCMP devices in APF's reserved `0xF8xx_xxxx` region.
- Preserve queue backpressure, ordered due-time dispatch, reset, and CDC
  semantics. Rendering stalls or absence must not alter the operation stream.
- Preserve the existing JT51 adapter and exact signed stereo 48 kHz Pocket
  AUDIO boundary. Logical left/right must not be swapped.
- On parse, storage, scheduler, queue, device, reset, or heartbeat failure,
  stop accepting playback work, reset JT51 and the audio adapter, discard
  queued/in-flight pre-fault operations, and remain silent.
- Preserve typed `ym2151`, `legacy_adpcm`, and `pcm8` routing. Future PCM is a
  required second device/audio source and must not be encoded as fake YM2151
  writes. Record resource headroom for its later mixer and buffering.
- Keep the previously proven safe-layout package immutable as a boot-control
  artifact. A changed M5 package must carry distinct identity and hashes and
  must not silently replace that control.
- Pin all openfpgaOS, SDK, compiler, template, queue, and JT51 revisions used by
  the experiment. Record the exact package contents and generated-artifact
  provenance. Distribution approval remains separate from this local M5
  decision.

M5 slice 4 may now integrate the C++ session, device MMIO endpoint, JT51, and
Pocket AUDIO path under those restrictions. M5 Pocket acceptance remains an
experimental result until the following production-promotion gates pass.

## Production-promotion gates

All of the following are required before stripped openfpgaOS can be called the
production RPCMP Pocket substrate:

1. Identify and eliminate, or bound with a reviewed memory-map explanation,
   the placement-sensitive blackout. A checksum-pinned lucky layout is not an
   acceptable resolution.
2. Record the final ELF/map boundaries for boot ROM, executable, initialized
   data, BSS, heap, stack, caches, and SDRAM, including startup copy/clear
   ownership and alignment.
3. Attribute initialized-data usage by symbol and move eligible large mutable
   workspaces to explicitly initialized zero-backed storage without changing
   public contract defaults or playback behavior.
4. Demonstrate that non-semantic code/BSS placement perturbations still pass
   three warm starts and one full power-off cold start on Pocket firmware 2.6.
5. Re-run the integrated M5 + queue + JT51 Quartus fit and require nonnegative
   setup/hold slack, reviewed reset/CDC paths, and no unexplained new
   unconstrained functional clock or external-I/O path.
6. Verify the documented APF boot/status sequence, continuous heartbeat,
   reset-enter/reset-exit behavior, bounded deferred-slot access, and silent
   failure behavior on hardware.
7. Record remaining ALM, M10K, DSP, SDRAM, firmware, and timing headroom against
   an explicit future PCM decoder, sample buffering, stereo mixer, and UI
   reserve. PCM need not be implemented in M5.
8. Run the complete M5 acceptance suite with a locally generated FM-only
   library: audible playback, correct stereo mapping, no red/fault state, one
   warm relaunch, and one cold start. No local music bytes or identifying corpus
   metadata may enter the repository.

If placement stability cannot be explained and demonstrated without pinning an
exact firmware layout, stop production integration and open a comparison spike
for a project-owned minimal VexRiscv SoC. That fallback must run the same M5
session and queue contract before it can replace this decision.

## Consequences

M5 can proceed to real hardware without claiming that the openfpgaOS boot issue
has been solved. The selected path reuses measured storage, input, framebuffer,
C++ runtime, queue, JT51, and audio work and is substantially shorter than
constructing a new SoC.

The distinction between experimental and production paths is now normative.
Hardware playback alone cannot promote the substrate; placement stability,
failure silence, APF lifecycle behavior, full timing/CDC review, and future PCM
headroom are equally required.

The project-owned SoC comparison previously requested by ADR-0004 is no longer
a prerequisite for the bounded M5 experiment. It becomes the mandatory
fallback when the selected substrate fails its placement-stability gate.

## Alternatives considered

- **Adopt stripped openfpgaOS as production immediately:** rejected because the
  placement-dependent blackout is reproducible and unexplained.
- **Continue blocking all M5 Pocket integration:** rejected because the
  measured stripped profile, services, queue/JT51 fit, target build, and
  firmware 2.6 tests are sufficient for a bounded experiment.
- **Build a project-owned VexRiscv SoC first:** deferred as the fallback because
  it requires new APF, SDRAM, storage, video, and runtime integration before it
  can exercise the already verified M5 application.
- **Use the M2 hardware sequencer:** rejected because it represents only a
  frozen operation list and cannot execute an arbitrary admitted MDX library.
- **Move MDX and library behavior into RTL:** rejected because it violates the
  portable, headless, replaceable-UI architecture without solving a measured
  M5 requirement.

## Evidence

- `docs/adr/0004-pocket-execution-substrate-gate.md`
- `docs/adr/0007-m2-pocket-validation-substrate.md`
- `docs/design/pocket-execution-candidates.md`
- `docs/design/pocket-openfpgaos-spike.md`
- `docs/milestones/M5-real-mdx-library-playback.md`
- `specs/core-rtl-device-queue-v1.md`
- `specs/ym2151-rtl-adapter-v1.md`
