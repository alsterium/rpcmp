# Media loop envelope v1

Status: adopted for M6 slice 3, 2026-09-13, from the loop/fade requirements in
the [transition proposal](../docs/design/pocket-player-transition-contract.md).
This is the Core's executable audio-boundary model. It produces stereo sample
values and consumes sealed, mapped progress intervals. It does not advertise
Pocket pause/gain support, implement MMIO, or replace the remaining transport,
policy ingress, automatic navigation or real audio adapter integration.

## Ownership and inputs

MediaLoopEnvelope has one serialized audio owner. begin(generation, target,
policy_revision) starts a new stream at frame zero after the adapter has proved
the previous stream quiescent. Generation and revision are nonzero u64 values;
generations strictly increase within an instance. Target is an optional u32:
absent means RepeatOne, otherwise 1..UINT32_MAX completed loops. Default maps
to 2; the UI's 2/3/5/infinite choices do not limit this internal range.
begin validates before replacing any state. It does not itself reset a device.

All timestamps here are 48 kHz stereo-frame boundaries. frame is the next
source frame to consume, also the count of source frames already consumed.
The caller supplies one resampled signed-16-bit stereo sample per boundary.
advance returns output and consumed: paused/stopped/finished/failed calls output
zero without consuming the supplied source frame. The adapter must retain the
unconsumed source and all upstream synthesis/queue state. This model alone does
not prove a real FM engine can freeze that state.

The producer maps each complete engine tick into a ProgressInterval containing
play generation, nonzero sequence, at_frame, until_frame (exclusive), completed
loops:u64 and ended. The interval seals all progress in [at_frame, until_frame).
The first begins at zero with sequence 1. Further intervals are contiguous,
nonempty, have consecutive sequences and nondecreasing counts. No interval can
follow ended. All lengths/order/identity are validated before queue mutation.
The fixed FIFO has 256 entries; Full preserves its sequence/coverage state so
the exact same interval can be retried. Old generations return StaleGeneration;
future generations, gaps, regressions, zero widths and malformed sequences are
Invalid. Generation/sequence exhaustion never wraps. A closed stream rejects
new intervals. Admission failure never silently drops progress.

The adapter must map [MDX progress](mdx-progress-v1.md) and its next tick boundary
to proven audible frame positions, merging zero-width mapped ticks if necessary.
It must retain the corresponding device batch under backpressure and cannot
let audio pass unsealed progress. No identity mapping or fixed Pocket latency
is assumed here. The model raises Underrun if a source frame would be consumed
without sealed coverage; it does not stretch time, synthesize natural end or
continue silently. Finishing an already scheduled fade needs no further sample.

## One boundary

advance applies the following order, independently of UI reads/publication:

1. Shared device fault: latch failure and output zero. Fault beats Stop.
2. The supplied control: Keep/Pause/Resume/Stop and optional repeat update,
   tagged with play generation. Default untagged Keep means no control.
   An explicit action/update requires the current nonzero generation. Old
   generations are ignored and reported as StaleGeneration while the current
   stream continues; future/zero generations are Protocol failures. The shared
   fault is never ignored because of generation. The frame result separately
   reports None/Applied/StaleGeneration/Rejected for control disposition.
   Updates carry a nonzero revision. Higher revisions replace the target;
   equal revision plus equal target is idempotent. Stale revisions or changed
   payload under an equal revision are Protocol failures. Bad target/tag is
   also Protocol. A failed stream stays inhibited until a valid new begin
   after physical quiescence. These are adapter protocol errors, not UI
   admission results; the existing command ingress retains that responsibility.
   Before begin, only Keep without an update or a fault is meaningful; other
   controls are Protocol failures.
3. Stop closes the stream at its current frame and discards pending progress.
   Pause freezes frame, loop notifications and gain. Repeat updates while
   paused change policy/revision only; ramp reconciliation waits for Resume.
4. If running, apply the due sealed progress interval, then reconcile the
   latest target and finish/ramp conditions. Natural end finishes immediately
   without adding silent fade time. Its reason is RepeatOne when target is
   absent, otherwise NaturalEnd. The Core must reset/restart or navigate from
   this result; this model never fabricates a restarted source stream.
5. If still running, apply gain to both channels and consume exactly one
   source frame. An injected positive max_frames ceiling (default UINT64_MAX)
   fails ResourceExhausted before consuming beyond it or wrapping the position.

Raw progress reservations remain valid across policy revisions. They are not
immutable future fade commands: the audio boundary uses the current policy.
Thus a change before or exactly at a loop boundary cancels the old decision.
Stop/new generation discards the old reservations, including an old end.

## Gain and completion

Gain is an integer numerator 0..240000 with denominator 240000. Phase is
Steady, Fading or RestoringGain. Reaching a finite target starts a 240000-frame
fade at the current boundary. For initial gain S, endpoint E and elapsed n,
gain = S + trunc_toward_zero((E-S)*n/D), with n clamped to [0,D].
Stereo output is trunc_toward_zero(sample*gain/240000), using signed 64-bit
products. No YM2151 registers change. The boundary at n=0 retains S; the
boundary at n=D finishes with LoopLimit and consumes no further source frame.
For intro 10 seconds plus a 30-second body twice, fade starts at 3360000 and
finishes at 3600000. UI cadence is not an input.

Changing to an already reached target during Fading retains the original
anchor/deadline. Changing to infinite or a larger unreached target cancels it
and restores from the current gain over 960 consumed frames. Cancellation at
full gain remains Steady. A new reached target during restoration starts a
fresh 240000-frame fade from the then-current gain. Same target/revision or an
order-only policy change cannot restart a ramp. Paused calls cannot advance
either ramp. A change at the would-be fade completion boundary is applied
before that completion, so a valid cancellation can restore from zero.

snapshot returns a copied value. Its gain/elapsed describe the current next
frame boundary; automatic completion is resolved by advance at that boundary,
after controls. Finished/faulted streams output zero and retain their frame and
count until the next begin. A failure is not an End reason. Status and phase
are internal observations pending public schema 2 policy integration.

## Acceptance

Use authored progress intervals and signed stereo samples. Verify exact 70/75
second boundaries, positive/negative rounding, finite RepeatOne versus Counted,
fade cancellation/restoration, pause and policy/end races, stale epochs,
256/257 backpressure with retry, coverage underrun, counter ceilings and an
identical consumed sample stream with arbitrary UI reads and inserted pauses.
The later RTL implementation must independently reproduce these arithmetic and
ordering rules; passing this model does not establish real device timing.
