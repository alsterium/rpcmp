# MDX Engine Contract v1

## Purpose and compatibility target

This contract defines bounded parsing and deterministic FM sequencing for the
MDX behavior common to MXDRV 2.06+16 and 2.06+17 Rel.X5-S. It is an RPCMP engine
contract, not an emulation of the Human68k resident-driver API.

Version 1 accepts the ordinary 9-track layout and sequences YM2151 tracks A–H.
It preserves the legacy ADPCM P track and PDX reference as typed input for the
mandatory future PCM extension, but does not emit PCM work. The 16-track PCM8
layout is recognized and rejected as unsupported in v1.

All source bytes are untrusted and immutable. Parsing and playback state must
not store writable counters, flags, or patched offsets in the source view.

## Input and structural model

Multibyte MDX fields are big-endian. An ordinary file is:

```text
title_bytes
0d 0a 1a
pdx_reference_bytes
00
BASE:
  u16 voice_data_offset
  u16 track_offset[9]       # A, B, C, D, E, F, G, H, P
  voice and track data at BASE-relative offsets
```

The first track offset determines the table width:
`track_count = (track_A_offset - 2) / 2`. Subtraction occurs only after checking
that the value is at least 2; the difference must be even. V1 accepts exactly
9 and recognizes exactly 16 as `unsupported_pcm8_layout`. Other widths are
`invalid_track_table`.

Every offset is an unsigned BASE-relative value and may equal another offset.
It must point within the file at or after the complete offset table. An offset
does not by itself define an object's end. Track decoding must therefore check
every instruction fetch against the complete input and its own bounded region;
structural parsing must not infer safety from offset sorting. Overlapping or
backward object regions are rejected unless a later contract defines sharing.

The parser returns bounded byte views or copied bounded metadata equivalent to:

```text
MdxDocumentV1 {
  original_title_bytes
  pdx_reference_bytes
  voices: map<u8, Voice>
  tracks: [TrackView; 9]
}

TrackView { logical_channel, target_kind, source_offset, source_length }
target_kind = ym2151(channel 0..7) | legacy_adpcm
```

Text decoding is outside structural validity. The original title and PDX bytes
are retained; display conversion may replace invalid Shift_JIS sequences but
must not alter structural parsing or dependency identity. The PDX reference is
metadata only. It is never opened by the engine and must later resolve through
a safe logical-blob dependency port.

An LZX body signature is `unsupported_compression`, not an offset table.

## Voice model

Each voice record is exactly 27 bytes: voice ID, feedback/connection, slot mask,
then six four-operator groups in M1, M2, C1, C2 order: DT1/MUL, TL, KS/AR,
AME/D1R, DT2/D2R, and D1L/RR.

V1 requires bits 7:6 of feedback/connection to be zero and masks no documented
field silently. Duplicate voice IDs are `duplicate_voice`; first-wins or
last-wins behavior is not accepted. A selected voice ID absent from the parsed
map is `missing_voice` before the corresponding note can emit operations.

## Command admission

Structural decoding recognizes command boundaries, but sequencing support is
granted opcode by opcode through a capability table. V1 must never guess the
length of an unknown or unsupported variable command.

The initial accepted FM subset is:

| Bytes | Semantic action |
| --- | --- |
| `00`–`7f` | rest for opcode + 1 ticks |
| `80`–`df`, `duration` | FM note and duration + 1 ticks |
| `ff value` | set global YM2151 Timer B |
| `fe register value` | ordered direct YM2151 write |
| `fd voice` | select parsed voice |
| `fc pan` | set FM output phase/pan |
| `fb volume` | set FM attenuation/volume value |
| `f8 gate` | set gate/staccato value |
| `f7` | suppress the next note key-off |
| `f6 count 00` | repeat start |
| `f5 rel16` | repeat end/back edge |
| `f4 rel16` | final-repeat escape |
| `f3 signed16` | detune |
| `f2 signed16` | per-tick portamento delta |
| `f1 00` | track end |
| `f1 rel16` | track loop/back edge |
| `f0 ticks` | key-on delay |
| `ef channel` | release a waiting channel |
| `ee` | wait for channel synchronization |

`ed` and `e9`–`ea` are recognized fixed/conditional-length commands but remain
unsupported until exact golden traces approve them. `eb` and `ec` likewise
remain unsupported until software LFO arithmetic and edge order are frozen.
`fa` and `f9` remain unsupported until their audible attenuation direction is
resolved by trace. `e8`, all `e7`, all `e6`, and `e0`–`e5` are unsupported.
Encountering one returns its stable error and byte offset before executing that
instruction. Unknown extensions are never skipped.

The P track is decoded sufficiently to classify it as inert only when it
contains no note/sample or device-affecting command before its terminal `f1 00`.
V1 admits playback only when P is inert. Otherwise preparation returns
`unsupported_pcm` before emitting any operation. This rule preserves P bytes
and semantic target kind for a future PCM sink.

