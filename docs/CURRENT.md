# Current work

Active milestone: [M6 — Album Player](milestones/M6-album-player.md)

The current task is the M6 compatibility reassessment requested on 2026-09-21.
The user selected [asaday/MDXPlayer](https://github.com/asaday/MDXPlayer) as the
reference: support every locally supplied file that played there, including
PCM. The MVP is track selection, play and stop. Follow the
[pinned source baseline and next comparison work](research/mdxplayer-compatibility.md).
Further Tracker/keyboard/policy expansion is deferred. Existing v1 behavior,
the r4 package and historical evidence remain intact; no replacement engine,
PCM implementation or production substrate has been approved by a test result.
Keep openfpgaOS as the comparison substrate while evaluating the reference
engine and target execution budget. This is a bounded investigation direction,
not a new production-substrate decision.

The reference-synthesis RV32 instruction experiment now excludes an unchanged
reference renderer on the unchanged single-issue 90 MHz CPU for the tested
eight-FM-channel load: even `-O3 -flto` needs 99.18 million instructions per
audio second, or 134.95 million with eight ADPCM voices. These are ISA counts,
not hardware timings. Compare a higher-throughput software-sound CPU/platform
against FM hardware plus CPU PCM; neither the current OS nor JT51 is mandatory.
See the baseline's synthesis-budget evidence and portability findings before
choosing a production design. No replacement design has been adopted.

CPU RTL simulation further measures about 131 million cycles per audio second
for the reference FM-8 + ADPCM-8 case even on the dual-issue candidate with
optimistic independent AXI memories. PCM-only cases need about 52–58 million
cycles on the current CPU. The leading next experiment is reference MXDRV on
CPU, FPGA FM and software PCM8 through a thin timed audio path, with a minimal
select/play/stop UI. This is a research recommendation, not a Pocket timing
pass or adoption of a replacement contract; see the baseline's RTL comparison.

The following records the implemented slice-5 baseline from the
[CPU sound connection](design/pocket-cpu-sound-connection.md), not the next work.
The opt-in [sound AXI binding](../specs/pocket-sound-axi-v1.md) now passes
three-phase bus/mailbox simulation, related RTL regressions and the host gate.
The final no-GPU fit uses 16,649/18,480 ALMs, has nonnegative timing across all
136 reported clock/corner cases, and passes the exhaustive four-mailbox CDC
audit. Existing shell external/legacy exceptions remain unproven; this is not
production-substrate promotion. The licensed
[bitmap canvas](../specs/pocket-bitmap-canvas-v1.md) now renders all three shared
views with copied commands and bounded pixel work. The actual
[Pocket application](development/pocket-player.md) now composes
catalog/player/backend/UI with APF input, a 64-bit CPU clock and the SDK draw
surface. Its loop services audio independently of publication and bounded
rendering; scripted application integration and the full RV32 app link pass.
The r3 hardware report confirms stereo playback, pause/resume, stop, navigation,
two-loop fade/next/end, shuffle and restart defaults on Firmware 2.6. Its test
tone is quiet; Tracker/keyboard remain frozen, with the keyboard stuck at
Waiting for performance data. Reported F110–400ms/S10493–20493us do not yet
establish the target service deadline. r4 fixes continuous read-ahead evicting
future display checkpoints, reduces bounded raster/Tracker projection work,
and raises the authored demo's voice level. Host regressions and the RV32
memory/stack gate pass. The user's Firmware 2.6/r4 report confirms audible
level, stereo, one-minute infinite playback, moving Tracker/channel-A keys and
clearing Waiting. Tracker is not synchronized to audio; input delay/audio cuts
remain view-dependent, and clicks between notes have been reported since M6.
Reported F is 300/360/250 ms for Tracker/keyboard/library and S is 8870 us.
The pause/resume/stop/next performance-display field was unanswered. These
results do not pass timing, noise or full M6 acceptance. The
[hardware page](development/pocket-player-hardware.md) identifies the preserved
r4 package; use the compatibility reassessment for the next task. D-pad panel
navigation and contextual B remain as requested on 2026-09-20.
On 2026-09-20 the user deferred persistent settings: each application launch
starts with AlbumOrder / Default (two loops, five-second fade), shuffle off and
no autoplay. In-session setting changes remain supported. Do not connect a
settings storage port or advertise `kPlaybackSettings` in this Pocket profile.
APF settings integration, SD readback and storage deadlines are future work,
not M6 completion gates. Preserve the already-tested optional components. The
settings adapter's
[flush command transport](../specs/pocket-apf-flush-v1.md) now connects CPU
requests to the APF handler and passes independent-clock simulation, including
busy ownership, delayed drain, unknown errors and reset. The local
[settings RAM owner](../specs/pocket-settings-ram-v1.md) now implements atomic
bank publication, reset-retained leases and separate media-readback storage.
Its synchronous boundary uses the actual Cyclone V RAM model; CPU/BRIDGE CDC,
APF size-table ownership, OS arbitration, SD readback and storage deadlines
remain deferred. The sound
[MMIO/CDC](../specs/pocket-sound-mmio-v1.md) now passes independent-clock RTL,
host checks and local fitted timing/CDC. The user approved the 1,000 us mailbox
watchdog on 2026-09-20. The [CPU MMIO client](../specs/pocket-sound-client-v1.md)
now retains each mailbox through completion or late-response drain, checks
deadline/clock failures and isolates journal errors from audio. Its word-level
host tests, sanitizer checks and RISC-V compile/link memory probe pass.
The [retained MDX producer](../specs/mdx-source-producer-v1.md) now passes
copied-batch/retry/epoch host checks and a RISC-V compile/link memory probe.
The [32-record output journal](../specs/pocket-output-journal-v1.md) now passes
native boundary, overflow/read-cadence and four-mailbox CDC checks with local
fitted timing. The [128-batch CPU history owner](../specs/mdx-output-history-v1.md)
now connects copied MDX checkpoints to actual output records with per-event
frames and explicit display loss; host gates, sanitizer tests and the RISC-V
memory probe pass. The [MDX backend](../specs/pocket-mdx-backend-v1.md) now
connects preparation, retained MMIO feeding, coherent control/capture ordering
and output-history publication. Scripted integration checks cover actual
TransportController/PlayerSession, cancellation, late responses, failure
recovery and publication-independent supply. See M6 for exact host/sanitizer
and RISC-V evidence. The CPU clock and application loop are now connected;
bounded target cadence and combined Pocket hardware acceptance remain pending.
The [synchronous sound session](../specs/pocket-sound-session-v1.md) now passes
all-phase local control/reset/inhibit tests, host/RTL regressions and registered
fit/timing; the active milestone records the commands and limitations. Pocket
integration follows. The user's 2026-09-13 review approves the sound plan;
local acceptance establishes the recorded functional transfer bounds, not
the CPU service deadline or Pocket playback.
The explicitly approved
[MDX audible progress](../specs/mdx-audible-progress-v1.md) now carries marker
checkpoints through native completion and selected samples into local envelope
intervals. Its independent native integration checks loop/end boundaries,
pause, live policy, bootstrap and fault recovery. Host/RTL regressions and the
local fit pass; M6 records the evidence. Pocket acceptance remains open.
The [retained media source queue](../specs/media-source-queue-v1.md) now provides
ordered audio-clock writes/markers with 64 copied entries. Larger MDX batches
stream through it; the queue distinguishes native busy/receipt backpressure
from missing input at an eligible dispatch opportunity. Marker dispatch seals
source supply. The composed progress owner maps loop/end separately; the
retained CPU producer now feeds MMIO and per-event history through the backend.
Full-load Pocket execution and bounded service cadence remain unverified.
The explicitly approved source-receipt contract now orders writes and zero-write
markers and retains their actual transfer/marker edge under backpressure.
Native receipt tests and the enveloped fault/reset integration pass. These
receipts are bus positions, not native-pipeline or audible completion.
The [native completion prefix](../specs/jt51-native-completion-v1.md) now
consumes those receipts through a bounded RAM queue and travels with selected
PCM through pending storage to the actual output frame. Its conservative bound
covers direct control processing and finite arithmetic; it does not flush
musical history or identify the earliest measurable waveform change.
The selected-sample availability bound is now derived and checked: after an
aligned stream start, at least 29 retained audio edges remain before consumption.
This supplies a local admission budget; it does not map receipts to samples.
The local enveloped path now carries the retained native sample-capture edge
through rational selection, pending storage and serialized output. This is
the sample-to-output part of the mapping. The progress owner now connects local
loop/end coverage; retained CPU batches and public publication remain separate.
The native source now keeps data asserted through the half-rate busy-latch
edge. A back-to-back burst reproduced accepted but unreflected operator writes;
the corrected path passes native bank-value checks across all 32 operator slots.
Earlier paced PCM comparisons did not cover this burst condition.
The [enveloped output](../specs/pocket-enveloped-audio-v1.md) now connects native
hold, gain and serialization with physical reset after terminal states. Its
decoded stereo stream matches the analytically scaled uninterrupted reference
after removing pauses. This proves consumption at the output boundary for
already-mapped input. The new progress owner's marker-to-output evidence is
separate from this compatibility wrapper's manual-input evidence.
The [RTL envelope](../specs/media-envelope-rtl-v1.md)
now evaluates mapped progress against the current policy and implements exact
fade/restoration arithmetic. Its analytical tests and registered timing probe
pass; the new progress owner supplies its local selected-checkpoint intervals.
The [local media output](../specs/pocket-media-audio-v1.md) now connects native
hold, in-flight writes and retained resampler/pending samples to continuous I2S.
Its decoded stereo stream matches uninterrupted execution after removing pause
frames. It is a synchronous internal boundary, not a CPU-controlled Pocket player.
The [JT51 hold fixture](../specs/jt51-hold-experiment-v1.md) now compares a generated
hold-capable engine with unmodified JT51 at retained clock edges. Its authored
trace passes; a cen-only negative control reproduces an in-flight register
update during hold. This is native-engine evidence, not Pocket pause acceptance.
The headless player and
three mock views now connect around the
[Core transport](../specs/playback-transport-v1.md),
[schema 2 ingress](../specs/player-command-v2.md) and
[published state](../specs/player-state-v2.md).
The internal transport now handles asynchronous preparation/reset/start/pause/
resume, cancellation, fault priority and deadlines with scripted host ports.
The public state publisher copies selection metadata, committed position,
pending operations and errors without polling audio or depending on UI reads.
PlayerSession connects transport, repeat policy and navigation with a 32-command
queue, 64-result replay window and shared projection/execution validation.
Submission and public reads perform no playback-port calls; publication can
be skipped without changing control work. Library/backend changes after
admission interrupt the batch as an asynchronous failure.
The MDX engine now exposes [sequencing progress](../specs/mdx-progress-v1.md):
checked TrackLoop counts and whole-FM-tick aggregation at the write timestamp.
This is read-ahead progress. The [media loop envelope](../specs/media-loop-envelope-v1.md)
consumes explicitly mapped progress intervals at audio-frame boundaries and
implements live repeat decisions, fade/restoration and paused sample ownership.
The [repeat policy profile](../specs/playback-policy-v2.md) now connects
SetPlaybackPolicy to the transport and audio boundary, with desired/applied
revisions, coherent media observations and finite RepeatOne restart.
Only declared repeat-capable ports accept it. The optional
[navigation profile](../specs/playback-navigation-v2.md) now connects album
auto-advance, explicit neighbours and whole-library shuffle through an injected
random port. Shuffle counts successful starts, keeps cancelled candidates
eligible and avoids automatic replay of visited history. The
[performance profile](../specs/performance-history-v2.md) now connects a bounded
committed-history collector and independent current FM channels through a
read-only injected port to public snapshots. Display loss/invalid observations
do not stop transport. [MDX observations](../specs/mdx-performance-v1.md) now
follow emitted key/pitch/voice writes transactionally. Their Core mapper uses
an explicit output-clock declaration and an injected source-tick/audio-frame
boundary; future observations are deferred.
[Persistent settings](../specs/playback-settings-v2.md) now connect bounded
record validation, asynchronous restore/commit and copied save observations.
Startup restores policy without autoplay; failures preserve the current policy
and transport. A retry after uncertain I/O first waits for quiescence and
rereads both slots. The [UI controller](../specs/player-ui-v1.md) now connects
replaceable input/focus tables, paged album/track browsing, transport waits,
serialized setting actions and bounded Tracker row projection using contracts
alone. Policy commands publish Applied/Failed execution results independently
of audio/save completion, including no-ops and interrupted batches.
The synchronous canvas now draws Tracker, keyboard and library around a shared
information/control panel. The host SVG mock links UI/contracts only, with
bounded text and explicit elision. Authored scenarios and browser-rendered
images verify mock layout; installed host fonts do not establish Pocket metrics.
The retained producer and output-history owner now connect through the sound
backend. The actual AXI sound binding and scripted application service loop
are verified; the coherent candidate is packaged and target measurements are
next. The APF settings adapter is deferred by the user's updated requirement.
See M6 for the executed checks and r3/r4 hardware reports; target timing,
noise and full playback acceptance remain unresolved.
Use the [transition proposal](design/pocket-player-transition-contract.md),
preserve v1 compatibility and adopt each contract before its implementation.
Slices 1–2 implement [host metadata](../specs/host-metadata-v1.md),
[ALBM storage](../specs/album-catalog-v1.md),
[folder ingestion](../specs/album-ingestion-v1.md) and
[Core-owned catalog pages](../specs/catalog-query-v1.md). Windows host checks,
focused Linux sanitizer tests and the catalog RISC-V link probe passed; see the
milestone for exact evidence. These are not M6 playback/UI/hardware acceptance.
The [earlier Q1–Q24 requirements](design/pocket-library-player-spec-draft.md)
describe the fuller album player. The user's 2026-09-21 compatibility target
and smaller MVP take precedence for current work. Do not resume UI expansion
from these historical requirements before the playback comparison.

[M5 evidence](milestones/M5-real-mdx-library-playback.md) remains preserved.
The user closed its investigation on 2026-09-12; M5 is deferred, not passed.
M6 explicitly owns the remaining timing/CDC, placement, APF failure/lifecycle
and future PCM/UI reserve gates. ADR-0008 production promotion remains gated.
No unrun APF2 hardware result becomes a pass by moving this pointer.

Use the active milestone's acceptance criteria and actual checks as the
completion authority. Update this pointer and milestone evidence when a slice
advances; do not duplicate historical diagnostic instructions here.
