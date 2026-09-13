# MDX audible progress v1

Status: adopted for local M6 slice 4 with explicit user approval on 2026-09-13.
Approval covers implementation and verification of the reviewed proposal;
acceptance requires executed milestone checks. Q1–Q24 controls and playback policy remain
unchanged. This does not define CPU MMIO, CDC, ACK deadlines or public snapshots.

## Checkpoint ownership

`rpcmp_jt51_progress_audio` connects the [source queue](media-source-queue-v1.md),
native source, [completion prefix](jt51-native-completion-v1.md), selected PCM
and [envelope](media-envelope-rtl-v1.md). Successful MDX ticks supply copied
writes followed by a marker carrying `item_loops:u64` and `item_end`. Zero-write
ticks still supply markers. The producer validates and retains the whole tick
before offering any write. Timestamps use the retained 12,288,000 Hz source
clock, with checked rational conversion from the MDX scheduler.

Writes require zero loops; marker loops cannot precede the last admitted marker's
count. Only accepted markers update that count. Reset/fault gives Closed=4;
otherwise invalid loop metadata gives Invalid=3 before queue Full/Closed. Other
item fields, statuses and dispatch coverage retain the source queue contract.
Reset discards the count with the queue; the producer discards old-generation
offers before releasing reset. Begin additionally requires at least one queued
item and native quiescence, with the existing generation/revision/target checks.

Shared modules gain a positive `PAYLOAD_WIDTH` parameter, default 1:

- Source queue copies opaque `item_payload` to `dispatch_payload` with each
  queued item. The new owner uses 65 bits `{ended, completed_loops}` on markers.
- Completion copies `receipt_update` and `receipt_payload` with each receipt.
  Retirement updates `native_prefix` and, only when flagged, `native_payload`
  on the same edge. Writes preserve the previous checkpoint. A same-edge native
  sample observes the old prefix and payload. Reset sets payload zero.
- Media output copies `src_payload` to pending `source_payload` and then
  `output_payload`, atomically with PCM, source edge and prefix. Invalid values
  expose zero; pause, replacement, reset and simultaneous old consumption/new
  selection follow the existing sample ownership rules.

The new owner uses 66 completion/sample bits `{checkpoint_valid, ended, loops}`.
It latches marker data at the actual marker acceptance, then attaches it to that
marker's retained receipt. Receipt delivery is atomic between the external
consumer and completion FIFO. A simultaneous old receipt/new marker uses the
old data for the receipt. Admission can finish during hold; native retirement
cannot. No independent delay queue estimates checkpoint completion.

Existing wrappers tie unused payload inputs to zero and preserve their external
ports, clocks, PCM and control behavior. The manually mapped enveloped wrapper
remains available. The new owner's pending/output checkpoint observations are
valid only with their actual sample; a valid sample may precede any checkpoint.
Multiple completed markers between selected samples collapse to the latest
checkpoint. Per-event performance history is a separate retained producer.

## Startup and output boundaries

Accepted Begin first admits sequence 1 covering [0,2), loops zero and not ended,
from the proven reset baseline. These first two audio frames contain no selected
native sample. This is not a successful engine checkpoint: checkpoint_valid
remains false until an actual marker completes. Later pre-marker PCM may carry
that same explicit baseline. Public history must not invent an engine update.

The owner arms frame consumption only after bootstrap admission. Native time
starts at a consuming audio boundary, with retained source edge zero on media
frame zero. Begin near the end of the serial period can defer start by at most
one 48 kHz audio frame (about 20.83 microseconds). Serial clocks keep running.

For pending frame f >= 2, admit sequence f covering [f,f+1), carrying that
selected sample's loops/end, before consumption. Selection at edge E permits
admission at E+1 and visibility at E+2. The existing selected-sample proof leaves
at least 29 retained edges before consumption. The owner registers each interval
once; Full retries without advancing, pause preserves pending ownership, and an
accepted end closes the feeder. It never wraps sequence/frame arithmetic.
Unexpected Invalid/Exhausted mapping status raises a registered DeviceFault;
MAX frame uses the existing envelope resource/control priority.

The envelope evaluates loops and end at their mapped boundary using current
policy. Pause precedes end and preserves the frame; Resume evaluates the same
boundary. Natural end does not consume its pending PCM: output sample/checkpoint
validity clears, while terminal frame/count/reason remain. Finite policy reports
NaturalEnd; infinite policy reports RepeatOne. Live policy and 240,000-frame
fade/960-frame restoration retain their existing arithmetic and priority.

Source starvation, device/converter errors and position/token/completion overflow
share the existing physical reset owner. Terminal states trigger its 2,048-clock
native reset. Faults must not form combinational enable feedback or stretch
native time to conceal missing supply. Old queue/pending/checkpoint data cannot
survive into a new generation.

## Acceptance

Verify opaque full-width payload FIFO order, Full retry, pointer wrap, reset,
held admission and retirement, and ordinary writes between checkpoint updates.
Check selected payload/position/prefix against independent quotient selection
and serialized PCM. Observe actual native transfers/markers and the certified
completion bound to derive loop, fade and end frames independently. Include
zero-write updates, receipt backpressure, pause on loop/end boundaries, live
policy changes, late-serial-phase Begin, empty Begin rejection, actual supply
failure and fresh-generation recovery. Negative controls must detect broken
mapping. Run host/architecture gates, sequential affected RTL regressions and
local registered synthesis/timing. These checks do not establish CPU feeding,
per-event history, sound/storage adapters, whole-Pocket timing or hardware pass.
