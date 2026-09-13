# MDX performance observations v1

Status: adopted for M6 slice 3, 2026-09-13. Adds read-ahead observations to the
transactional [MDX engine](mdx-v1.md), for the
[committed public history](performance-history-v2.md). Existing device bytes,
ordering, clocks and playback semantics are unchanged. This is not a new queue
or hardware completion signal.

## Engine observations

The YM2151 router maintains an observation shadow independently of its driver
state. It sees successful emitted writes, including direct writes addressed to
a different channel from the source track. Driver key flags are not proof of
hardware gate state. New engine state assumes a reset, key-off chip; pitch and
voice are unknown until configured. The owner still owes the real device reset.

Each channel observation contains known/unknown logical gate, optional pitch in
1/64 semitone steps above nominal KC=0, and optional applied MDX voice number.
Gate means at least one operator is keyed, not measured amplitude. Explicit
operator-mask rising edges produce KeyOn even when another operator was already
on. The transition to mask zero produces KeyOff. Identical nonzero masks do not
invent another attack. Normal retriggers preserve Off/On; ties do not fabricate
an attack. Gate expiry and delayed notes follow the existing emitted writes.

Normal paired KF/KC writes produce one PitchChanged after the pair; raw pitch
writes are observed individually. Pitch requires both registers to be known.
Canonical KC low nibbles are 0,1,2,4,5,6,8,9,A,C,D,E. Other encodings are reported
unknown rather than guessing undocumented aliases. Noise mode makes channel H
pitch unknown. This reports the base key pitch, not LFO/operator detune,
frequency multipliers, spectral fundamental or waveform measurements.

An applied voice is published only after its full operator load and control
write. SelectVoice alone does not apply it. Direct modification of known voice
registers invalidates its identity; writing identical bytes does not. Normal
volume/pan control preserves identity. A later full voice load can restore a
known identity; selecting the same cached voice cannot undo raw modifications.
CSM enables timer-driven attacks that this write observer cannot time. It marks
capture loss and all gates unknown; explicit key-off after leaving CSM restores
a channel's known gate. CSM does not change accepted writes or stop audio.
The capture-loss flag remains set on subsequent batches while any gate remains
unknown; the committed collector keeps its generation-wide loss flag.

Each successful router tick returns independent current channels, write count,
up to 256 events in source order, count of events evicted from the tick's prefix,
and an unknown-capture-loss flag. Event contains channel, kind, state after the
event and `after_write:u16` (1-based index in the emitted batch, <=8192). It may
share an index with another observation; it never reorders device writes.
The newest 256 observations survive excess capture. Capture capacity never
changes the write batch, causes playback backpressure or fails the driver tick.

`MdxEngineState.performance` stamps the batch at the start of the serviced tick,
in the explicitly supplied scheduler tick rate, including zero-write ticks.
Only a successful advance call produces an observation. Document, routing or
timestamp failure rolls back the observation shadow and batch with the engine.
Consumers copy each successful batch once; publication cadence is not a cursor.

## Mapping to committed history

The Core adapter takes an explicit play generation, mapped audio frame and
committed-through frame. It defers future batches without recording or losing
them; the owner retains them. It does not infer audible time from scheduler
time, queue insertion or UI time. On an eligible batch it converts independent
channels and events, then commits through PerformanceHistory. A supplied batch
must correspond to that boundary's final checkpoint; do not publish an older
checkpoint as current merely because its events are no longer in the future.
The actual sound/CDC queue must provide this mapping in M6 slice 4.

`MdxPerformanceBoundary.at_frame` is the final channel-checkpoint frame for the
whole batch. Optional event frames provide exactly one committed frame per
retained observation, in the event array's source order. They must be monotonic,
not later than the final checkpoint, and identical when events share an
`after_write` index. This lets a serialized sound adapter preserve different
audible commit times within one driver tick. Absence explicitly declares that
every event shares the checkpoint frame; it must not substitute for missing
per-write timing. An incomplete final checkpoint defers the entire display
batch; earlier event timestamps remain intact when the checkpoint arrives.

Pitch conversion declares the effective chip clock explicitly. The existing
[RTL adapter](ym2151-rtl-adapter-v1.md) uses 3,579,545 Hz; the MDX Timer B model's
4,000,000 Hz constant is not evidence of that audio clock. Mapping supports those
two explicit clocks, without modifying either. Unsupported clocks/malformed
observations are rejected as display data. The caller must not stop playback
because a display conversion fails.

At 3,579,545 Hz, KC $3E/KF 0 is MIDI-like note 60 (C-4), and KC $4A/KF 0 is 69
(A-4). KF uses its top six bits. These nominal mappings follow the primary
[X16 sound reference](https://github.com/X16Community/x16-docs/blob/master/X16%20Reference%20-%2011%20-%20Sound%20Programming.md).
For 4 MHz add 1200*log2(4000000/3579545) cents, quantized to 12305/64 cents.
Round to the nearest MIDI-like note, then nearest signed cent offset, with
halfway values away from zero. The clock correction is a displayed base-pitch
calculation, not a claim of measured chip frequency. The independent
[ymfm frequency derivation](https://github.com/aaronsgiles/ymfm/blob/main/src/ymfm_fm.ipp)
documents the nominal clock and operator/prescale relationship. No third-party
implementation is added as a runtime dependency.

## Acceptance

Authored bytes and register expectations verify normal retrigger, tie, delay,
portamento, raw cross-channel key/pitch/voice changes, zero operator mask,
unknown pitch, noise/CSM, prefix overflow and rollback. Existing register
fixtures remain unchanged. A generated engine-to-public-snapshot case proves
future deferral, correct committed ordering/current values and frame mapping.
Pitch checks include the nominal C-4/A-4 anchors and 4 MHz MDX pitch 45*64+5
mapping to note 60 with a rounded zero-cent offset. Host tests and cross linking
do not establish real audible timing, accurate Pocket pitch or hardware gates.
