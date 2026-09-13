# JT51 native completion v1

Status: adopted internal M6 slice 4 completion contract, specified before implementation,
2026-09-13. Extends the approved ordered receipts and sample positions in
[Pocket enveloped audio v1](pocket-enveloped-audio-v1.md). This is a local
audio-clock boundary; it does not define CPU/MMIO/CDC completion.

## Meaning of completion

A native sample carries an ordered operation prefix. A token enters that prefix
only after the corresponding actual bus/marker receipt has passed the bound
below. The prefix certifies completion of direct control processing through a
full operator round and finite arithmetic/accumulation pipeline. Later writes
may supersede earlier controls. It does not require every intermediate setting
to produce a distinct PCM value.

Phase accumulators, envelope levels/states, operator feedback, LFO waveform
value/signs/counters and noise/timer state are retained musical history. They
are not flushed by this certificate. In particular, a key-off can retain a
release tail, a slow attack can remain silent, and timer/CSM attacks cannot be
timed from write receipts. Existing unknown-gate/capture-loss rules still apply.

The certificate is conservative, not the first measurable waveform change.
It is not permission to publish read-ahead engine state: the token must travel
with a real selected sample through pending storage to its consuming frame.
The latest complete batch checkpoint at that boundary supplies channel state;
per-write event frames are preserved and an incomplete batch stays deferred,
as required by [MDX performance observations](mdx-performance-v1.md).

## Bound and scope

For the pinned JT51 revision `985a573`, count **768 subsequent enabled
`cen_p1` edges**, excluding the receipt edge. The hold-capable derivative keeps
these transitions identical to the native source after held edges are removed.
The decomposition is deliberately conservative; terms are sequential allowances,
even where their physical paths run concurrently:

| Allowance | Direct-processing path |
| --- | --- |
| 128 steps | Configuration bank and global direct-control staging, detailed below. These paths run concurrently from the receipt; no register update waits for the LFO multiplier. |
| 512 steps | LFO AM/PM finite serial multiplier pipeline. Starting with arbitrary old `out1`, `out2`, carry and output latches, two 256-step serial schedules replace all direct operands and both output calculations. |
| 128 steps | One complete 32-operator round, at most 17 arithmetic pipeline steps, and at most 72 further steps from operator contribution to native sampled PCM: 32 + 17 + 72 = 121 <= 128. Noise mixing is selected once per round and joins the same accumulator. |

For configuration, allow an MMR latch, 32 slots to visit affected fields,
32-stage configuration/key rings, a key output latch and another full scan:
1 + 32 + 32 + 1 + 32 = 98 <= 128. Channel banks use one bank latch and at most
an eight-channel read cycle. Native busy preserves operator update data over
the visit. The parallel global path needs LFO update/reload latches, two
16-slot counter/clock staging rounds, waveform carry reset and seven output
shift steps; even allowing eight latch steps and a further 16-slot boundary
gives 8 + 32 + 16 + 7 = 63 <= 128. Noise comparison/update uses two latch steps.
Retained oscillation, timer and noise history is not waited out. The subsequent
512-step allowance starts only after these direct operands are available.

The LFO bound cuts retained value/sign/counter state, not finite multiplier
state. In `jt51_lfo`, `out1` replaces one of seven bits per enabled step. The
serial product's carry is discarded at each cycle F; its old `out2` operand is
discarded for bit zero. Bit seven supplies constant zero. A 256-step schedule
computes both AM and PM; a second schedule includes a complete product using
fresh `out1` bits. Schedule reset depends only on the 16-step `cnt1` clock,
not frequency, depth, waveform or test settings. Startup and all steady phases
must be covered, including the native nonblocking output-latch edge.

The accumulator bound follows its eight-channel ring, reset on C2, total
collection on C2 and stereo latch on the next C1. An impulse offered at scan
phase `p` is sampled after `64-p` steps for p < 24 or `96-p` otherwise, so the
largest delay is 72. Algorithm and pan gates can omit contributions; they do
not extend that storage lifetime. Saturation and PCM encoding are combinational.

These are control-use and finite-pipeline bounds, not convergence bounds for
the full synthesizer's musical state. A different native revision, clock
enable scheme or pipeline requires a new proof before using this constant.

## Retained source edges and queue

At F = 12,288,000 and R = 3,579,545 Hz, the 768th subsequent half-rate enable
occurs no later than `ceil(1536*F/R) = 5273` retained source edges after a
receipt. With pre-edge accumulator A in [0,F) and phase C in {0,1}, its offset
is `ceil(((1536-C)*F-A)/R)`. The already-stored receipt-edge enable is excluded.
This is about 0.429 ms of source time, not a wall-clock or CPU deadline.

A 32-entry synchronous FIFO retains each receipt token and checked
`due_edge = receipt_at_edge + 5273`. Full capacity backpressures the receipt;
there is no drop or replacement. A valid registered head retires on the first
edge with `media_enable` and `source_edge >= due_edge`. Its token becomes visible after
that edge, so a same-edge native sample still carries the old prefix.
Head prefetch can delay retirement; it cannot make it early.

Queue admission may drain a receipt during hold; retirement and its prefix
remain frozen. Reset invalidates the queue and sets the prefix to zero.
Unsigned addition overflow raises DeviceFault and enters the shared reset
path. Tokens inherit the source's nonzero, ordered, nonwrapping contract.

The enveloped owner accepts a source receipt only when both its existing
receipt consumer and this FIFO can accept it. Neither consumer sees duplicate
delivery. The original receipt payload and actual edge remain unchanged.

## Sample ownership

`rpcmp_media_output` adds `src_prefix`, `source_prefix`, and `output_prefix`,
all u64. The input prefix is captured atomically with selected PCM and its
source edge. Pending replacement, simultaneous old-frame consumption and new
selection, pause, empty output and reset use exactly the position/PCM rules.
Invalid pending/output values expose prefix zero; valid prefix zero denotes a
real sample before any operation has completed.

The enveloped owner exposes `pending_prefix` and `output_prefix` under its
existing pending/output validity signals. Source receipts and native prefix
retirement alone are not public audible completion. Scheduled operations,
sealed source coverage, mapped progress admission, retained MDX batches and
the committed-event collector remain necessary before advertising that feature.

## Verification

Check the native LFO dataflow against two copies with different multiplier
state at all serial start phases, including startup. Check native accumulator
impulses at all 32 phases. Independently count subsequent native enables at
sample capture; no tagged sample may precede the 768-step bound.
Exercise queue full/backpressure, reset, hold admission, held retirement,
prefetch, full-width token/edge retention and addition overflow. Decode PCM and
check its prefix and source position together through selection and output.
An early-retirement negative control must fail. Existing receipt, pause, burst,
envelope, host architecture and compatibility gates remain required.
