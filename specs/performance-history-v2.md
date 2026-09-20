# Performance observations — schema 2 profile

Status: adopted for M6 slice 3, 2026-09-13. Implements the bounded observation
portion of the [history proposal](../docs/design/pocket-performance-history-contract.md).
This additive profile does not change v1 or the device queue. It is a copied
C++ value contract, not a wire ABI. Engine extraction and the actual audible
commit adapter remain separate integration requirements.

## Public values

Capability bit 6 declares `PlayerSnapshot.performance_history`. Without the bit
the optional value is absent. The value includes its independent eight current
FM channel observations: ID equals array index 0–7. This groups the history and
current values at one committed boundary instead of adding an unbounded channel
list. A channel has optional logical gate, MIDI-like note 0–127, signed cents
offset (only when note is known), and optional typed `MdxFmV1` voice number.
Unknown stays absent. No strings, register addresses or opcodes are exposed.
Note 60 displays C-4; frequency-to-note conversion is the adapter's responsibility.

History version is 1. Fields are play generation, observed-through frame,
next sequence (initially 1), count <=256, fixed event array, retention/capture
loss flags and availability. Each event carries sequence, frame, channel state
after the event and kind KeyOn / KeyOff / PitchChanged / InstrumentChanged.
Events require a known gate; KeyOn requires true and KeyOff false. Repeated
KeyOn is meaningful. Other same-value changes may be omitted by the adapter.

Times are 48,000 Hz media frames, ordered by frame then original source order.
An event at 100 is eligible only at a committed observation boundary >=100.
Read-ahead, queue insertion and rendering are not commit notifications.
Published observations must match both the selected play generation and position.
Current channel values come from an independent committed checkpoint, never by
replaying the retained history. Paused sources freeze both values and events.

Availability values are Available, Degraded, Exhausted, Waiting, Invalid.
The first three are the proposal's captured states. Waiting and Invalid refine
the proposal for a supported source that has not supplied a matching checkpoint,
or supplies malformed/future data. These placeholders contain no events, unknown
channels, next sequence 1 and no loss flags, with the outer generation/position.
Empty, Loading, Stopped and Error publish Waiting. Playing, Paused and Ended
may publish captured data. Unsupported is represented by absence, not Waiting.
The public validator rejects malformed display values. The Core publisher first
replaces malformed source data with Invalid, so a display fault cannot become
a transport failure. Old generation/position data becomes Waiting; future data
becomes Invalid. It never substitutes the previous song's observations.

## Bounded Core collector and read port

`PerformanceHistory` accepts only already committed batches, each containing a
generation, boundary, eight independent current channels, <=256 ordered changes,
known lost-event count before those changes, and an unknown-loss flag. Changes
must be between the previous and new boundary, inclusive. The adapter owns
read-ahead retention/mapping and must report all omitted changes as loss; it
must not call commit with future changes. This collector is not an audio queue.

The M6 CPU output-history owner additionally supplies optional per-change known
omitted counts (exactly one per retained change) and a trailing omitted count.
These internal fields default to zero/absent for existing callers. They preserve
event numbering when a journal loses a middle span rather than only a prefix.
Validate bounded counts/pointers and the total sequence advance before mutation;
assign each per-change omission immediately before that change and trailing loss
after all changes. Any known omission sets capture loss. Overflow retains the
same Exhausted behavior. Public snapshot/event values are unchanged.

Validate the whole batch before accessing bounded arrays or updating observations.
Reject older generations without modifying the current song. Other malformed
batches mark capture loss and preserve the last valid checkpoint. The caller
must continue audio; the result describes display capture only. A corrected
batch may subsequently be committed. `begin` accepts only a strictly newer,
nonzero generation and clears all events/flags. Stop's new generation uses the
same rule. No automatic reset based on a delayed producer message is allowed.

Assign event numbers before ring insertion. Known lost counts consume sequence
numbers; unknown counts set capture loss. The newest 256 events survive;
eviction sets sticky retention loss. Either loss makes availability Degraded.
Never wrap sequence: reserve UINT64_MAX as the unassignable next number, enter
Exhausted before overflow and stop recording for this generation. Independent
current channels and committed position continue updating while Exhausted.
No callback, allocation, audio backpressure or device-write result is involved.

The injected `PerformanceReader` copies an already committed Core observation.
It must not poll hardware, consume events, advance playback, or wait for UI.
The single Core owner serializes commit and copying; it is not a lock-free ISR
API. Snapshot publication can be skipped. UI reads only immutable copies and
never receives this port. The ring updates independently of publication.
Event size <=64 bytes; ring, two public surfaces and a UI window <=64 KiB;
collector plus these copies <=128 KiB. Host sizeof and target link measurements
are evidence for these values, not an integrated Pocket resource claim.

## Acceptance

- Authored same-frame On60/ch0, On64/ch1, Off/ch0, On60/ch0 retains four events.
- A batch containing future frame 100 at boundary 99 is rejected; a corrected
  commit at 100 publishes it in source order. No UI read can advance it.
- 300 events retain 45–300, next 301, retention loss; known/unknown capture loss
  is distinct. Current channels remain independent even with missing events.
- Pause and stale generation do not change captured history; Stop removes it.
- Reject length, version, channel, gate/note, order and boundary corruption.
- Exhaustion freezes event recording, not current state or audio controls.
- Headless, sparse and dense publications have identical scripted audio traces.

Tracker row formatting, engine-derived pitch/voice observations and real hardware
commit mapping are still required by M6; these host checks do not establish them.
