# Pocket enveloped audio v1

Status: adopted for M6 slice 4's synchronous native/output integration,
2026-09-13. This composes [media envelope RTL v1](media-envelope-rtl-v1.md)
and [Pocket media audio v1](pocket-media-audio-v1.md). It does not define MMIO,
CDC, CPU ACK deadlines or a source-tick to audible-frame mapper.

## Shared output and source

`rpcmp_media_output` owns the existing rational conversion, pending stereo
sample and continuously running I2S serializer. At a frame boundary its owner
supplies `frame_consume` and transformed signed-16 stereo values. `source_left`
and `source_right` expose the old pending sample, or zero when empty. The output
captures the transformed old sample before accepting a new native sample on the
same edge. Between boundaries `media_enable` retains the consuming decision;
this same enable freezes conversion, native clock generation and bus writes.
No transformed sample is loaded on a nonconsuming boundary.

`INITIAL_RUNNING` selects reset-time ownership only. The existing
`rpcmp_pocket_media_audio` wrapper uses true, with `!pause_request` and unchanged
source values. Its public ports and behavior remain unchanged. The new player
uses false and the envelope's `consume` and scaled samples. Urgent stream reset
still clears output/pending immediately without resetting serial phase or the
wall-frame counter. Converter diagnostics retain their existing meaning.

`rpcmp_jt51_media_source` contains the existing hold-capable native engine,
clock accumulator and busy-aware write owner. Both old and new audio wrappers
use it. It does not interpret physical controls, policy or progress. Held
source-valid pulses and accepted in-flight writes retain their old semantics.

## Source sample positions

M6 slice 4 adds synchronous sample-position observations to these internal
modules. The existing pause-only wrappers keep their ports and sample behavior.
This observation connects the native sample capture edge to physical output;
it does not assign an MDX tick or register write to that native sample.

`rpcmp_jt51_media_source.source_edge:u64` numbers retained audio-clock edges,
starting at zero after power/stream reset. Its value before an enabled edge is
that edge's index. It increments once on each `media_enable` edge, including
edges without a native sample or write. Hold freezes it; reset clears it even
while held. It is a 12,288,000 Hz source clock, not the 48,000 Hz media-frame
counter. At UINT64_MAX the final indexed edge sets sticky
`source_edge_exhausted` and retains MAX, without wrapping. The enveloped owner
treats exhaustion as a shared DeviceFault and resets; no later edge is valid in
that stream. Pause-only consumers which do not use positions ignore this
additional observation.

`rpcmp_media_output` takes `src_at_edge:u64` with each accepted native sample.
Rational selection drops or captures the position together with both channels,
using the same pending slot. `source_valid` and `source_at_edge` expose that
old pending position before a boundary. `output_valid` and `output_at_edge`
identify the sample loaded into the serializer at a consuming boundary, and
remain stable throughout that frame. An empty or nonconsuming boundary has no
source position. Invalid observations have position zero; a valid position may
also be zero. On simultaneous consumption/capture the output uses the old
position, and pending receives the new one. Overflow replaces both sample and
position and retains the existing error; clear-flags does not erase positions.
Urgent reset clears both pending and output observations immediately at its
synchronous edge, including during a partially serialized frame.

`rpcmp_jt51_enveloped_audio` wires the actual `source_edge` to the converter,
exposes it along with `pending_source_valid/pending_source_edge` and
`output_source_valid/output_source_edge`, and preserves its existing frame and
gain ownership. Positions are local to the current reset/play stream; an owner
must associate them with its existing generation. Zero startup/pause frames do
not invent native samples. These observations do not authorize loop/end
publication: native write/pipeline delay and the ordered progress mapping remain
separate obligations. In particular, busy-clear is still not an audible marker.

## Integrated generation and reset

`rpcmp_jt51_enveloped_audio` connects the envelope to this shared source/output.
All inputs are synchronous to 12.288 MHz; asynchronous `reset_n` release must
be synchronized. Begin, progress, control and state fields retain the envelope
contract. Its `frame_tick` is the serializer boundary while native reset is
inactive. Consumption commits the sample loaded at that boundary; the physical
bits follow over that stereo frame. It is not a queue-admission acknowledgement
or proof of the producer's source-tick mapping.

The wrapper owns a 2,048-edge native-reset hold, using the adopted conservative
initialization interval. Power reset, explicit `stream_reset`, shared device
fault, converter underflow/overflow, and each new terminal envelope state reset
native/output state. The hold extends for at least 2,048 audio edges after the
trigger is withdrawn. Serial clocks and wall-frame count continue. `resetting`
reports this reset; `quiescent` becomes true only after it finishes and no
generation is active. A Begin during reset or active playback is Invalid and
preserves the current envelope. No source edge or write is accepted before a
subsequent valid Begin reaches its first consuming boundary.

Stop, natural end and loop completion retain their envelope reason/position,
discard reservations and start physical reset. Fault remains a failure, with
priority over Stop. An explicit stream reset during an active generation is a
DeviceFault, preventing resume of an old position against a cleared source.
Converter underflow/overflow also close the stream as DeviceFault; they are
observed after the converter registers them. Clipping remains a diagnostic,
as the native source is already signed 16-bit. Physical reset clears converter
flags; the envelope's failure stays latched until valid Begin. After reset,
only a strictly newer Begin can reopen playback. There is no automatic replay.

Pause loads a whole zero stereo frame, retaining pending samples, native state,
media position and gain. Policy changes while paused reconcile on Resume.
Terminal reset may urgently truncate a frame; ordinary Pause cannot. Controls
offered during reset have no frame boundary result; the future control adapter
must retain them or reject them according to its separately adopted protocol.

## Acceptance and limits

Compare decoded external stereo samples with an uninterrupted unscaled native
reference multiplied by the independently calculated contract gain, after
removing pause frames. Exercise cancellation, completed restoration, policy
changes while paused, both signs/channels, old-generation controls, finite end,
reset during hold and a new silent generation. Keep the existing converter and
real-JT51 pause suites passing after sharing their internals. The standalone
envelope's full-duration arithmetic tests remain required.

Use independently counted native strobes/retained edges and closed-form
rational selection to check the source position of each serialized frame.
Check all 64 position bits, valid zero versus empty, coincidence with a new
sample, overflow/drop, holds, urgent reset and the source counter's final real
increment. Counter exhaustion must invalidate the stream rather than wrap or
silently fabricate another position.

Synthesis must preserve native and progress RAM inference. This local integration
does not establish APF lifecycle/CDC, full-player timing/resources, firmware 2.6
behavior, or the correspondence of MDX progress to the audible sample timeline.
