# Pocket output journal v1

Status: implementation contract within the CPU connection plan approved on
2026-09-13, M6 slice 4. This is display observation, never audio backpressure.
The [retained producer](mdx-source-producer-v1.md) supplies logical tokens;
the [native completion prefix](jt51-native-completion-v1.md) supplies actual
output correspondence. Neither CPU queue acceptance nor a timer is substituted.

## Output records

At a consuming serial boundary, record the selected pending sample's prefix
when it advances beyond the last recorded prefix. Use the active generation
and the pre-consume media frame (the zero-based index of that output frame).
The consumed position then becomes frame + 1. Bootstrap without a selected
sample, unchanged prefixes and nonconsuming boundaries do not produce ordinary
records. A consumed zero-amplitude sample still carries its prefix.

NaturalEnd and RepeatOne instead record the pending checkpoint actually applied
at the terminal boundary, with that unchanged media frame and natural-end=true.
This record may repeat the preceding prefix. Its PCM was not consumed. The tap
copies pending data before the boundary and confirms the resulting natural
termination before publishing it. Stop, fade completion, failure and Reset do
not publish unconsumed future checkpoints. Once terminated, no second terminal
record is produced. Session Reset discards the old epoch's journal and tap data.

Each record contains sequence, generation, frame and prefix (all u64), plus the
natural-end flag. Epoch belongs to the enclosing sound session and all entries
in a journal belong to its current epoch. Sequence starts at 1, advances for
every output record, and reserves UINT64_MAX as exhausted. It never wraps.

## Bounded retention and loss

The audio-clock FIFO retains 32 records in order. Reads do not pop it. A pop
must name the current head sequence; absent or different heads reject it.
Simultaneous successful pop and arrival retain the new record even when full.
When full without a pop, discard the new record and increment a saturating
u64 loss count. Existing head data remains stable. Sequence gaps reveal where
records were omitted. Exhaustion stops FIFO insertion and increments loss for
subsequent records; it does not stop audio.

An independent latest-record copy updates on every arrival, even when the FIFO
is full/exhausted. Its sequence is zero after sequence exhaustion. It supports
recovery of an independently retained complete engine checkpoint; it cannot
recover missing event times. The CPU marks such history Degraded and never
guesses the first output frame of a lost event. Loss count, latest, sequence
exhaustion and FIFO contents clear together on session Reset.

No ready/hold/fault signal returns to audio, native receipts or the MDX source.
This observation path must produce identical audio for frequent reads, delayed
reads, full retention and no reader.

## CPU ownership

The MMIO journal channel copies a query and one coherent response
across the existing mailbox mechanism. Peek and explicit pop are distinct
commands; a response stays immutable until release. Query validation includes
the expected sound epoch. A late old-epoch pop cannot remove a new head.
Normal session Reset clears the journal but does not clear a borrowed mailbox;
its response remains labelled with the original epoch. Common reset retains
the existing all-mailbox cancellation rule. Numeric MMIO placement is specified
in the journal extension to [sound MMIO](pocket-sound-mmio-v1.md).

## Acceptance

Check empty/single/full FIFO, simultaneous pop/arrival, pointer wrap, rejected
pop, loss saturation, sequence exhaustion and Reset. Compare real native writes,
selected prefixes and serialized output to independently derived records;
cover pause/resume, natural end with unconsumed PCM, Stop/fault and epoch reuse.
Run journal/MMIO independent-clock tests, affected native regressions, host
architecture checks and local fitted timing/CDC before Pocket integration.
