# M3 — MDX FM Parsing and Sequencing

Status: active on 2026-09-01. M2 is complete. The historical MXDRV and local
corpus investigation is recorded in `docs/research/mdx-mxdrv-format.md`; this
milestone starts by freezing `specs/mdx-v1.md` before parser implementation.

Progress: slices 1 through 3 and the bytecode-admission portion of slice 4 are
complete. The MDX v1 contract freezes the bounded FM scope and mandatory
additive PCM extension seam. The aggregate-only corpus auditor scans each
physical track region once without following control flow, stops at unknown
variable-length extensions rather than guessing, and emits no paths, titles,
PDX names, payload bytes, or per-file hashes. The structural parser now
validates immutable input transactionally, exposes fixed YM2151/legacy-ADPCM
track targets and decoded voice records through borrowed views, recognizes but
rejects PCM8, and passes every-byte truncation plus offset, overlap, voice,
limit, and malformed-header tests. The non-executing track decoder now admits
only the approved fixed-boundary subset, reports unsupported extensions without
guessing their length, and transactionally rejects active P-track intent.
The admitted bytes are also converted to endian-independent typed semantics,
with duration expansion and signed operands covered by exact golden cases.
Control-flow preparation now validates loop targets against decoded instruction
boundaries and proves the immutable repeat-start/end/escape relationships used
by the historical self-modifying counter scheme. Control flow and device traces
are executed by a per-playback track state machine with immutable source bytes,
bounded same-tick instruction/branch work, repeat frames, and explicit sync
wait state. Document ticks now commit A-through-H semantic batches and sync
release effects transactionally in historical service order. The initial
YM2151 routing stage emits transactional Timer B, direct-write, voice,
pan/pitch, carrier-level, and key-on register order. Gate key-off, tie,
key-on delay, and per-tick portamento now use the MXDRV-derived tick lifecycle.
Timer B intervals are converted to scheduler ticks with an exact integer
remainder accumulator and the historical `c8` initial value.
Timed FM writes can now be staged into the existing fixed-capacity scheduler in
allocation-free chunks while preserving order and resumable backpressure.
The document machine, FM router, and rational timeline now advance through one
transactional engine call, with complete document/control-flow preparation
before the first tick.
The first self-authored FM fixture now passes a reproducible differential check
against pinned independent `mdxtools`; exact-order agreements and the approved
note-write differences are recorded in the MDX research report.
Headless playback and playback interleaved with repeated real UI rendering now
produce identical timestamped device traces through the bounded scheduler.

## Objective

Parse untrusted 9-track MDX data and deterministically sequence tracks A–H into
ordered YM2151 `DeviceOp` values, with bounded failures and golden traces that
agree with independent behavioral oracles. Preserve a typed device-routing
boundary so PDX/MSM6258 and PCM8 support can be added without replacing the MDX
container parser, FM sequencer, or generic scheduler contract.

## In scope

- Freeze the MDX v1 structural, command, timing, error, and resource contract.
- Add a read-only corpus auditor that reports aggregate structure and opcode
  coverage without exposing titles, filenames, or music bytes.
- Implement a dependency-free, bounds-checked parser for ordinary 9-track MDX.
- Retain original title and PDX-reference bytes as bounded metadata.
- Represent all nine logical tracks with an explicit target kind: eight
  YM2151 tracks and one legacy ADPCM track.
- Implement the approved FM command subset with immutable source bytes and
  separate per-playback control-flow state.
- Preserve MXDRV's same-tick A-through-H service order and ordered register
  writes, including direct OPM writes and Timer B changes.
- Produce deterministic host traces and compare self-authored fixtures against
  an MXDRV-derived oracle and an independently implemented decoder.
- Keep parser, sequencer, and tests headless and independent of Pocket, UI,
  rendering, wall time, and concrete sound RTL.

## PCM extensibility requirement

Future PCM support is a mandatory design property, not M3 playback scope.

- Parsing must preserve the P track, the PDX reference, 16-track layout
  detection, and device intent instead of discarding or translating them into
  YM2151 operations.
- Sequencing must route semantic device actions through a typed engine-side
  sink. The M3 sink supports YM2151 register/reset operations; unsupported PCM
  actions return a typed capability error before any partial playback is
  admitted.
- The scheduler and player must be able to register more than one typed device
  sink in a later contract version. MDX code must not assume that every track
  maps to a YM2151 channel or that one `DeviceId` is globally sufficient.
