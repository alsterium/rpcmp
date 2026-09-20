# Pocket MDX backend v1

Status: internal implementation contract within the approved CPU sound
connection plan, M6 slice 4. Connects the existing preparation/audio/history
ports through the [CPU client](pocket-sound-client-v1.md), retained MDX producer
and output-history owner. No public player schema, product controls, MMIO words
or watchdog value changes.

## Single owner and preparation

One platform owner implements PreparationPort, AudioTransportPort and the
read-only PerformanceReader. Service runs independently of rendering and public
snapshot reads. A service consumes each client's available result once, performs
at most one MDX driver tick, and submits at most one request on each free channel.
There are no hardware waits, heap allocations or implicit retry loops. The
existing bounded whole-document parse/admission runs while sound is reset;
its wall time and playback service cadence require target measurement.

Preparation borrows the validated logical library for the requested catalog
generation and resolves the track/blob by ID using the existing MDX admission.
CatalogSession gains a Core-only checked borrowed-library accessor, not a public
UI query. A stale/unavailable generation has no borrowed result. The library
owner still retains immutable bytes until preparation is quiescent and sound
Reset completes. The existing MDX preparation helper gains an overload accepting
separate scratch objects so preparation and playback share one engine workspace;
the old workspace API retains its behavior.

Only confirmed successful Reset enables preparation for its generation/epoch.
Startup epoch zero has no capture authority: the platform's expected INHIBIT
can latch hardware fault before the first Reset. A capture must match both its
requested epoch and its sampled epoch to the acknowledged nonzero epoch before
its health bits are interpreted. Reset acceptance advances the sampled epoch
before clearing old faults; a stale query returning that new epoch must not
undo a later Reset Success. Transfer/protocol failures still inhibit sound.
Each produced checkpoint is copied to display retention before its first audio
offer. Feed Accepted advances the retained producer; Full retries the same item
on a later service. Preparation is Ready after a Full response proves prefill,
or the complete finite source is accepted. No complete-tick requirement is
imposed on the 64-item queue; large ticks continue streaming after Start.
Cancellation stops new engine/feed work without treating Reset as complete.
Release after confirmed Reset retires the borrowed document. It cannot free
hardware-owned client copies or start an obsolete prepared selection.

## Controls and capture ordering

Begin retains one copied control; a transient client Busy is not exposed as a
failed begin. The Core's injected control timeout continues to bound this outer
ownership, and its emergency_silence cancels any control not yet submitted.
The client's approved watchdog starts on actual MMIO submit. No production
Core deadline or target service-period guarantee is introduced here.

Every control invalidates earlier media capture eligibility, including within
the same generation/epoch. Publish active media only from a capture submitted
after that control's completion. Completion can be returned without a current
media observation (outer generation zero); do not synthesize media from an ACK.
Core consumes the completion before a subsequently captured coherent position.
Captures made during a pending non-Reset control can report faults but cannot
advance position or restore old policy. During Reset, old-epoch health remains
historical until the Reset reply and fresh capture establish the recovery;
communication failures remain shared throughout. Reset establishes a strictly newer epoch and
clears old observations; old borrowed transfers still drain independently.

Shared communication/clock failures and coherent hardware faults inhibit sound.
A copied Reset success cannot clear a newer inhibit. Startup inhibit before the
first explicit Reset is expected. A capture from a cleared older epoch cannot
reassert a historical fault; current-epoch faults remain shared even when their
position is ineligible. Normal terminal closure stops source supply. A current
feed Closed is resolved against a capture submitted after that reply, so a
natural end is not misreported as a device failure. Unexpected closure fails.
Controller remains the authority for selection, policy, navigation and errors.

## Display acquisition

Process retained journal heads before using independent latest recovery; never
skip still-retained event times merely because latest is newer. Peek and Pop are
separate operations, and copied pre-Pop heads can be deduplicated by sequence.
Only a successful Empty query permits applying latest recovery. A lost or
malformed journal response makes display Waiting until valid acquisition resumes;
it neither inhibits audio nor stalls feed/control. Actual gaps use the existing
Degraded history semantics, without invented timestamps.

Ordinary playback uses the coherent capture's consumed output prefix. Held Pause
requires a journal Empty query submitted after a held-pause capture following
the current control ACK. Later captures of that same held pause retain the
proof; each new control invalidates it. Natural
termination requires the retained natural-end record at the captured terminal
frame. Only corresponding epoch/generation/position data can publish history.
Public reads copy previously published history and perform no device service.
Stop/failure or unproven boundaries publish Waiting, not read-ahead channels.

## Acceptance and limits

Use actual TransportController/PlayerSession with authored MDX/library data and
scripted independent MMIO/clock responses. Check preparation/prefill, partial
ticks and Full retry, cancellation/reselection, all controls, old same-epoch
captures across ACKs, Reset epochs, shared failures, terminal Closed races and
borrowed storage. Compare offers across absent/dense publication and journal
loss; verify exact known history frames and Waiting on unproven observations.
Run host/architecture/format/static, sanitizer and RISC-V memory/link checks.
Scripted results do not prove RTL execution, real CPU throughput, whole-Pocket
timing/decoder, storage, combined font/framebuffer memory or firmware acceptance.
