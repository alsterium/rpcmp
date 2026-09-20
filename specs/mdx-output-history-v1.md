# MDX output history owner v1

Status: implementation contract within the CPU connection plan approved on
2026-09-13, M6 slice 4. This connects the copied
[MDX producer checkpoints](mdx-source-producer-v1.md) and actual
[output records](pocket-output-journal-v1.md) to the existing committed
[performance history](performance-history-v2.md). No public schema, playback
policy or physical input behavior changes.

## Ownership

One Core owner retains at most 128 display batches, independently of audio
retention and UI reads. Begin requires a confirmed sound Reset, strictly newer
nonzero play generation and epoch; common hardware reset requires a fresh
owner. Cancel closes this display owner, not the device. Old-generation/epoch
input is inert. Each successful producer checkpoint is copied before offering
its first audio item. Validate counts, token span, source order and event write
indices before accessing fixed arrays. The marker token is preceding token +
write count + 1, without wrap. Duplicate batches are not inserted twice.

The M6 target's 64-item audio read-ahead can contain 64 zero-write tick
markers, plus a producer batch awaiting space. A further 32 output records
can await CPU acquisition. The original 16-batch bound evicted not-yet-output
checkpoints continuously during sparse music, preventing any current-channel
publication. The internal bound is revised to 128 for that integration window.
It is not a guarantee of lossless history for arbitrary bursts or reader stalls;
overflow and recovery still follow the rules below. No audio ready signal or
public snapshot capacity depends on this display storage.

When full, discard the oldest display batch and count its omitted events,
including its existing prefix loss. Retain the newer independent channel
checkpoint. Missing/malformed display data marks capture loss and may defer
current values; it never stalls the producer, raises an audio fault or changes
device offers. Recovery requires a subsequent valid completed checkpoint.
An unknown source span before a newer retained batch travels with that batch;
it must not mark an earlier completed checkpoint as lost before the gap is output.

## Exact event mapping

An event's logical token is preceding token + after_write. Its frame is the
first output record whose prefix includes that token. Record sequences and
prefixes must not go backwards; ordinary records advance prefix, while a
natural-end record may repeat it. Output frames increase between records;
multiple batches/events may map to the same record/frame. A natural-end record
applies the pending checkpoint without claiming its PCM was consumed.

Keep the entire batch until its marker is included by an output prefix. Preserve
all already known event frames while later events/checkpoint remain pending.
Commit completed batches in source order, including several on the same frame.
Never advance observed-through past an incomplete batch's pending events and
then insert those events at an earlier frame.

A record sequence gap means some first-output frames are unavailable. Mark
unmapped event tokens below the arriving prefix as missing, preserving earlier
known frames. For an ordinary record, an event exactly at its advancing prefix
still has a proven frame; a natural-end record may repeat a prior prefix and
does not give that inference after a gap. Recovery from the independent latest
record follows the same rule, allowing sequence zero only for exhausted journal
recovery. After exhaustion, repeated/older zero-sequence latest copies are inert
by their monotonic output frame, preserving still-pending batches. No missing
event receives an estimated timestamp. The completed batch's
independent channels can still recover current state when all its writes are
covered; incomplete later writes keep public history Waiting.

Known omissions consume event sequence numbers at their actual positions in
source order. This requires an additive internal collector input: optional
per-change omitted counts and a trailing omitted count, in addition to the
existing batch-prefix count. A missing span before a retained event advances
the sequence before that event; trailing loss advances it after the final event.
Validate every span/count and checked total before inserting or numbering
events. Overflow preserves the existing Exhausted behavior, including updates
to the independently valid current channels, and never wraps. The public
event/snapshot structures remain unchanged.

## Publication boundary

Processing output records is separate from publishing a coherent transport
observation. The adapter supplies generation, epoch, through-frame, applied
prefix and natural-end kind proven for that observation. For ordinary playback,
the copied sound capture's selected output prefix provides that proof and its
consumed position is strictly after the corresponding record frame. A paused
capture requires a drained journal while the same pause remains held; natural
end requires its retained terminal record at the unchanged terminal frame.
The adapter must not substitute the currently queued or latest CPU-produced token.

The owner publishes only after the matching prefix has been processed and a
complete independent channel checkpoint covers it. A newer unseen prefix,
partially committed batch, unresolved display gap or missing terminal record
returns Waiting. A valid boundary may advance through-frame without adding
events when no later writes have changed the channel checkpoint. Repeated
immutable reads do not advance mapping, sequence or audio. Stop/error publication
uses the existing public Waiting behavior. MMIO polling and the enclosing sound
adapter remain separate from this host-runnable owner.

## Acceptance

Verify different event frames inside one batch, equal after_write indices,
several completed batches on one frame, zero-write/end, partial-batch deferral,
pause boundaries, stale/duplicate/malformed input and u64 arithmetic. Exercise
both the 128-batch retention overflow and a missing middle output record; retain
known timestamps and put sequence gaps only around omitted events. Recover
independent current channels after loss/exhaustion. Connect authored real MDX
and compare audio offers with dense, sparse and absent display reads. Measure
host and RISC-V ownership sizes; do not promote host mapping to Pocket playback.