- Adding PDX/MSM6258 or PCM8 must be additive: it may add device-operation and
  capability contracts, but must not change accepted 9-track FM bytes, FM
  event ordering, parser results, or the Core/UI boundary.
- PDX lookup remains a library/utility responsibility using logical blobs and
  safe dependency resolution; the engine never opens source paths.

M3 tests must include a fake unsupported PCM sink and prove deterministic,
all-or-nothing rejection. They do not synthesize, decode, schedule, or mix PCM.

## Explicitly out of scope

- PDX sample decoding, MSM6258 synthesis, PCM8 playback, or FM/PCM mixing
- Acceptance of the 16-track PCM8 layout or opcode `e8`
- `e6`, `e7`, and undefined `e0`–`e5` extensions
- Final metadata transcoding, library dependency matching, seek, or pre-analysis
- Pocket firmware growth, a production execution substrate, or new RTL
- Committing copyrighted MDX/PDX data or original driver binaries

## Implementation slices

1. Freeze `specs/mdx-v1.md`, including the PCM extension seam, stable errors,
   limits, accepted commands, integer timing, and service order.
2. Add the privacy-preserving corpus auditor and record aggregate command/layout
   coverage plus compatibility gaps against the local collection.
3. Add hand-authored byte fixtures and malformed boundary cases, then implement
   structural parsing only.
4. Implement the approved FM command subset and exact semantic/event traces;
   keep control-flow state separate from source bytes.
5. Differentially verify register ordering and timing against two independent
   oracles, document disagreements, and approve or narrow compatibility.
6. Connect the bounded MDX engine to the existing scheduler on the host and
   prove UI/render delay does not alter output. Pocket playback is a later
   integration gate and does not replace host acceptance.

Each slice must keep host verification green. A parser that has not passed the
sequencing slices must not claim MDX playback compatibility.

## Acceptance criteria

1. Every external length, offset, count, branch target, arithmetic operation,
   and allocation is checked before use and is subject to caller-supplied
   limits no greater than the v1 hard ceilings.
2. Ordinary 9-track files are parsed without modifying source bytes; malformed,
   compressed, extended, PCM-active, and resource-exhausted inputs return
   stable typed results with bounded offsets where applicable.
3. Self-authored fixtures cover every accepted opcode and every length,
   offset, voice, branch, loop, synchronization, and arithmetic boundary.
4. For identical input, limits, device capabilities, and requested tick span,
   repeated runs emit byte-identical ordered operations and results.
5. Timer B conversion uses exact integer/rational accumulation. Rendering,
   snapshots, audio callbacks, filesystem order, locale, and wall time cannot
   change sequencing.
6. Same-tick FM work is serviced A through H. Exact YM2151 writes match the
   approved traces from two independent oracle paths or a documented decision
   narrows the accepted command set.
7. Infinite musical loops and synchronization waits are supported during
   bounded playback but cannot make parsing, analysis, or a test call hang.
8. Active PCM and PCM8 fail before scheduling any operation in M3. Parsed PCM
   intent and dependencies remain representable for a later typed device sink.
9. Adding a fake PCM-capable sink requires no parser rewrite, no FM trace
   change, and no dependency from engines to platform, RTL, filesystem, or UI.
10. Core builds and tests headlessly, UI still builds from contracts and mocks,
    and dependency checks continue to reject `runtime -> ui`.

## Verification

- Every slice: `pwsh -NoProfile -File tools/host-verify.ps1`.
- Parser slice: exact fixture bytes, truncation at every byte boundary, hard
  ceiling tests, and deterministic result serialization.
- Sequencer slice: golden semantic traces, golden YM2151 operation traces,
  budget exhaustion, equal-tick ordering, and delayed-renderer independence.
- Oracle slice: record exact tool revisions, invocation settings, trace hashes,
  and every excluded or unresolved behavior. Private corpus bytes stay outside
  the repository.

## Known constraints and decision gates

- `DeviceOp` v1 contains only reset/register writes and the M2 scheduler is
  configured for one YM2151 device. PCM requires an additive device-operation
  contract and multi-device admission design before implementation.
- The historical source proves the service traversal A–H–P. M3 freezes A–H
  ordering; P ordering becomes executable only with an approved PCM contract.
- `fa`/`f9`, LFO edge behavior, synchronization, repeat escape, and arbitrary
  direct OPM writes require exact traces before their implementations can be
  accepted. The v1 contract may initially classify an unproven command as
  unsupported rather than approximate it.
- Pocket admission limits must be measured before hardware integration. M3
  hard ceilings are safety maxima, not a promise that every Pocket build can
  admit the maximum file.
