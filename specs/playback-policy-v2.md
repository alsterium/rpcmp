# Playback policy — schema 2 repeat profile

Status: adopted for M6 slice 3. This extends the source-level
[command](player-command-v2.md), [state](player-state-v2.md) and
[transport](playback-transport-v1.md) contracts; v1 is unchanged. These C++
types are not a serialized ABI. The additive
[navigation profile](playback-navigation-v2.md) now connects album/shuffle order
when the Core owner supplies its random port.

## Commands and desired state

`SetPlaybackPolicy = 7` carries exactly one `PlaybackPolicy` and no selection.
The seven existing commands prohibit this payload. Order is `AlbumOrder = 0`
or `ShuffleLibrary = 1`; repeat is `Default = 0`, `RepeatOne = 1` or
`Counted = 2`. Only Counted carries a count, from 1 through UINT32_MAX.
Default maps to two loop bodies, RepeatOne to an absent (unlimited) target,
and Counted to its count. Natural finite tracks play once except RepeatOne,
which prepares the same track again after an audible end.

Capability bit 3 declares policy observations; bit 4 declares repeat control.
Without the navigation extension, this profile accepts AlbumOrder only and a
valid ShuffleLibrary request returns UnsupportedCapability. The navigation
extension accepts it and implements its order; it must never be accepted and
ignored. Existing backends default to no repeat
capability and retain their existing behavior. Bit 4 requires bit 3.

The existing command identity, replay, sequence, payload, projection and queue
rules apply. Policy can change without a selection, during preparation or
pause, and in a recoverable error. Terminal failure still rejects it.
An identical policy is an accepted no-op. A changed policy increments a
nonzero Core revision (initially 1) and records its command ID. Exhaustion
latches ResourceExhausted rather than wrapping. Desired policy survives Stop,
reselection, catalog replacement and failures. Durable settings are a later
adapter; this contract does not imply persistence across process restarts.
As with other commands, a changed admission context can interrupt an entire
accepted batch before execution; it does not erase already applied settings.

## Audio application and observations

The optional port extension is declared by `supports_policy()`. A Start carries
the desired target and revision. Pause/Resume also carry the latest policy,
and a standalone `SetPolicy = 4` applies a change on an already started stream.
Reset prohibits the payload. Only one audio control is outstanding. Commands
may coalesce to the final policy before a request begins. Already issued
requests keep their captured payload, even if another policy is accepted.
Completion must echo operation, generation, kind, revision and target exactly.
SetPolicy retains position; while paused its ACK must name the held frame.
Existing injected deadlines and failure/quiescence rules apply unchanged.

The backend applies the control at a 48 kHz media boundary using the
[media loop envelope](media-loop-envelope-v1.md), and supplies its coherent
snapshot with the audio observation. A current started stream must report
the acknowledged revision/target and the same generation/frame as its outer
observation. Invalid/missing current media, or a changing policy capability
while the stream is active, causes explicit failure and silence inhibition.
An old-generation observation is ignored; a shared device fault is never
ignored because its accompanying metadata is old.

Public policy state contains the desired policy/revision, last changing
command ID, and optional current-generation media: frame, applied revision
and target, loop count, phase, gain numerator, ramp elapsed/duration and end
reason. Loop counts fit u32 or are absent with an explicit overflow flag.
Gain denominator is 240000; fades last 240000 frames and restoration 960.
The published media frame equals the outer position. Applied revision may
lag desired revision; equality requires the matching target. Media clears
on invalidation/failure and may remain at an Ended track's final position.
Old schema 2 snapshots without the extension remain valid.

Acceptance is not audio completion. A queued policy cannot retrospectively
cancel an end already committed at an audio boundary. Finite RepeatOne
restarts only for an observed RepeatOne end while the desired mode is still
RepeatOne. End held during pause is acted on after resume. Internal restart
uses a new play generation and normal reset/preparation ownership, without
fabricating a public command ID. Automatic next-album-track and shuffle are
specified by the optional navigation extension; they are not implied merely
by the repeat capability.

## Verification

Use generated progress and the real envelope in a scripted audio port with
explicit media advancement. Verify desired/applied revisions around ACKs,
pause and preparation changes, exact fade/restoration boundaries, finite
RepeatOne restart, malformed/unsupported/duplicate/no-op commands, stale
observations, failures, capability changes and checked counter exhaustion.
Submission and copied snapshot reads perform no port calls; varying publication
frequency must not alter the audio/control trace. Host checks do not establish
the Pocket audio mapping, actual hardware freeze, MMIO or CDC behavior.
