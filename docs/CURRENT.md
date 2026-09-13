# Current work

Active milestone: [M6 — Album Player](milestones/M6-album-player.md)

The current task is M6 slice 4: connect native-engine hold to frame-boundary
pause with retained resampler/pending samples and continuous silent I2S, then
prove gain/audible commit and adopt sound-control/CDC/ACK from that evidence.
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
The real audible mapping,
pending observation storage and sound/storage adapters remain to be wired.
See M6 for the executed checks; this does not establish a working Pocket audio
adapter, rendered Pocket UI or M6 integration acceptance.
Use the [transition proposal](design/pocket-player-transition-contract.md),
preserve v1 compatibility and adopt each contract before its implementation.
Slices 1–2 implement [host metadata](../specs/host-metadata-v1.md),
[ALBM storage](../specs/album-catalog-v1.md),
[folder ingestion](../specs/album-ingestion-v1.md) and
[Core-owned catalog pages](../specs/catalog-query-v1.md). Windows host checks,
focused Linux sanitizer tests and the catalog RISC-V link probe passed; see the
milestone for exact evidence. These are not M6 playback/UI/hardware acceptance.
The [accepted Q1–Q24 requirements](design/pocket-library-player-spec-draft.md)
define the intended album player; do not reopen settled product questions.
Continue through the milestone's host-verifiable slices until a concrete
hardware check or an unresolved product decision requires the user.

[M5 evidence](milestones/M5-real-mdx-library-playback.md) remains preserved.
The user closed its investigation on 2026-09-12; M5 is deferred, not passed.
M6 explicitly owns the remaining timing/CDC, placement, APF failure/lifecycle
and future PCM/UI reserve gates. ADR-0008 production promotion remains gated.
No unrun APF2 hardware result becomes a pass by moving this pointer.

Use the active milestone's acceptance criteria and actual checks as the
completion authority. Update this pointer and milestone evidence when a slice
advances; do not duplicate historical diagnostic instructions here.
