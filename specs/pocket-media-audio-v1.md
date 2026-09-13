# Pocket media audio v1

Status: adopted for M6 slice 4's local pause/output implementation, 2026-09-13.
This defines new internal modules, not MMIO, CDC, host ACK deadlines or an
advertised backend capability. Existing audio/queue/reset v1 modules retain
their contracts. Gain, audible tags and control transport follow this boundary.

## Output and ownership

`rpcmp_pocket_media_audio` runs at 12,288,000 Hz. Its serializer always advances
256 ticks per stereo frame, including while paused or stream-reset. A common
active-low `reset_n` initializes everything; deassertion must be synchronized.
All other inputs are synchronous to this clock, not asynchronous CPU controls.

The sample conversion, signed 18-to-16 saturation, one pending stereo sample,
sticky flags and modulo-2^32 diagnostic counters follow
[audio adapter v1](pocket-audio-adapter-v1.md). Conversion advances only when
`media_enable` is high. Held `src_valid` is not a sequence of new samples.
At a running boundary, consume the old pending sample before capturing any
new source sample on that same edge. Overflow still replaces a pending sample
and reports it; underflow after startup still emits and reports silence.

The new serializer follows the official [APF AUDIO definition](https://www.analogue.co/developer/docs/bus-communication)
(checked 2026-09-13): LRCK low is left, high is right, four MCLK ticks per bit,
with the MSB one bit slot after the LRCK transition. In each half frame slot 0
is zero, slots 1..16 carry bits 15..0, and slots 17..31 are zero. Legacy v1 puts
its MSB in slot 0; that legacy contract and implementation are not changed.
Comparison with v1 is of decoded sample values, not identical serial phase.

## Pause and reset

`pause_request` is a desired level, sampled on the edge following serial phase
255. `paused` reports the level applied at that boundary. Requests withdrawn
before the boundary have no effect. A stable request takes 1..256 clock edges
to apply; this is a local synchronous bound, not a CPU/CDC acknowledgement bound.

On the pause boundary, do not consume pending, accept source data or advance
the source engine. Instead load a complete zero stereo frame. While paused,
keep rate phase, pending samples and all source state. Resume at a boundary
consumes precisely the retained pending sample and advances that retained media
edge once. A running frame is never partially muted by Pause. `media_enable`
qualifies the upcoming edge: at a boundary it reflects the requested state;
otherwise it reflects the already-applied state. Owners must use this same
enable for source clock-enable generation, bus transactions and media time.

`stream_reset` is an urgent synchronous reset, with priority over pause. It
clears converter/pending/frame data, flags and selected count without resetting
serial phase or wall-frame count. It may truncate a frame; Stop/fault silence
is distinct from ordinary Pause. No pre-reset pending sample may return.
It sets `paused`; after release, the desired pause level is applied at the next
boundary. Clear-flags works during pause; a new fault on the same edge wins.

## JT51 owner

`rpcmp_jt51_media_audio` binds the [generated hold engine](jt51-hold-experiment-v1.md)
to the converter. It retains the existing rational 3,579,545 Hz enable algorithm
and busy-aware address/data write sequence from [JT51 adapter v1](ym2151-rtl-adapter-v1.md).
The new write-only handshake accepts `dev_valid && dev_ready`; reset is the
separate priority input. Hold freezes the enable accumulator, its registered
enables, and every in-flight bus signal/state. `dev_ready` is false on frozen
edges, including the pause boundary. Input payload must stay stable until
accepted; an accepted write remains owned by this module until completion/reset.

The data pulse must include a `cen_p1` edge, which is also a `cen` edge. JT51's
busy latch and operator scan use that half-rate enable. Releasing data on an
earlier `cen`-only edge can miss busy entirely, allowing later writes to replace
an operator update before its scan slot. Waiting for `cen_p1` is a compatible
correction to the write-completion guarantee; no public maximum write latency
or exact pulse length was specified. Address still completes on `cen`; accepted
payload ownership and pause/reset priority are unchanged. This correction is
for the M6 source; it does not change the frozen legacy v1 wrapper.

During stream reset the write state is cleared and JT51 remains reset, while
its clock-enable generator continues to clock the reset stages. The owner must
hold stream reset for at least 2,048 audio edges before first playback or after
an interrupted stream. This conservative local initialization interval is the
existing native-fixture warmup, not a proven minimal reset or public ACK deadline.
Release leaves the engine held until the next running boundary.

Acceptance compares decoded stereo output with uninterrupted execution after
removing inserted pause frames. It covers source/boundary coincidence, pending
and empty holds, request changes, in-flight writes, sticky diagnostics, reset
while held and continuous serial clocks. Native source equivalence is separately
covered against unmodified JT51. Full integration, gain/commit, CDC, placement
and firmware 2.6 acceptance remain required.
