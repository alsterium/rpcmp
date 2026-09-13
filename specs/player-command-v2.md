# Player command schema 2 — transport profile

Status: adopted for M6 slice 3, 2026-09-13, from the
[transition proposal](../docs/design/pocket-player-transition-contract.md).
The v1 API and M0 implementation remain unchanged. This profile connects the
seven transport intents to the [Core transport](playback-transport-v1.md) and
[public state](player-state-v2.md). The additive
[repeat policy profile](playback-policy-v2.md) now connects settings to the
audio port. The [navigation profile](playback-navigation-v2.md) connects
album/shuffle order and explicit neighbours. The optional
[settings profile](playback-settings-v2.md) connects persistence and startup
gating; [performance history](performance-history-v2.md) has a separate capability.
Before an injected settings controller finishes startup, syntactically valid
commands other than Stop receive ResourceBusy. No storage port is called by
submission or public reads.

## Public boundary

`contracts::v2::CommandIngress::submit(PlayerCommand)` returns a copied
`CommandResult`. Commands and results are fixed-capacity source-level values,
not a C++ wire ABI. Snapshot capability bit 2 advertises this transport ingress;
the independent SnapshotPublisher does not advertise an ingress by itself.
The UI receives this interface and const SnapshotSource/CatalogReader only.
No command contains physical controls, UI views, engine state or file offsets.

PlayerCommand has schema_version=2, nonzero command_id:u64, optional
expected_snapshot_sequence:u64, CommandKind, optional TrackSelection and optional PlaybackPolicy.
CommandKind values are PlayTrack=0, LoadTrack=1, Play=2, Pause=3, Resume=4,
TogglePause=5, Stop=6, SetPlaybackPolicy=7, NextTrack=8, PreviousTrack=9.
Only PlayTrack/LoadTrack require a selection, with nonzero
library generation and TrackId; all other commands prohibit that field.
Unknown tags and malformed selection shapes are MalformedRequest.
Only SetPlaybackPolicy requires a policy; all other kinds prohibit it. Its
shape and capability rules are defined in the repeat policy profile.

CommandResult has response schema_version=2, command ID, outcome
Accepted/Rejected/Duplicate, stable reason, the latest published sequence
observed at submission, and optional original admission. Original admission
exists only for Duplicate and contains the original Accepted/Rejected outcome,
reason and observed sequence. It never recursively embeds another duplicate.
Different payloads under a retained ID cannot change its original result.

Reason values are None=0, UnsupportedSchema=1, InvalidCommandId=2,
DuplicateCommandId=3, StaleCommandId=4, StaleSnapshotSequence=5,
MalformedRequest=6, InvalidState=7, LibraryUnavailable=8, StaleLibrary=9,
UnknownTrack=10, UnsupportedCapability=11, ResourceBusy=12, QueueFull=13,
TerminalFailure=14, NoNextTrack=15, NoPreviousTrack=16. These describe admission,
not audio completion. Navigation availability and unresolved random ordering
are defined by the navigation profile.

## Admission and replay

One Core owner serializes submit, step, catalog mutation and publication;
reentrant or concurrent calls are not permitted. Submission performs no audio
or preparation operations, publishes nothing and never waits for an ACK.

Validation follows the M0 ordering, made explicit for this profile:

1. Supported command schema. Unsupported schema does not consume an ID.
2. Nonzero ID, retained replay, then strict high-water ordering. A retained
   result returns Duplicate; older unretained IDs are StaleCommandId. These
   outcomes neither enqueue nor extend the replay window.
3. A fresh ID advances the high-water mark. Expected snapshot sequence, when
   present, must equal the latest published sequence. Several accepted FIFO
   commands may intentionally cite that same sequence before a publication.
4. Payload shape, then the Core transport's identity/capability/state rules
   against a projection containing all earlier accepted commands. A session
   must have completed its first control step, and its transport must be
   synchronized to the current catalog, before it can admit work.
5. Queue capacity: exactly 32 commands, including accepted no-ops.

Each fresh supported nonzero ID consumes one of the 64 retained results even
when a later sequence, payload, state or capacity check rejects it. History
and queue use fixed arrays; no allocation or unbounded replay set is allowed.
Rejected candidates do not change the projection. QueueFull also leaves it
unchanged. IDs never wrap; changing an intent after rejection needs a new ID.

The same pure transport transition validates both projected admission and
execution. PlayTrack/LoadTrack project Loading; Play/Pause/Resume/TogglePause
there remain invalid. Pause then Resume while playing can both be accepted
before either ACK, using the preceding intent as projected state. No-op Play
while Playing, Pause while Paused and Stop while Stopped remain accepted and
do not advance play generation. LoadTrack still prepares without starting.
Play from Paused has the same pause-capability requirement as Resume.

## Control boundary, interruption and publication

PlayerSession owns transport, ingress queue and publisher. The platform calls
step with injected monotonic real microseconds independently of rendering;
each step drains the bounded FIFO once and may publish at most one normal
snapshot. Disabling publication for a step cannot change audio/control work.
Queue admission never directly starts playback from a UI callback.

At a boundary, shared device faults and catalog synchronization precede the
admitted batch, then control/preparation completions, deadlines and committed
position/end follow it. The first queued command records the admission context
(catalog status, play generation, failure/terminal state and pause capability).
If an external catalog change or backend fault invalidates that context before
execution, the entire batch is interrupted. An existing playback failure stays
authoritative; catalog interruption otherwise becomes recoverable Library error,
and a changed backend capability becomes DeviceFault. No later command from
that batch can restart sound. This is an asynchronous failure visible in the
snapshot, not a second user-visible rejection of an accepted command.

An otherwise unexplained projection/execution disagreement is a terminal
Protocol error. Resource/port failures during execution retain their actual
failure. A Reset ACK with an asserted shared fault remains terminal ResetFailed.
A fault that reappears after a successful reset is a new fault, even if the
previous DeviceFault error still awaits recovery selection. It inhibits output,
requests a fresh reset and prevents that selection from clearing the fault.

The session refreshes admission projection after every control step. Submission
against a changed, unsynchronized catalog is ResourceBusy after otherwise-valid
payload/transport validation; it cannot silently reuse the old selection.
External catalog changes without queued commands keep the existing catalog
lifecycle. Public OpenLibrary/CloseLibrary commands are outside this profile.

The session handles publication failures by inhibiting playback with a terminal
Protocol/ResourceExhausted failure and returning the publication failure to its
Core owner. It can publish the resulting fatal state if the namespace remains
usable; it never turns the failed publication result into success. On backwards
clock input, transport reports terminal Clock while publication retains a
nondecreasing timestamp. UI reads only copy the last complete snapshot.

The final sequence value UINT64_MAX is reserved by PlayerSession for a terminal
snapshot: ResourceExhausted unless an earlier terminal failure is already
authoritative. Further steps still service outstanding ownership,
but cannot issue another publication in that namespace. The Core-only step
result includes the current transport observation, so the owner can still
inspect quiescence after public sequence exhaustion. A new PlayerSession is a
new command/observation namespace and requires the old ports to be quiescent.

## Verification

Use authored public commands and scripted ports with the independent album
fixture. Verify no submission-side port calls; original-result replay; stale
IDs/sequences; malformed/unknown/stale selection before sound interruption;
32/33 queue and 64/65 history boundaries; compound pause/resume and T/U/Stop;
fault/closure after acceptance; command versus natural-end ordering; and
read/publication-frequency independence. Keep contracts-only mock ingress and
v1 compatibility checks separate from Core tests. Hardware transport, complete
UI and settings/policy acceptance remain the later M6 slices.
