# Core playback transport v1

Status: adopted for the transport portion of M6 slice 3, 2026-09-13, from
[the transition proposal](../docs/design/pocket-player-transition-contract.md).
This is an internal Core control contract, not PlayerCommand/PlayerSnapshot v1
or the [public schema 2 observations](player-state-v2.md). UI cannot include these implementation types.
The schema 2 ingress wraps this control contract; policy, loop/gain, history,
settings and UI remain subsequent parts of slice 3. This establishes the asynchronous transport they
will drive; it does not substitute mock playback for the eventual engine/RTL.

## Ownership and step order

TransportController uses a const CatalogSession for generation/ID validation,
a PreparationPort and an AudioTransportPort. One Core control owner serializes
all calls. Ports never render, read physical buttons or mutate UI. Each step
receives monotonic real microseconds and at most 32 already-admitted Core intents.
It returns one decision per intent, in FIFO order. Public command IDs, replay,
optimistic snapshot sequencing and queuing remain the schema 2 ingress's job.
Intent IDs are nonzero provenance, not a second duplicate detector.
The [schema 2 ingress](player-command-v2.md) uses the same pure transport
projection as execution and supplies an optional admission context for its
already accepted batches. That mode interrupts a batch on catalog/backend
context change and reports an unexplained disagreement as terminal Protocol;
it never turns an accepted public command into a second public rejection.

A step samples audio once, handles shared device faults and catalog changes,
applies intents, consumes control/preparation completions, checks deadlines,
publishes committed media position/end, then starts at most one operation on
each port. This gives faults priority over commands and commands priority over
old completion/end notifications. An oversized batch is rejected before access;
control/fault/deadline service continues. There are no busy waits or allocations.

Supported intents are PlayTrack, LoadTrack, Play, Pause, Resume, TogglePause and
Stop. Select intents carry library generation and TrackId. They check catalog
availability, generation and logical ID before disturbing sound. Other intents
must not carry selection fields. Malformed/invalid/unsupported/busy/terminal
requests leave the intended transport unchanged. A valid selection during
recoverable Error requires confirmed reset and quiescent ports.

The internal snapshot reports actual transport, selected identity, current play
generation, committed position in 48,000 Hz stereo frames, projected transport,
last pending intent, current port operations, failure and confirmed silence.
It also copies the catalog status synchronized by this step and declares pause
capability for the [schema 2 publisher](player-state-v2.md). `prepared` requires
a selected track in the prepared generation; storage retained after terminal
failure and library close is ownership in flight, not a prepared selection.
Projection permits Pause then Resume while Pause is in flight. Loading a new
track still projects Loading, so Play/Pause/Resume/TogglePause there are rejected.
No-op Play while projected Playing, Pause while projected Paused, and Stop while
projected Stopped preserve generation and do not duplicate controls. Resume is
valid only while projected Paused. Empty/unselected playback is invalid.

## Lifecycle and resource identity

Play generation advances on selection, destructive Stop, restart of an
unprepared/Ended selection, and catalog invalidation. It starts at zero; issued
generations are nonzero u64. Port operation IDs are a separate nonzero monotonic
u64. Constructor seeds represent the last values issued by the containing Core
session if replacing this component. Neither counter wraps; exhaustion is a
terminal resource failure. A backwards real clock or zero timeout configuration
is likewise terminal, rather than disabling deadlines.

Every catalog generation change cancels preparation, clears selection and
requires a new audio reset. After reset, Ready means Stopped without selection,
Empty means Empty, Loading remains Loading, and catalog Error means library
Error. Same-library track preparation never invalidates CatalogSession.
The owner must retain old library bytes until preparation and audio have become
quiescent; catalog invalidation alone does not release those borrowed buffers.
Controller and ports must outlive their outstanding work. Destruction is not
cancellation or silence. Shutdown closes the catalog and services the controller
until reset and preparation quiescence; a terminal unconfirmed reset requires
platform-level quiescence before releasing the old source. A late valid reset
can release prepared storage while terminal Error and output inhibition remain.
Library validation finishing within a generation cannot clear a shared playback
failure; recovery still requires confirmed reset and explicit new selection.

