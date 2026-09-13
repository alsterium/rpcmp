# Retained media source queue v1

Status: adopted for the synchronous scheduled-source portion of M6 slice 4,
2026-09-13. This connects timestamped write batches to the existing
[source receipt boundary](pocket-enveloped-audio-v1.md). It does not change
the CPU-clock [queue v1](core-rtl-device-queue-v1.md), define MMIO/CDC, or turn
source coverage into already-mapped envelope progress.

## Ordered input and ownership

`rpcmp_media_source_queue` has one synchronous audio-domain owner and 64 RAM
entries. `reset_n` and `stream_reset` must share the native stream's reset;
the adapter must discard old-generation offers before releasing it. Admission
and head prefetch may run while media is held. Dispatch and coverage retirement
use the native `media_enable` and pre-edge `source_edge` (12,288,000 Hz retained
edges). This clock is not the MDX Timer B clock. Timestamp conversion and CDC
remain the adapter's responsibility.

An item is either an 8-bit address/value write, or a batch-ending marker.
Every item in a batch has the same `item_at:u64`, initially zero. A marker
has `item_until:u64`, the exact next batch's start, strictly greater than its
own start. Subsequent items must use that next start. A final marker instead
sets `item_end` and zero `item_until`; it closes admission. Zero-write batches
still have a marker. Write items require zero until/end; marker address/value
must be zero. No guessed natural end or arbitrary silence timeout closes input.

The producer validates and retains the complete MDX tick, then streams its
writes and marker in order. The maximum 8,192-write MDX batch need not fit this
64-entry queue. Accepted items are copied. Status while valid is Accepted=1,
Full=2, Invalid=3, Closed=4; otherwise None=0. Invalid and Full preserve RAM,
order and closure, so the same item can be retried. Validation precedes Full.
Full uses pre-edge occupancy, including a head dispatched on that edge.
Reset/fault/closed admission reports Closed. No implicit retry or overwrite
occurs. After an accepted final marker, earlier queued items still dispatch.

## Dispatch and source coverage

The registered head dispatches when due (`source_edge >= item_at`) and the
corresponding native write/marker ready accepts it. Writes and markers share
one FIFO, so a marker never overtakes its writes. Native tokens and actual
receipts are assigned by the existing source, not by enqueueing. Ready stalls,
busy time and native completion capacity can make an operation late; its
timestamp is never rewritten or used as evidence of an earlier transfer.

Head prefetch uses a synchronous RAM read. Dispatch invalidates the head;
the following edge can load the next head, for dispatch on a later edge.
This bounded internal bubble is part of this queue, not a dropped operation.
No same-address RAM read/write value is needed. Reset invalidates ownership
without resetting memory data. Capacity includes the cached head.

A dispatched marker seals absence of any further writes before its `until`.
A dispatched final marker seals the remaining source stream. Until then,
unsubmitted parts of the current batch may still exist. Source supply underrun
means all of the following hold on an enabled edge:

- the queue is empty;
- the native source is ready to accept another operation;
- the current source edge is outside the last dispatched marker's coverage;
- no final marker has dispatched.

The native write ready is the availability witness even with no write offered;
the source contract supplies it independently of valid. An in-flight write or
a blocked receipt prevents readiness and therefore is not a supply underrun.
A known queued head, including a future head or a head being prefetched, also
prevents supply underrun. The producer must refill before the empty opportunity:
an enqueue on that same edge is too late. This rule permits streaming batches
without requiring an entire batch to be prefetched while the clock is stopped.
It does not certify CPU service deadlines or guarantee every legal input is
schedulable at its nominal tick rate.

Underrun latches `supply_fault` on that edge and disables further dispatch and
admission. It does not stretch native time. The enclosing sound owner must
route this registered fault to its shared device-fault/reset path; using a
combinational starvation-to-enable feedback loop is forbidden. Reset clears
the fault and both admission/dispatched coverage. Paused time cannot underrun.

This is source-dispatch coverage only. Loop/end intervals still need retained
batch metadata, the [native completion prefix](jt51-native-completion-v1.md),
and actual selected-sample/frame mapping before envelope admission. A final
source marker is not by itself permission to publish end or stop audio.

## Acceptance

Check copied FIFO order, 64/65 admission and retry, pointer wrap, same-edge
admission/dispatch, held admission, future due times, partial and zero-write
batches, invalid/closed offers, reset with queued/cached/in-flight work, and
64-bit timestamps without wrap. Distinguish native busy/receipt backpressure
from empty eligible dispatch, including refill on the deadline edge.
Connect the queue to real pinned JT51, stream an authored maximum-size batch,
and observe actual bus writes/receipts and selected output prefixes. Compare
against an independent unqueued source where dispatch opportunity is the same.
Run synthesis/timing for the new RAM and control path. These local checks do
not establish a Pocket CPU producer, audible MDX progress, or hardware acceptance.
