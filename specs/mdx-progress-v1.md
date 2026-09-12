# MDX sequencing progress v1

Status: adopted for the loop-count portion of M6 slice 3, 2026-09-13, from
the [transition proposal](../docs/design/pocket-player-transition-contract.md).
This is an internal engine observation, not a public UI snapshot or an audible
completion. The existing [MDX v1](mdx-v1.md) instruction, device-write and
timestamp semantics remain unchanged.

## Counting and atomicity

TrackPlaybackState adds completed_loops:u64, initially zero. Only a successfully
executed TrackLoop back edge increments it. Short RepeatStart/RepeatEnd/
RepeatEscape, waiting, rest, note and UI mute/solo have no effect on the count.
An increment beyond UINT64_MAX returns the existing ArithmeticOverflow with
the loop instruction's source offset/channel. It never wraps or saturates.
The track's entire tick, including the counter, is transactional on failure.

DocumentPlaybackState adds completed_loops:u64 and ended:bool. After all A-H
tracks successfully complete a driver tick, completed_loops is the minimum
among tracks without TrackEnd. Waiting and silent active tracks still take
part. TrackEnd removes a track only after the complete A-H batch succeeds.
The inert P track never participates. No wall-clock timeout synthesizes an end.
If all eight FM tracks have ended, ended becomes true and completed_loops
retains its last value: absence of active tracks does not invent another loop.
New playback starts with a value-initialized state and zero counters.

Same-tick loops and ends are aggregated once, after the entire driver tick.
Failure in any later channel rolls back earlier counts, ended flags, cursors
and the output batch. A later YM2151 routing or timestamp failure also rolls
back this progress as part of advance_mdx_tick's existing transaction.

## Timestamped engine observation

MdxEngineState adds a SequencedProgress value with at_tick:u64,
completed_loops:u64 and ended:bool. A successful advance_mdx_tick stamps it at
the **start** of the serviced driver tick, exactly where that tick's device
writes are stamped, including a tick with zero device writes. The tick rate
is the explicit scheduler_tick_rate passed to the engine. It is not implicitly
48 kHz, and the post-tick timeline position is not the event timestamp.
Only a successful engine call makes the observation valid; default state is
not evidence that anything was executed or heard.

The Core adapter must copy this value together with the accepted write batch,
add the play generation, and preserve it under bounded queue backpressure.
It must map scheduler time to the actual audio commit position before applying
loop policy, publishing counts or advancing to another track. Reading the
read-ahead engine state as audible progress violates this contract. That
queue/commit adapter and gain/policy integration are subsequent M6 work; no
new capability is advertised by this engine-only change.

## Verification

Use authored exact bytes for nested repeats, TrackLoop, TrackEnd and indefinite
wait. Check unequal channel periods, same-tick end/removal, all-ended state,
the inert P exclusion, zero-write progress and u32/u64 boundaries. Seed internal
counters near the limits to check overflow without billions of ticks. Prove
rollback when a later channel, routing or timestamp conversion fails. Preserve
existing semantic/register trace fixtures; no golden regeneration is needed.