Selection enters Loading, requires reset, then prepares. PlayTrack starts after
successful preparation; LoadTrack stops prepared at position zero. Stop cancels
preparation, retains selection, requires reset and confirms Stopped at zero only
after reset succeeds. Destructive operations report Loading while settling;
Error is reported immediately with silence confirmation separate. Play from
prepared Stopped starts it; otherwise Stopped/Ended Play prepares a new generation.
Explicit selection always starts Playing, including from Paused.

Only one sound control is outstanding. A replacement intent cannot reuse an old
reset ACK as its reset completion: it waits for that operation, then issues its
own generation-tagged reset. Old Start/Pause/Resume success never changes the
new intention. Shared control failure, reset failure and device fault are never
discarded merely because their operation's generation became old.

## Preparation port

begin(request) is nonblocking; false means it started nothing and is quiescent.
The request carries operation ID, play generation and logical selection.
poll(operation_id) returns Pending, Ready, Failed or Cancelled. A terminal poll
means the operation will no longer touch its workspace/source. Ready retains a
prepared session until release(); it does not start sound. Cancel requests are
nonblocking and must still be polled; requesting cancellation is not completion.
Cancellation is sent once. A cancelled operation may race with Ready/Failed;
obsolete results are discarded without replacing the current selection/error.

There is one preparation workspace. The next preparation begins only after the
old operation is quiescent and sound reset is confirmed. A prepared session in
use by sound is released only after reset. A preparation timeout records Error,
requests cancellation and retains ownership until terminal poll; late Ready
does not start playback. Recovery stays busy while that ownership is unresolved.

## Audio port

The port declares whether true state-preserving pause is available. Unsupported
Pause/Resume/TogglePause, including Play used as Resume, are rejected.
Capability is sampled by a control step, not by snapshot reads or submission.
begin(control) is nonblocking; false means
no request was started. Controls are Reset, Start, Pause and Resume, each with
operation ID and play generation. Start uses the successfully prepared session.
observe() is nonblocking and returns shared fault, committed play generation,
media frame, committed natural-end flag and at most one consumed completion.
Completions echo ID, generation and control kind and carry Success/Failed plus
the committed frame. Reset success proves silence/quiescence and frame zero.
Pause success proves a frozen position; Resume success keeps that position.
Start success confirms playback. In-flight means pending, never assumed success.

Old unrelated completions are ignored without retiring a current operation.
A matching ID with wrong generation/kind or an invalid outcome is a terminal
protocol error. For current playback, committed position cannot go backwards;
while confirmed Paused it cannot advance. Pause/Resume ACK positions obey those
same constraints. Regressions or a violated freeze guarantee cause failure.
The port, not Core's polling/render cadence, owns media timing. A committed end
is retained during Pause and applied after Resume. Stop/reselection invalidate
old end notifications. End requires reset, retains final position/selection and
confirms Ended; the later order/repeat coordinator decides any next selection.

emergency_silence() is a nonblocking, latched output inhibit independent of the
normal control mailbox. It does not claim reset completed or release buffers.
Only a successful explicit Reset may clear the inhibit. The hardware adapter
must prove its bound; no MMIO/cycle count is adopted here. Control timeout,
reset failure, protocol/resource/clock failure latch terminal Error and inhibit
output; no new normal controls are issued. A timely non-reset control failure
or device fault requests reset and remains recoverable only after success and
fault clearance. Failure of an old shared operation has the same consequences.
A Reset ACK observed with a shared fault is terminal ResetFailed. Reassertion
after a successfully cleared/reset fault starts a new failure/reset generation,
even while the earlier DeviceFault error still awaits recovery selection.
Stop cannot turn a failed reset into a successful-looking state.

## Deadlines and verification

Preparation and control deadlines are injected positive u64 microsecond durations.
Elapsed subtraction avoids deadline-addition overflow. A terminal completion
observed on the deadline wins over timeout; only still-pending work times out.
Timeout does not mean cancellation/quiescence. Polling outstanding work continues
after failure, but a late success never clears terminal Error. Real hardware
values must be derived and adopted in M6 slice 4, not guessed from host tests.

Host tests use scripted ports and independent operation traces for startup reset,
prepared vs immediate playback, rapid replacement/cancellation, pause/resume
projection, stale end/ACK, shared failures, exact deadlines, backwards clocks,
counter exhaustion, borrowed-workspace release and unsupported pause. Verify
that rendering is absent and old v1 gates still pass. Native target linking is
evidence of toolchain compatibility only; no true pause/audio result is inferred.
