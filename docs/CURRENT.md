# Current work

Active milestone: [M6 — Album Player](milestones/M6-album-player.md)

The current task is **organizing the UI interaction requirements** after
accepting the [Firmware 2.6 / Minimal Player r5 hardware report](milestones/M6-album-player.md#minimal-player-r5-hardware-result--2026-09-22).
The user selected this scope and settled [UI Q1–Q2](design/pocket-mdx-compatibility-plan.md#ui-interaction-requirements):
B returns from tracks to playlists without stopping playback when the list has
focus, and stops playback when the controls have focus. B never changes panels.
L/R switches panels; the D-pad stays within the focused panel. In lists,
up/down selects entries and left/right retains page navigation. The remaining
bindings are still being decided; this is not behavior already implemented in r5.
r5 is the accepted hardware baseline. The normal three-list/42-entry collection
passes browsing during playback, original-list automatic advance, track selection
changing the playback list, list-end stop, per-list position restoration,
transport/repeat controls, FM/PCM stereo playback and restart defaults. No input
delay, dropout or noise was reported. The 100 × 300 entry check is also reported
as OK, with index load/validation I 6,598 ms; normal I is 20 ms. These are not
total boot times. Reported timings are R 12,691 / F 473 / D 1,594 / V 669 us;
Q and failed-track skipping were not reported. The scale data repeats the same
six songs, rather than testing 30,000 distinct songs.
This completes the [multiple-playlist slice](design/pocket-mdx-compatibility-plan.md#multiple-playlist-requirements),
not M6 or production-substrate acceptance. The [implementation evidence](milestones/M6-album-player.md#multiple-playlist-implementation--2026-09-22)
records Full 90/90, native/reference, RV32 resource and package checks.
Keep the r5 package as the baseline during UI requirements work; existing M3Us
need a [newly imported HPL2 collection](development/m3u-library.md).
Minimal Player r4 was the preceding accepted hardware baseline. It passes the
[loop/repeat boundary](design/pocket-mdx-compatibility-plan.md#loop-and-repeat-switching-boundary):
Y cycles 2, 3, 5 loops and repeat one, including changes during playback, pause
and fading. Natural endings, M3U-end behavior, session-setting retention,
manual controls, browsing and silent/two-loop restart defaults also pass.
No noise, dropout, position jump, tempo problem or input delay was reported.
Observations: R 5161 us, F 434 us, D 1307 us, V 671 us and Q 1023 frames.
The numeric track count and failed-track skipping were not reported in this
check. The [r4 implementation evidence](milestones/M6-album-player.md#loop-and-repeat-implementation--2026-09-22)
records Full 90/90, native/reference, RTL, RV32, fit/CDC and package checks.
This completes the loop/repeat slice, not M6 or production-substrate acceptance.
The [pause/resume boundary](design/pocket-mdx-compatibility-plan.md#pause-and-resume-boundary)
uses X to pause/resume, A to start the selected track from its beginning and B
to stop. The implementation keeps X in the replaceable input bindings and holds
FM/PCM, fade progress and buffered audio at a stereo frame boundary. The historical
`out/build/minimal-player-r4-update.zip` preserves the installed M3U/HPL1
collection and updates the matching HYB4 FPGA/boot ROM/application together;
the current Japanese procedure covers r5/HPL2 instead.
See the [implementation evidence](milestones/M6-album-player.md#pause-and-resume-implementation--2026-09-22).
Minimal Player r3 was the preceding accepted hardware baseline: the 27-track list,
FM/PCM pause and resume, browsing while paused, A/B controls, held/repeated X,
fade continuation, automatic advance and restart/power-cycle recovery all pass
the user's report. No noise, position jump, tempo problem or input delay was
reported. Observations: R 5421 us, F 494 us, D 1423 us, V 666 us and Q 960 frames.
Failed-track skipping was not reported in this check; its host evidence remains
separate. The pause/resume slice is complete, not M6 or production-substrate acceptance.
The [Firmware 2.6 / Minimal Player r2 hardware report](milestones/M6-album-player.md#minimal-player-r2-hardware-result--2026-09-22)
passes the [continuous-playback slice](design/pocket-mdx-compatibility-plan.md#continuous-playback-boundary):
M3U-order automatic advance, one intro plus two loop bodies and a five-second
FM/PCM fade, natural endings once, list-end stop, independent browsing position
and playing marker, manual switching/stop/restart and reboot/power-cycle recovery.
No noise, dropouts, input delay or errors were reported. Failed-track skipping
was not exercised on hardware because no errors occurred; its authored host
coverage remains in the [implementation evidence](milestones/M6-album-player.md#continuous-playback-implementation--2026-09-21).
r2 was the preceding accepted hardware baseline. Observations: R 12755 us, F 478 us,
D 1554 us, V 664 us and Q 947 frames. The update package remains
`out/build/minimal-player-r2-update.zip`, core `0.14.0-player-r2`, with matching
HYB2 FPGA/boot ROM/application. HPL1 and the
[M3U import workflow](development/m3u-library.md) are unchanged.
The remaining M6/production-substrate gates are not completed by that report.
The user's [27-track hardware report](milestones/M6-album-player.md#m3u-27-track-hardware-result--2026-09-21)
passes on Firmware 2.6 / Minimal Player r1: 27 imported, zero excluded, correct
M3U order and Japanese titles, FM/PCM stereo playback, browsing during playback,
song switching and stop/restart, with no reported errors, noise or dropouts.
This completes the M3U -> MDX/PDX preparation -> HPL1 -> Pocket playback slice.
The implementation previously passed Full **90/90**; see the
[implementation evidence](milestones/M6-album-player.md#m3u-library-import--2026-09-21).
The M3U importer did not change Pocket's player, firmware or RTL. Other
library/UI expansion remains deferred.
The [Minimal Player r1 hardware check](milestones/M6-album-player.md#minimal-player-r1-hardware-result--2026-09-21)
passes on Firmware 2.6: startup silence, six-song Japanese navigation, FM/PCM
stereo playback, browsing without reported delay/noise/dropout, song switching,
stop/restart and reboot/power-cycle playback. Third-track observations are
R 7818 us, F 449 us, V 658 us and Q 1013 frames; D was not reported.
This completes the minimal selection/play/stop hardware slice, not every M6
or production-substrate gate. The local package remains
`out/build/minimal-player-r1.zip`, core `0.13.0-player-r1`, using HPL1 prepared
pairs. Host and package checks are in the [implementation evidence](milestones/M6-album-player.md#minimal-player-implementation--2026-09-21).
On 2026-09-21 the user accepted six-song playback, continued
playback, song endings and loops and closed the current MDX compatibility
investigation. Follow the [next MVP step](design/pocket-mdx-compatibility-plan.md#current-acceptance-and-next-mvp-step)
and [project charter](../AGENTS.md#project-charter-2026-09-21): use the shortest
practical implementation; previous RPCMP compatibility is not required.
Do not resume exhaustive corpus verification as a prerequisite. Diagnose and
fix playback-engine defects when encountered. The evidence below records what
was measured, separately from the user's acceptance.
The first connected check now produces 48 kHz mixed audio from reference-driver
PCM and native JT51 simulation: eight authored cases and six private 10.24-second
prefixes pass stream/reconstruction checks. See the
[offline hybrid evidence](research/mdxplayer-compatibility.md#offline-hybrid-audio-experiment).
The minimal HYB1 transport now connects CPU-side word writes, two vendor
dual-clock FIFOs, native FM, wide mixing and 48 kHz audio pins. Three-phase
simulation verifies continuous refill, stop/reset, EOF and starvation; see the
[streaming evidence](research/mdxplayer-compatibility.md#target-streaming-transport).
The [CPU/AXI connection](research/mdxplayer-compatibility.md#cpu-renderer-and-actual-shell)
now passes reference stream comparisons, three-phase actual-AXI tests, the SDK
link/budget, whole-shell fit and scoped HYB1 timing/CDC audit, and Full host
verification (87/87). CPU RTL plus a modeled FIFO shows no starvation in three
authored PCM formats and one short real-input run; it is not real SDRAM/OS timing.
The Firmware 2.6/r2 user report confirms six real songs' melody/tempo, stereo,
stop/restart and PCM playback, plus restart/power-cycle recovery. Four songs
have no reported noise; two have clicks/buzz only on Pocket. Observed maxima
are 14.539 ms rendering and 0.460 ms feeding, with sampled queue minimum 808.
The HYB1 serializer omitted the official one-SCLK I2S delay, and its test receiver
made the same mistake. An independent protocol correction reproduces doubled
values; large peaks cross the sign boundary. The
[HYB1 r3 output-fix candidate](development/pocket-hybrid-hardware.md)
keeps the CPU renderer, OS and six
prepared inputs unchanged. Correct signed output, three-phase HYB1/AXI tests,
baseline RTL, full-shell fit/scoped CDC and package readback pass; final Full
host verification passes 88/88. See the
[r3 evidence](research/mdxplayer-compatibility.md#hyb1-r3の出力配置パッケージ検証).
The user's r3 follow-up confirms the noise disappeared in both affected songs,
with stereo/PCM, approximately one minute of stable playback, stop/restart and
reboot/power-cycle playback all reported OK. See the
[hardware result](research/mdxplayer-compatibility.md#hyb1-r3-hardware-follow-up).
The reported two-song noise defect is resolved. The subsequent
[user acceptance](research/mdxplayer-compatibility.md#compatibility-verification-accepted)
confirms six-song playback, continued playback, song endings and loops are
satisfactory and closes the compatibility investigation. No per-song r3 retest
matrix or exhaustive corpus comparison was supplied; neither is required to
close this investigation under the user's decision. Inherited shell external
constraints and full M6 integration acceptance remain separate open work.
The user selected [asaday/MDXPlayer](https://github.com/asaday/MDXPlayer) as the
reference: support every locally supplied file that played there, including
PCM. The MVP is track selection, play and stop. Follow the
[approved prototype direction](design/pocket-mdx-compatibility-plan.md) and its
[pinned source evidence](research/mdxplayer-compatibility.md).
Further Tracker/keyboard/policy expansion is deferred. Continue with CPU MXDRV/
PCM8 and FPGA FM, retaining the existing openfpgaOS facilities for the next
minimal player step. The r4 package and historical evidence remain intact.
The compatibility acceptance does not itself settle production-substrate gates.

The reference-synthesis RV32 instruction experiment now excludes an unchanged
reference renderer on the unchanged single-issue 90 MHz CPU for the tested
eight-FM-channel load: even `-O3 -flto` needs 99.18 million instructions per
audio second, or 134.95 million with eight ADPCM voices. These are ISA counts,
not hardware timings. The CPU/FPGA comparison selected hardware FM plus CPU PCM
for the prototype; neither the current OS nor JT51 is mandatory if it fails.
See the baseline's synthesis-budget evidence and portability findings. The
hybrid prototype direction is approved; production acceptance remains pending.

CPU RTL simulation further measures about 131 million cycles per audio second
for the reference FM-8 + ADPCM-8 case even on the dual-issue candidate with
optimistic independent AXI memories. PCM-only cases need about 52–58 million
cycles on the current CPU. The recommended design is reference MXDRV on CPU,
FPGA FM and software PCM8 through a thin timed audio path, with a minimal
select/play/stop UI. The
[design proposal](design/pocket-mdx-compatibility-plan.md) consolidates the
alternatives, library direction and next prototype's acceptance criteria.
An 80-track native prefix comparison preserves FM event positions/order and
PCM output when FM synthesis is removed. A separate 4 MHz native JT51 bus
experiment passes 3,072 writes and operator-bank checks. These support the
split; they do not establish a complete hybrid player, whole-corpus playback,
Pocket timing or production acceptance. The offline mix, target transport and
CPU application are now connected; the hardware follow-up and subsequent user
acceptance are recorded above. Broader corpus comparison is no longer next.

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
The [transition proposal](design/pocket-player-transition-contract.md) records
the historical v1-compatible approach. The current charter supersedes its
compatibility requirement for the replacement player.
Slices 1–2 implement [host metadata](../specs/host-metadata-v1.md),
[ALBM storage](../specs/album-catalog-v1.md),
[folder ingestion](../specs/album-ingestion-v1.md) and
[Core-owned catalog pages](../specs/catalog-query-v1.md). Windows host checks,
focused Linux sanitizer tests and the catalog RISC-V link probe passed; see the
milestone for exact evidence. These are not M6 playback/UI/hardware acceptance.
The [earlier Q1–Q24 requirements](design/pocket-library-player-spec-draft.md)
describe the fuller album player. The user's 2026-09-21 compatibility target
and smaller MVP take precedence for current work. The compatibility investigation
is now closed; these historical requirements do not expand the minimal MVP.

[M5 evidence](milestones/M5-real-mdx-library-playback.md) remains preserved.
The user closed its investigation on 2026-09-12; M5 is deferred, not passed.
M6 explicitly owns the remaining timing/CDC, placement, APF failure/lifecycle
and future PCM/UI reserve gates. ADR-0008 production promotion remains gated.
No unrun APF2 hardware result becomes a pass by moving this pointer.

Use the active milestone's acceptance criteria and actual checks as the
completion authority. Update this pointer and milestone evidence when a slice
advances; do not duplicate historical diagnostic instructions here.
