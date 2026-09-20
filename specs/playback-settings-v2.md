# Persistent playback settings — schema 2

Status: adopted for M6 slice 3, 2026-09-13. Implements the host-verifiable part
of the [settings proposal](../docs/design/pocket-playback-settings-contract.md).
The [policy profile](playback-policy-v2.md) remains authoritative for playback:
desired policy changes immediately, same-value commands are no-ops, and policy
revision exhaustion remains a terminal ResourceExhausted. Storage failure does
not change policy, stop playback or become an audio/reset result.

## Public observation and startup

Pocket deployment note (2026-09-20): the user deferred persistence for M6.
That build does not inject this optional controller or advertise its capability;
each launch uses the policy profile's defaults. The contract below remains
unchanged for consumers that opt into persistent settings.

Capability bit 7 (`kPlaybackSettings`) declares an optional copied `settings`
value in PlayerSnapshot. Absence preserves existing constructors/backends.
It contains the current policy revision, optional persisted revision, restore
state, save state and optional stable storage error. The policy itself remains
in the existing policy observation; the revisions must agree.

Restore states are Reading, Restored, Missing, Recovered, Invalid, Conflict,
Unsupported, IoError and TimedOut, in that order starting at 0. Save states are
NotSaved, Pending, Writing, Saved, Failed and Unavailable, starting at 0.
Persisted revision, when present, is nonzero and not greater than current.
Saved requires equality. Failed/Unavailable require an error; other states
have none. Restore state records startup history and is not rewritten by a
later successful save. Restoring a valid record makes session revision 1 Saved;
the record sequence and session revision are independent counters.

Storage errors (starting at 1) are Io, Unsupported, InvalidRecord, Conflict,
Timeout, Protocol, SequenceExhausted, RequestExhausted, Clock,
InvalidConfiguration, NotDurable, ReadbackMismatch, UnsupportedBackend and
NotQuiescent. These do not use PlaybackErrorCode.

An injected settings controller initially blocks all commands except Stop with
ResourceBusy after syntactic validation. It starts its read on the first Core
step. Successful/default restoration sets only the initial desired policy,
without a command ID, revision increment, selection, or automatic playback.
No late result can replace policy after startup was released. Unsupported
repeat/shuffle backends retain their default policy and report Unavailable /
UnsupportedBackend instead of advertising successful restoration.
Storage is stepped by the Core owner after transport work; reads/submission and
audio/render callbacks do no storage I/O. Publication can be skipped entirely.

## Record and slot selection

Each logical slot is Missing, Bytes (bounded array plus independently checked
length), IoError or Unsupported. A record is exactly 64 bytes, little-endian:

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 4 | ASCII RPS1 |
| 4 | 2 | version 1 |
| 6 | 2 | length 64 |
| 8 | 8 | sequence, 1..UINT64_MAX |
| 16 | 1 | order 0 AlbumOrder / 1 ShuffleLibrary |
| 17 | 1 | repeat 0 Default / 1 RepeatOne / 2 Counted |
| 18 | 2 | flags: count present in bit 0, other bits zero |
| 20 | 4 | count >=1 if Counted, otherwise zero |
| 24 | 36 | reserved zero |
| 60 | 4 | CRC-32/ISO-HDLC of bytes 0..59 |

Use the existing container CRC implementation (poly EDB88320, initial/xor-out
FFFFFFFF; independent `123456789` check CBF43926). Never serialize a C++ struct.
Validate bounded length before access. Recognizable RPS1 with at least six
bounded bytes and a non-v1 version is Unsupported, even if its remaining bytes
do not follow v1. Invalid v1 data never contributes a sequence or policy.

Choose the greatest valid sequence. Equal sequence/equal bytes chooses A;
equal sequence/different bytes is Conflict, restoring defaults. One valid plus
invalid is Recovered, one valid plus Missing is Restored. Both Missing is
Missing/NotSaved; no valid record with invalid bytes is Invalid/Failed.
Any unreadable slot prevents using another as the latest: defaults/IoError.
Any Unsupported slot protects both slots from writes for the session and sets
restore Unsupported/save Unavailable; an otherwise readable valid slot may
supply policy, but is not claimed Saved. Unsupported has priority over IoError.

Do not automatically save defaults after Missing/Invalid/Conflict/IoError.
A later actual user policy change permits a new attempt. Write the slot other
than the latest valid one, B for identical copies, A if neither is selected.
The next sequence is one greater than the maximum validated sequence, or 1
if none. In Conflict this preserves one conflicting record while explicitly
writing the user's new policy at a greater sequence. Never erase the latest
record first; sequence exhaustion makes saving Unavailable.

## Asynchronous ownership

PlaybackSettingsPort has bounded nonblocking BeginRead(request ID),
BeginCommit(request ID, slot, immutable record, policy revision), Poll,
Cancel(request ID) and Quiescent(request ID). Begin false guarantees no acquired
ownership or side effects. Accepted BeginCommit copies its input; the port
does not borrow mutable controller bytes. Read and commit are serialized with
at most one active request. Poll returns at most one completion per Core step.
Completion echoes ID, operation and revision (zero for read); commits also
echo slot and return readback bytes plus an explicit durability guarantee.
Success requires matching identity, confirmed quiescence, durability and exact
64-byte readback equality. Merely reading the new bytes is not durability.
The owner supplies an exclusive, initially quiescent port. A replacement
session must not reuse request IDs until prior work is quiescent or the platform
has reset it. Cancel tolerates an already-completed request: a result may have
become quiescent before the owner observes that its deadline has expired.

Only the newest unsaved revision is retained. Save begins 250,000 microseconds
after its last change on the monotonic wall clock, including while paused.
An active write finishes using its captured bytes/revision; an older successful
write may update persisted revision, but never declares a newer revision Saved.
Then the latest pending value is saved. Same-value commands do not retry.

Failures stop automatic retries, including a newer value already queued at the
time of failure. A subsequent actual policy change permits one new attempt.
After uncertain write/read failure, retry must first confirm old I/O quiescence
and reread both slots; this reread determines sequence/slot only and cannot
restore over the current policy or establish durability of an uncertain write.

Read, commit and cancellation-quiescence timeout durations are injected,
nonzero microseconds; actual Pocket values require slice 4 evidence. At the
deadline timeout takes precedence over an unconsumed completion. Cancel is
requested once; time passing is not proof of quiescence. Until confirmed, no
new operation starts. Quiescence timeout disables storage for the session.
Old-ID completions are ignored; a matching ID with inconsistent kind/revision/
slot or non-quiescent completion is Protocol failure and requires cancellation.
Request IDs, record sequences and wall time never wrap; invalid configuration
or reversed time disables storage. The port outlives pending work.

## Acceptance and remaining integration

Host tests cover exact authored record bytes, CRC/length/enum/count/reserved
rejection, both-slot selection, unknown format protection, every interrupted
write prefix, startup gating/no autoplay, changed-during-write revisions,
failure/retry/rescan, stale completion and timeout/quiescence races, independent
clock and publication frequency, bounds and unavailable backends. Fixed buffers
and one pending value keep memory independent of command volume.
The actual APF slot/flush/readback adapter, timing bounds and power-cycle tests
remain M6 slices 4–6. This profile alone does not complete Q19 on hardware.
