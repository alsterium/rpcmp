# Media envelope RTL v1

Status: adopted for the local M6 slice 4 gain/reservation implementation,
2026-09-13. `rpcmp_media_envelope` realizes the
[Core envelope contract](media-loop-envelope-v1.md) in the audio clock domain.
It accepts already-mapped progress intervals and signed 16-bit stereo frames;
it does not invent their audible mapping, expose MMIO, or advertise a backend
capability. I2S integration and sound-control CDC remain subsequent gates.

## Local ports and ordering

Data and control inputs are synchronous. `reset_n` asynchronously resets the
local controller; its release must be synchronized to this clock. `begin_valid`
offers a nonzero, strictly increasing generation, nonzero policy revision and
optional nonzero u32 loop target. `quiescent` must certify that the previous
physical stream is reset; a shared device fault prevents Begin. Accepted Begin
clears the queue and starts at frame zero, full gain, unpaused. An invalid Begin
preserves existing state. Begin does not reset JT51 itself.

`progress_valid` offers generation/sequence/at/until/loops/ended. The queue has
256 entries. Admission and validation follow the Core contract, including old
generation, closed stream, sequence exhaustion, contiguous intervals, monotonic
loops, nonzero width and Full without mutation. Full is tested before any pop
on the same edge; retry is explicit. A newly admitted interval becomes visible
on the following edge, so it must be admitted before its consuming frame tick.
Accepted Begin closes progress admission on that edge. An interval admitted
on an ending frame may be discarded with the closed stream.

The FIFO uses synchronous RAM plus bypass for a newly pushed head. No memory
contents need initialization; occupancy and head ownership gate every read.
Queued entries store at/until/loops/ended, while sequence/generation validation
is retained once per queue. No private music data is stored in test fixtures.

`frame_tick` requests one 48 kHz media boundary; tests may accelerate these
ticks without claiming real wall time. `control_valid` offers generation,
Keep/Pause/Resume/Stop, and an optional revision/target update for that boundary.
Control fields must be stable through the tick. Old-generation controls are
ignored; invalid current/future tags or policy regressions fail Protocol.
Equal revision/equal target is idempotent. Shared device fault has priority.

Boundary ordering follows Core: fault, control/policy, stop/pause, due progress
and natural end, ramp reconciliation/completion, resource/coverage checks,
then stereo gain and consumption. A policy change at a reserved loop boundary
or the would-be fade-completion boundary takes effect before the old decision.
Raw progress reservations remain valid across policy changes. Pause keeps
frame, loops, ramp gain and elapsed fixed; policy can change while paused.

The combinational `consume`, output stereo, boundary gain and control result
describe the offered tick using pre-edge state plus its control. The state
outputs after the edge describe the next boundary, as the Core snapshot does.
Consumers must use `consume` to preserve their pending source and upstream
state on a paused/closed/failed boundary. A frame counter never wraps;
`MAX_FRAMES` is a positive u64 ceiling, default UINT64_MAX.

Admission encoding: None=0, Accepted=1, StaleGeneration=2, Invalid=3, Closed=4,
Full=5, ResourceExhausted=6. Control: None=0, Applied=1, StaleGeneration=2,
Rejected=3. Phase: Steady=0, Fading=1, RestoringGain=2. End: None=0, Stopped=1,
NaturalEnd=2, RepeatOne=3, LoopLimit=4. Failure: None=0, DeviceFault=1,
Protocol=2, Underrun=3, ResourceExhausted=4. These are local RTL encodings,
not a CPU register map. Terminal failure/end clears pending reservations;
only a valid later Begin reopens the stream.

## Integer arithmetic

Gain has denominator 240000, fade duration 240000 consumed frames and restore
duration 960. Interpolation exactly matches
`S + trunc_toward_zero((E-S)*n/D)`. A quotient/remainder recurrence advances
one consumed frame at a time; the analytical product/division is the test
oracle. Both channels use `trunc_toward_zero(sample*gain/240000)` with signed
64-bit intermediates and no YM2151 register changes. Boundary n=0 retains S;
fade boundary n=D ends without consuming another frame. Cancelling at n=D
may restore from zero. Reached-to-reached policy changes retain the deadline.
Every consuming boundary uses its already-registered gain; starting a new
ramp changes the anchor/slope, not that boundary's gain. Sample scaling is
therefore evaluated independently of control validation, with zero selected
for a rejected or ending boundary. The arithmetic and ordering are unchanged.

Acceptance includes exact 70/75-second frame numbers, both sample signs,
partial-gain restart, cancellation, pause, natural end, old epochs, current
policy conflicts, 256/257 admission, retry, coverage gaps and counter ceilings.
RTL synthesis must preserve bounded storage and explicitly report arithmetic
cost. Local simulation/synthesis does not establish audible mapping, APF timing,
CPU ACK bounds or firmware 2.6 behavior.
