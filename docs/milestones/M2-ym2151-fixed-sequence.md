# M2 — YM2151 Fixed Test Sequence

Status: active from 2026-09-01. M1 is complete. Begin with the host device
operation contract and deterministic trace; no YM2151 RTL dependency is
selected by this milestone document.

Progress: slices 1 and 2 are complete. The v1 portable values and self-authored
17-operation, 3.5-second sequence are implemented with a 254-byte exact golden
trace. A host scheduler now validates streams atomically and dispatches through
a typed device port from a preallocated 64-entry maximum ring, with tested
ordering, backpressure, full rejection, reset, fault, and time bounds. No
hardware protocol, third-party RTL, or audio adapter is claimed by these slices.

## Objective

Produce one deterministic, synthetic YM2151 register sequence through the
platform-neutral scheduler/device boundary, verify the same ordered writes in
RTL simulation, and finally hear the bounded sequence through Pocket's exact
48 kHz AUDIO output without making rendering or UI service part of timing.

## In scope

- Freeze a v1 `DeviceOp` contract for timestamped reset and 8-bit register
  writes addressed by typed `DeviceId`.
- Define a short, self-authored fixed sequence as an exact golden operation
  trace with explicit tick rate, end time, and reset state.
- Implement a bounded host scheduler and fake device port with deterministic
  ordering, backpressure, and overflow behavior.
- Specify and simulate the Core-to-RTL queue/register protocol, including
  reset, full/reject behavior, due-time dispatch, wrap/reuse, and CDC.
- Select the YM2151-compatible RTL revision and Pocket execution substrate only
  through accepted ADRs that close the remaining ADR-0004 gates.
- Convert the selected device's sample stream to signed 16-bit stereo at
  exactly 48 kHz with defined silence, clipping, underflow, and overflow.
- Build and verify one checksum-pinned local Pocket package, then record the
  firmware version, hashes, audible result, and long-run/reset observations.

## Explicitly out of scope

- MDX parsing or sequencing
- PDX, MSM6258, PCM8, mixing multiple sound devices, or general resampling
- User-selectable tracks, transport UI, final visualization, or audio controls
- Treating a research bitstream or arithmetic resource estimate as a selected
  production dependency
- Redistribution of a GPL-covered bitstream until the corresponding-source,
  notices, and repository licensing process is approved and documented

## Implementation slices

1. Freeze `specs/device-op-api.md`; add the fixed synthetic sequence and a
   byte-exact host golden trace.
2. Add the bounded scheduler/device-port implementation and deterministic host
   tests for ordering, equal timestamps, reset, full/reject, and tick overflow.
3. Freeze the production Core-to-RTL MMIO/FIFO protocol and prove it with a
   self-checking RTL simulation. Research-only addresses are not inherited
   implicitly.
4. Resolve the execution-substrate and YM2151 RTL/license choices in accepted
   ADRs, with exact revisions, build inputs, and distribution obligations.
5. Implement and simulate the 48 kHz Pocket AUDIO adapter, including rational
   clock conversion and all boundary states, then close timing and resource
   gates for the integrated build.
6. Package the fixed sequence for hardware and record audible playback,
   relaunch/reset behavior, queue diagnostics, and a sustained-run result.

Each slice must leave host verification green. Hardware work begins only after
the equivalent host trace and RTL simulation pass. A sound heard from an
unversioned or unconstrained build is diagnostic evidence, not acceptance.

## Acceptance criteria

1. The fixed sequence is self-authored and represented by an exact ordered
   trace of fixed-width operations; repeated runs produce identical bytes.
2. Scheduling uses integer time with an explicit tick rate. UI/render calls,
   wall-clock sleeps, and audio callbacks are not timing authorities.
3. Invalid device IDs, register widths, non-monotonic traces, arithmetic
   overflow, queue full, and reset during pending work have bounded results.
4. Equal-timestamp operations retain source order, and backpressure never
   silently drops or reorders a register write.
5. Core and the sequence test run headlessly; UI remains buildable without
   runtime, engines, device adapters, or RTL.
6. Self-checking RTL simulation covers reset, due time, ordering, full/reject,
   overflow clear, pointer wrap/reuse, register handshakes, and relevant CDC.
7. The integrated build records all clocks, timing slack, resource use, warning
   baseline, bitstream hash, and exact third-party revisions.
8. The AUDIO boundary emits signed 16-bit stereo at exactly 48 kHz and has
   deterministic reset/silence, clipping, underflow, and overflow behavior.
9. Pocket hardware audibly plays the complete fixed sequence and then reaches
   its specified terminal state. Relaunch and reset reproduce that behavior.
10. Architecture/dependency checks and all affected host tests remain green.
11. No MDX parser, copyrighted music, proprietary ROM/sample data, or
    unapproved redistributable third-party binary is added.

## Verification

- Every slice: `pwsh -File tools/host-verify.ps1`.
- RTL slices: run the repository's self-checking Questa workflow and record the
  simulator/tool version plus the exact PASS summary.
- Integrated Pocket slice: run Quartus analysis, synthesis, fit, assembly, and
  timing; validate APF JSON and package layout; record native/reversed RBF and
  ZIP hashes before copying to SD.
- Hardware acceptance: use the recorded package on a recorded Pocket firmware
  version and report audible completion, reset/relaunch, diagnostics, and the
  sustained-run duration.

## Known constraints and decision gates

- ADR-0004 remains accepted and unsuperseded. The current checksum-pinned safe
  application layout permits bounded experiments but does not approve arbitrary
  firmware growth.
- The pinned JT51 research fit demonstrates capacity and internal timing only.
  It does not select JT51, connect 48 kHz Pocket audio, or authorize release.
- Pocket's APF AUDIO rate is fixed at 48 kHz. Device clock and rational sample
  conversion must be specified and tested before hardware acceptance.
- The literal tone and duration are not a fidelity oracle for later MDX work;
  M2 proves transport, ordering, synthesis integration, and the audio path.