## PCM extension seam

Parser output is device-neutral. The sequencer consumes logical tracks and
submits typed semantic actions to an injected sink registry:

```text
EngineDeviceSink {
  capabilities() -> fixed capability set
  prepare(action_batch) -> accepted | typed rejection
  commit(action_batch) -> accepted | backpressure | device_fault
}

Semantic target kinds have stable identities distinct from `DeviceId`:
`ym2151`, `legacy_adpcm`, and `pcm8`. A routing adapter maps a target kind and
logical channel to a registered device and generic operation contract. No MDX
component knows RTL ports, MMIO addresses, audio mixing, or filesystem paths.

Preparation is transactional across every target required by the document. If
any required sink or capability is absent, no batch is committed to any sink.
M3 supplies only the YM2151 route, so active P and all PCM8 intent deterministically
return unsupported. A later PCM contract must define sample operations, clocks,
backpressure, reset, and mixing; it may extend the registry without changing
the v1 parser result or FM action trace.

The current `DeviceOp` v1 stream remains the YM2151 routing output. PCM actions
must not be encoded as fake YM2151 registers or platform-specific payloads.

## Tick semantics and ordering

Timer B byte `T` defines the exact rational tick duration at the historical
4 MHz OPM clock:

```text
tick_seconds = 1024 * (256 - T) / 4,000,000
tick_rate    = 4,000,000 / (1024 * (256 - T))
```

The sequencer retains `T` and converts elapsed driver ticks to the scheduler's
integer domain with a checked rational accumulator. Remainders carry forward;
rounded BPM and floating point are display-only. A Timer B command affects
subsequent elapsed intervals at the exact service point where it is decoded.

At each driver tick, logical tracks are serviced in A, B, C, D, E, F, G, H,
then P order, matching the examined MXDRV interrupt traversal. V1 emits only
A–H work because P must be inert. Operations at an equal scheduler timestamp
retain this channel order and the exact within-command register-write order.
Direct OPM writes participate in the same order; no sorting by register,
device, or operation kind is permitted.

Parsing, gate expiry, key-on delay, portamento, repeat control, synchronization,
and Timer B changes are driven by engine ticks, never video frames, UI calls,
audio callbacks, wall time, or filesystem activity.

## Limits and termination

Every call receives explicit limits. A caller may lower but never exceed these
v1 hard ceilings:

| Resource | Hard ceiling |
| --- | ---: |
| MDX input bytes | 1,048,576 |
| title bytes | 4,096 |
| PDX reference bytes | 255 |
| logical tracks | 16 recognized; 9 accepted |
| voice records | 256 |
| decoded instructions per track per tick | 4,096 |
| control-flow branches per track per tick | 4,096 |
| simultaneously active repeat frames per track | 64 |
| pending semantic actions in one prepared batch | 4,096 |
| requested driver ticks per sequencing call | 1,048,576 |
| total instructions per sequencing call | 16,777,216 |

No allocation occurs before the relevant size/count limit is checked. All
offset addition, signed-relative target calculation, tick accumulation, pitch,
portamento, and timestamp conversion use checked fixed-width integer arithmetic.

Infinite track loops and indefinite synchronization are valid musical states.
The caller advances playback through bounded tick requests. Exhausting an
instruction, branch, batch, or call budget returns `budget_exhausted` without
hang or unchecked partial commit. Runtime repeat counters and sync state belong
to one playback instance and reset on stop/reload.

## Stable results and diagnostics

The v1 result set distinguishes at least:

```text
ok
input_too_large | title_too_long | pdx_reference_too_long
missing_title_terminator | missing_pdx_terminator
arithmetic_overflow | range_outside_input | overlapping_regions
invalid_track_table | unsupported_pcm8_layout | unsupported_compression
invalid_voice_record | duplicate_voice | missing_voice
truncated_instruction | invalid_branch_target | invalid_repeat
invalid_sync_channel | unsupported_opcode | unsupported_extension
unsupported_pcm | missing_device_capability
budget_exhausted | time_overflow | device_backpressure | device_fault
```

Where an input byte caused the result, diagnostics include its bounded absolute
byte offset and, when known, logical track. Diagnostic text is optional and not
stable. A rejected parse or prepare call leaves output empty and changes no
engine, scheduler, or device state. Backpressure after an accepted transactional
batch preserves pending order and timestamps according to the scheduler/device
contract.

## Determinism and conformance

For identical input bytes, limits, capabilities, initial state, and bounded tick
requests, parser results, semantic actions, YM2151 operations, timestamps, and
errors are byte-identical. Host locale, pointer values, source paths, rendering,
snapshot publication, and wall time are not observable inputs.

Conformance requires self-authored exact-byte fixtures for every accepted
opcode and malformed boundary. FM event traces must be checked against the
pinned MXDRV-derived implementation and an independent decoder or emulator
register trace. Third-party implementations are research oracles, not runtime
dependencies, and copyrighted corpus bytes are never committed.
