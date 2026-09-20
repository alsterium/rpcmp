# Retained MDX source producer v1

Status: implementation contract within the CPU connection plan approved on
2026-09-13. This implements its copied, bounded MDX feed batch in M6 slice 4;
it preserves the engine and the [source queue](media-source-queue-v1.md).

## Ownership and sequencing

The Core producer owns one immutable audio batch of at most 8,192 writes and
one final marker. It stores shared time/loop metadata once and copies only the
address/value writes. It has no filesystem, MMIO, clock, UI or rendering access.
Its caller supplies an admitted immutable MDX document and engine workspace.
The adapter must confirm sound Reset before beginning a generation/epoch, and
retain library bytes until the enclosing preparation/audio lifecycle releases
them. Resetting this CPU owner alone does not establish physical silence.

Begin requires nonzero generation/epoch, a strictly newer epoch and a generation
not earlier than the preceding generation. Fresh objects start at epoch zero;
confirmed common hardware reset requires a fresh CPU owner as well. Begin
clears audio retention, read-ahead continuity, faults and logical token count.
The caller also starts a fresh engine state for that epoch; Begin does not
mutate the separately owned engine state or workspace.
Cancel closes CPU production and drops its copied batch; it neither releases
a borrowed device mailbox nor claims Reset success.

Before copying a batch or offering its first item, validate count <=8,192,
one common timestamp equal to the next expected start (initially zero),
nondecreasing loop count, write channels in A-H and room for every write plus
its marker in the u64 logical token sequence. Nonfinal next time must be
strictly greater than its start; final ticks use zero next time. Invalid input
changes neither retained data nor continuity. Exhaustion never wraps.
Markers carry loop/end metadata; writes have zero loops/until/end. Zero-write
ticks still produce a marker. Once the final marker is Accepted, no next tick
can be retained until a new confirmed Reset begins an epoch.

Taking an offer copies generation, epoch, logical token and all queue fields
to the adapter and marks one outstanding item. The adapter may retain it while
its CPU mailbox is busy. No second offer is produced until completion. Only
Accepted advances the cursor/token. Full releases this one outstanding offer
and permits the exact same item/token to be taken again. Invalid/Closed/Stale
or adapter failure for the current item latches a feed failure; the caller
must enter the existing transport fault/silence path. A mismatched generation
or epoch is an old completion and has no effect. A mismatched token, duplicate
current completion or unknown status is a protocol error, with no advancement.
The token is adapter-owned identity: MMIO echoes generation/epoch, while the
adapter retains the token of its one submitted offer. It must not infer a
token from the latest CPU cursor after reset.

Logical tokens count Accepted queue items, including markers. They correspond
to native tokens only if all items are dispatched in order within the same
sound epoch. Queue acceptance is not native or audible completion. Reset
discards the correspondence; no publication may treat queued items as played.

## Engine connection and display ownership

The engine helper advances only when the retained audio slot is empty and
open. It calls the existing transactional engine with scheduler rate
12,288,000 Hz. MDX Timer B remains 4,000,000 Hz and the audio chip clock remains
3,579,545 Hz. It advances a candidate engine state in caller-provided workspace,
validates/copies the complete result, then commits engine state and a copied
display checkpoint together. Busy/error does not retick or commit a candidate.
The large engine workspace is caller-owned persistent storage, not a temporary
per-call stack allocation.
The checkpoint contains generation/epoch, preceding and marker tokens, and the
successful tick's SequencedPerformance. This is still read-ahead data.

The enclosing history owner must copy a successful checkpoint into its separate
128-batch display retention before the next successful engine advance. Losing
display capacity must degrade history rather than stall this audio producer.
This layer does not implement that display queue, output journal or mapping;
those remain the following approved layer. Its pending audio ownership never
depends on UI snapshot consumption or event retention capacity.

## Acceptance

Verify maximum/oversized and late-malformed batches before any offer, copied
bytes after caller mutation, repeated Full on writes and markers, partial feed,
zero-write/end, duplicate/wrong/old completions, cancellation/new epoch and u64
boundaries. Connect authored MDX through the real engine, check integer clock
conversion independently and prove pending retries do not advance the engine.
Measure retained/workspace sizes and keep target allocation/link acceptance
separate. Host tests prove the producer; they do not certify Pocket service
speed, MMIO deadlines, output history or playback.
