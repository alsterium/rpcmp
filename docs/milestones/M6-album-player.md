# M6 — Album Player

Status: active on 2026-09-13, following the user's instruction to continue until
hardware evidence or a new product decision is needed.

Current direction changed on 2026-09-21: the user requires every locally supplied
file that played in [asaday/MDXPlayer](https://github.com/asaday/MDXPlayer), with
PCM included. MVP controls are selection, play and stop. Further visualization
and playback-policy expansion is deferred. The user approved the
[hybrid prototype](../design/pocket-mdx-compatibility-plan.md) and the
[simple-implementation charter](../../AGENTS.md#project-charter-2026-09-21),
waiving backward compatibility with earlier RPCMP APIs/formats. The first
[offline mixed-audio check](../research/mdxplayer-compatibility.md#offline-hybrid-audio-experiment)
passes. The [HYB1 streaming transport](../research/mdxplayer-compatibility.md#target-streaming-transport)
also passes authored CPU-side-write to audio-pin simulation. The subsequent
[CPU/actual-AXI connection](../research/mdxplayer-compatibility.md#cpu-renderer-and-actual-shell)
passes nine reference-stream comparisons, the SDK link/budget, three-phase AXI
simulation, whole-shell fit/scoped CDC audit and Full host checks (87/87).
The Firmware 2.6/r2 report confirms real stereo/PCM playback and stop/restart
for six songs, with peak-specific noise on two. The omitted I2S one-bit delay
was reproduced after correcting the test receiver from the official protocol.
The [HYB1 r3 hardware candidate](../development/pocket-hybrid-hardware.md)
fixes that output while keeping the CPU renderer and prepared songs unchanged.
The r3 candidate passes three-phase HYB1/AXI and baseline RTL, fitted timing/scoped
CDC, package readback and final Full host verification (88/88); see the
[output-fix evidence](../research/mdxplayer-compatibility.md#hyb1-r3の出力配置パッケージ検証).
The [r3 hardware follow-up](../research/mdxplayer-compatibility.md#hyb1-r3-hardware-follow-up)
reports both affected songs free of the previous noise, with stereo/PCM, stable
one-minute playback, stop/restart and reboot/power-cycle playback OK. This closes
the reported two-song noise defect. The user subsequently accepted all six songs,
continued playback, song endings and loops and explicitly closed the current
MDX compatibility investigation; see the
[acceptance record](../research/mdxplayer-compatibility.md#compatibility-verification-accepted).
The planned exhaustive comparison is no longer a prerequisite; handle newly
encountered playback defects with focused diagnosis and engine fixes.
The [minimal selection/play/stop player](#minimal-player-implementation--2026-09-21)
now passes its [Firmware 2.6 / r1 hardware check](#minimal-player-r1-hardware-result--2026-09-21).
The user subsequently approved the [PC-side M3U import workflow](#m3u-library-import--2026-09-21)
over that working baseline: M3U defines MDX order and the PC creates the
Pocket-specific collection with required PDX dependencies.
The user's [27-track M3U hardware report](#m3u-27-track-hardware-result--2026-09-21)
now passes that connected import/playback workflow with zero exclusions and
no reported problems. The user has now approved
[continuous M3U-order playback](../design/pocket-mdx-compatibility-plan.md#continuous-playback-boundary)
as the next slice. The [r2 implementation](#continuous-playback-implementation--2026-09-21)
now passes its [Firmware 2.6 hardware report](#minimal-player-r2-hardware-result--2026-09-22).
r2 established the continuous-playback baseline; failed-track skipping was not
exercised on hardware because no errors occurred. The subsequent approved slice is
[pause/resume with provisional X input](../design/pocket-mdx-compatibility-plan.md#pause-and-resume-boundary);
the [r3 implementation](#pause-and-resume-implementation--2026-09-22) now passes
its [Firmware 2.6 hardware report](#minimal-player-r3-hardware-result--2026-09-22).
r3 was the preceding accepted hardware baseline. The user subsequently approved
[loop/repeat switching with provisional Y input](../design/pocket-mdx-compatibility-plan.md#loop-and-repeat-switching-boundary).
r4 passes Full 90/90, its affected integration gates and the subsequent
[Firmware 2.6 hardware report](#minimal-player-r4-hardware-result--2026-09-22).
r4 established the preceding hardware baseline. The user subsequently selected
[multiple-playlist support](../design/pocket-mdx-compatibility-plan.md#multiple-playlist-requirements)
and settled its Q1–Q6 requirements: multiple M3U inputs plus optional folder
import, uninterrupted browsing across lists, track selection changing the active
list, explicit name/order rules and per-list session cursor/page retention.
The [r5 implementation](#multiple-playlist-implementation--2026-09-22) supports
100 lists of up to 300 entries using HPL2. Host checks cover 30,000 registrations,
and the target build uses 4,486,412 static bytes. The subsequent
[Firmware 2.6 / r5 hardware report](#minimal-player-r5-hardware-result--2026-09-22)
passes the normal three-list/42-entry flow and reports the 100 × 300 entry check
as OK. Index load/validation I is 20 ms normally and 6,598 ms at scale; total
boot time and Q were not separately reported. r5 is now the accepted baseline.
The user selected [UI interaction requirements](../design/pocket-mdx-compatibility-plan.md#ui-interaction-requirements)
as the next task. Q1 assigns B by focus: return from tracks to playlists without
stopping playback, or stop when the controls have focus. B does not move between
panels. Q2 uses L/R for panel switching and the D-pad within a panel, retaining
up/down entry selection and left/right pages in lists. Q3 leaves X/Y unassigned,
with A activating control icons. Q4 makes the play/pause icon pause/resume the
playback track or restart the last-played track after stop; initial playback
starts from the track list. Q5 adds previous/next within the playback playlist
to the play/pause, stop and repeat-count icons. Q6 starts the target from its
beginning even from pause/stop. Q7 disables the unavailable direction at the
first/last entry without wrapping or changing playback state. Q8 remembers
control-icon focus during the session and retains list cursor/page positions.
Q9 removes the back row from track pages; B returns to playlists. The operation
table is consolidated. Q10 selects elapsed time only, frozen during pause.
Q11 scrolls the selected overlong name after a brief dwell and elides other rows.
Q12 fixes the lower information as playback title, playback playlist name,
entry number / count, elapsed time and transport state, with the repeat setting
on its control icon. Q13 places previous/play-pause/next above stop/repeat in two
rows at the lower right. Q14 makes either L or R toggle panels while retaining
their focus. The user authorized implementation as Minimal Player r6 using the
[UI boundary](../design/pocket-mdx-compatibility-plan.md#ui-implementation-boundary).
The [r6 implementation](#minimal-player-r6-ui-implementation--2026-09-22) passes
its local gates and the user's [Firmware 2.6 hardware report](#minimal-player-r6-hardware-result--2026-09-22).
r6 established the preceding baseline. The [r7 UI follow-up](../design/pocket-mdx-compatibility-plan.md#r7-ui-follow-up) adds
X panel switching, scrolling playback information and a two-arrow loop icon.
It now passes the user's [Firmware 2.6 hardware report](#minimal-player-r7-hardware-result--2026-09-22).
r7 is the accepted hardware baseline; the next requirement has not been selected.
These UI slices do not complete M6. Inherited shell external timing constraints
and the remaining player integration stay separate. The original objective,
adopted contracts and results below remain historical evidence; they do not
override this scope or turn the existing FM subset into full MDX compatibility.

## Objective and adoption

Implement the accepted Q1–Q24 [album-player requirements](../design/pocket-library-player-spec-draft.md):
100–300 FM-only tracks, album browsing, transport/loop/shuffle controls,
Tracker/keyboard/library views and playback settings. On 2026-09-20 the user
deferred persistence: every application launch starts with AlbumOrder / Default
(two loops with five-second fade), shuffle off and no autoplay. Live setting
changes remain required. Persistent settings and their APF storage integration
are future work, not completion gates for this milestone. Existing optional
contracts and tested components remain available without enabling them in the
Pocket build. Audio remains independent of UI. Work proceeds one slice at a time.

Slice 1 adopts the host portion of the [text profile](../design/pocket-text-rendering-profile.md)
as [host metadata v1](../../specs/host-metadata-v1.md). CP932 table 2.01 and
utf8proc 2.11.3 are host ingestion dependencies; the normalized byte writer,
Pocket runtime and current v1 Core/UI/device protocols remain independent.
Subsequent contract proposals must be adopted explicitly at their slice entry;
this milestone does not silently adopt unresolved MMIO or hardware deadlines.

Slice 2 adopts [album catalog v1](../../specs/album-catalog-v1.md) for the
optional ALBM payload and bounded writer/reader. This preserves rpcmlib 1.0
and the old writer API. The proposal's claim that identical tracks already
failed in the old writer was incorrect: only the new album API rejects equal
TrackIdentity input. NFC remains a host producer requirement. Folder ingestion
adopts [album ingestion v1](../../specs/album-ingestion-v1.md), including explicit
exclusion/global-failure and native publication rules. Core generation/pages
adopt [catalog query v1](../../specs/catalog-query-v1.md): copied bounded pages,
Core-owned invalidation and a separate read-only interface. This does not adopt
PlayerCommand/PlayerSnapshot schema 2 or change v1 transport behavior.

Slice 3 first adopts [Core playback transport v1](../../specs/playback-transport-v1.md)
for asynchronous preparation/audio control, cancellation and deadlines. It is
internal Core machinery. [Player state schema 2](../../specs/player-state-v2.md)
adopts copied transport observations and Core-owned publication.
[Transport command schema 2](../../specs/player-command-v2.md) adopts the bounded
ingress, replay/projection and PlayerSession boundary. The remaining
policy/history/settings/UI contracts follow in this slice.
[MDX sequencing progress v1](../../specs/mdx-progress-v1.md) adopts checked
TrackLoop counters and transactional, timestamped read-ahead observations.
Audible commit, loop policy and gain integration remain subsequent work.
[Media loop envelope v1](../../specs/media-loop-envelope-v1.md) adopts the
executable audio-boundary model: sealed mapped progress, generation-tagged
controls, live repeat decisions, integer stereo gain and pause/end ordering.
[Repeat policy schema 2](../../specs/playback-policy-v2.md) adopts the public
setting command, desired/applied revisions, coherent audio observations and
finite RepeatOne restart through a capability-gated port extension.
[Navigation schema 2](../../specs/playback-navigation-v2.md) adopts the bounded
album index, Start-confirmed shuffle history, injected rejection-sampled random
input, neighbour commands and automatic selection through normal transport.
[Performance history](../../specs/performance-history-v2.md) adopts the bounded
collector/read port and optional current-channel/history publication.
[MDX performance observations](../../specs/mdx-performance-v1.md) adopt emitted-write
observations and conversion at an injected, correlated audio boundary. The
effective output clock is explicit; the MDX timer model does not select it.
[Persistent settings](../../specs/playback-settings-v2.md) adopts two checked
records, asynchronous ownership, startup gating and copied storage observations.
It preserves the adopted policy-exhaustion behavior over the earlier proposal.
[Album UI v1](../../specs/player-ui-v1.md) adopts replaceable input/focus tables,
bounded browsing, Tracker/keyboard projection and the host mock canvas. Its
additive policy-command result closes the UI's ordered-setting feedback loop.
The real audio/storage/input/framebuffer adapters remain subsequent work; this
does not advertise a new capability on the existing Pocket backend.
Hardware ACK deadlines and MMIO remain slice 4 decisions based on RTL evidence.

Slice 4 adopts [Pocket media audio v1](../../specs/pocket-media-audio-v1.md) for
the synchronous write/hold/output modules. It separates urgent stream reset
from ordinary frame-boundary pause and retains source/pending ownership. The
new serializer uses the official one-bit I2S delay; old v1 serial behavior and
queue/reset APIs remain unchanged. This does not adopt CPU control or CDC/ACK.

Slice 4 also adopts [media envelope RTL v1](../../specs/media-envelope-rtl-v1.md)
for the 256-entry mapped-progress queue, policy/epoch validation, frame-boundary
gain and cancellation. Its inputs require an independently proven audible
mapping. This local controller does not adopt new MMIO or CPU ACK deadlines.

Slice 4 adopts [Pocket enveloped audio v1](../../specs/pocket-enveloped-audio-v1.md)
for shared native/output ownership, gain before serialization and locally owned
physical reset after terminal states. Already-mapped progress remains an input;
this does not adopt the producer mapping or sound-control/CDC protocol.

The M6 write owner also requires its data pulse to reach JT51's `cen_p1` busy
latch edge. This remains a `cen` edge and preserves the existing port contract;
no exact pulse length or public maximum write latency was previously promised.
It corrects accepted burst writes being overwritten before their native scan,
not a product-policy choice or a change to legacy v1 MMIO/audio semantics.

Slice 4 adds the source-sample-position observation in
[Pocket enveloped audio v1](../../specs/pocket-enveloped-audio-v1.md): retained
audio-edge indices travel with the actual selected/pending/serialized sample.
This adopts the sample-to-output part of the mapper, with explicit empty/pause/
reset and exhaustion semantics. It does not equate native capture time with a
register's audible effect or adopt the remaining MDX progress mapping.

With explicit user approval on 2026-09-13, slice 4 also adopts ordered source
receipts in that same local contract. Writes and zero-write markers receive
stream-local tokens; the actual completed bus/marker edge is retained until
delivery, with reserved capacity, hold/reset and exhaustion semantics. This
does not equate bus receipt with native-pipeline or audible completion.

Slice 4 adopts [JT51 native completion v1](../../specs/jt51-native-completion-v1.md)
for the conservative direct-control processing prefix, receipt capacity and
sample/prefix ownership. Its native schedule/accumulator derivation and local
RAM/timing evidence support this internal boundary. Retained musical history
is not flushed, and first waveform difference is not the completion oracle.
The scheduled-progress producer, retained MDX batches and CPU/CDC/ACK remain
separate work; this does not advertise them as implemented.

Slice 4 adopts [retained media source queue v1](../../specs/media-source-queue-v1.md)
for ordered audio-clock writes/markers and source-dispatch coverage. A 64-entry
queue streams larger immutable MDX batches; source busy/receipt backpressure
is distinct from missing input at an eligible empty dispatch opportunity.
This is not mapped envelope coverage, a CPU deadline, or a change to queue v1.

With explicit user approval on 2026-09-13, slice 4 adopts
[MDX audible progress v1](../../specs/mdx-audible-progress-v1.md): marker metadata
travels through the actual native completion FIFO and selected PCM into mapped
envelope intervals. The reset baseline and at-most-one-audio-frame startup
deferral are approved; Q1–Q24 policy remains unchanged. Implementation and
verification follow this adoption; no acceptance result is implied by approval.

After reviewing the concrete CPU connection proposal, the user approved its
adoption on 2026-09-13. The [CPU connection plan](../design/pocket-cpu-sound-connection.md)
covers separate control/feed/capture mailboxes, normal reset/inhibit, retained
MDX feeding and bounded output history. Its first implementation contract is
[Pocket sound session v1](../../specs/pocket-sound-session-v1.md). Exact MMIO
words and CPU/CDC deadlines will be specified and verified before their layer
is implemented; this approval does not predeclare their acceptance.
The next layer is specified in [Pocket sound MMIO v1](../../specs/pocket-sound-mmio-v1.md):
three copied request/response mailboxes, coherent state capture and an independent
acknowledged emergency notification. Epoch is the session's checked feed epoch;
old transfers drain with their original identity rather than resetting toggles
while data is borrowed. Journal capability remains unadvertised until its layer.

## Slices and acceptance

Slice 4 also adopts the internal [settings RAM owner v1](../../specs/pocket-settings-ram-v1.md)
within the approved storage-adapter direction. It defines synchronous bank,
read-lock and media-readback ownership without changing the Core record or
reserving a physical address. APF/CPU integration and durable media evidence
remain required before advertising settings support on Pocket.

1. **Host metadata:** strict CP932 embedded titles, filename fallback with
   reasons, bounded UTF-8/UTF-16 conversion and NFC. Connect the existing host
   packer while retaining its explicit-title invocation. Verify independent
   Unicode examples, invalid encodings, capacity boundaries, identical output
   for canonically equivalent metadata and unchanged source blobs. Vendor exact
   source bytes and licenses so offline builds do not fetch dependencies.
2. **Album ingestion and catalog:** adopt ALBM and the 300-track/32 MiB profile;
   scan native folder paths, retain IDs and natural numeric order, report mixed
   exclusions, reject global failures/collisions transactionally. Test 300/301,
   size limits, duplicate identities, malicious sections, generation-scoped
   catalog pages and reproducibility without private music fixtures.
3. **Headless player and mock UI:** adopt schema 2, policy/history/settings
   contracts and input tables. Prove transitions, cancellation, loop counting,
   shuffle completion, save races and focus/commands against injected ports and
   snapshots. Preserve v1 compatibility; prove no renderer dependency.
4. **Sound feasibility:** prove state-preserving pause, stereo
   frame boundaries, cancellable fade reservations and audible commit in RTL;
   derive the sound-control port/CDC/ACK contract from those results. Implement
   the sound adapter. Use sequential RTL suites and fault injection before
   target integration. The previously planned APF settings RAM-slot
   flush/readback and timeout ownership are deferred by the 2026-09-20 decision.
5. **Pocket UI and integration:** generate licensed bitmap fonts, connect the
   real input/framebuffer/catalog/player adapters, cross-build and measure ELF,
   stack and framebuffer/font allocation. Review exact official APF mappings
   when implementing them. Run synthesis/timing/CDC and package-coherence checks.
6. **Hardware acceptance:** firmware 2.6 verifies playback, pause/resume, fade,
   navigation while playing, layout/readability, settings returning to defaults and
   failure silence across relaunch/power cycles. Stop for the user once a
   concrete checked candidate and focused hardware checklist are ready.

Follow the [harness verification milestones](../development/harness.md): focused
tests and Fast during iteration, Full when the approved connected behavior works
and at candidate/acceptance gates. RTL, cross-build, fit and hardware checks are required
when those paths change; host-only results cannot establish their acceptance.

## Carried integration gates

On 2026-09-20 the user explicitly replaced Q19's persistence requirement with
per-launch defaults. This resolves the scope change without changing the
optional settings protocol. Pocket integration omits its storage controller,
capability and save-status UI. In-session policy changes, audio behavior and
all other carried gates remain required. Historical storage evidence below is
retained; its statements of what was next do not override this decision or
[CURRENT](../CURRENT.md).

For this documentation change, `pwsh -File tools/host-verify.ps1 -CheckSetupOnly`
passed, then `pwsh -File tools/host-verify.ps1` passed **81/81** in **483.99 s**,
including format, tidy and architecture checks. Log:
`out/build/m6-settings-deferred-host.log`. Production code/package inputs did
not change in this unit; RTL and hardware acceptance do not apply to the
documentation update. The subsequent AXI integration has separate evidence.

[M5](M5-real-mdx-library-playback.md) is deferred with its historical evidence,
not declared complete. Its diagnosis workstream remains closed by the user's
2026-09-12 decision. The following gates have explicit owners in this milestone:

| Unfinished gate | Owner and required evidence |
| --- | --- |
| M5 acceptance 6: integrated timing/CDC, external-I/O constraints and headroom | Slice 5: actual new fit, reviewed constraints and clock/reset crossings; explicit future PCM/UI reserves |
| General placement/clean-source reproducibility and ADR-0008 promotion | Slice 5: coherent source/ROM/OS/app build and placement evidence; retain ADR-0008 fallback criterion |
| Broader APF lifecycle, transport timeout and in-playback fault silence | Slices 4 and 6: simulation plus targeted firmware 2.6 results |
| Unrun APF2 primer-exclusion hardware probe | Historical M5 result remains unverified; slice 6 checks the new integrated loader and failure path, without requiring another old-candidate investigation |
| Production substrate promotion | After slices 5–6 and all ADR-0008 gates; never inferred from host success or user acceptance of this feature plan |

## Progress

Slice 1 is complete on 2026-09-13. The packer now accepts an omitted title,
decodes CP932 strictly and reports filename fallback reasons. Both automatic
and explicit metadata use bounded NFC; original MDX blobs remain unchanged.
The writer has a separate target without the host Unicode dependency.

Executed evidence:

- `pwsh -File tools/host-verify.ps1 -CheckSetupOnly`: PASS after correcting the
  child process's code page for compiler dependency discovery.
- Fresh configuration and a clean rebuild, then the metadata/ingest/writer/
  session/preflight/incremental-build subset: 10/10 PASS, 2.98 seconds.
- `pwsh -File tools/host-verify.ps1`: 54/54 PASS, 148.22 seconds, including
  formatting, clang-tidy and positive/negative architecture checks. Log:
  `out/build/m6-metadata-host.log` (ignored).
- `python -B tests/harness/harness_tests.py`: 7/7 PASS.
- Offline Docker `rpcmp-openfpgaos-toolchain:14.2.0-3`, read-only source and
  temporary build directory: native GCC 13.3.0 compiled the host packer and
  metadata/writer/stable-ID tests; all passed, followed by the same Python
  native-path CLI cases. This was a focused POSIX build, not the Windows host
  gate or a RISC-V cross-build. Command script and log:
  `out/build/m6-metadata-posix.py`, `out/build/m6-metadata-posix.log` (ignored).

An isolated header-change fixture reproduced a pre-existing CMake/MSVC include
decoding failure; the corrected UTF-8 child environment passes that fixture.
After rebuilding stale consumers, the original ingestion/session tests pass.
No expected trace or golden value was regenerated to conceal the failure.

No RTL, synthesis, cross-build, font rendering or hardware inputs changed in
slice 1; those checks were not run. No M6 hardware capability or production
promotion has been established.

Slice 2 storage and logical lookup are implemented on 2026-09-13. The optional
ALBM section retains explicit album/member order independently of stored ID
order. Validation checks ownership, references, key syntax/uniqueness and the
300-track/album, 1 MiB string-data and 32 MiB file limits before publishing a
borrowed view. Failed opens preserve the previous view. Old writer goldens and
old-reader compatibility are retained.

The independently encoded [fixture](../../tests/fixtures/rpcmlib/README.md)
predates the writer/reader implementation. Tests compare its complete bytes,
mutate CRC-correct logical conditions, and exercise exact/over-limit capacities.
Executed storage evidence:

- `pwsh -File tools/host-verify.ps1`: 55/55 PASS, 160.72 seconds, including
  formatting, clang-tidy and positive/negative architecture checks. The first
  run passed functional tests but failed tidy on 13 type/style/reserve
  diagnostics; these were corrected without suppression. Final log:
  `out/build/m6-album-host.log` (ignored).
- Focused Windows catalog/library/writer/session/preflight tests: 14/14 PASS,
  7.99 seconds before the final extra owner/key and 300-album cases.
- Offline native GCC 13.3.0 with ASan/UBSan: album catalog, old writer and
  stable-ID tests PASS, without sanitizer findings. Script/log:
  `out/build/m6-album-posix.py`, `out/build/m6-album-posix.log` (ignored).
- Offline Docker `make --file spikes/pocket/openfpgaos/Makefile
  M5_FILE_OUT_DIR=/repo/out/build/m6-album-legacy-cross m5-file-budget`: PASS.
  Final RISC-V ELF has no undefined symbols, text 67,396, data 228 and BSS
  2,997,016 bytes; conservative stack bound 24,848 bytes. An initial `substr`
  call pulled an unavailable exception runtime into the bare target link;
  checked pointer/length views removed that dependency without runtime stubs.
  This builds the existing M5 app, not an integrated M6 player. Script output:
  `out/build/m6-album-legacy-cross.log` and its `budget.json` (ignored).
- `python -B tests/harness/harness_tests.py`: 7/7 PASS; navigation/preset check
  `python -B tools/check_harness.py`: PASS after the progress update.

At that storage checkpoint, folder scanning, mixed exclusions, native
transactional output and Core-owned generation/pages remained in slice 2.
No RTL inputs or hardware package changed
in this storage unit, so RTL simulation, synthesis and hardware tests were not
run. The target cross-build does not establish playback or hardware acceptance.

Slice 2 folder ingestion and native publication are implemented on 2026-09-13.
`rpcmp_album_pack` groups direct folder contents, follows deterministic natural
filename order and reports exclusions separately from skipped files and title
fallbacks. Global failures publish nothing; a completed temporary file replaces
the destination only after write/flush/close. Input MDX aliases, linked outputs
and directories are rejected. OS path/access constraints still apply, and the
source tree must stay unchanged during the operation.

The host API uses injected read/output ports for failure tests; native adapters
have a separate target. Reused MDX preparation now exposes an internal metadata
step, avoiding per-track intermediate library serialization. No dependency,
Core/UI schema, RTL or Pocket application input changed in this unit. Cross-
build, RTL simulation, synthesis and hardware tests are therefore not run here.
Core generation/pages remain in slice 2; M6 playback/UI/hardware are pending.

Executed folder-ingestion evidence:

- `pwsh -File tools/host-verify.ps1`: 57/57 PASS, 174.03 seconds, including
  the batch/CLI cases, existing single-file ingestion, format, clang-tidy and
  positive/negative architecture checks. Final log:
  `out/build/m6-ingest-host.log` (ignored).
- Focused album/metadata/library/writer/session/preflight checks: 16/16 PASS,
  8.54 seconds before the final diagnostic-path guard. An additional 4097-byte
  path case then failed because rejection copied the oversized input into the
  diagnostic; the guard fixes that case without truncating accepted metadata.
  The final full gate covers it. Failure evidence:
  `out/build/m6-ingest-boundary-before.log` (ignored).
- Offline Docker native GCC 13.3.0 with ASan/UBSan: batch tests and both real
  native-path CLI suites PASS, without sanitizer findings. Production CLI
  compilation uses the same no-exception/no-RTTI settings as CMake; an initial
  script mismatch caused a typeinfo link failure and was corrected in the
  runner. Script/log: `out/build/m6-ingest-posix.py` and
  `out/build/m6-ingest-posix.log` (ignored).
- Native cases verify Unicode/NFC names, parent relocation, Windows junction /
  POSIX symlink skips, mixed/all exclusions, MDX hard-link output rejection and
  retained destinations on global/publication errors. The Python ALBM inspector
  is independent of the C++ writer. Unit cases cover reverse enumeration,
  100-digit numbers, 300/301 accepted tracks and injected I/O stage failures.

The first authored test helper mistakenly removed 11 instead of the literal
12 bytes of `RPCMP ORACLE`; correcting that input construction left the intended
order and exclusion assertions unchanged. No source fixture/golden was changed.
Native output handles retain their OS type. The scanner reads Windows kind and
size in one native attribute query, which also avoids a clang analyzer enum
diagnostic inside MSVC's `filesystem::file_size`; no warning suppression was added.

Slice 2 Core generation/catalog pages are implemented on 2026-09-13, completing
the slice's host acceptance. `CatalogSession` invalidates references at open/
reload/close start, rejects obsolete completions and exposes copied 16-item pages
through the separate const `CatalogReader` interface. Names use complete UTF-8
prefixes up to 96 bytes with explicit truncation. Core-only track resolution
checks generation and ID without changing transport. Playback command integration
and actual browsing UI remain slice 3; v1 behavior is preserved.

Executed catalog-query evidence:

- `pwsh -File out/build/m6-catalog-focused.ps1`: 13/13 PASS, 7.76 seconds
  (contracts, catalog, library/session and architecture subset).
- `pwsh -File tools/host-verify.ps1`: 60/60 PASS, 183.06 seconds, including
  format, clang-tidy and architecture/harness checks. Log:
  `out/build/m6-catalog-host.log` (ignored).
- Offline Docker GCC 13.3.0 with ASan/UBSan: contract-only mock pages and Core
  session tests PASS. Production objects use no exceptions/RTTI; tests also
  disable RTTI consistently with the polymorphic read port. Script/log:
  `out/build/m6-catalog-posix.py`, `out/build/m6-catalog-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-catalog-cross/Makefile catalog-check`:
  PASS for a link-only probe exercising the new paths with the pinned Pocket SDK.
  RISC-V ELF has no undefined symbols; text 21,660, data 188, BSS 1,800 bytes;
  conservative stack bound 13,040 bytes. This is not the integrated player and
  was not executed on hardware. Script/source/budget under
  `out/build/m6-catalog-cross`, log `out/build/m6-catalog-cross.log` (ignored).

Tests use the original independently encoded album fixture and literal IDs,
300-item pagination, stale/duplicate/failed loads, unchanged-build-ID reorder,
counter exhaustion, copied-page lifetime and authored UTF-8/mock boundaries.
An initial test helper violated the existing folder-basename/name rule; fixing
the helper preserved the storage contract. A focused NUL case then reproduced
one missing mock-page rejection (12/13 subset tests passed); the page validator
now follows rpcmlib v1's embedded-NUL prohibition. The new draft's contrary
sentence was corrected explicitly to that existing rule. Failure log:
`out/build/m6-catalog-nul-before.log` (ignored). No golden was regenerated.

Dependency checks now reject player-to-UI, UI-to-player/library and contracts-to-
implementation edges, with independent negative cases for includes and repeated
CMake link declarations. No RTL, APF definition or hardware package changed in
this unit, so RTL simulation, synthesis and hardware tests were not run.
Continue with slice 3; the platform's borrowed-buffer lifetime and serialized
control context remain integration responsibilities, not guarantees of the API.

Slice 3's internal asynchronous transport is implemented on 2026-09-13. It
validates catalog identity before selection, separates projected intent from
confirmed sound state, serializes generation-tagged resets and keeps cancelled
preparation storage owned until quiescence. Committed 48 kHz media positions
come from the audio port, not UI cadence. Unsupported pause is rejected; ACK
timeouts and failed resets never become success. Public schema 2 ingress and
snapshots, policy/loop/gain/history/settings and UI remain in slice 3.

Executed transport evidence:

- `pwsh -File out/build/m6-transport-focused.ps1`: 12/12 PASS, 7.21 seconds
  before the final test-only optional guards/invalid-tag construction change.
- `pwsh -File tools/host-verify.ps1`: final 61/61 PASS, 186.58 seconds, including
  format, clang-tidy, v1 and architecture checks. Log:
  `out/build/m6-transport-host.log` (ignored).
- Offline Docker GCC 13.3.0, ASan/UBSan: final scripted transport suite PASS.
  Production objects have no exceptions/RTTI; tests also disable RTTI for the
  polymorphic port boundary. Script/log: `out/build/m6-transport-posix.py` and
  `out/build/m6-transport-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-transport-cross/Makefile transport-check`:
  RISC-V link-only probe PASS, with no undefined symbols. Text 25,020, data 188,
  BSS 1,800 bytes; conservative stack bound 11,680 bytes. Probe source, makefile
  and budget are under `out/build/m6-transport-cross`; log is
  `out/build/m6-transport-cross.log` (ignored). Its test ports are not real
  Pocket adapters, and the executable was not run on hardware.

Independent scripted traces cover rapid T/U/Stop, old Reset/Start ACKs, Pause/
Resume projection, end held during pause, command/end ordering, catalog reload,
start/poll failures, exact deadlines, clock/counter limits and buffer ownership.
Three additional failure cases first failed: a library completion replaced an
existing device Error, a malformed late Reset ACK lifted terminal inhibition,
and a valid late Reset ACK retained storage unnecessarily. The fixes preserve
Error/inhibition and release only proven-quiescent storage. Failure log:
`out/build/m6-transport-failure-before.log` (ignored). No golden was regenerated.

Initial tidy findings were in test code: optional observations now fail explicitly
when absent instead of being dereferenced unchecked; the malformed enum case uses
an authored raw tag byte. The same rejection assertion remains, with no warning
suppression. No RTL, APF configuration or hardware package changed, so RTL
simulation, synthesis and hardware checks were not run. Real pause, output-inhibit
latency and ACK deadline bounds still require the slice 4 adapter/RTL evidence.

Slice 3's public transport observations are implemented on 2026-09-13.
`contracts::v2::SnapshotSource` exposes fixed-capacity copied values, and
`SnapshotPublisher` resolves the current track's title/album through catalog
pages. Its cache is scoped to library generation and TrackId. Publication
never polls playback ports; UI reads never drive publication or media time.
Pending intent, preparation/cancellation, shared control ACKs and error state
remain distinct. Public command ingress and non-transport observations remain
the next parts of slice 3.

Executed state-publication evidence:

- `pwsh -File tools/host-verify.ps1`: final 63/63 PASS, 212.27 seconds,
  including format, tidy, v1 compatibility and architecture rejection checks.
  Log: `out/build/m6-state-host-final.log` (ignored).
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  39 PASS (`out/build/m6-state-links.py`, ignored).
- Focused new tests: `ctest --preset host-msvc -R
  'player_state_v2|snapshot_publisher'`: 2/2 PASS, 0.13 seconds before the final
  metadata-cache tests. The older transport/catalog/architecture subset was
  12/12 PASS, 7.61 seconds.
- Final functional source under offline Docker GCC 13.3.0 ASan/UBSan:
  schema 2 contracts, publisher and transport suites PASS. Production disables
  exceptions/RTTI; tests disable RTTI. Script/log are
  `out/build/m6-state-posix.py` and `out/build/m6-state-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-state-cross/Makefile state-check`:
  RISC-V link-only probe PASS, no undefined symbols. Text 28,884, data 188,
  BSS 1,800 bytes; conservative stack bound 17,056 bytes. Source/Makefile and
  budget are under `out/build/m6-state-cross`; log is
  `out/build/m6-state-cross.log` (ignored). These are test ports, not a Pocket
  player binary, measured audible timing or hardware acceptance.

A focused case first failed when terminal failure was followed by library
close: internal `prepared` remained true without a selected track, preventing
a consistent snapshot. The getter now requires a selection. Storage remains
owned until quiescence exactly as before; this fix does not release it early.
`out/build/m6-state-before.log` records the two failed assertions in that case.
The contract-only target uses authored snapshots and tests UTF-8/count/tag/rate
bounds, absent values, old operation generations and unchanged v1 rejection.
Publisher tests use the independent album fixture for pause/stop, metadata
cache invalidation, malformed pages, retained copies, read-frequency independence,
clock reversal, sequence exhaustion and terminal fault visibility.

The first full host gate passed 62 tests and failed tidy on a declaration/
definition parameter-name mismatch. The names were aligned without suppression.
No RTL/APF/package inputs changed: RTL simulation, synthesis and hardware checks
were not run. The publisher remains a Core component awaiting command ingress
and platform integration; a publication failure must be handled by that owner.

Slice 3's public transport command ingress is implemented on 2026-09-13.
PlayerSession connects the seven schema 2 commands to the transport and state
publisher. Admission uses a 32-command FIFO, a 64-result replay window and the
same pure transition as execution. Submission and snapshot reads make no
playback-port calls. Catalog/fault changes after admission interrupt the batch
as an asynchronous failure; accepted receipts are not retroactively rejected.
Policy/loop/gain, history/settings and mock UI/input remain in slice 3.

Executed command-ingress evidence:

- `pwsh -File tools/host-verify.ps1`: final 65/65 PASS, 228.58 seconds,
  including format, tidy, v1 compatibility and architecture/harness checks.
  Log: `out/build/m6-ingress-host.log` (ignored).
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  47 PASS (`out/build/m6-ingress-links.py`, ignored).
- `pwsh -File out/build/m6-ingress-focused.ps1`: 16/16 PASS, 7.64 seconds
  before the final admission-context and malformed-batch boundary additions;
  those additions are covered by the full gate above. Log:
  `out/build/m6-ingress-after.log` (ignored).
- Offline Docker GCC 13.3.0 ASan/UBSan: command/state contract, PlayerSession,
  transport and publisher suites PASS. Production disables exceptions/RTTI;
  tests disable RTTI. Script/log: `out/build/m6-ingress-posix.py` and
  `out/build/m6-ingress-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-ingress-cross/Makefile ingress-check`:
  RISC-V link-only probe PASS, no undefined symbols. Text 32,124, data 188,
  BSS 1,800 bytes; conservative stack bound 22,912 bytes. Source/Makefile and
  budget are under `out/build/m6-ingress-cross`; log is
  `out/build/m6-ingress-cross.log` (ignored). This probe uses scripted ports,
  was not executed on Pocket and is not an integrated player memory budget.

The independent tests cover replay/ID/sequence ordering, 32/33 queue and 64/65
history boundaries, projected pause/resume, T/U/Stop, fault/closure after
acceptance, natural-end ordering and identical port traces with different
publication/read frequencies. They also cover first-step/catalog synchronization,
unsupported pause aliases and admitted execution disagreements. Public sequence
exhaustion remains directly tested at the publisher boundary; PlayerSession's
last-sequence reservation was reviewed but not injected end to end.

A baseline compiled from HEAD 0812bd4 reproduced a reasserted device fault after
a successful reset being cleared by reselection. The fix tracks the fault edge,
inhibits output and starts a fresh reset generation. The current-source focused
failure also exposed Play from Paused bypassing the pause-capability check;
it now follows Resume. Logs: `out/build/m6-ingress-baseline.log` and
`out/build/m6-ingress-before.log` (ignored). No golden was regenerated.
An earlier test incorrectly expected recovery when Reset ACK arrived while the
fault stayed asserted: the existing terminal ResetFailed behavior was correct,
and the assertion was corrected before isolating the actual reassertion bug.

No RTL, APF definition or hardware package changed. RTL simulation, synthesis
and hardware checks were not run; these host/link results do not establish
physical pause, output-inhibit latency or real ACK deadlines.

Slice 3's MDX sequencing progress is implemented on 2026-09-13. TrackLoop
increments checked u64 counters, the whole FM tick aggregates active tracks
after all channels succeed, and the engine stamps the result at that tick's
write timestamp. Waiting tracks remain active; short repeats do not count.
This remains read-ahead state, not audible loop completion or a public snapshot.
The bounded audible-commit adapter, policy/gain and automatic navigation remain
subsequent parts of slice 3 and the sound-port integration.

Executed MDX-progress evidence:

- `pwsh -File out/build/m6-progress-focused.ps1`: 29/29 PASS, 3.02 seconds;
  MDX, M3/M4/M5 host and architecture subset. Log:
  `out/build/m6-progress-focused-final.log` (ignored).
- `pwsh -File tools/host-verify.ps1`: 66/66 PASS, 231.14 seconds, including
  format, tidy, existing semantic/register traces and architecture/harness.
  Log: `out/build/m6-progress-host.log` (ignored).
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  42 PASS (`out/build/m6-progress-links.py`, ignored).
- Offline Docker GCC 13.3.0 ASan/UBSan: progress, track, document and engine
  suites PASS. Production disables exceptions/RTTI; tests disable RTTI.
  Script/log: `out/build/m6-progress-posix.py` and
  `out/build/m6-progress-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-progress-cross/Makefile progress-check`:
  final RISC-V link-only probe PASS, no undefined symbols. Text 13,912,
  data 184, BSS 892,432 bytes; conservative stack bound 9,024 bytes.
  Source/Makefile/budget: `out/build/m6-progress-cross`; log:
  `out/build/m6-progress-cross-final.log` (ignored). The probe was not run on
  Pocket and is not the integrated player's total memory budget.

Authored exact-byte cases verify unequal loop periods, same-tick end/removal,
indefinite wait, nested repeat/escape, all-ended state, inert P exclusion,
zero-write timestamps, u32/u64 boundaries and rollback after later channel,
routing and timeline failures. Expected counts and timestamps come from these
small byte sequences and the existing Timer B rational formula. No golden was
regenerated. The first build failed on two test references to logical_channel;
they were corrected to the existing DecodeResult field logical_track.

The first link probe exceeded the initialized-data limit because its direct
static declarations placed default-initialized engine workspaces in .data
(753,088 bytes). Matching the existing M5 probe's explicit construction in
zero-backed BSS storage fixed the probe, without changing production code or
relaxing the 4,096-byte data limit. Initial log:
`out/build/m6-progress-cross.log` (ignored).
No RTL/APF/package inputs changed; RTL simulation, synthesis and hardware
checks were not run. Existing host trace checks do not establish sound timing.

Slice 3's audio-boundary loop/gain model is implemented on 2026-09-13.
MediaLoopEnvelope consumes sealed progress intervals in a bounded FIFO and
applies the current repeat target before that boundary's progress/end decision.
It produces signed stereo samples with the specified 5-second fade and 20-ms
restoration, freezes source consumption during pause, and reports natural end,
RepeatOne restart intent, loop limit, Stop and failure separately. Raw future
progress survives policy changes; it is not a stale, uncancellable fade command.
The MDX integration test uses an authored host mapping with zero pipeline delay;
it does not infer the Pocket mapping. Public policy ingress/state, navigation
and the real audio adapter remain to be connected. Slice 3 is not complete.

Executed loop-envelope evidence:

- `pwsh -File out/build/m6-envelope-focused.ps1`: 10/10 PASS, 0.70 seconds,
  before the final shared-fault/revision edge assertions; the full gate includes
  them. Log: `out/build/m6-envelope-focused-fixed.log` (ignored).
- `pwsh -File tools/host-verify.ps1`: final 67/67 PASS, 241.15 seconds,
  including format, tidy, existing traces and architecture/harness checks.
  Log: `out/build/m6-envelope-host.log` (ignored).
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  44 PASS (`out/build/m6-envelope-links.py`, ignored).
- Offline Docker GCC 13.3.0 ASan/UBSan: media envelope and MDX progress suites
  PASS. Production disables exceptions/RTTI; tests disable RTTI. Script/log:
  `out/build/m6-envelope-posix.py`, `out/build/m6-envelope-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-envelope-cross/Makefile envelope-check`:
  RISC-V link-only probe PASS, no undefined symbols. Text 17,428, data 184,
  BSS 904,848 bytes; conservative stack bound 9,408 bytes. The probe explicitly
  constructs its workspaces in BSS using the existing M5 pattern. Source,
  Makefile and budget: `out/build/m6-envelope-cross`; log:
  `out/build/m6-envelope-cross.log` (ignored). It was not executed on Pocket
  and does not measure the integrated player or actual audio latency.

Authored sample/interval tests cover the exact 3,360,000/3,600,000 frame example,
signed rounding, finite Counted/RepeatOne, cancellation at the fade endpoint,
restoration and refade, pause/end/policy races, old generations, malformed
intervals, 256/257 FIFO retry, underrun and injected frame ceilings. The consumed
sample stream is identical with inserted pauses and extra snapshot reads.
Maximum generation/count/revision values are exercised; exhaustion of the
consecutive u64 interval sequence was reviewed, not driven through UINT64_MAX
submissions. No golden was regenerated or frame deadline moved to match code.

A focused new case first failed because a delayed old-generation Stop affected
the new stream. Generation-tagged control now returns StaleGeneration without
altering current playback; shared faults still take priority regardless of the
tag. Failure log: `out/build/m6-envelope-stale-before.log` (ignored). A subsequent
test build failed on signed/unsigned optional comparison; the literal was typed
unsigned, preserving the assertion and all warning settings.
No RTL/APF/package inputs changed. RTL simulation, synthesis and hardware tests
were not run. In particular, upstream FM-state retention and the physical
write-to-sample mapping are obligations of the later adapter/RTL slices.

Slice 3's repeat policy connection is implemented on 2026-09-13.
PlayerSession accepts capability-gated SetPlaybackPolicy commands and publishes
desired settings/revision separately from audio-applied settings/revision.
Start, Pause, Resume and SetPolicy capture their policy; acknowledgements must
echo it and current media must agree with the acknowledged generation/frame.
Changing policy preserves selection, position and transport intent. Finite
RepeatOne uses normal reset/preparation with a new generation, and a paused end
waits for resume. Settings survive Stop, catalog changes and failures in memory.
The existing Pocket backend does not declare the extension. Album/shuffle
navigation, durable settings, history and UI remain incomplete in slice 3.

Executed repeat-connection evidence:

- `pwsh -File tools/host-verify.ps1`: final 68/68 PASS, 295.43 seconds,
  including formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-policy-host-final.log` (ignored).
- `pwsh -File out/build/m6-policy-focused.ps1`: 12/12 PASS, 0.99 seconds after
  the capability-fault correction. Log: `out/build/m6-policy-focused-fixed.log`
  (ignored); the later full gate includes the final checked test observation.
- Offline Docker GCC 13.3.0 ASan/UBSan: six suites PASS (schema 2 command/state,
  ingress, transport, publisher and repeat-policy connection). Production
  disables exceptions/RTTI; tests disable RTTI. Script/log:
  `out/build/m6-policy-posix.py`, `out/build/m6-policy-posix-final.log` (ignored).
- Offline Docker `make --file out/build/m6-policy-cross/Makefile policy-check`:
  RISC-V link-only probe PASS with no undefined symbols. Text 35,980, data 188,
  BSS 1,800 bytes; conservative stack bound 37,744 bytes. Probe and budget:
  `out/build/m6-policy-cross`; log: `out/build/m6-policy-cross-final.log`
  (ignored). This is not an executed or integrated Pocket player budget.
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  62 PASS (`out/build/m6-policy-links.py`, ignored).

The scripted audio port runs the real envelope against independently authored
frame intervals. Tests cover captured/in-flight revisions, coalescing and no-op
settings, preparation/pause changes, exact 240,000-frame fade, restoration,
finite RepeatOne restart, held end, stale media/ACK, missing/malformed media,
unsupported capabilities, queue capacity, revision/count bounds, failures and
publication-frequency independence. No expected trace was regenerated.

A focused additional case first failed because a simultaneous catalog close
erased a newly detected loss of audio capability. Catalog synchronization now
preserves that failure with the same priority as a shared fault. The failing
log is `out/build/m6-policy-capability-before.log` (ignored).
The first full gate failed tidy on six redundant optional initializers,
seven unchecked optional accesses and one unreserved test vector. Explicit
absence defaults preserve GCC aggregate-source compatibility; checked access
and a known-capacity reservation address the other diagnostics. The first
Linux run also caught the old scripted port's missing new enum branch; that
port now fails explicitly if sent repeat control. No check was suppressed.
Initial logs: `out/build/m6-policy-host.log`, `out/build/m6-policy-posix.log`
(ignored).
No RTL/APF/package inputs changed. RTL simulation, synthesis and hardware
tests were not run; host traces and link success do not prove audible mapping,
actual FM-state freeze or hardware silence.

Slice 3's album/shuffle navigation is implemented on 2026-09-13. A Core-owned
index copies at most 300 logical IDs in display order. Next/Previous and
committed ends select through the existing reset/preparation machinery.
Shuffle pins the current track, uses injected bounded rejection sampling,
registers successful current-generation Starts, preserves unstarted cancelled
candidates, and distinguishes manual history replay from automatic selection.
It stops after one cycle. RepeatOne restart preserves the cycle; explicit
neighbours override repeat. Desired/applied repeat state remains independent.

The navigation profile requires an injected random port and repeat capability.
Existing constructors/backends retain their prior behavior. Public observations
copy neighbour availability and cycle identity/counts, without exposing the
mutable order or requiring UI/render activity. Performance history, durable
settings, mock UI/input and the real Pocket adapters remain in M6.

Executed navigation evidence:

- `pwsh -File tools/host-verify.ps1`: 69/69 PASS, 324.03 seconds, including
  formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-navigation-host.log` (ignored).
- `pwsh -File out/build/m6-navigation-focused.ps1`: 13/13 PASS, 0.71 seconds.
  Log: `out/build/m6-navigation-focused-final.log` (ignored).
- Offline Docker GCC 13.3.0 ASan/UBSan: seven suites PASS (schema 2 command/state,
  ingress, transport, publisher, repeat/navigation integration and navigation
  order/bounds). Script/log: `out/build/m6-navigation-posix.py`,
  `out/build/m6-navigation-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-navigation-cross/Makefile navigation-check`:
  RISC-V link-only probe PASS, no undefined symbols. Text 43,220, data 188,
  BSS 1,800 bytes; conservative stack bound 63,216 bytes. Probe and budget:
  `out/build/m6-navigation-cross`; log: `out/build/m6-navigation-cross.log`
  (ignored). This is not an executed or integrated Pocket player budget.
- `python -B tools/check_harness.py`: PASS; changed Markdown local file links:
  72 PASS (`out/build/m6-navigation-links.py`, ignored).

The four-track permutation is authored from the tail swaps (draws 0 for bound 3,
then 1 for bound 2), independently of planner output. Generated 1/300-track
metadata verifies cardinality/uniqueness, and 301/duplicate catalogs preserve
the previous index on rejection. Tests cover the 31-reject/32nd-accept and
32-reject limits, missing random input, checked cycle exhaustion, album edges,
paused/manual selection, cancellation before Start, preparation failure, live
order changes, finite RepeatOne, Stop/restart, and identical control traces
with dense versus skipped publication. No golden was regenerated.
No RTL/APF/package inputs changed. RTL simulation, synthesis and hardware
tests were not run; actual audio timing, source freeze and production resource
headroom remain integration gates.

Slice 3's committed performance collector/publication profile is implemented
on 2026-09-13. The [adopted profile](../../specs/performance-history-v2.md)
adds an optional copied 256-event window with independent current FM channels.
An injected Core read port supplies committed values; old/missing or malformed
observations become Waiting or Invalid without a transport error. Capture loss,
retention loss and exhausted display counters remain visible. Current channels
continue updating after event-number exhaustion. UI reads and publication cadence
do not drive collection. Engine note/voice extraction, actual audio commit
mapping, Tracker/keyboard projection and settings/input remain incomplete.

Executed performance-collector evidence:

- `pwsh -File tools/host-verify.ps1`: final 70/70 PASS, 345.84 seconds,
  including formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-history-host-final.log` (ignored).
- `pwsh -File out/build/m6-history-focused.ps1`: 14/14 PASS, 1.07 seconds.
  Log: `out/build/m6-history-focused-final.log` (ignored). This precedes the
  additional public profile checks and scalar publisher metadata accessors;
  the completion gate covers those final changes.
- Offline Docker GCC 13.3.0 ASan/UBSan: eight suites PASS (performance history,
  schema 2 command/state, ingress, transport, publisher, repeat/navigation and
  navigation order). Script/log: `out/build/m6-history-posix.py`,
  `out/build/m6-history-posix-final.log` (ignored). Production disables exceptions/RTTI;
  tests disable RTTI. No private music fixture was used.
- `out/build/host-msvc/rpcmp_performance_history_tests.exe`: PASS, event 32,
  snapshot 8,344, collector 8,360 bytes. GCC host reports the same sizes.
  Compile-time checks bound each event, four observation windows and the
  collector; the integrated UI/audio image is not yet measured.
- Offline Docker `make --file out/build/m6-history-cross/Makefile history-check`:
  RISC-V link-only PASS with no undefined symbols. Text 46,332, data 188,
  BSS 1,800 bytes; conservative stack bound 113,472 bytes. Probe/budget:
  `out/build/m6-history-cross`; log: `out/build/m6-history-cross-final.log`
  (ignored). The executable was not run and is not an integrated player image.
- `python -B tools/check_harness.py`: PASS; `python -B out/build/m6-history-links.py`:
  44 changed-Markdown local file links PASS.

Authored events verify same-frame retrigger/short On/Off, future-frame rejection,
stale generations, independent current channels, 256/257/300 retention, known
and unknown capture loss, invalid lengths/IDs/order/pitch/gate, sequence overflow,
Pause/Stop, copied-value isolation and equal scripted audio control traces with
dense, sparse or absent publication. No trace or expected value was regenerated.
The initial host build caught C++20-only syntax in this C++17 repository, then
implicit integer narrowing in optional test values. Both were corrected without
changing language/toolchain settings or suppressing warnings. Initial logs:
`out/build/m6-history-focused.log`, `out/build/m6-history-focused-fixed.log`
(ignored). The first cross probe referenced a misspelled source path in its
generated Makefile; the corrected probe passed without production changes.
The initial completion gate passed 69/70 checks and failed tidy on two size
budget expression types and one test loop index type (348.48 seconds;
`out/build/m6-history-host.log`, ignored). Explicit `size_t` arithmetic and a
size-matched test index resolved all three diagnostics; focused analysis of
the changed sources passed (`out/build/m6-history-tidy-fixed.log`, ignored).
No RTL/APF/package inputs changed. RTL simulation, synthesis and hardware
checks were not run. Host commit boundaries are supplied by scripted producers;
these results do not establish an actual audible commit or correct MDX pitch.

Slice 3's MDX performance extraction and Core mapping are implemented on
2026-09-13. The [adopted profile](../../specs/mdx-performance-v1.md) follows
emitted writes with a separate transactional register shadow, including raw
cross-channel writes. It captures actual operator-mask edges, base KC/KF pitch
and applied voice identity. Unknown CSM gates and noise pitch remain unknown;
bounded observation loss never changes device writes or stops sequencing.

The Core mapper takes an explicit sound clock and a correlated source-tick/
audio-frame boundary. Optional per-event frames preserve serialized write
timing within a tick; publication waits for the full channel checkpoint.
The real audio adapter still owes audible mapping, pending observation storage
and exactly-once consumption. This does not complete M6 slice 3 or Pocket UI.

Executed MDX-observation evidence:

- `pwsh -File tools/host-verify.ps1`: final 71/71 PASS, 359.70 seconds, including
  formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-mdx-performance-host-complete.log` (ignored).
- `pwsh -File out/build/m6-mdx-performance-focused.ps1`: 33/33 PASS,
  1.97 seconds after the per-event frame correction. Log:
  `out/build/m6-mdx-performance-frames-fixed.log` (ignored).
- Offline Docker `python3 -B /repo/out/build/m6-mdx-performance-posix.py`:
  GCC 13.3.0 ASan/UBSan, eight suites PASS (MDX observations, router, engine,
  timeline, progress, performance collector, PlayerSession and renderer
  independence). Log: `out/build/m6-mdx-performance-posix-final.log` (ignored).
  Production disables exceptions/RTTI; tests disable RTTI.
- Offline Docker `make --file out/build/m6-mdx-performance-cross/Makefile
  mdx-performance-check`: RISC-V link-only PASS, no undefined symbols.
  Text 58,952, data 188, BSS 904,176 bytes; conservative stack bound 129,072,
  largest frame 60,352 bytes. Probe/budget: `out/build/m6-mdx-performance-cross`;
  log: `out/build/m6-mdx-performance-cross-final.log` (ignored).
  This executable was not run and is not an integrated Pocket player image.
- Offline Docker `make --file spikes/pocket/openfpgaos/desktop.mk
  OUT_DIR=/repo/out/build/m6-mdx-performance-legacy-desktop m5-desktop`: PASS,
  33 writes and unchanged digest `f1f04f5a8695a112`. Log:
  `out/build/m6-mdx-performance-build-fixed.log` (ignored).
- Offline Docker `make --file spikes/pocket/openfpgaos/Makefile -j2`
  with `verify m5-budget m5-audio-budget m5-file-budget`: all four existing
  Pocket probes cross-link with no undefined symbols. Fresh output overrides
  `OUT_DIR`, `M5_OUT_DIR`, `M5_AUDIO_OUT_DIR`, `M5_FILE_OUT_DIR` respectively use
  `/repo/out/build/m6-mdx-performance-legacy-{base,session,audio,file}`.
  Session/audio/file budgets pass: text 58,388/61,716/69,892, data 964/968/228,
  BSS 910,852/911,360/3,008,576 and conservative stack 24,304/24,624/25,056 bytes.
  Log: `out/build/m6-mdx-performance-legacy-cross.log` (ignored).
  These builds were not deployed and do not reopen the closed M5 investigation.
- `python -B tools/check_harness.py`: PASS;
  `python -B out/build/m6-mdx-performance-links.py`: 74 local file links PASS.

Authored/generated inputs verify retrigger, tie, delay, portamento, zero operator
mask, raw key/pitch/voice updates, noise/CSM, 300-event prefix loss, malformed
bounds and transactional rollback after routing/timestamp failures. Independent
nominal KC anchors and integer clock correction check displayed base pitch.
An engine-to-public-snapshot case checks generation ownership and future deferral.
No music corpus, proprietary data or regenerated golden is added.

A focused case with event frames 100, 103 and 110 first failed two assertions
because the initial mapper flattened them to checkpoint 110. After correction,
individual times survive deferred publication; reversed frames, count mismatch
and different times for a shared write index are rejected. Logs:
`out/build/m6-mdx-performance-frames-before.log` and
`out/build/m6-mdx-performance-frames-fixed.log` (ignored).
Initial focused static analysis found a Boolean simplification, an out-of-range
enum cast in a malformed-input test and a widening multiplication. These were
corrected without suppressions; focused analysis and the final full gate pass.
Final build review found the existing cross/desktop Makefile source lists still
omitted the new observer implementation. The desktop target reproduced the
undefined-symbol failure (`out/build/m6-mdx-performance-build-before.log`,
ignored); both Makefiles now include it in their base and library-session lists.
The successful legacy checks above cover the corrected builds.

RTL/APF definitions are unchanged. RTL simulation, synthesis, packaging and
hardware checks were not run. The engine used by Pocket probes has changed;
host/link evidence does not establish accurate Pocket pitch, audible commit,
hardware gate state or final player resource headroom.

Slice 3's persistent-settings controller and public connection are implemented
on 2026-09-13. The [settings profile](../../specs/playback-settings-v2.md)
adopts fixed 64-byte records, two-slot validation/selection and an injected
asynchronous port. PlayerSession gates startup commands, restores only the
initial desired policy, observes subsequent revisions before starting I/O and
publishes copied restore/save results. There is no automatic playback or
automatic overwrite of failed/unknown startup records.

Writes capture their record/revision. A later policy change remains pending;
old completion cannot mark it Saved. Failures preserve current policy/transport,
and retry first confirms quiescence and rereads both slots to determine the
next sequence. Unknown formats and exhausted counters disable saving without
changing the playback error. Policy revision exhaustion retains its already
adopted terminal behavior. The actual APF storage adapter remains slice 4.

Executed settings evidence:

- `pwsh -File tools/host-verify.ps1`: 72/72 PASS, 369.25 seconds, including
  formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-settings-host.log` (ignored).
- `pwsh -File out/build/m6-settings-focused.ps1`: final 15/15 PASS, 0.69 seconds.
  Log: `out/build/m6-settings-focused-final.log` (ignored).
- Focused clang-tidy on the six changed implementation/test files: PASS.
  Log: `out/build/m6-settings-tidy.log` (ignored); no suppression was added.
- Offline Docker `python3 -B /repo/out/build/m6-settings-posix.py`: GCC 13.3.0
  ASan/UBSan, eight suites PASS (settings, schema 2 state/command, PlayerSession,
  policy, transport, publisher and performance collector). Production disables
  exceptions/RTTI; tests disable RTTI. Log: `out/build/m6-settings-posix.log`
  (ignored).
- Offline Docker `make --file out/build/m6-settings-cross/Makefile settings-check`:
  RISC-V link-only PASS, no undefined symbols. The combined MDX/performance/
  PlayerSession probe injects a scripted storage port. Text 64,520, data 188,
  BSS 904,176 bytes; conservative stack bound 130,832 and largest frame 60,864
  bytes. Probe/budget: `out/build/m6-settings-cross`; log:
  `out/build/m6-settings-cross.log` (ignored). This is not an executed or
  integrated Pocket player image/budget.
- `python -B tools/check_harness.py`: PASS;
  `python -B out/build/m6-settings-links.py`: 81 local file links PASS.

The exact record fixture was independently packed with Python struct/zlib.
Tests cover valid and malformed lengths/CRC/fields, conflicting copies,
unknown-version protection, all 65 overwrite-prefix boundaries, initial read
failure/no autoplay, captured versus latest revisions, 10,001 changes while
one write is active, durability/readback failure, rescan after uncertain writes,
wrong/stale completions, deadline priority, quiescence timeout and counter/time
bounds. A paused PlayerSession preserves position/policy on storage failure;
dense and absent publication produce identical scripted audio-control order.
These are generated metadata/control data, not private music or hardware trials.

A focused race first failed one assertion: a policy command arriving exactly
when the old debounce expired started a write of the old revision. PlayerSession
now supplies the just-applied policy before stepping storage. The new revision
restarts the debounce and is the next captured write. Failing/corrected logs:
`out/build/m6-settings-deadline-before.log` (14/15, 0.73 seconds) and
`out/build/m6-settings-deadline-fixed.log` (15/15, 0.96 seconds), both ignored.
No expected value or golden was regenerated from implementation output.

RTL/APF definitions are unchanged. RTL simulation, synthesis, packaging and
hardware tests were not run. Real flush durability, timeout bounds, power-cycle
recovery and complete UI/resource/timing acceptance remain M6 integration work.

Slice 3's UI controller/input and Tracker row projection are implemented on
2026-09-13. [Album UI v1](../../specs/player-ui-v1.md) adopts replaceable button
and focus tables, catalog pages, immutable snapshot consumption, selection/stop
guards and an eight-action policy queue. Core now publishes the last consumed
policy command's Applied/Failed result, including same-value commands and
interrupted batches, independently of audio ACK and settings durability.

The UI links only contracts. Tests use authored snapshots and a generated
catalog, without runtime, player or library implementations. Browse metadata
does not replace current-track metadata, accepted selection blocks a pause
against the old snapshot, and held B cannot navigate after a stop completes.
Stop remains idempotent: already-Stopped observations confirm that generation
instead of awaiting an impossible new one. A settled Ended state retires an
obsolete pause/play wait. Tracker projection preserves same-frame retriggers,
distinguishes retention from missed/capture events, and keeps current keyboard
channels independent of the retained rows.

Executed UI evidence:

- `pwsh -File tools/host-verify.ps1`: final 73/73 PASS, 396.04 seconds, including
  formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-ui-host-final.log` (ignored).
- `pwsh -File out/build/m6-ui-focused.ps1`: final 17/17 PASS, 0.91 seconds, including
  schema/transport/policy/settings regressions, both UI suites and architecture
  positive/negative checks. Log: `out/build/m6-ui-focused-final.log` (ignored).
- Offline Docker `python3 -B /repo/out/build/m6-ui-posix.py`: GCC 13.3.0
  ASan/UBSan, the contracts-only album UI suite PASS. Production disables
  exceptions/RTTI; tests disable RTTI. Log: `out/build/m6-ui-posix-final.log` (ignored).
- Offline Docker `python3 -B /repo/out/build/m6-settings-posix.py`: eight affected
  Core/contract suites PASS with GCC ASan/UBSan. This reruns settings, state,
  ingress, session, policy, transport, publisher and performance checks after
  adding execution receipts. Log: `out/build/m6-ui-core-posix.log` (ignored).
- Offline Docker `make --file out/build/m6-ui-cross/Makefile ui-check`: RISC-V
  link-only PASS, no undefined symbols. Combined MDX/performance/settings/session
  and UI probe: text 72,768, data 188, BSS 904,176 bytes; conservative stack bound
  160,768 and largest frame 78,928 bytes. Budget/log: `out/build/m6-ui-cross`
  and `out/build/m6-ui-cross-final.log` (ignored). This is not an executed player image
  or the complete Pocket memory budget.

The new feedback test initially failed in `player_policy` (14/15, 1.02 seconds,
`out/build/m6-ui-feedback-before.log`). Connecting execution receipts made the
same focused run pass (15/15, 1.14 seconds, `out/build/m6-ui-feedback-focused.log`).
Additional authored cases cover input edge priority/repeat/rebinding, full
300-album return lookup, stale/malformed pages, eight/nine queued settings,
rejection/failure/no-ACK duplicates, invalid observations and counter limits.
No golden was generated from implementation output.

Review found a UI transaction bug: a newer snapshot with regressed history
sequence could replace the last good view and consume a policy result before
being rejected; rereading it could re-enable input. Three authored assertions
failed (`out/build/m6-ui-regression-before.log`, 16/17 PASS, 0.85 seconds).
Continuity is now checked before copying any state or consuming acknowledgements.
The final focused and sanitizer runs above include this case. Focused clang-tidy
passed after checked optional access and byte-authored invalid tags in tests;
logs: `out/build/m6-ui-tidy-fixed.log` and `out/build/m6-ui-tidy-final.log` (ignored).
No suppression was added. `python -B tools/check_harness.py` passed;
`python -B out/build/m6-ui-links.py` checked 66 local file links.

Canvas rendering, reviewable host images, Pocket fonts/input/framebuffers and
the real adapters are still pending. RTL/APF inputs are unchanged; RTL simulation,
synthesis, packaging and hardware tests were not run for this controller unit.

Slice 3's canvas renderer and host SVG mock are implemented on 2026-09-13.
Tracker, eight-channel keyboard and album/track pages share the fixed metadata,
transport/policy controls and save-status panel. Rendering consumes a checked
UI view synchronously, without commands, Core calls or heap allocation. The
host executable links UI/contracts and its SVG port only. Reproduction commands
and the seven authored scenarios are in the [UI README](../../core/ui/README.md).

The keyboard uses independent current channels, distinguishes OFF/unknown gate
and labels paused held state. Browsing never replaces the current track below.
Long or source-truncated metadata keeps a visible ellipsis; unknown performance,
capture gaps, save failures and playback errors remain distinct. No music data,
new font dependency or generated image is committed.

Executed renderer evidence:

- `pwsh -File tools/host-verify.ps1`: 75/75 PASS, 427.36 seconds, including
  formatting, clang-tidy and positive/negative architecture checks.
  Log: `out/build/m6-render-host.log` (ignored).
- `pwsh -File out/build/m6-ui-focused.ps1`: final 18/18 PASS, 1.45 seconds,
  including UI, Core/contract regressions, SVG scenarios and architecture checks.
  Log: `out/build/m6-render-focused-final.log` (ignored).
- `ctest --preset host-msvc -R 'player_render|player_ui_svg'`: final 2/2 PASS,
  0.73 seconds. Log: `out/build/m6-render-tests-final.log` (ignored).
- Offline Docker `python3 -B /repo/out/build/m6-render-posix.py`: GCC 13.3.0
  ASan/UBSan, UI controller and renderer suites plus all 21 SVG view/scenario
  combinations PASS. Production disables exceptions/RTTI; tests disable RTTI.
  Log: `out/build/m6-render-posix-final.log` (ignored).
- Offline Docker `make --file out/build/m6-render-cross/Makefile render-check`:
  RISC-V link-only PASS, no undefined symbols. The combined MDX/performance/
  settings/session/UI probe uses a counting canvas, without a framebuffer/font
  or real sound port. Text 87,028, data 188, BSS 904,176 bytes; conservative
  stack bound 162,176 and largest frame 78,928 bytes. Budget/log:
  `out/build/m6-render-cross/budget.json`, `out/build/m6-render-cross.log`
  (ignored). The executable was not run; this is not the complete player budget.
- `pwsh -File out/build/m6-render-preview.ps1`: generated six SVG/PNG pairs in
  `out/ui-m6-render` (ignored) using an isolated headless Edge profile. Visual
  inspection at 640x480 covered all three playing views, paused keyboard, long
  Japanese title and track-load error. The corrected playing/paused keyboard
  images were reinspected after changing silent-channel labels to OFF.
- `python -B tools/check_harness.py`: PASS;
  `python -B out/build/m6-render-links.py`: 45 local file links PASS.

The tests use authored notes, metadata and draw positions, not renderer-generated
goldens. They check independent current pitch versus retained history, common
panel placement, clipping, UTF-8 boundaries, XML parsing/escaping and repeatable
output. The maximal elapsed-time expectation was independently derived by
integer division of UINT64_MAX by 48,000. Review found that valid UTF-8 containing
U+FFFE/U+FFFF could produce invalid XML. The added case first failed one assertion
(`out/build/m6-render-xml-before.log`); the SVG adapter now emits replacement
glyphs for those XML-excluded characters, and the final checks include that case.
Initial focused tidy diagnostics were corrected without adding suppressions.

These images use installed host fonts and provisional metrics; they establish
mock layout, not Pocket Japanese readability. No RTL/APF/package inputs changed;
RTL simulation, synthesis, packaging and hardware checks were not run for this
unit. Next is slice 4 sound/storage feasibility and the real audible-commit,
pending-observation and asynchronous persistence adapters, followed by Pocket
font/input/framebuffer integration. M6 hardware acceptance remains pending.

Slice 4's native JT51 hold feasibility fixture is implemented on 2026-09-13.
The [experiment profile](../../specs/jt51-hold-experiment-v1.md) retains the clean
pinned checkout and generates a separate module namespace under ignored `out/`.
Native enables freeze their existing consumers; four enable-independent
processes get explicit hold with reset priority. The original shift-register
process shapes preserve RAM inference. Source/output/recipe hashes, process
counts and the upstream license are recorded with generated source. No GPL HDL,
bitstream or new third-party dependency is committed.

The comparison oracle is the unmodified JT51 receiving only retained media clock
edges in the testbench. The candidate always receives the fabric clock. Native
stereo, extended samples, sample strobe, status/IRQ/control outputs and the
selected-address register are compared, without generating a new golden.
This is not an I2S-output or complete-pause proof. Resampler/pending ownership,
zero-frame insertion, gain/commit, queue/CDC/ACK and fit remain to be connected.

Executed native-hold evidence:

- `pwsh -File tools/rtl-jt51-hold-verify.ps1 -CenOnly`: expected negative control
  reproduced. The bench fails with `MMR changed while held` at media cycle 2049,
  hold=1, before the first address reaches a retained oracle edge. The script
  requires this exact failure and the one-error/zero-warning summary, never a
  positive PASS marker. Log: `out/build/m6-hold-negative-ce.log` (ignored).
- `pwsh -File tools/rtl-jt51-hold-verify.ps1`: PASS against real pinned JT51,
  zero simulator errors/warnings. 745 native stereo sample pairs, including
  273 nonzero left and 505 nonzero right, match; 260 register writes and 855
  automatic pauses cover all 256 media sub-tick phases. There are 8,563 held
  cycles, including the explicit initial-address hold; 107 coincide with an
  address pulse, 40 with a data pulse and 2,224 with busy. Noise, four LFO
  waveforms, timers, retriggers and reset while held are exercised. Log:
  `out/build/m6-hold-positive-ce.log` (ignored). These are simulation counts,
  not physical Pocket samples or elapsed playback time.
- `pwsh -File tools/rtl-verify.ps1`, followed by
  `pwsh -File tools/rtl-jt51-verify.ps1`: 9 boundary/model and 5 real-JT51 legacy
  benches PASS, each with zero simulator errors/warnings. Logs:
  `out/build/m6-hold-rtl.log`, `out/build/m6-hold-legacy-jt51.log` (ignored).
- `python -B tests/rtl/jt51_hold_prepare_test.py`: 5/5 host guards PASS.
  Offline Docker `python3 -B /repo/tests/rtl/jt51_hold_prepare_test.py`: the same
  5/5 PASS on Linux; log: `out/build/m6-hold-posix-final.log` (ignored).
- `pwsh -File tools/host-verify.ps1`: final host gate 76/76 PASS in 424.25 s,
  including format, tidy and architecture rejection fixtures. Log:
  `out/build/m6-hold-host-final.log` (ignored).
- `python -B tools/check_harness.py`: PASS;
  `python -B out/build/m6-hold-links.py`: 50 local file links PASS.

Host generation guards use authored metadata and mocked Git responses, without
vendor HDL. They reject an output outside `out/`, existing output, a wrong
revision and dirty pinned input before publication, and check that comments
cannot create clock ports. Real source transformation is covered by the RTL
comparison rather than expected text copied from the generator.

The first single-engine synthesis rejected the initial outer guard around an
asynchronous reset (Quartus error 10200 in the timer flag). Keeping the reset
branch first resolved it. A first synthesizable version then expanded the
shift-register storage into 3,999 registers and zero block-memory bits.
The final recipe gates existing native enables and only four independent
processes, retaining RAM inference. The same native comparison still passes.
These failures prompted the implementation changes; no expected trace or
warning suppression was altered. Logs: `out/build/m6-hold-synthesis.log` and
`out/build/m6-hold-synthesis-fixed.log` (ignored).

`python -B out/build/m6-hold-synth.py`, then Quartus 25.1std.0 Build 1129
`quartus_map.exe out/research/m6-jt51-hold-synth-ce/jt51_hold_probe
--read_settings_files=on --write_settings_files=off`: final single-engine
Analysis & Synthesis PASS for Cyclone V 5CEBA4F23C8, zero errors and 17 warnings.
The report gives 1,091 registers, 2,974 block-memory bits and one DSP; it does
not report placed ALMs or timing. The warnings are the same categories as the
existing unmodified-engine report: nine unused write-side nets in initialized
ROMs, constant dout[6:2], and the upstream unused cen input. No suppression is
added. Final log: `out/build/m6-hold-synthesis-ce.log`; project and source/hash
manifest: `out/research/m6-jt51-hold-synth-ce` (ignored).

The default legacy audio path and APF/package inputs are unchanged. Full fitting,
timing/CDC, cross-build, packaging and hardware checks were not run for this fixture;
there is no new player image or advertised pause capability. Next connect this
hold to the output boundary and prove that removing inserted stereo-zero frames
recovers the unpaused sample stream, including in-flight writes and reset.

Slice 4's synchronous pause/output boundary is implemented on 2026-09-13.
The new JT51 owner retains its clock-enable accumulator and in-flight write;
the converter retains rate phase and pending stereo samples. Pause takes effect
at a stereo boundary, replacing only complete output frames with zero. Resume
consumes the retained edge once. Urgent stream reset clears sound and a retained
write while the serializer keeps running. The local reset warmup is 2,048 audio
edges; a minimal reset interval and CPU/CDC/ACK deadline have not been inferred.

The new serial output includes the official one-bit delay after LRCK. The old
v1 serializer's phase is preserved. No CPU/queue mapping, app, SDK, APF JSON,
package or new third-party source is changed. The architecture allowlist adds
only the two adopted M6 modules; dependency rejection rules remain intact.

Executed pause/output evidence:

- `pwsh -File tools/rtl-media-audio-verify.ps1`: both benches PASS with zero
  simulator errors/warnings. The final log is `out/build/m6-media-final.log`.
  The converter comparison uses the unmodified v1 converter at retained test
  clock edges and checks every new serial bit, including delay/padding. It
  covers all 256 request phases, seven phases of a 7:3 test ratio, 354
  source/boundary coincidences, occupied/empty holds and sticky diagnostics.
  It observes 1,298 compared wall frames; the real-JT51 bench is separate.
- The real-JT51 bench runs independently timestamped writes through two new
  owners, one uninterrupted and one paused. It decodes the external serial
  pins and compares 1,484 stereo frames after removing pause frames, including
  1,130 nonzero left and 1,238 nonzero right frames. All 512 authored operations
  are accepted. Held boundaries include nine address, five data, 328 busy,
  eight native-sample and 772 pending-sample observations. Sixteen coarse rate
  phase buckets include the known initial zero phase; this is not exhaustive
  coverage of every full-rate accumulator value. A final retained write is
  cancelled by reset, followed by continued hold and silent resumed output.
  Native-engine equivalence to unmodified JT51 remains the prior fixture's
  separate evidence, not a claim that this bench uses a different engine oracle.
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: 9 boundary/model and 5 real-JT51 legacy
  benches PASS, each with zero simulator errors/warnings. Logs:
  `out/build/m6-media-rtl.log`, `out/build/m6-media-legacy-jt51.log`.
- `python -B out/build/m6-media-synth.py`, then Quartus 25.1std.0 Build 1129
  `quartus_map.exe out/research/m6-media-audio-synth-final/media_audio
  --read_settings_files=on --write_settings_files=off`: single-owner Analysis &
  Synthesis PASS, zero errors and 11 reviewed warnings. The new clock-accumulator
  narrowing warning was corrected by an explicit width with an invariant bound;
  the final simulation includes that change. The remaining warnings are nine
  upstream ROM write-side nets and two messages for constant `audio_clipped`
  (the JT51 input is already signed 16-bit). No suppression was added.
  The report has 1,293 registers, 2,974 block-memory bits and one DSP; no placed
  ALM/timing claim is made. Final log: `out/build/m6-media-synthesis-final.log`;
  project and owned-source hashes: `out/research/m6-media-audio-synth-final`.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 422.27 seconds, including
  format, tidy and architecture rejection fixtures. Log:
  `out/build/m6-media-host.log`. `python -B tools/check_harness.py` and
  `python -B out/build/m6-media-links.py`: PASS, with 55 local file links.

Initial test runs matched the compared samples but failed coverage assertions:
the synthetic source lacked empty gaps, and the real trace lacked the initial
zero-rate hold. Added stimuli cover these cases and a held sample pulse without
changing the oracle or weakening the assertions. Initial logs are
`out/build/m6-media-output-first.log` and `out/build/m6-media-integrated-first.log`.
All logs, generated GPL derivatives and synthesis projects above are ignored.

This proves the exercised local pause/output behavior, not complete player
acceptance. Full fitting/timing, CDC, cross-build, packaging and hardware were
not run for this unit. Next are cancellable gain reservations and audible
commit, followed by CPU control, media queue/CDC and asynchronous storage.

Slice 4's mapped-progress and gain RTL controller is implemented on 2026-09-13.
`rpcmp_media_envelope` owns a 256-entry synchronous RAM queue, validates epochs,
sequences and policy revisions, and evaluates the current policy before due
progress or completion. A quotient/remainder recurrence implements the adopted
240,000-frame fade and 960-frame restore exactly. The inputs must already have
been mapped to audible frames; no CPU-to-audio latency or identity mapping is
assumed. The controller is not yet connected to the native/I2S output.

Executed envelope evidence:

- `pwsh -File tools/rtl-media-envelope-verify.ps1`: PASS, zero simulator errors
  or warnings. The final reviewed bench checks 4,243,649 boundaries across 15
  scenario streams, including exact fade start at 3,360,000 and completion at
  3,600,000, cancellation on the reserved-loop and zero-gain boundaries,
  partial-gain restart, one-unit restoration, paused policy changes and signed
  rounding. These are accelerated logical frame ticks, not Pocket elapsed time.
  FIFO tests cover 256/257 capacity, retry, full/pop priority and 300 consecutive
  one-entry pop/push collisions across pointer wrap. Epoch/policy errors,
  natural end, fault priority and a three-frame counter ceiling are checked.
  Sequence exhaustion injects a UINT64_MAX register state rather than claiming
  to execute that many admissions. Log: `out/build/m6-envelope-final-reviewed.log`.
- The interpolation oracle uses the analytical product/division from the
  adopted contract, independently of the RTL recurrence. Signed half-gain also
  has literal +1/-1 expectations for +3/-3 inputs. The initial bench read the
  combinational control result after its edge; sampling it before that edge
  corrected the observation, without changing the Protocol failure expectation.
  Initial log: `out/build/m6-envelope-first.log`.
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`, then
  `pwsh -File tools/rtl-media-audio-verify.ps1`: 9 boundary/model, 5 real-JT51
  legacy and 2 pause/output benches PASS, with zero simulator errors/warnings.
  Logs: `out/build/m6-envelope-rtl.log`, `out/build/m6-envelope-legacy-jt51.log`,
  `out/build/m6-envelope-media-audio.log`.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 422.05 seconds, including
  format, tidy and architecture rejection fixtures. Log:
  `out/build/m6-envelope-host.log`. `python -B tools/check_harness.py` and
  `python -B out/build/m6-envelope-links.py`: PASS, with 56 local file links.

The first registered timing probe fitted but failed timing: worst setup
-2.731 ns and removal -0.274 ns. Path inspection found generation/control
validation ahead of the stereo multiplication/division. Every consuming frame
uses its already-registered gain, including the first frame of a newly anchored
ramp. Evaluating scaling from that register moves validation to the final
zero/output selection without changing the arithmetic. The removal failure was
the probe's raw input reset reaching asynchronous clears at the active edge;
the final probe supplies reset release through a falling-edge register, meeting
the controller's synchronous-release precondition. This is a fixture reset
source, not the eventual APF reset/CDC adapter. Initial reports and paths:
`out/research/m6-envelope-timing-probe`, `out/build/m6-envelope-setup-paths.rpt`,
`out/build/m6-envelope-removal-paths.rpt`.

`python -B out/build/m6-envelope-timing-prepare.py`, followed by Quartus
25.1std.0 Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe`
on `out/research/m6-envelope-timing-final/envelope_probe`: final registered-probe
fit and multicorner timing PASS for 5CEBA4F23C8, seed 1. The 81.380 ns constraint
is conservatively rounded down from the 12.288 MHz period. No false paths,
multicycle paths or warning suppressions were added. All four models have
nonnegative setup/hold/recovery/removal/pulse-width slack, with worst setup
12.615 ns and hold 0.094 ns; setup and hold are fully constrained in this probe.
STA reports zero errors/warnings. Fitting has zero errors and three reviewed
warnings: unavailable LogicLock and two messages about the five automatically
assigned probe pins. Those pins are not a Pocket pinout or I/O sign-off.
The probe, including input/output shift registers, uses 2,446 ALMs, 2,099
registers, 49,408 memory bits in five RAM blocks, and two DSPs. Source/wrapper
hashes and reports are in that project; logs are
`out/build/m6-envelope-timing-final-{map,fit,sta}.log`.

All generated logs/probes remain ignored. The fit is for this registered
controller fixture, not the complete player or an APF image. Cross-build,
integrated placement/CDC/board timing, packaging and hardware checks were not
run for this unit. Next connect the envelope to the retained native/output
path and prove audible commit before adopting the real sound-control port.

Slice 4's local native/envelope/output integration is implemented on 2026-09-13.
`rpcmp_jt51_enveloped_audio` gives the envelope the old pending resampled sample,
loads scaled stereo on the same boundary that increments media position, and
uses that consuming decision to retain native, write and converter state.
`rpcmp_media_output` and `rpcmp_jt51_media_source` share the existing internals
with the old pause wrappers; their public behavior remains unchanged.

The integrated owner holds physical reset for at least 2,048 edges after each
terminal state or reset/fault trigger. Begin requires completed reset and no
active generation. Stop/end position and failure reasons survive physical
reset; a newer Begin is required to reopen playback. Explicit reset during
active playback is a failure, preventing resume of an old position against
cleared native state. Serial clocks remain continuous.

Executed integration evidence:

- `pwsh -File tools/rtl-media-audio-verify.ps1`: both existing pause/output
  benches PASS after sharing their internals. The converter still matches v1;
  real-JT51 comparison remains 1,484 stereo frames, 512 writes and all 16 rate
  buckets, with zero simulator errors/warnings. Only private test hierarchy
  paths changed; no behavioral expectations were regenerated. Log:
  `out/build/m6-envelope-output-shared.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS, zero simulator
  errors/warnings. External I2S decoding matches 3,200 consumed frames against
  uninterrupted native output multiplied by an analytical policy timeline.
  Of these, 2,758 differ from the unscaled samples. Left/right positive counts
  are 1,597/1,554 and negative counts 1,545/1,532. All 56 authored two-tone
  writes complete; 37 nonconsuming frames are silent. The trace exercises a
  paused policy change, full 960-frame restoration, partial restoration
  interrupted by a new fade, reached-to-reached policy, stale-generation Stop
  and another paused fade. This reference shares the native implementation;
  equivalence to unmodified JT51 remains the separate earlier hold-fixture test.
  Log: `out/build/m6-enveloped-final.log`.
- Nine terminal/reset scenarios cover Stop, finite end, sealed-coverage failure,
  explicit reset during hold, simultaneous fault/Stop, injected converter
  underflow/overflow, finite RepeatOne intent and a future-generation protocol
  error. Each checks physical reset duration and quiescence. A subsequent
  silent generation verifies old tone/pending samples cannot return. Converter
  fault injection tests wiring from the sticky registers, not the physical
  cause of starvation/overflow; those causes remain in the converter suite.
- `pwsh -File tools/rtl-media-envelope-verify.ps1`, then
  `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: full 4,243,649-boundary envelope and
  9 boundary/model plus 5 real-JT51 legacy benches PASS, with zero simulator
  errors/warnings. Logs: `out/build/m6-enveloped-envelope.log`,
  `out/build/m6-enveloped-rtl.log`, `out/build/m6-enveloped-legacy.log`.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 461.27 seconds, including
  format, tidy and architecture rejection fixtures. Log:
  `out/build/m6-enveloped-host.log`. `python -B tools/check_harness.py` and
  `python -B out/build/m6-enveloped-links.py`: PASS, with 60 local file links.

The first integrated registered probe fitted but failed hold timing. Detailed
inspection found one violating path from `clk_audio` to `captured[357]`, worst
-0.388 ns. The generic probe was sampling the directly forwarded `audio_mclk`
as data on that same clock's edge. The final probe excludes that forwarding
wire from its data capture; every other output, including LRCK and DAC, remains
captured. MCLK continuity remains checked in serial simulation. No production
RTL, clock period, false/multicycle path or warning suppression was changed to
resolve this fixture error. Initial project/report:
`out/research/m6-enveloped-timing`, `out/build/m6-enveloped-hold-paths.rpt`.

`python -B out/build/m6-enveloped-timing.py`, followed by Quartus 25.1std.0
Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe` on
`out/research/m6-enveloped-timing-final/enveloped_probe`: final fit and all
four timing models PASS for 5CEBA4F23C8, seed 1, period 81.380 ns. Worst setup
is 12.739 ns and hold 0.052 ns; recovery/removal/pulse-width slack is also
positive, all TNS zero, and setup/hold are fully constrained within this probe.
The probe includes 669 input and 426 output data bits with shift/capture
registers and the same falling-edge reset-release fixture as the earlier
envelope probe. It is not a CPU CDC or Pocket board timing result.

Resources including this fixture are 3,834 ALMs, 3,453 registers, 52,424 memory
bits in 16 RAM blocks and three DSPs. The progress FIFO and native shift storage
still infer RAM. Analysis & Synthesis has zero errors and nine reviewed upstream
ROM write-side-net warnings. Fitting has zero errors and the same three reviewed
probe warnings (LogicLock and automatic/incomplete pin assignments). STA has
zero errors/warnings. The five probe pins do not establish Pocket I/O timing.
Source hashes and reports are in that final project; logs are
`out/build/m6-enveloped-final-{map,fit,sta}.log`. All generated projects,
GPL derivatives and logs remain ignored.

Already-mapped progress is still an input to this local composition. The
producer's source-tick mapping, CPU/media queue and CDC/ACK protocol remain
the next slice 4 work. This does not establish a Pocket-controlled player or
firmware 2.6 acceptance. No JSON, APF shell, package or hardware input changed;
cross-build, APF lifecycle/CDC and hardware checks were not run for this unit.

While establishing the audible mapper's prerequisites on 2026-09-13, a native
burst test found accepted operator writes could be lost. The source released
its data pulse at the next `cen` edge, while JT51's busy latch uses `cen_p1`.
If that edge was the other half, busy was never asserted, and following data
replaced the pending operator update before its scan slot. The earlier paced
audio comparisons did not prove burst write reflection.

The corrected M6 source holds data through `cen_p1`, still a `cen` edge, so
JT51 observes busy before the adapter can start the next bus transaction.
This preserves public ports, clock rate, pause/reset ownership and the existing
write guarantee. No previously specified exact pulse length or maximum write
latency changes. The frozen legacy v1 source/MMIO and hardware packages remain
unchanged; this finding is not evidence about a firmware 2.6 failure.

Executed burst evidence:

- `pwsh -File out/build/m6-native-burst-probe.ps1`: the initial 32-TL-write
  probe accepted all 32 but reflected only the last operator, with 62/64 final
  scan-value mismatches. Log: `out/build/m6-native-burst-before.log`. This
  exploratory runner's native exit was zero despite simulator Fatal; the log's
  failure, not process success, is the evidence.
- `pwsh -File tools/rtl-media-audio-verify.ps1` before the fix: FAIL in the new
  checked burst bench after 96 accepted operations: operator 14 TL expected 15,
  actual reset value 127. The production runner correctly returned failure.
  Log: `out/build/m6-media-burst-before.log`.
- The same command after the fix: all three benches PASS, zero simulator
  errors/warnings. The new native-bank test checks 3,072 writes and 2,048 scan
  observations across 32 reset/start offsets, including 45,044 held edges, 466
  address-held, 529 data-held and 41,644 busy-held observations. It checks TL,
  DT1/MUL and KS/AR independently from the accepted bytes and documented native
  scan-stage offsets; no waveform golden or expected register value changed.
  Log: `out/build/m6-media-burst-fixed.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`, then
  `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: composed output and all 9 boundary/model
  plus 5 real-JT51 legacy benches PASS, zero simulator errors/warnings.
  The composed output still matches 3,200 analytically scaled frames and passes
  all nine reset/terminal scenarios. Logs: `out/build/m6-burst-enveloped.log`,
  `out/build/m6-burst-rtl.log`, `out/build/m6-burst-legacy.log`.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 428.29 seconds, including
  format, tidy and architecture rejection fixtures. Log:
  `out/build/m6-burst-host.log`. `python -B tools/check_harness.py` and
  `python -B out/build/m6-burst-links.py`: PASS, with 57 local file links.
  The final fitted production-source and wrapper byte hashes match the worktree.

`python -B out/build/m6-burst-timing.py`, followed by Quartus 25.1std.0
Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe` on
`out/research/m6-burst-timing/enveloped_probe`: fit and all four timing models
PASS using the previous 81.380 ns registered-data probe, same device and seed.
Worst setup is 11.468 ns, hold 0.126 ns; recovery/removal/pulse-width slack is
positive and all TNS zero. Resources including the fixture are 3,845 ALMs,
3,475 registers, 52,424 memory bits in 16 RAM blocks and three DSPs. Map has
the same nine reviewed upstream ROM-net warnings, fit the same three reviewed
probe pin/LogicLock warnings, and STA zero errors/warnings. No false path,
multicycle, warning suppression or constraint relaxation was introduced. Logs:
`out/build/m6-burst-{map,fit,sta}.log`; source hashes/reports are in that project.
This is not full-player placement, Pocket I/O/CDC or hardware acceptance.

An additional latency observation (`pwsh -File out/build/m6-native-latency-probe.ps1`,
log `out/build/m6-native-latency.log`) sees TL bank output change 33–40 retained
audio-clock edges after busy clears in its 32 single-write cases. Its ring
update-to-bank-output interval is 32 `cen_p1` edges. These sampled cases are
not an exhaustive latency bound or a PCM-output measurement; they disprove
using busy-clear alone as the audible checkpoint. The source-progress mapper
must also account for the native pipeline and output conversion/serialization.

The mapper, CPU/CDC transport and real Pocket integration remain pending.
Cross-build, APF/package and hardware inputs did not change in this fix, so
those checks were not run. All exploratory sources, logs and GPL derivatives
remain ignored.

The sample-to-output portion of the mapper now retains a source-edge position
with each actual selected stereo sample. The native clock owns a checked u64
retained-edge counter; the converter stores positions in the same pending and
output transactions as PCM. The enveloped owner exposes pending and serialized
positions and routes counter exhaustion through its existing fault/reset path.
Empty/startup/pause frames have no source position, while position zero remains
a valid sample value. Legacy wrapper ports and decoded audio remain unchanged.

Executed sample-position evidence on 2026-09-13:

- `pwsh -File tools/rtl-media-audio-verify.ps1 -OutputOnly`: both benches PASS,
  zero simulator errors/warnings. The position bench uses independent integer
  quotient selection and authored 64-bit positions while checking every I2S
  bit. It checks 371 emitted sample/position pairs, 2,182 selected inputs,
  2,912 dropped inputs, 1,809 replacements, 153 selection/boundary coincidences,
  519 held-pending boundaries, 159 empty consuming boundaries and one valid
  zero position. Log: `out/build/m6-position-output.log`.
- `pwsh -File out/build/m6-position-negative.ps1`: the ignored negative-control
  copy substitutes the newly arriving position only on selection/consumption
  coincidence. The unchanged test detects the specific position mismatch at
  retained edge 512. Its simulator Fatal and one error are the expected negative
  result, not a production test pass. No checked-in source is modified by the
  control. Log: `out/build/m6-position-negative.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS, zero simulator
  errors/warnings. All 3,200 decoded frames still match the analytical gain
  reference, including 2,758 changed by scaling. Independently counted source
  edges and quotient selection identify 3,198 native positions. Two initial
  frames are empty: reset scan starts at zero with its sample flag clear, the
  second native strobe arrives after two 32-slot scans, and conversion selects
  strobe 2 rather than strobe 1. The first pending sample is therefore consumed
  at frame 2. The test also checks pauses, ten terminal/reset cases and the real
  u64 final increment after a held near-limit counter. Log:
  `out/build/m6-position-enveloped-fixed.log`.
- Sequential `pwsh -File tools/rtl-media-audio-verify.ps1`,
  `pwsh -File tools/rtl-verify.ps1`, and
  `pwsh -File tools/rtl-jt51-verify.ps1`: all four M6 media benches, nine
  boundary/model benches and five real-JT51 legacy benches PASS, zero simulator
  errors/warnings. Native burst reflection remains 3,072 writes and 2,048 scan
  observations. Logs: `out/build/m6-position-{media,rtl,legacy}.log`.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-position-links.py`: PASS, with 61 local file links.
  Fitted source and registered-probe byte hashes match the current worktree.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 486.74 seconds, including
  formatting, clang-tidy (470.94 seconds), harness and positive/negative
  architecture checks. Log: `out/build/m6-position-host.log`.

The initial integrated position check failed in the finite RepeatOne reset
case because its test helper sampled the falling edge immediately after End,
before synchronous reset had reached a receiving rising edge. Diagnostic log
`out/build/m6-position-enveloped-debug.log` showed generation 9 still retaining
edge 2,048 and pending edge 1,983 at that intervening half-cycle. The helper now
checks after the receiving rising edge; its zero-position, silence and
2,048-edge hold assertions remain enforced. No production reset delay changed.

`python -B out/build/m6-position-timing.py`, followed by Quartus 25.1std.0
Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe` on
`out/research/m6-position-timing/enveloped_probe`: map/fit and all four timing
models PASS with the same 81.380 ns registered-data probe, device and seed.
The probe captures all 620 data-output bits, including the new positions, and
has 669 registered data-input bits. The forwarded MCLK is not a data capture.
Worst setup is 11.757 ns and hold 0.095 ns; recovery/removal/pulse-width slack
is positive and all TNS zero. Resources including the fixture are 3,983 ALMs,
3,873 registers, 52,424 memory bits in 16 RAM blocks and three DSPs. Map has
nine reviewed upstream ROM-net warnings; fit has three reviewed probe
pin/LogicLock warnings, including unassigned pins; STA has zero errors/warnings.
No timing exception or warning suppression was added. Source hashes and reports
are in the project; logs are `out/build/m6-position-{map,fit,sta}.log`.

This establishes native sample-capture position through output conversion,
including held frames. It does not assign write batches, events or zero-write
MDX ticks to native samples. Native pipeline delay, ordered progress mapping,
CPU/CDC/ACK and sound/storage adapters remain the current work. The local fit
is not Pocket I/O/CDC or full-player timing/placement. Cross-build, APF/package
and hardware checks were not run because those inputs remain unchanged. The
standalone envelope suite was not repeated: its source/arithmetic is unchanged
and the affected integration was exercised above. All generated probes, logs
and GPL derivatives remain ignored.

### Ordered native source receipts (2026-09-13)

The user explicitly approved the source-receipt proposal. The shared source
now numbers accepted writes and zero-write markers, reserves its receipt slot
before a write, and retains the actual transfer/marker edge until delivery.
Write priority and the in-flight bus owner prevent a marker from overtaking a
write. A held source may deliver an already completed receipt without advancing
native state or source time. Reset suppresses handshakes and clears the stream;
offering another operation after the last valid token faults without wrapping.
The enveloped owner exposes the receipt ports and routes exhaustion through its
existing DeviceFault/reset. The old pause-only wrapper always drains receipts,
so its ready equation reduces to the old bus-owner condition for valid streams.

Executed RTL evidence:

- `pwsh -File tools/rtl-media-audio-verify.ps1`: all five benches PASS, each
  with zero simulator errors/warnings. The new bench independently counts
  retained edges and observes the physical data pulse, without deriving
  receipt positions from the DUT's token/timestamp or FSM registers. It covers
  all 32 data-release phases, simultaneous write/marker priority, a reserved
  slot, blocked delivery, consume/accept on one edge, 32 deliveries during hold,
  and 42 resets including every in-flight bus phase. The original run used
  36 writes and 164 markers. Log: `out/build/m6-receipt-media.log`.
- The final bench additionally sends the last valid full-width token through
  the in-flight **write** register (previously that boundary used a marker).
  Focused `vlog.exe -quiet -sv -work work F:/source/rpcmp/tests/rtl/jt51_source_receipt_tb.sv`
  and `vsim.exe -c -quiet -lib work jt51_source_receipt_tb -do 'run -all; quit -code 0'` in the same generated
  media-audio work library: PASS, 37 writes, 163 markers, 195 delivered receipts,
  32 held deliveries, 42 resets, phase mask ffffffff, zero errors/warnings.
  Four in-flight writes and one blocked marker are deliberately reset rather
  than delivered. Log: `out/build/m6-receipt-focused.log`.
- `pwsh -File out/build/m6-receipt-negative.ps1`: an ignored copy deliberately
  substitutes admission time for data-transfer completion. The bench rejects
  it with `receipt position/order mismatch after edge`, Errors 1 / Warnings 0
  as expected. This is negative-control evidence, not a production PASS.
  Log: `out/build/m6-receipt-negative.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS, 3,200 decoded frames,
  2,758 scaled changes, 3,198 native positions, completed 960-frame restoration
  and 11 terminal/reset cases. The added case checks the final valid marker
  through the wrapper, refusal during Pause, and a real token-counter failure
  followed by shared reset. It does not force the failure signal. All reset
  cases now also check discarded receipts and restarted operation numbering.
  Log: `out/build/m6-receipt-enveloped.log` (zero errors/warnings).
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: all nine/five benches PASS with zero
  errors/warnings, executed sequentially. Logs:
  `out/build/m6-receipt-rtl.log`, `out/build/m6-receipt-legacy.log`.
- `pwsh -File tools/host-verify.ps1`: 76/76 PASS in 570.00 seconds, including
  format, clang-tidy (551.28 seconds), harness and positive/negative architecture
  checks. Log: `out/build/m6-receipt-host.log`. The final strengthened RTL-only
  bench was separately compiled/executed as above; no host source changed.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-position-links.py`: PASS, with 61 local file links.
  `Get-FileHash -Algorithm SHA256` against the registered fit's `sources.json`
  matches all four production sources and its probe to the tested worktree.

`python -B out/build/m6-receipt-timing.py`, followed by Quartus 25.1std.0
Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe` on
`out/research/m6-receipt-timing/enveloped_probe`: map/fit and all four timing
models PASS. The same 5CEBA4F23C8, seed 1, 81.380 ns registered-data fixture
now has 671 input bits and captures all 815 output bits; MCLK remains a direct
clock output, not captured data. Resources including the fixture are 4,185 ALMs,
4,360 registers, 52,424 memory bits in 16 RAM blocks and three DSPs. Native and
progress RAM inference is preserved. Worst setup is 11.292 ns and hold 0.047 ns;
recovery/removal/pulse-width slacks are positive, all TNS zero, and no setup/hold
paths are unconstrained. Map has nine reviewed upstream ROM write-net warnings;
fit has three reviewed fixture pin/LogicLock warnings, including five unassigned
pins; STA has zero errors/warnings. No exception, relaxed constraint or warning
suppression was added. Logs: `out/build/m6-receipt-{map,fit,sta}.log`.

These are actual bus/marker positions, not a native-pipeline completion fence.
The next work remains mapping those positions and ordered MDX observations to
the selected audio samples, with source coverage/deadline handling, then
sound-control/CDC/ACK and real sound/storage adapters. Whole-player/Pocket pin
timing, APF/package, cross-build and hardware checks were not run: those inputs
are unchanged by this local unit. The standalone envelope arithmetic is also
unchanged; its affected native/output integration was run above. This does not
complete the hardware milestone or reopen the closed M5 investigation.

### Native sample availability (2026-09-13)

The source clock and reset/scan equations now establish the local time available
between native sample capture and output consumption. The derivation is in
[Pocket enveloped audio v1](../../specs/pocket-enveloped-audio-v1.md). It preserves
production behavior and adds no delay or public port. Its assumptions include
the pinned clock ratios, native reset, and the first source edge at consuming
frame 0, as the enveloped Begin provides. An arbitrary converter input or a
source attached midway through a frame does not inherit this guarantee.

For every initial clock phase, the k-th selected native sample falls in
`[256*k, 256*(k+1)-29]`. The old pending sample is available at the next consuming boundary;
Pause freezes the source edge count. Checking all 614,400 selections and proving
the exact 157,286,400-edge period translation extends the integer bound to later
periods before stream exhaustion. The 29-edge value is a conservative bound;
the native cases below observed 32..256 edges, not a measured 29-edge minimum.

Executed evidence:

- `python -B tests/rtl/media_clock_math.py`: two tests PASS, covering initial
  state bounds and the complete 614,400-selection/715,909-native-sample period.
  This independent integer calculation is also registered as `media_clock_math`
  in the host CTest gate.
- `pwsh -File tools/rtl-media-audio-verify.ps1`: all six benches PASS, with zero
  errors/warnings each. The new native phase bench checks 8,192 native sample
  edges over 32 reset lengths, all three reachable initial pulse/phase classes,
  8,000 concurrent writes and Pause/Resume. It observes 7,008 selections, 7,040
  consuming frames and 32,544 held edges. Log: `out/build/m6-phase-media.log`.
  The final bench additionally compares the actual converter selection counter
  to its closed-form ordinal on every edge and rejects unknown/out-of-range
  initial clock state. Focused compilation/execution with
  `vlog.exe -quiet -sv -work work F:/source/rpcmp/tests/rtl/jt51_media_phase_tb.sv`
  and `vsim.exe -c -quiet -lib work jt51_media_phase_tb -do 'run -all; quit -code 0'`
  in `out/sim/media-audio`: the same counts PASS, zero errors/warnings.
  Log: `out/build/m6-phase-focused.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS, including the new
  availability-window assertion for all 3,198 valid source positions across
  3,200 decoded stereo frames, gain changes and pauses. Existing 11 reset cases
  remain passing. Log: `out/build/m6-phase-enveloped.log`, zero errors/warnings.
- `pwsh -File out/build/m6-phase-negative.ps1`: changing only an ignored native
  derivative's reset-time sample flag makes the phase test fail: first sample
  edge 4 versus the independently expected 224. The expected `native strobe
  equation` failure is observed with Errors 1 / Warnings 0; the original JT51
  and production RTL remain unchanged. Log: `out/build/m6-phase-negative.log`.
- `pwsh -File tools/host-verify.ps1`: 77/77 PASS in 429.31 seconds, including
  the new arithmetic proof, format, clang-tidy (412.94 seconds), harness and
  positive/negative architecture checks. Log: `out/build/m6-phase-host.log`.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-position-links.py`: PASS, with 62 local file links.

This test/derivation unit changes no production RTL or firmware. Synthesis,
the unrelated legacy RTL suites, cross-build, package and hardware were not
repeated; their inputs are unchanged. The affected media and enveloped suites
were run above. Native write/pipeline-to-sample mapping, source coverage and
the sound-control/CDC/ACK and platform integration remain current M6 work.
The 29-edge local budget does not establish a CPU/CDC deadline or authorize
audible progress publication on receipt delivery.

### Native operation prefix through output (2026-09-13)

[JT51 native completion v1](../../specs/jt51-native-completion-v1.md) now connects
actual source receipts to a conservative direct-control completion prefix and
retains that prefix with selected PCM through pending storage and serialization.
The 32-entry synchronous RAM FIFO checks its 64-bit due addition, preserves
backpressure and retires only while source time advances. The enveloped owner
delivers a receipt atomically to its external consumer and the completion FIFO.
Overflow enters the existing DeviceFault/native-reset path.

The bound is 768 subsequent native half-rate enables, excluding the receipt
edge: 128 steps for configuration/global staging, 512 for finite LFO serial
arithmetic and 128 for an operator round/arithmetic/accumulation. The clock
equation bounds those enables by 5,273 retained audio edges. Registered prefix
retirement prevents a same-edge sample from prematurely seeing the new token.
This is a direct-control-use bound; phase/envelope/feedback/LFO/noise/timer
musical history persists. It is not an empirical first-PCM-difference bound.
The earlier amplitude probes were not used as maximum-latency oracles.

Executed evidence:

- `python -B tests/rtl/media_clock_math.py`: three tests PASS, including the
  independent subsequent-enable equation and its extreme accumulator/phase
  states. The existing complete rational sample-availability proof still passes.
- `pwsh -File tools/rtl-media-audio-verify.ps1`: all nine benches PASS, each
  with zero errors/warnings. The completion FIFO accepts 133 and retires 130
  receipts; three occupied/prefetched entries are deliberately reset away.
  It checks 20 full cycles, 94 held cycles, 96 simultaneous admissions/retirements,
  ten reset cases, pointer wrap and the exact last-safe/first-overflow due
  positions. The unchanged source-receipt, burst, pause and phase tests pass.
  Log: `out/build/m6-completion-media.log`.
- The same command runs the unmodified pinned native LFO and accumulator
  components. LFO dependency tags cover 1,024 start cases (startup and all
  steady phases, both values of its unreset serial-reset latch), 512 period
  checks and 468,480 fresh-output checkpoints. Two copies with differing
  multiplier intermediates agree whenever tagged fresh; their retained
  musical state stays identical. The latest first-fresh result is 375 enabled
  steps, within the separately derived 512 allowance. The accumulator checks
  192 exact sample positions from 32 authored impulses; observed contribution
  delay is 41..72 enabled steps. Neither experiment claims waveform settling.
- `pwsh -File out/build/m6-completion-negative.ps1`: expected rejection of both
  ignored-only mutants. A 5,272-edge retirement fails `early native completion`;
  removing the native LFO's bit-zero product restart fails `tagged intermediate
  differs`. Each simulation reports one expected error and zero warnings;
  the runner requires the specific failure and no PASS marker. Production
  sources and the pinned native checkout remain unchanged by the mutants.
  Log: `out/build/m6-completion-negative.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS, 3,200 decoded stereo
  frames, 3,198 source positions, 3,704 native samples and 3,178 output frames
  carrying a nonzero prefix, 56 writes, 34 ordered multicast markers and 13
  reset cases. A test-owned native-enable count rejects a sample prefix before
  768 subsequent enables. The paced fixture also derives every native/output
  prefix directly from actual receipt edges, requiring exact correspondence;
  its receipt-spacing assertion establishes head-prefetch availability. This
  catches missing or late prefixes as well as premature ones. PCM still matches the analytically scaled uninterrupted
  reference. A zero-write marker remains incomplete across Pause; a full FIFO
  plus one blocked source receipt recovers without loss/duplicate delivery or
  withdrawing external valid. A real due-addition overflow triggers reset.
  Log: `out/build/m6-completion-enveloped.log`. The initial integration run
  reached its final coverage assertion but incorrectly inspected the expected
  prefix after Stop had invalidated output. The fixture now records its actual
  serialized occurrence before Stop; reset invalidation remains required.
  Initial log: `out/build/m6-completion-enveloped-initial.log`.
- Sequential `pwsh -File tools/rtl-verify.ps1` and
  `pwsh -File tools/rtl-jt51-verify.ps1`: all nine boundary/model and five legacy
  JT51 benches PASS, with zero simulator errors/warnings. Logs:
  `out/build/m6-completion-rtl.log`, `out/build/m6-completion-legacy.log`.
- `pwsh -File tools/host-verify.ps1`: 77/77 PASS in 420.24 seconds, including
  format, clang-tidy (404.41 seconds), harness and positive/negative architecture
  checks. Log: `out/build/m6-completion-host.log`. No host source changed after
  that gate; the final stronger RTL-only mapping assertions were separately
  compiled and run by the enveloped command above.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-completion-links.py`: PASS, with 70 local file links.

`python -B out/build/m6-completion-timing.py`, followed by Quartus 25.1std.0
Build 1129 `quartus_map.exe`, `quartus_fit.exe` and `quartus_sta.exe` on
`out/research/m6-completion-timing/enveloped_probe`: map/fit and all four timing
models PASS. The 5CEBA4F23C8, seed 1, 81.380 ns registered-data fixture has 671
input bits and captures all 943 output bits; MCLK is forwarded directly.
Resources including the fixture are 4,308 ALMs, 4,709 registers, 56,520 memory
bits in 20 RAM blocks and three DSPs. The new 32 x 128 FIFO infers dual-port RAM
with a registered read address; no reset/read-during-write RAM behavior is
relied on. Native/progress RAM inference remains intact. Worst setup is
10.552 ns and hold 0.107 ns; recovery/removal/pulse-width slacks are positive,
all TNS zero, and setup/hold are fully constrained. Map's nine upstream ROM
unused-write-net warnings and fit's three fixture pin/LogicLock warnings
(including five unassigned pins) were reviewed; STA has zero errors/warnings.
No exception, relaxed constraint or warning suppression was added. Logs:
`out/build/m6-completion-{map,fit,sta}.log`. SHA-256 checks against `sources.json`
match all five production sources and the probe to this tested worktree.

This is local audio-domain completion evidence. The scheduled source queue,
sealed coverage, retained MDX batches, progress admission and per-event committed
mapping still need to be connected before publishing loop/end/history from this
path. CPU/CDC/ACK and sound/storage adapters follow. Whole-player/Pocket pin
timing, cross-build, APF/package and hardware checks were not run: their inputs
have not yet been connected to this local unit. The unchanged standalone gain
arithmetic suite was not repeated; its affected native/output integration was.
This does not complete M6 hardware acceptance or reopen the M5 investigation.

### Scheduled source queue and streaming coverage

Implemented on 2026-09-13 under [source queue v1](../../specs/media-source-queue-v1.md).
The 64-entry synchronous RAM queue copies timestamped writes and batch markers,
validates consecutive batch starts, and dispatches in order against retained
native time. Admission/head prefetch can continue while held. A marker seals
the next source interval only when dispatched; a final marker closes source
input without claiming audible end. An empty eligible dispatch outside sealed
coverage raises a registered supply fault through the existing shared reset.
Busy/native receipt backpressure remains distinct from missing source input.

The maximum MDX tick need not fit the FIFO before starting. The authored native
fixture streams 8,192 writes plus three markers through 64 slots, including
write traffic overdue across the next nominal batch boundary. It compares
actual receipts and native PCM with independently authored bytes sent to an
unqueued native source at the same accepted edges. Source times are not changed
to conceal serialization or backpressure. Its preloaded envelope interval is
explicit test input, so it does not establish mapped MDX loop/end progress.

Executed checks:

- `pwsh -File tools/host-verify.ps1`: 77/77 PASS, 431.59 seconds, including
  format (0.47 seconds), tidy (415.76 seconds), harness and positive/negative
  architecture checks. The allowlist adds only this RTL file; dependency
  direction checks are unchanged. Log: `out/build/m6-source-queue-host.log`.
- `pwsh -File tools/rtl-media-audio-verify.ps1`: ten benches PASS, each with
  zero errors/warnings. The new queue bench records 379 admissions and 375
  dispatches (four items deliberately discarded by reset/fault), two full
  offers, ten invalid/closed offers, one simultaneous admission/dispatch,
  72 held admissions, ten resets and two exact supply-fault cases. It covers
  pointer wrap, high-bit timestamps, zero-write/final markers and a refill on
  the missed opportunity. Log: `out/build/m6-source-queue-media.log`.
- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: both benches PASS with
  zero errors/warnings. The existing 3,200-frame gain/position/prefix check is
  unchanged. The scheduled fixture records 8,192 writes, 8,195 ordered receipts,
  9,130 native PCM comparisons (9,092 nonzero), three admissions during Pause,
  3,794 writes beyond the next nominal marker time, and the final prefix across
  two output frames. It also resets a real data pulse with queued work and
  checks missing supply in the next generation. Its 109,948 Full observations
  are retry clocks, not lost items. Log: `out/build/m6-source-queue-enveloped.log`.
- `pwsh -File out/build/m6-source-queue-focused.ps1`: final native queue case
  PASS with the same counts, zero errors/warnings, at 18:47:57. This recompiles
  and executes the entire strengthened SV test: it captures the address phase
  and data phase from the real bus, instead of using the source's address
  latch as its address witness. Production inputs are unchanged from the full
  runner and fit. This SV-only refinement does not require repeating the
  unchanged host gate. Log: `out/build/m6-source-queue-focused.log`.
- `pwsh -File out/build/m6-source-queue-negative.ps1`: two expected failures.
  Ignoring due time fails `changed/reordered/early dispatch`; ignoring native
  readiness in the supply check fails the independent fault oracle. Each
  mutant has one error, zero warnings and no PASS marker. Mutants remain under
  ignored `out/sim/source-queue-negative`; production/native files are unchanged.
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: nine and five benches PASS, respectively,
  each with zero simulator errors/warnings. All RTL runs were sequential.
  Logs: `out/build/m6-source-queue-{rtl,legacy}.log`.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-source-queue-checks.py`: PASS; the latter validates
  seven synthesis input hashes and local links in changed Markdown.

Initial unit runs exposed missing test coverage for an invalid marker byte and
simultaneous admission/dispatch; the fixture now actually presents those cases,
including the required head-prefetch edge, without lowering its assertions.
The first native run failed its pause check because the fixture's serial phase
had not followed power reset. The corrected fixture also asserts that control
is offered on the actual frame boundary. No production change or expected PCM
regeneration was used to make these test-stimulus errors pass.

`python -B out/build/m6-source-queue-timing.py` generated a separate registered
queue-plus-enveloped probe in `out/research/m6-source-queue-timing`. Quartus
25.1std.0 Build 1129 map/fit/sta on 5CEBA4F23C8, seed 1, 81.380 ns completed
successfully: 4,433 ALMs, 5,008 registers, 65,864 memory bits, 24 RAM blocks and
three DSPs for the complete local probe. The queue is a 64 x 146 simple dual-port
RAM with synchronous read address and no memory reset; its 9,344 bits remain
in RAM. The registered fixture has 800 input and 954 observation bits; MCLK
is forwarded rather than sampled as data.

All four timing models have zero TNS and fully constrained setup/hold. Minimum
setup is 12.661 ns, hold 0.098 ns; all recovery/removal/pulse-width slacks are
positive. Map's nine upstream ROM unused-write-net warnings and fit's three
fixture pin/LogicLock warnings (including five unassigned pins) were reviewed.
STA has zero errors/warnings. No new exception, constraint relaxation or warning
suppression was added. Logs: `out/build/m6-source-queue-{map,fit,sta}.log`.

CPU feeding, retained MDX checkpoints/per-event frames, and the source-to-mapped
envelope producer remain to be connected. Source supply coverage alone is not
loop/end coverage. Sound-control/CDC/ACK and storage adapters follow. Whole
Pocket fit/CDC, cross-build, APF/package and hardware were not run: this unit
does not yet change or connect those targets. The unchanged standalone gain
arithmetic suite was not repeated; its native/output integration was run.

### Local MDX checkpoint-to-output connection

Implemented on 2026-09-13 following explicit approval of
[MDX audible progress v1](../../specs/mdx-audible-progress-v1.md).
`rpcmp_jt51_progress_audio` now connects scheduled marker loop/end metadata,
the actual native completion FIFO, selected PCM and automatic envelope intervals.
The shared queues/converter carry opaque payloads; the old wrappers bind zero
and preserve their external APIs. No second completion timer estimates progress.

Begin requires supplied input, admits the explicit reset baseline [0,2), then
arms a consuming audio boundary. Selected checkpoints admit one interval per
frame; Pause retains it without duplicates. Natural end evaluates its pending
checkpoint without consuming that PCM. Source starvation and mapping failures
use the shared reset owner. CPU feeding and public/per-event history are still
separate work, not inferred from this local connection.

Executed checks:

- `pwsh -File tools/rtl-media-audio-verify.ps1 -OutputOnly`: four benches PASS;
  the expanded source/completion/sample oracles check 65/66-bit payload copies,
  ordinary-write preservation, Full retry, pointer wrap, hold and reset.
  Log: `out/build/m6-progress-output.log`.
- `pwsh -File out/build/m6-progress-focused.ps1`, then the final
  `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: PASS. The latter runs all
  three integration benches. The new bench checks 316 consumed frames,
  310 selected samples and 182 nonzero frames across six checked starts/ends,
  one fade and restoration, one simultaneous marker receipt replacement and
  one merged checkpoint sample. Actual authored native bus/marker positions,
  the certified completion bound, independent quotient selection and analytical
  gain determine the expected frame metadata and every serialized PCM bit.
  Loop/end-boundary Pause/Resume, Begin at phases 0/73/127/254/255, invalid
  metadata/empty Begin, real starvation and fresh-generation recovery pass.
  The last fault/recovery pair checks terminal/reset outcomes separately from
  the six-start PCM oracle. Logs: `out/build/m6-progress-{focused,enveloped}.log`.
- The same enveloped command preserves the existing 3,200-frame integration
  results (3,198 positions, 34 multicast receipts, 13 reset cases) and the
  8,192-write streaming case (8,195 receipts, 9,130 native PCM comparisons,
  3,794 overdue writes, two final-prefix frames and two reset/fault cases).
- `pwsh -File tools/rtl-media-audio-verify.ps1`: ten benches PASS, including
  379 source admissions/375 dispatches and 133 completion admissions/130
  retirements. Differences are deliberately discarded reset/fault fixtures,
  not dropped work. Native hold/burst/receipt/phase and LFO/accumulator proofs
  retain their existing checks. Log: `out/build/m6-progress-media.log`.
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: all configured benches PASS.
  All positive RTL simulations report zero errors/warnings and run sequentially.
  Logs: `out/build/m6-progress-{rtl,legacy}.log`.
- `pwsh -File out/build/m6-progress-negative.ps1`: three expected mutants
  rejected. Read-ahead marker data instead of native-completed payload fails
  sample ownership at retained edge 6,158; omitting bootstrap arming fails the
  initial frame's envelope state; adding one to mapped loops fails at frame 3.
  Each has exactly one error, zero warnings and no PASS marker. Production and
  pinned native sources remain unchanged. Log: `out/build/m6-progress-negative.log`.

The first new integration compilation rejected four test calls missing a task
argument. Giving that test helper its intended default fixed compilation;
no production logic, golden trace or assertion was relaxed. The final fixture
also exercises marker replacement and multiple checkpoints per selected sample.

`python -B out/build/m6-progress-timing.py` generated a registered local probe;
`pwsh -File out/build/m6-progress-fit.ps1` ran Quartus 25.1std.0 Build 1129
map/fit/sta on 5CEBA4F23C8, seed 1, 81.380 ns. The complete probe uses 4,651 ALMs,
5,212 registers, 72,168 memory bits, 27 RAM blocks and three DSPs. Source RAM is
64 x 211 bits and completion RAM 32 x 195, both simple dual-port, single-clock
RAMs. Its 542 input/1,080 observation registers constrain local paths; forwarded
MCLK is not treated as data. These are not Pocket pin or CDC constraints.

| Timing model | Setup | Hold | Recovery | Removal | Pulse width |
| --- | ---: | ---: | ---: | ---: | ---: |
| Slow 85 C | 15.691 | 0.374 | 31.916 | 2.314 | 39.084 |
| Slow 0 C | 13.214 | 0.369 | 32.183 | 2.154 | 38.999 |
| Fast 85 C | 31.943 | 0.169 | 36.363 | 1.149 | 39.474 |
| Fast 0 C | 32.930 | 0.133 | 36.664 | 1.028 | 39.464 |

All slacks are ns; every TNS is zero and setup/hold are fully constrained.
Map's nine warnings concern unused write nets of initialized upstream ROMs.
Fit's three warnings concern LogicLock/incomplete I/O and five unassigned
fixture pins. They were reviewed; STA has zero errors/warnings. No timing
exception, relaxed constraint or new warning suppression was added.
Logs: `out/build/m6-progress-{map,fit,sta}.log`.

- `pwsh -File tools/host-verify.ps1`: all 77 tests PASS, 440.43 seconds,
  including format (0.64 s), tidy (421.21 s), harness and positive/negative
  architecture checks. The only dependency-check change is the exact new local
  RTL allowlist entry; Core/UI coupling rejection is unchanged.
  Log: `out/build/m6-progress-host.log`.
- `python -B tools/check_harness.py` and
  `python -B out/build/m6-progress-checks.py`: PASS; the latter checks seven
  synthesis input hashes, changed Markdown local links and timing summaries.

Whole-Pocket fit/CDC, cross-build, APF/package and hardware were not run: this local unit does
not connect those targets. The unchanged standalone envelope arithmetic suite
was not repeated; the affected native/gain/output integration ran instead.

## Slice 4 sound session execution — 2026-09-13

The reviewed CPU connection plan is now adopted. Its first implemented layer,
`rpcmp_sound_session`, copies Reset/Start/Pause/Resume/SetPolicy requests and
retains the exact completion until release. It validates nonzero identities,
policy revisions and generations before invoking the envelope. Reset advances
a checked feed epoch and explicitly clears logical state and old native work;
only physical quiescence permits success and clearing sticky inhibit/fault.
The original progress `stream_reset` fault behavior is preserved; direct
callers tie the new `session_reset` low. Native receipts are consumed locally.
No CPU MMIO, CDC, producer or history journal is implemented by this layer.

The final local request-phase oracle covers all 256 serial phases. Reset takes
2,050 subsequent audio edges, Start at most 259, and boundary controls at most
257. The last includes one observation edge after application to detect a
concurrent failure. Resume ACK preserves the held pre-consume frame even when
latest media state has advanced. An unread response does not hold playback;
emergency inhibit acts independently and does not rewrite completed responses.
These local bounds do not establish a CPU watchdog deadline.

Executed checks:

- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: all four benches PASS,
  including 3,200 decoded stereo frames, 8,192 authored writes/8,195 receipts,
  and the existing 316-frame marker-to-output oracle. Final additional boundary
  fixtures were checked with `pwsh -File tools/rtl-enveloped-audio-verify.ps1
  -SessionOnly`: PASS, 256 phases, 1,585 responses, five emergency stages and
  4,184 nonzero serial bits. It covers queued/in-flight normal Reset, paused
  Reset, prior-fault recovery, same-edge item rejection, stale epochs/IDs,
  inability to restart a used generation after Reset, live fade/restoration,
  an end overtaking an admitted control, actual starvation and u64 exhaustion.
  Logs: `out/build/m6-session-{enveloped,focused}.log`.
- `pwsh -File tools/rtl-media-audio-verify.ps1`, then
  `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: PASS. All RTL suites ran sequentially;
  positive simulations report zero errors/warnings.
  Logs: `out/build/m6-session-{media,rtl,legacy}.log`.
- `pwsh -File out/build/m6-session-negative.ps1`: three expected mutants
  rejected: returning Resume's latest frame, losing the Reset acceptance-edge
  fault, and admitting a request while a completion is unread. Each fails its
  targeted assertion with one error, zero warnings and no PASS marker.
  Production/native source files were not mutated.

The acceptance-edge fault test first reproduced a defect in the new session:
a fault asserted only on the Reset admission edge could disappear before the
wait state observed it. Admission now records Failed while preserving the
destructive reset and inhibit; the unchanged regression passes. The initial
counter-limit fixture used an illegal second procedural driver on an always_ff
register; a test-only force/release fixture fixed compilation without changing
production or suppressing diagnostics.

`pwsh -File out/build/m6-session-fit.ps1` generated a registered local probe
in `out/research/m6-session-timing-r1` and ran Quartus 25.1std.0 Build 1129
map/fit/sta for 5CEBA4F23C8, seed 1, 81.380 ns. The probe has 571 registered input
bits and 1,040 observation bits; forwarded MCLK is not treated as data. It uses
5,102 ALMs, 5,687 registers, 72,168 memory bits, 27 M10Ks and three DSPs.
An initial probe assertion incorrectly totaled the input width as 671; the
actual port sum is 571. Generation was rerun in a fresh directory because the
pinned native generator rejects an existing output. No production or timing
constraint was changed to address these preparation failures.

| Timing model | Setup | Hold | Recovery | Removal | Pulse width |
| --- | ---: | ---: | ---: | ---: | ---: |
| Slow 85 C | 12.331 | 0.339 | 30.924 | 2.025 | 39.084 |
| Slow 0 C | 10.232 | 0.325 | 31.423 | 1.938 | 39.001 |
| Fast 85 C | 32.488 | 0.159 | 35.122 | 0.842 | 39.472 |
| Fast 0 C | 33.248 | 0.106 | 35.774 | 0.770 | 39.464 |

All slacks are ns; every TNS is zero and setup/hold have no unconstrained paths.
Map's nine warnings concern unused write nets in initialized upstream ROMs.
Fit's three warnings concern LogicLock, incomplete I/O and the five unassigned
probe pins; they are reviewed local-fixture limitations. STA reports zero
errors/warnings. No timing exception, relaxed bound or warning suppression was
added. Logs: `out/build/m6-session-{map,fit,sta}.log`.

`python -B tools/check_harness.py` and
`python -B out/build/m6-session-checks.py` pass; the latter checks all eight
synthesis input hashes, changed local Markdown links and the timing summaries.
`pwsh -File tools/host-verify.ps1` passes all 77 tests in 467.68 seconds,
including format (0.47 s), tidy (451.43 s), harness and positive/negative
architecture checks. The dependency allowlist adds only the new local RTL file;
Core/UI separation guards remain enabled. Log: `out/build/m6-session-host.log`.
Whole-Pocket fit/CDC, cross-build, APF/package and hardware were not run because
this unit has no CPU/board integration. Unchanged standalone envelope arithmetic
and output-only benches were not repeated; affected native integration ran.
Next is the approved CPU MMIO/CDC layer with concrete word layout and derived
ACK deadlines, followed by retained MDX feeding and bounded output history.

## Slice 4 sound MMIO execution — 2026-09-13 / 2026-09-20

The new CPU-local block at `0x40000400` transports control, feed and capture
through independent copied mailboxes. Staging may change while an older copy
is outstanding; response words remain fixed until explicit release. The
session remains the semantic validator. Feed Full returns a completed result
without occupying control; retry requires a new explicit submit. Epochs are
checked without resetting a borrowed toggle. A reset originating at either
platform domain asserts common reset, followed by independent synchronized
release. The emergency path acknowledges delivery and retains one coalesced
follow-up, so a later short write cannot disappear behind an earlier ACK.
Journal capability is reserved, not advertised by this implementation.

Executed on 2026-09-13:

- `pwsh -File tools/rtl-sound-mmio-verify.ps1`: PASS. The generic 129-bit request /
  257-bit response bench completes 64 requests and responses with destination
  and source backpressure. The native MMIO integration passes initial audio
  phase offsets 0, 173, 5,555, 40,690 and 81,379 ps. Each completes 13 control,
  70 feed and 29 capture transactions and 18 reset-stage cases. Actual nonzero
  serial-bit counts are 3,081 except at 5,555 and 81,379 ps, which give 3,080.
  All positive simulations report zero errors/warnings.
  Log: `out/build/m6-mmio-rtl.log`.
- The independent test software checks every register access direction,
  reserved/partial/misaligned/out-of-range accesses, complete feed payloads at
  the session boundary, 28 authored native tone writes, queue Full/retry,
  coherent capture and retained responses during live policy/playback changes.
  Reset covers six transfer stages in each mailbox, alternating the originating
  domain. Emergency follow-up, failed Reset/recovery, actual starvation and
  natural end are exercised. Completion latency is checked at the first CPU
  visibility edge rather than at the later software poll.
- `pwsh -File out/build/m6-mmio-negative.ps1`: five expected mutants rejected:
  live rather than held requests, live capture-frame words, replacing offered
  epochs with the current epoch, dropped emergency follow-up, and ignoring the
  audio reset input. Each produces its targeted failure, one error, zero
  warnings and no PASS marker. A strengthened full-payload oracle caught the
  live-request mutant earlier than the original helper expected; only that
  expected diagnostic was corrected. Production files were not mutated.
- `pwsh -File tools/host-verify.ps1`: 77/77 PASS, 458.98 seconds, including
  format (0.46 s), tidy (443.19 s) and architecture rejection fixtures.
  Log: `out/build/m6-mmio-host.log`.

The initial seed-1 Auto Fit at 90 MHz CPU /
12.288 MHz audio had Fast/0 C hold slack -0.006 ns. A second fit imposed an
additional 0.100 ns minimum delay on the synchronous receipt-token-to-completion
RAM interface; it still missed that stronger requirement (worst -0.076 ns).
Neither initial fit is recorded as timing acceptance. No clock was slowed,
path hidden or RTL failure converted to success.

On 2026-09-20, `pwsh -File out/build/m6-mmio-fit-r2.ps1` reran the same RTL,
seed and constraints with Standard Fit / Extra physical synthesis effort.
Quartus 25.1std.0 Build 1129 passes all 40 timing summaries, including the
additional 0.100 ns receipt-interface hold requirement. The registered local
probe (`out/research/m6-mmio-timing-r2`) uses 6,215 ALMs, 10,288 registers,
72,168 memory bits, 27 M10Ks and three DSPs on 5CEBA4F23C8. Its CPU command
shift register is 70 bits and observation register 34 bits; audio fault and
two observed audio bits are registered. Nine fixture pins have no Pocket pin
assignment; forwarded MCLK is not treated as data.

| Timing model / clock | Setup | Hold | Recovery | Removal | Pulse width |
| --- | ---: | ---: | ---: | ---: | ---: |
| Slow 85 C / CPU | 1.517 | 0.371 | 6.294 | 1.044 | 4.784 |
| Slow 85 C / audio | 14.353 | 0.285 | 68.058 | 3.152 | 39.068 |
| Slow 0 C / CPU | 1.399 | 0.359 | 6.539 | 0.969 | 4.637 |
| Slow 0 C / audio | 11.910 | 0.277 | 68.504 | 2.998 | 38.976 |
| Fast 85 C / CPU | 6.393 | 0.162 | 8.611 | 0.392 | 4.964 |
| Fast 85 C / audio | 52.481 | 0.133 | 74.698 | 1.321 | 39.476 |
| Fast 0 C / CPU | 6.703 | 0.132 | 8.918 | 0.355 | 4.962 |
| Fast 0 C / audio | 53.966 | 0.103 | 75.483 | 1.172 | 39.467 |

All slacks are ns, every TNS is zero, and setup/hold have no unconstrained
paths. Map's nine warnings are unused write nets in initialized upstream ROMs.
Fit's three diagnostics concern LogicLock licensing, incomplete I/O and the
nine unassigned fixture pins; these are local-probe limitations. STA reports
zero errors/warnings. Logs: `out/build/m6-mmio-r2-{map,fit,sta}.log`.

From that fitted project, `quartus_sta -t ../../build/m6-mmio-cdc-audit.tcl`
passes all four corners with zero errors/warnings. It checks every optimized
bundle destination (694 request and 1,421 response bits); a separate complete
clock-to-clock query has exactly those same endpoint counts. Maximum request
data delay is 3.898 ns against 81.380 ns; maximum response delay is 4.738 ns
against 11.111 ns. Minimum bundle setup slack is 5.802 ns. All 13 explicit
first-to-second synchronizer pairs have positive setup/hold. The exception
reports preserve bundle setup and second-stage timing, with only justified
first-stage/reset exceptions and bundle hold exceptions. No blanket clock
group exception is used. The tool recognizes all 13 chains; its inferred
inhibit-status chain extends into the fixture's observation shift register,
so the reported MTBF is not promoted to a board reliability guarantee.
Log: `out/build/m6-mmio-r2-cdc-audit.log`; endpoint evidence:
`out/build/m6-mmio-cdc-paths.txt`.

`python -B out/build/m6-mmio-checks.py` passes ten source/probe hashes, changed
Markdown links, all 40 timing summaries, zero unconstrained setup/hold paths,
CDC coverage and expected warning counts. The contract records the derived
CPU-visible edge bounds; a watchdog value has not been adopted.

The host changed during the interruption: Scoop's current LLVM became 23.1.1
while 22.1.8 remained installed. The initial setup correctly failed the pin;
process-local PATH selected 22.1.8 and setup passed. The first full configure
still selected Scoop's current path via the cached/CMake hint, so the local
build cache was given explicit 22.1.8 format/tidy paths. No repository pin,
global tool selection or quality gate was relaxed. With those paths,
`pwsh -File out/build/m6-mmio-host-sep20.ps1` configures then executes
`pwsh -File tools/host-verify.ps1`: all 77 tests PASS in 586.18 seconds,
including format (1.68 s), tidy (556.76 s), harness, incremental rebuild and
architecture checks. Log: `out/build/m6-mmio-host-sep20.log`.

`pwsh -File tools/rtl-verify.ps1`, then
`pwsh -File tools/rtl-jt51-verify.ps1`, pass on 2026-09-20 with nine and five
benches respectively, each reporting zero errors/warnings. They were run
sequentially by `out/build/m6-mmio-rtl-regressions.ps1`; logs are
`out/build/m6-mmio-sep20-{rtl-verify,rtl-jt51-verify}.log`. The unchanged
standalone media/envelope suites were not repeated; their prior session
regression evidence is above, and the new MMIO bench exercises the complete
native/session path. Final link/harness/hash checks and `git diff --check` pass.

Whole-Pocket decode, cross-build, board timing, APF/package and firmware 2.6
hardware checks were not run: this block is not yet connected to the CPU/board.
Retained MDX production, bounded output/per-event history and sound/storage
adapters remain before integration. The watchdog proposal is pending adoption;
neither the transfer bound nor the local fit is hardware acceptance.

## Slice 4 mailbox watchdog adoption — 2026-09-20

The user approved the concrete 1 ms proposal and asked to continue. The MMIO
contract now fixes 1,000 us from successful submit for control/feed/capture,
completion-before-expiry priority, and retained ownership after timeout.
The CPU adapter must test these rules; approval does not claim implementation,
real CPU service timing or hardware acceptance. This resolves the pending
adoption recorded in the preceding execution entry.

## Slice 4 retained MDX producer execution — 2026-09-20

The [retained producer contract](../../specs/mdx-source-producer-v1.md) was
written before implementation within the approved CPU connection plan. Core
now copies one validated batch of up to 8,192 register writes plus its marker,
retains each offer through Full/retry, and advances only on Accepted. A fresh
confirmed sound epoch discards old CPU retention; late old replies cannot
complete a new offer. Cancelling this owner does not establish device silence.
The engine helper uses 12,288,000 scheduler ticks per second, commits a
candidate state and copied display checkpoint only after retention succeeds,
and performs no engine work while audio remains pending. MMIO access and
display retention are not implemented by this unit.

Executed checks:

- `pwsh -File out/build/m6-producer-focused.ps1`: the real CMake producer
  target builds and `ctest --preset host-msvc -R '^mdx_source_producer$'`
  passes 1/1 (0.05 s test, 0.11 s total). The authored engine trace checks
  17 ticks against the independent interval 22,020,096/125 audio ticks.
  Every item of the 8,192-write fixture survives three Full retries with
  identical copied fields, including its marker. Invalid late writes,
  zero-write/end, identity/failure handling, cancellation and u64 limits pass.
- `pwsh -File tools/host-verify.ps1`, with process PATH and cached CMake tool
  paths selecting the unchanged LLVM 22.1.8 pin: 78/78 PASS in 499.34 s,
  including format (0.52 s), tidy (481.17 s), architecture rejection fixtures
  and incremental rebuild. Log: `out/build/m6-producer-host.log`.
- `out/build/host-msvc/rpcmp_mdx_source_producer_tests.exe`: PASS; host sizes
  are 16,464 bytes retained audio, 1,073,416 bytes workspace and 3,192 bytes
  copied display checkpoint.
- After the user restarted Docker, server 29.7.2/linux and the existing
  `rpcmp-openfpgaos-toolchain:14.2.0-3` image were available. The command below
  compiles and links the producer plus real MDX engine using GCC 14.2.0,
  `rv32imafc/ilp32f`, the pinned SDK `a408ddc12aed0dfaa4aa22c06af82f829db77126`,
  static BSS workspace and authored commands. ELF32 little-endian RISC-V,
  entry 0x10400000 and no undefined symbols were verified.

```powershell
docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 make --file out/build/m6-producer-cross/Makefile producer-check
```

The link probe has text 17,652, data 200 and BSS 932,256 bytes, total 950,108,
within the existing 56,623,104-byte static and 4,096-byte initialized-data
limits. `elf_budget.py` passes: 35 compiler stack-usage records sum to 9,456
bytes, largest frame 7,264, no dynamic frames, against the 524,288-byte limit.
This sum covers the compiled C++ records, not unreported SDK/library frames
or a measured device stack high-water mark. `riscv-none-elf-nm --print-size
--size-sort --demangle` confirms target retained audio 16,464, workspace
895,224 and checkpoint 3,192 bytes. Producer's largest own frame is 80 bytes;
the caller must keep its large workspace outside the stack. Logs and budget:
`out/build/m6-producer-cross.log`, `out/build/m6-producer-cross/budget.json`.

`python -B tools/check_harness.py`, changed Markdown link checks and
`git diff --check` pass. No RTL input changed in this C++ unit, so RTL/fit were
not repeated. This link probe was not executed on Pocket and does not establish
CPU service speed, mailbox deadlines or combined player/font/framebuffer memory
acceptance. Whole-Pocket build/package and firmware 2.6 tests remain pending.
Next are the separately bounded 16-batch display retention, 32-record output
journal/per-event mapping and sound/storage adapters before integration.

## Slice 4 output journal execution — 2026-09-20

The [output-journal contract](../../specs/pocket-output-journal-v1.md) implements
the approved CPU connection plan's 32-record display retention. The native
owner now copies the actual consumed prefix and zero-based output frame, and
separately retains the pending checkpoint applied at NaturalEnd/RepeatOne.
The latter's PCM is not consumed. Pause, unchanged prefixes, Stop and fault
do not invent output records. A bounded audio-clock RAM FIFO never returns
backpressure to audio. Full loses the arriving display record, records a
saturating loss count and preserves an independent latest copy. Sequence
exhaustion preserves audio and never wraps.

MMIO minor 1 adds the previously reserved journal capability and a fourth
copied request/response mailbox. Peek does not pop; Pop requires the expected
epoch and exact head sequence. A normal Reset clears journal ownership but
leaves borrowed CPU responses intact. Late old-epoch Pops cannot remove a
new epoch's head even when its sequence is equal. Public v1 playback and the
existing MMIO control/feed/capture words retain their meanings.

Executed RTL checks:

- `pwsh -File tools/rtl-enveloped-audio-verify.ps1`: all four benches PASS,
  zero errors/warnings. The independent native/sample/boundary oracle checks
  34 output records, including six natural-end records, alongside 316 frames,
  310 samples, 182 nonzero frames, six starts/ends, fade/restoration and
  fault recovery. The source queue still passes 8,192 writes/8,195 receipts;
  the session passes 256 phases and 1,585 responses. The unchanged enveloped
  compatibility path passes its serialized PCM/gain oracle.
  Log: `out/build/m6-journal-native.log`.
- `pwsh -File tools/rtl-sound-mmio-verify.ps1`: FIFO boundary/loss/sequence
  tests, the 64-request generic mailbox test, and independent-clock phases
  0, 173, 5,555, 40,690 and 81,379 ps PASS with zero errors/warnings. Each
  phase checks 17 controls, 198 feeds, 35 captures, 2,371 journal queries and
  24 either-origin common-reset transfer stages. A 64-marker real native
  stream produces identical output record frames/prefixes/end flags with no
  reader (32 dropped records) and frequent Pops (zero drops). It reaches the
  same natural end; latest survives overflow. Full-width fields are checked
  with explicitly injected observation values, not a claimed long device run.
  Log: `out/build/m6-journal-mmio-final.log`.
- `pwsh -File out/build/m6-journal-negative.ps1`: four generated mutants are
  rejected by their expected assertions: next-frame timestamp, natural end
  from the cleared output rather than applied pending checkpoint, old-epoch
  Pop, and live rather than copied head data. Each has one expected error,
  zero warnings and no PASS. Production sources were not mutated.
- `pwsh -File tools/rtl-verify.ps1`, then
  `pwsh -File tools/rtl-jt51-verify.ps1`: nine and five benches PASS with zero
  errors/warnings. These and the final MMIO/negative checks run sequentially
  in `out/build/m6-journal-regressions.ps1`; logs use
  `out/build/m6-journal-{base,jt51,negative}.log`.

`pwsh -File out/build/m6-journal-fit.ps1` uses Quartus 25.1std.0 Build 1129,
5CEBA4F23C8, seed 1, Standard Fit / Extra physical synthesis and the preceding
probe's 90 MHz CPU / 12.288 MHz audio constraints, including its stronger
0.100 ns receipt-to-RAM hold requirement. All 40 timing summaries have positive
slack and zero TNS. There are no unconstrained input/output paths or clocks.

| Timing model / clock | Setup | Hold | Recovery | Removal | Pulse width |
| --- | ---: | ---: | ---: | ---: | ---: |
| Slow 85 C / CPU | 1.460 | 0.378 | 3.199 | 4.353 | 4.750 |
| Slow 85 C / audio | 13.655 | 0.258 | 65.460 | 2.014 | 39.083 |
| Slow 0 C / CPU | 1.495 | 0.373 | 3.255 | 4.280 | 4.597 |
| Slow 0 C / audio | 11.275 | 0.245 | 66.202 | 1.897 | 38.997 |
| Fast 85 C / CPU | 6.005 | 0.164 | 7.568 | 2.260 | 4.962 |
| Fast 85 C / audio | 51.874 | 0.131 | 73.072 | 0.815 | 39.475 |
| Fast 0 C / CPU | 6.390 | 0.149 | 7.756 | 2.127 | 4.962 |
| Fast 0 C / audio | 53.433 | 0.104 | 74.058 | 0.732 | 39.465 |

All slacks are ns. The registered local probe uses 7,855 ALMs, 14,690 registers,
80,392 memory bits, 34 M10Ks and three DSPs. Its nine fixture pins are not Pocket
pin assignments. Map has the same nine unused upstream ROM write-net warnings;
fit has three license/incomplete-fixture-pin diagnostics; STA has zero warnings.
The CDC audit covers every optimized destination at four corners: 887 request
and 2,267 response endpoints, exactly matching the complete clock-to-clock
queries. All 15 explicit synchronizer pairs have positive setup/hold. Maximum
bundle delays are 5.291 ns request and 4.909 ns response, with minimum setup
slack 6.103 ns. No blanket clock-group exception is used. Logs are
`out/build/m6-journal-{map,fit,sta,cdc-audit}.log`; endpoint evidence is
`out/build/m6-journal-cdc-paths.txt`. `python -B out/build/m6-journal-evidence.py`
passes all 11 source/probe hashes, summary/endpoint coverage and warning counts.

`pwsh -File tools/host-verify.ps1` with the pinned LLVM 22.1.8 process/cache
paths passes 78/78 in 519.11 s, including format (0.85 s), tidy (497.36 s),
architecture rejection fixtures and incremental rebuild. Log:
`out/build/m6-journal-host.log`. The harness, changed Markdown link checks and
`git diff --check` pass after evidence/navigation updates. No warning suppression,
relaxed bound, regenerated audio golden or global tool change was introduced.

Whole-Pocket decoder/build/package, CPU journal service and performance-history
publication, combined player memory, and firmware 2.6 hardware tests remain
pending. The fit is a registered local probe, not board timing acceptance.
Next is the separately bounded 16-batch CPU display retention and per-event
mapping, followed by sound/storage adapters and Pocket integration.

## Slice 4 CPU output-history execution — 2026-09-20

The [MDX output-history contract](../../specs/mdx-output-history-v1.md) connects
the retained producer and output records within the approved CPU plan.
`MdxOutputHistory` copies 16 display batches independently of audio ownership.
It preserves individual first-output frames until a whole checkpoint completes,
then publishes only against a matching proven sound observation. Partial or
unseen prefixes remain Waiting. Natural end uses the unchanged terminal frame;
an idle/paused observation adds no events. The eventual sound adapter supplies
the coherent observation and MMIO journal service; this owner never reads a
device or executes a playback command.

Known omissions consume sequence numbers at their source positions through
additive internal per-event/trailing loss inputs. Earlier known timestamps
survive missing middle records. FIFO overflow discards old display batches,
and independent latest records can restore complete current channels without
guessing omitted times. Malformed display input cannot stop audio. Public
snapshot/event schemas and Q1–Q24 remain unchanged.

A focused failing case reproduced future source-gap loss appearing on an
earlier intact checkpoint. The correction retains that gap with its following
batch until its actual output; the regression checks both sides of the gap.
A second failing case showed repeated zero-sequence latest copies after journal
exhaustion discarding a pending checkpoint. Such repeated/older copies are now
inert by their monotonic frame; their later completion retains known event times.

Executed checks:

- `pwsh -File out/build/m6-history-focused.ps1` builds the three affected
  targets and passes `mdx_output_history`, `performance_history` and
  `mdx_performance`. Authored records cover distinct/equal frames, several
  batches on one frame, zero-write/end, deferred publication, middle loss,
  latest/exhausted-journal recovery, 16-batch overflow, stale/malformed data
  and u64 boundaries. Real authored MDX supplies identical 320-offer traces
  (160 accepted items plus Full retries) across dense/sparse/absent reads and
  delayed display processing. Log: `out/build/m6-history-focused.log`.
- `out/build/host-msvc/rpcmp_mdx_output_history_tests.exe`: PASS; the host
  output-history owner occupies 113,312 bytes.
- `docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 python3 out/build/m6-output-history-sanitize.py`:
  PASS for output history, committed history and retained producer using native
  GCC 13.3.0 with AddressSanitizer/UndefinedBehaviorSanitizer, leak detection and
  halt-on-error. Log: `out/build/m6-output-history-sanitize.log`.
- The Docker RISC-V compile/link probe below passes with GCC 14.2.0,
  C++17, `rv32imafc/ilp32f`, `-Werror`, no exceptions/RTTI and the pinned SDK.
  It includes the real MDX engine/producer, mapper, collector, output-history
  owner and public snapshot validation, with no UI or device adapter.

```powershell
docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 make --file out/build/m6-output-history-cross/Makefile history-check
```

The resulting ELF32 little-endian RISC-V image has no undefined symbols:
text 27,260 bytes, data 204, BSS 1,054,392, total 1,081,856. Its 62 compiled
stack records total 27,312 bytes; the largest is `PerformanceHistory::begin`
at 8,496 bytes, with no dynamic frames. Limits remain 56,623,104 static bytes,
4,096 initialized-data bytes and 524,288 stack bytes. ELF symbol sizes confirm
the target history owner is 113,288 bytes, retained source 16,464, producer
workspace 895,224 and public history snapshot 8,344. Large owners use persistent
storage in this probe. These figures do not yet include the complete player,
framebuffer/font allocation or a target execution measurement. Evidence:
`out/build/m6-output-history-cross.log` and its directory's `budget.json`.

`pwsh -File tools/host-verify.ps1` with LLVM 22.1.8 passes 79/79 in 450.69 s,
including format (0.49 s), tidy (434.18 s), architecture rejection fixtures
and incremental rebuild. Log: `out/build/m6-output-history-host-final.log`.
Earlier tidy findings were corrected; the final loss span is a plain aggregate
so omitted initialization is accepted by GCC as well as MSVC, with no warning
suppression. The final sanitizer/cross probes include these corrections.
The harness, changed Markdown links (86 targets) and `git diff --check` pass.

No RTL, device protocol or package inputs changed in this CPU-only unit.
RTL simulation/synthesis and firmware 2.6 hardware tests were not rerun;
the preceding journal evidence remains separate. CPU sound/storage adapters,
whole-Pocket integration and hardware acceptance remain pending.

## Slice 4 CPU sound-client execution — 2026-09-20

The [CPU client contract](../../specs/pocket-sound-client-v1.md) implements the
existing four-mailbox MMIO protocol and approved 1,000-us critical watchdog.
`SoundMmioClient` injects MMIO and a monotonic clock, copies requests/results,
validates response words and retains borrowed hardware ownership after failure.
Full is returned to the retained MDX producer without an implicit retry. Normal
Reset does not cancel old feed/capture/journal requests. Their responses keep
their original identity until drained; unread CPU results also block reuse.

A matching completion wins at the deadline. Timeout never becomes late success,
and clock reversal inhibits sound without starting a new timer epoch. Journal
acquisition has a separate 1,000-us display deadline: its timeout or malformed
response leaves audio/control/feed/capture available. This internal display
service choice does not change the approved audio deadline or public schemas.

A focused failure reproduced a copied Reset success clearing the client fault
after a newer CPU INHIBIT. Recovery is now allowed only when no INHIBIT followed
that Reset's submission. The immutable earlier wire success remains a past
response; it cannot clear the newer fault. Reproduction log:
`out/build/m6-sound-client-inhibit-repro.log`.

Executed focused checks:

- `pwsh -File out/build/m6-sound-client-focused.ps1`: builds and passes
  `sound_mmio_client`. Independently authored word transcripts check high
  halves, copied ownership, slot independence, Full/retry, obsolete epochs,
  malformed echoes/fields, deadline boundaries, late responses, clock reversal
  and u64/sequence limits. The host client occupies 552 bytes. Log:
  `out/build/m6-sound-client-focused.log`.
- LLVM 22.1.8 `clang-tidy -p out/build/host-msvc --config-file=.clang-tidy`
  on the client source and test: PASS. The invalid-enum fixture uses the same
  byte-copy corruption technique as existing transport tests, retaining the
  rejection assertion without adding a warning suppression.
- `docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 python3 out/build/m6-sound-client-sanitize.py`:
  PASS with native GCC 13.3.0, AddressSanitizer/UndefinedBehaviorSanitizer,
  leak detection and halt-on-error. Log:
  `out/build/m6-sound-client-sanitize.log`.

```powershell
docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 make --file out/build/m6-sound-client-cross/Makefile client-check
```

This RISC-V compile/link probe passes with GCC 14.2.0, C++17,
`rv32imafc/ilp32f`, `-Werror`, no exceptions/RTTI and the pinned SDK. It links the
client's four channels with the volatile MMIO adapter and a placeholder clock;
it is not an executable Pocket service/clock integration. The ELF32
little-endian image has no undefined symbols: text 11,452, data 200 and BSS
2,344 bytes, total 13,996. All 51 compiled stack-usage records sum to 1,136
bytes; the largest frame is probe `main` at 304 bytes, with no dynamic frames.
Limits remain 56,623,104 static, 4,096 initialized-data and 524,288 stack bytes.
The sum excludes unreported SDK/library frames and is not a device high-water
measurement. Target persistent client storage is 544 bytes (ELF symbol 0x220).
Logs, symbols and budget: `out/build/m6-sound-client-cross.log` and its
directory's `symbols.txt` / `budget.json`.

`pwsh -File tools/host-verify.ps1` with pinned LLVM 22.1.8 passes 80/80 in
623.06 s, including format (0.53 s), tidy (605.89 s), architecture rejection
fixtures and incremental rebuild. Log: `out/build/m6-sound-client-host.log`.
`python -B tools/check_harness.py`, the changed Markdown link check (82 targets)
and `git diff --check` pass after the evidence/navigation updates.

No RTL/protocol/package inputs changed in this CPU-only unit; RTL/fit and
firmware 2.6 hardware checks were not repeated. The enclosing AudioTransportPort
backend, real producer/history service, storage adapter, combined memory and
whole-Pocket integration remain pending. Client compile/link and scripted
clock checks do not establish a measured Pocket service deadline or playback.

## Slice 4 recovery Reset integration correction — 2026-09-20

Connecting the real sound client exposed a Core control-loop error: each
repeated observation of a latched device fault sent another INHIBIT during
the recovery Reset. The sound session correctly treats a newer emergency as
superseding that Reset, so polling the old fault could make recovery fail.
A focused scripted port with that documented Reset/emergency rule reproduced
seven failed assertions before the correction. The controller now preserves
the existing latched inhibit while the same fault's recovery Reset is pending.
New fault edges, control timeouts, terminal failure and invalid Reset ACKs keep
their explicit inhibit paths. This restores the adopted recovery semantics;
it does not weaken the sound session or change its hardware protocol.

`pwsh -File out/build/m6-reset-recovery-focused.ps1` passes
`playback_transport`, including persistent-fault recovery, a new fault while
Reset is pending and recovery timeout. The reproduction and passing logs are
`out/build/m6-reset-recovery-{repro,focused}.log`.
The Docker `m6-reset-recovery-sanitize.py` run passes the controller test with
GCC 13.3.0, AddressSanitizer/UndefinedBehaviorSanitizer, leak detection and
halt-on-error. The existing `m6-settings-cross/Makefile settings-check` probe
rebuilds the changed controller with RISC-V GCC 14.2.0 and passes link/budget
checks: text 65,504, data 188, BSS 904,176 bytes, total 969,868; 185 compiled
stack records total 131,024, largest probe-main frame 60,976, no dynamic frames.
Limits remain 56,623,104 static / 4,096 data / 524,288 stack bytes. Logs:
`out/build/m6-reset-recovery-{sanitize,cross}.log`. This is a compile/link probe,
not execution of the Pocket backend or a measured device stack high-water mark.

`pwsh -File tools/host-verify.ps1` runs all 80 gates: 79 pass, including tidy
(483.57 s), architecture and incremental rebuild; only format fails because
the initial formatting command was denied by the sandbox. After applying the
pinned formatter with approved access, `ctest --preset host-msvc --rerun-failed
--output-on-failure` passes format. No warning suppression or test relaxation
was added. Logs: `out/build/m6-reset-recovery-{host,rerun}.log`.
RTL/fit and firmware checks were not rerun for this Core-only correction.
The separately in-progress MDX backend is not covered by this correction's
completion evidence; its integration tests and target probes remain pending.

## Slice 4 CPU MDX backend integration — 2026-09-20

The internal [Pocket MDX backend](../../specs/pocket-mdx-backend-v1.md) now
joins the real MDX preparation/producer, sound MMIO client and output-history
owner through PreparationPort, AudioTransportPort and PerformanceReader.
It checks catalog generation before borrowing immutable library data and
shares the large engine scratch between admission and playback. The original
MDX preparation helper remains source-compatible. No UI or renderer dependency
is added, and public reads do no MMIO work.

Controls retain ownership while their mailbox is busy. Captures submitted
before a control ACK cannot restore an older position, pause state or policy.
Finite-source prefill and a Full response both permit Start; the first tick
need not fit in the 64-entry queue. Cancel/reselection retain old mailbox
ownership through drain. A feed Closed is resolved against a subsequently
submitted capture, so a natural end is distinct from unexpected closure.

Integration exposed three behavioral failures, reproduced before correction:

- Waiting history lacked the required channel IDs. It now copies the existing
  unknown-channel representation.
- Repeated captures of one held pause invalidated the journal drain proof;
  a journal timeout could also republish earlier data as available. Pause proof
  now belongs to the acknowledged control, and acquisition failure persists
  until a valid journal result. Log: `out/build/m6-backend-repro.log`.
- A failed control completion changes the Core error from DeviceFault to
  AudioControl. The same latched fault was then mistaken for a new fault,
  inhibiting/cancelling the recovery Reset. Only a fresh fault edge now starts
  another recovery. Core regression reproduced six failures; the backend
  reproduced failed recovery on the control mailbox. Logs:
  `out/build/m6-backend-controller-repro.log` and
  `out/build/m6-backend-fault-repro.log`.

`pwsh -File out/build/m6-backend-focused.ps1` passes `mdx_backend`, and
`pwsh -File out/build/m6-reset-recovery-focused.ps1` passes the updated
`playback_transport`. Logs: `out/build/m6-backend-focused.log` and
`out/build/m6-backend-controller.log`. Authored finite/looping MDX and a copied
MMIO model exercise actual TransportController and PlayerSession. Cases cover
partial first ticks, verbatim Full retry, all controls, stale captures,
cancel/reselection, old epochs, control/feed/capture timeouts and recovery,
terminal Closed races, held-pause frames, journal-only failure and equal feed
offers with absent/dense publication. The MMIO model is scripted independent
stimulus, not an RTL simulator. The terminal fixture sets gain zero as required
by the adopted media contract; its earlier nonzero-gain variant was correctly
rejected as Protocol. Host backend persistent storage is 1,235,488 bytes.

```powershell
docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 python3 out/build/m6-backend-sanitize.py
docker run --rm --mount type=bind,source=F:\source\rpcmp,target=/repo --workdir /repo rpcmp-openfpgaos-toolchain:14.2.0-3 make --file out/build/m6-backend-cross/Makefile backend-check
```

Both final-source Docker checks pass. Native GCC 13.3.0 runs the backend tests
with AddressSanitizer/UndefinedBehaviorSanitizer, leak detection and
halt-on-error. RISC-V GCC 14.2.0 compiles/links the backend and PlayerSession
with the pinned SDK, `rv32imafc/ilp32f`, C++17, `-Werror` and no exceptions/RTTI.
The ELF32 little-endian probe has no undefined symbols: text 84,616, data 200,
BSS 1,077,024 bytes, total 1,161,840. All 296 compiled stack records sum to
107,904 bytes; the largest is probe main at 11,104, with no dynamic frames.
Limits remain 56,623,104 static / 4,096 initialized-data / 524,288 stack bytes.
This sum excludes unreported SDK/library frames and is not measured high-water
usage. The probe uses a placeholder clock and no actual storage/UI loop.
Logs: `out/build/m6-backend-{sanitize,cross}-final.log`; budget:
`out/build/m6-backend-cross/budget.json`.

`pwsh -File tools/host-verify.ps1` with pinned LLVM 22.1.8 passes 81/81 in
501.26 s, including format (0.75 s), tidy (483.50 s), the Core recovery
regression, architecture rejection fixtures and incremental rebuild. Log:
`out/build/m6-backend-host.log`. After documentation updates,
`python -B tools/check_harness.py`, `python -B out/build/m6-backend-links.py`
(90 local file links) and `git diff --check` pass.

Focused LLVM 22.1.8 analysis found declaration-name mismatches, duplicate
prefill branches and a test integer-widening expression. These were corrected
without suppressions; `m6-backend-tidy-rerun.log` records the passing source/test
rerun. RTL, synthesis/fit, packaging and firmware checks were not repeated:
this unit changes CPU C++ and its internal connection contract, not RTL/MMIO
definitions or package inputs. APF settings storage, a real target clock and
service cadence, whole-Pocket memory/layout and firmware 2.6 acceptance remain
pending. This completes the scripted CPU backend unit, not M6 slice 4 or Q19.

### APF flush command transport (2026-09-20)

Slice 4 adopts [APF flush transport v1](../../specs/pocket-apf-flush-v1.md).
The opt-in `apf-flush.patch` adds CPU command 5, a versioned capability word,
copied request ownership and the official APF `0x0188` command. It connects
through the real Pocket CDC, parameter latch and completion/drain tracker.
It does not yet implement the atomic save RAM or OS storage service.

The integration test reproduced three existing transport defects: a delayed
first AXI W beat used AW+4, GETFILE did not latch its own bridge parameters,
and the busy guard could overwrite a pending request. Authored AXI/APF
transactions now cover these paths, including a simultaneous first AW/W and
a second INCR beat. Independently restoring each old behavior fails the
corresponding check (`out/build/m6-apf-negative-{first-address,busy-overwrite,
getfile-payload}.log`). An additional `0xFFF8` Host result was truncated to
success; `m6-apf-error-repro.log` records the failure. Flush now maps unknown
results above 7 to error 7. Existing file-command result encodings are retained.
Shared peripheral declarations were moved before first use for Questa, and
the unused BRAM port's constant was corrected to its declared width.

- `tools/pocket_m5_audio_prepare.py` with the pinned source/netlist/ROM and
  `--apf-lifecycle --apf-flush` creates a new isolated tree. The original M5
  invocation remains unchanged. Final-source tree: `out/build/m6-apf-flush-final`.
- `pwsh -File tools/rtl-apf-flush-verify.ps1 -PreparedTree out/build/m6-apf-flush`:
  PASS at 0, 173, 5,555 and 11,110 ps phase offsets of 90 MHz/74.25 MHz clocks,
  plus disabled-capability PASS, all with zero errors/warnings. Scenarios cover
  repeated flush, explicit/unknown errors, copied identity, busy rejection,
  stale DONE, delayed drain, Host responsiveness and reset/late result. The
  fixture compiles actual peripheral/handler code and extracts actual top-level
  CDC/drain wiring; only the unused PSX controller and datatable memory are
  modeled. Log: `out/build/m6-apf-flush-rtl-final.log`.
- `pwsh -File tools/rtl-apf-verify.ps1` passes both the new tree with `-Flush`
  and the preserved `openfpgaos-m5-apf-primer-final` tree without it. Logs:
  `m6-apf-lifecycle-{final,legacy}.log`. The sequential `rtl-verify.ps1` and
  `rtl-jt51-verify.ps1` suites pass; logs: `m6-apf-{rtl,jt51}-regression.log`.
- `pwsh -File tools/host-verify.ps1` with LLVM 22.1.8: 81/81 PASS, 541.77 s,
  including format, tidy and architecture checks (`m6-apf-flush-host.log`).
  Focused `m5_audio_boot_tests.py` (3 tests), `firmware_pair_tests.py` (7 tests)
  and `check_harness.py` also pass after the tooling changes.
- Docker 29.7.2 serves the pinned `rpcmp-openfpgaos-toolchain:14.2.0-3` image.
  RISC-V GCC 14.2.0 compiles `out/build/m6-apf-header.c` against the patched
  firmware headers with `-std=c11 -O2 -Wall -Wextra -Werror -march=rv32imafc
  -mabi=ilp32f`. This checks the new register definitions, not an OS save service.

`pwsh -File out/build/m6-apf-fit-final.ps1` completes Quartus 25.1std.0 Build
1129 map/fit/STA on the final prepared tree, 5CEBA4F23C8, seed 1 and four fitter
processors. Usage is 13,910/18,480 ALMs, 20,284 registers, 1,184,426 memory bits,
171 RAM blocks and 13 DSP blocks. `python -B out/build/m6-apf-timing-audit.py`
checks all 136 four-corner summaries: TNS is zero throughout; minimum setup /
hold / recovery / removal / pulse-width slack is 0.451 / 0.101 / 3.777 / 0.338 /
0.555 ns. Logs: `m6-apf-flush-final-{map,fit,sta}.log`; parsed results:
`m6-apf-flush-timing.json`. The final sources are identical after LF normalization
to the simulated tree and a second clean preparation (`m6-apf-flush-repro`).
`m6-apf-final-verify.py` verifies that identity, 93 documentation links and
rejection of `--apf-flush` without lifecycle before output creation.

These numbers describe the M5-derived substrate with its old Queue-v1 audio,
not the combined M6 sound/UI. No new clock cuts or warning suppressions were
added. Map reports 1,089 warnings, fit 13 and STA one; fit diagnostics include
PLL reset/lock, incomplete/ignored I/O assignments and non-dedicated clock
routing. STA's unmatched `analog_fb_stride_reg` filter remains visible.
The inherited asynchronous clock groups and disabled APF SPI delay lines do
not prove those CDC/external paths. Their review, final combined fit and PCM/UI
reserve remain slice 5 gates; this result is not whole-Pocket timing acceptance.

All logs and generated trees above are ignored under `out/`. SD readback,
durability, storage deadlines, reset-retained save banks and firmware 2.6
acceptance remain open; no settings capability is advertised by this unit.

### Atomic settings RAM owner (2026-09-20)

The [settings RAM contract](../../specs/pocket-settings-ram-v1.md) now has a
synchronous implementation in `rpcmp_settings_ram.sv`. Two 64-byte banks per
logical slot hide CPU/Host partial writes. Publication switches bank and length
together. Separate Host/CPU read locks and published CPU leases freeze borrowed
data across application/Host reset. A separate 64-byte readback area accepts
only external data and seals after complete byte coverage. A retained
changed-since-load marker prevents a replacement software session from treating
a CPU-updated RAM mirror as proof of SD contents. Core CRC/format rules and the
public settings API are unchanged; this unit reserves no physical address.

Executed evidence:

- `pwsh -File tools/host-verify.ps1` with LLVM 22.1.8: **81/81 PASS**,
  **483.08 s**, including format, tidy and architecture/dependency fixtures.
  The first run was 80/81 because this new RTL file was missing from the exact
  milestone scope list. Registering only that adopted path fixed the failure;
  `ctest --preset host-msvc -R '^architecture'` passed all five positive/negative
  cases, followed by the complete successful rerun. Logs:
  `out/build/m6-settings-host.log` and `out/build/m6-settings-host-final.log`.
- `pwsh -File tools/rtl-settings-verify.ps1`: **PASS**, 10,871 checked commands,
  zero simulator errors/warnings, using the actual `altera_mf_ver` Cyclone V
  RAM model. Every interrupted byte prefix, all lengths 0..64, partial lanes,
  full-width invalid indexes/lengths/addresses, atomic visibility, separate
  slots/readback, concurrent ports, reset, late writes, retained responses and
  media-uncertainty retention are checked. A mixed-port collision assertion
  ensures unspecified RAM read-during-write is never used as an oracle.
  Log: `out/build/m6-settings-ram-rtl.log`.
- `python -B out/build/m6-settings-negative.py`: four ignored mutations fail
  the authored checks: incomplete publication, overwriting the active bank,
  premature lease reuse and losing the media-uncertainty marker on release.
  Production source was not modified for these controls.
- Sequential `pwsh -File tools/rtl-verify.ps1` and
  `pwsh -File tools/rtl-jt51-verify.ps1`: **PASS**. Logs:
  `out/build/m6-settings-rtl-regression.log` and
  `out/build/m6-settings-jt51-regression.log`.
- `pwsh -File out/build/m6-settings-fit.ps1` and
  `python -B out/build/m6-settings-audit.py`: **PASS** for the final registered
  local probe, Quartus 25.1std.0 Build 1129, 5CEBA4F23C8, seed 1, 74.25 MHz.
  It uses **711 ALMs**, **606 registers**, **4,096 memory bits / two M10Ks**,
  and no DSPs. All 20 four-corner timing summaries have zero TNS; minimum
  setup **2.257 ns**, hold **0.097 ns**, recovery **8.458 ns**, removal
  **0.275 ns**, pulse width **5.101 ns**. The unconstrained clock/input/output
  summaries are zero. Reports: `out/build/m6-settings-fit-r3/output_files`;
  source/probe hashes: `out/build/m6-settings-timing.json`.

The first RAM description expanded to 3,588 ALMs / no block RAM. The final
implementation directly binds the existing Cyclone V primitive; its platform
and installed Altera license dependency are recorded in the contract. The
final map and STA have zero warnings. The local fitter retains three visible
diagnostics for virtual-pin/LogicLock support, incomplete I/O assignments and
the unassigned probe clock pin. There are no added timing cuts or suppressions.
This registered probe measures the local synchronous block; it does not
establish board pin timing, CPU CDC or combined Pocket resource headroom.

Docker Engine 29.7.2 and container `rpcmp-openfpgaos-toolchain:14.2.0-3` execute
normally; `riscv-none-elf-gcc --version` reports 14.2.0. A C11 APF-header probe
also compiles with `-O2 -Wall -Wextra -Werror -march=rv32imafc -mabi=ilp32f`;
`riscv-none-elf-readelf -h out/build/m6-settings-docker-check.o` confirms ELF32,
little-endian RISC-V / RVC / single-float ABI. No firmware source/API changed
in this unit, so no new target firmware image was cross-built or
packaged. CPU/BRIDGE CDC, APF Host/all-complete routing, reset-retained ID/size
table ownership, the OS arbiter and actual SD readback remain the next slice 4
connections. No firmware 2.6 test was performed and no durable-save capability
is advertised. All generated probes, mutation copies and logs stay under `out/`.

## Slice 5 sound AXI binding verified, 2026-09-20

After the user deferred persistence, the current integration adopts
[sound AXI binding v1](../../specs/pocket-sound-axi-v1.md). The opt-in source
preparer connects the real peripheral to sound MMIO and generated JT51.
Reads now assert the sound strobe at the actual read-latch edge. Data/error
remain held through AXI backpressure; invalid write masks and unsupported
sound bursts do not submit work. A pending B response cannot be overwritten.
The default M5 profile remains separately compiled with the new macro absent.

Executed integration evidence so far:

- `pocket_m5_audio_prepare.py` with `--apf-lifecycle --apf-flush --player-sound`
  generated `out/build/m6-player-sound-prepared` from the pinned inputs. Its
  peripheral/top source hashes match the initial measured candidate.
- `pwsh -File tools/rtl-player-axi-verify.ps1 -PreparedTree
  out/build/m6-player-sound-prepared`: PASS at 0, 5,555 and 81,379 ps phases,
  72 writes each and respectively 2,589 / 2,588 / 2,589 reads. Each simulator
  summary has zero errors/warnings. All four mailbox channels cross the actual
  binding. Log: `out/build/m6-player-axi-final.log`.
- `python -B out/build/m6-player-negative.py`: both authored negative controls
  fail at the expected assertions: an early read strobe returns zero instead
  of RSM1; accepting another AW overwrites the pending-response ownership.
  Mutations are isolated under `out/sim/player-axi-negative-*`.
- Sequential `rtl-player-axi-verify.ps1`, `rtl-apf-flush-verify.ps1` and
  `rtl-apf-verify.ps1 -Flush`, using that prepared tree, followed by
  `rtl-sound-mmio-verify.ps1`, `rtl-verify.ps1` and `rtl-jt51-verify.ps1`: PASS.
  Logs: `out/build/m6-player-{axi,apf,lifecycle,mmio,rtl,jt51}-regression.log`.
- `pwsh -File out/build/m6-player-fit.ps1`: whole-core synthesis passes (zero
  errors, 1,085 visible warnings), but fitting **fails**. The fitter reports
  20,870 / 18,480 ALMs (113%) and requires 2,108 LABs versus 1,848 available
  (errors 170012 and 11802). Timing analysis did not run. The failed fit's
  entity estimates attribute about 3,810 ALMs to GPU, 7,722 to sound and
  5,260 to the CPU subsystem; these are not successful placement figures.
  Logs: `out/build/m6-player-sound-{map,fit}.log`.

The player variant now uses the inherited `EXCLUDE_GPU` branch with inactive
GPU bus masters; terminal/framebuffer scanout remain available for CPU drawing.
It clears GPU feature bits and sets the OS capability descriptor's `gpu_base`
to zero. Other capabilities and the default M5 profile remain unchanged.

- Latest CPU-video prepared source: `out/build/m6-player-cpuvideo`.
  `rtl-player-axi-verify.ps1` passes its three phases with 2,589 reads and 72
  writes each, including GPU capability checks. `rtl-apf-flush-verify.ps1`
  passes the disabled-player regression on the same tree. Logs:
  `out/build/m6-player-cpuvideo-{axi,apf}.log`.
- The changed OS `caps_table.c` compiles with container GCC 14.2.0 using
  `-std=gnu11 -O2 -Wall -Wextra -Werror -march=rv32imafc -mabi=ilp32f`.
  This is an object compile, not a linked ROM/OS image or boot result.
- `pwsh -File tools/host-verify.ps1`: final **81/81 PASS**, **577.21 s**,
  including architecture, format and tidy. Log:
  `out/build/m6-player-axi-completion-host.log`. The earlier run passed in
  658.77 s; the final gate includes the subsequent preparation changes.
  Focused harness, harness-negative, firmware-pair, placement and failure-package
  checks also pass (5/5 in 2.31 s).
- `pwsh -File out/build/m6-player-cpuvideo-fit.ps1`: map/fit/STA complete.
  This resource probe uses **16,550/18,480 ALMs**, 27,831 registers,
  955,174 memory bits, **151/308 M10Ks** and **8/66 DSP blocks**. It leaves
  1,930 ALMs (10.4%); this is not an allocation proof for future PCM/UI work.
  Logs: `out/build/m6-player-cpuvideo-{map,fit,sta}.log`. An earlier no-GPU
  attempt accidentally concatenated its macro with the last QSF line; its
  repeated capacity failure was invalid as evidence for GPU removal.

The resource probe uses inherited CPU/audio clock-group cuts. Final player
preparation removes that blanket cut and adds explicit bundle limits and
first-stage exceptions from sound MMIO v1. The prior exclusion is retained
only for CPU/audio paths through the separate legacy PCM serializer/FIFO,
which does not drive AUDIO in this profile. Same-domain legacy paths remain
timed. An intermediate fit without that scoped exception exposed the old
crossings and failed timing; it is not acceptance evidence. Synthesis and
fitted PLL clock names differ; the constraint now requires exactly one of
the two observed names. This resolves warning 330000 without skipping checks.

Final prepared source is `out/build/m6-player-cdc`, with project
`out/build/m6-player-cdc-scoped-fit`, Quartus 25.1std.0 Build 1129,
5CEBA4F23C8, seed 1 and four processors. Actual commands were
`quartus_map --write_settings_files=off ap_core`,
`quartus_fit --write_settings_files=off ap_core`, then
`pwsh -File out/build/m6-player-cdc-scoped-finish.ps1` (STA and four-corner
audit). STA does not accept `--write_settings_files`; the driver's initial
CLI rejection ran no analysis and was corrected before these final checks.
`python -B out/build/m6-player-final-evidence.py` verifies the reports:

- **16,649/18,480 ALMs**, 27,821 registers, 955,174 memory bits,
  **151/308 M10Ks**, **8/66 DSP blocks**. Remaining ALMs: 1,831 (9.9%).
- All **136** timing summary rows have nonnegative slack and zero TNS.
  Minimum setup/hold/recovery/removal/pulse-width slack is respectively
  **0.349 / 0.110 / 3.178 / 0.268 / 0.555 ns**.
- All **32** mailbox/direction/corner cases cover every optimized destination,
  satisfy the 81.380/11.111 ns data limits and have positive setup slack.
  Every second synchronizer stage and both local reset-release chains pass
  setup/hold checks. Timed CPU/audio endpoints match the bundle endpoint totals.
- The independent negative control (`m6-player-cdc-negative.tcl`) reinstates
  the blanket clock cut and fails at the expected zero-bundle-path assertion.
- The final peripheral, top, OS capability source and JT51 manifest are byte
  identical to the CPU-video sources used in the passing RTL/compiler checks.
- Final overlay formatting uses the existing zero-context patch convention
  with sequential application. A fresh preparation at
  `out/build/m6-player-ready-lf` also reproduces the fitted peripheral, top,
  core constraints, OS capability source and JT51 manifest byte for byte.

Reports and logs: `out/build/m6-player-cdc-scoped-{map,fit,sta,audit}.log`,
`m6-player-cdc-scoped-cdc-paths.txt` and `m6-player-final-evidence.json`.
Missing clocks/endpoints are errors; no warning suppression was added. Visible
inherited diagnostics include PLL lock/compensation, incomplete/ignored I/O
assignments, bridge clock routing, unavailable LogicLock, an optimized-away
analog stride constraint and synthesis warnings. The inherited external-I/O,
legacy PCM and other shell CDC exclusions still need the broader slice 5
review; passing constrained paths does not prove those exceptions.

The next unit is the target service loop, input, licensed bitmap fonts and CPU
renderer, with coherent ROM/OS/app builds, memory/cadence measurements and
packaging. Future PCM/UI reserve and firmware 2.6 acceptance remain open.
No hardware test or playable M6 package is claimed by this binding unit.

## CPU bitmap font and bounded canvas — 2026-09-20

The [Pocket bitmap canvas](../../specs/pocket-bitmap-canvas-v1.md) now implements
the shared PlayerCanvas independently of player/runtime. It records at most
2048 copied drawing commands, then rasterizes a caller-bounded pixel count into
a checked 640x480 indexed surface. Transparent glyph pixels count toward the
budget; partial/failed frames cannot be reported complete. All three current
views fit the list. Text is strictly validated, clips by actual glyph advance,
and keeps the complete 96-byte public prefix when space permits the added
ellipsis. The maximum Japanese prefix initially lost one unnecessary glyph;
an authored 32-character case failed before giving the internal copy three
additional UTF-8 bytes, and passes afterward.

The fixed, licensed RPCMP Bitmap JP generation uses the pinned CP932 table,
Unifont Japanese 16.0.04 and ingestion's utf8proc 2.11.3. Windows and Linux
generation produce identical bytes (`1a2c9dba40b52e180e7e95c26a789a27fe5adc13a1bba0f5a5bd844eb976859f`).
The 7488 glyphs use 233040 bitmap bytes and 89856 index bytes, below the
8192-glyph / 512 KiB limits. The font's actual ellipsis/replacement advance is
8 pixels; the existing host SVG mock retains its provisional metrics. Full
copyright/license notices and source identity are in `third_party/unifont`.

Executed checks:

- `pwsh -File tools/host-verify.ps1`: **83/83 PASS**, 500.83 s, including
  format, clang-tidy, architecture and rejection fixtures. Final log:
  `out/build/m6-bitmap-host-final-r2.log`. An earlier full run reported three
  integer-width diagnostics and one flags-enum diagnostic in the new helper;
  explicit index/offset types and reuse of ingestion's existing `normalize_utf8`
  resolved them. No check or warning was suppressed. Focused changed-file tidy
  passed before this final full run.
- `ctest --preset host-msvc -R '^(bitmap_canvas|bitmap_font_source)$'`:
  **2/2 PASS**, 0.28 s. The separate
  `python -B tests/architecture/catalog_boundary_tests.py` passed, including
  new canvas-to-player/runtime/library/sound rejection fixtures.
- Docker `sh out/build/m6-bitmap-linux.sh`: PASS, native pinned NFC generation
  through the existing ingestion `normalize_utf8`, byte comparison against
  Windows, and source glyph/index validation. Log: `out/build/m6-bitmap-linux-final.log`.
- Docker `sh out/build/m6-bitmap-sanitize.sh`: PASS with GCC AddressSanitizer
  and UndefinedBehaviorSanitizer. Final log: `out/build/m6-bitmap-sanitize-final-r2.log`.
- Docker `make -f out/build/m6-bitmap-cross/Makefile bitmap-check`: PASS with
  RISC-V GCC 14.2.0, the pinned SDK, `-Werror`, no exceptions/RTTI and no undefined
  symbols. This is a standalone renderer link probe: text 346060, initialized
  data 192, BSS 564200 bytes; conservative recorded stack sum 2208 bytes,
  largest frame 784. The BSS includes one probe framebuffer and copied view;
  the canvas itself is 237624 bytes. The probe's 910452 static bytes pass its
  2 MiB limit, but are not the final application/SDK framebuffer memory budget.
  Log: `out/build/m6-bitmap-cross-final-r2.log`; budget: `out/build/m6-bitmap-cross/budget.json`.
- Optional `rpcmp_bitmap_canvas_tests.exe out/build/m6-bitmap-view-` produces
  authored Tracker/keyboard/library PPMs. The converted PNGs were inspected
  for actual Japanese/ASCII glyphs, borders and layout. They are host raster
  output, not Pocket screenshots or hardware readability evidence.

No RTL, FPGA placement or hardware check was rerun: this unit changes the CPU
font/canvas, build and host tests only. It does not change the verified sound
binding. Settings remain per-launch defaults with no storage adapter.
Next: real input, service-loop cadence, SDK framebuffer presentation and the
coherent ROM/OS/app package. Font-coverage notices during PC ingestion and
on-device density/readability remain integration work.

### Slice 5 actual Pocket application integration (2026-09-20)

The [M6 application](../development/pocket-player.md) now connects the real
CatalogSession, PocketMdxBackend, PlayerSession, PlayerUi and BitmapCanvas.
This is the actual target composition and main loop, not a standalone link
probe. The SDK bridge loads slot 4 in 64 KiB chunks (32 MiB maximum), checks
the 640x480 indexed CPU-video profile and retains the OS-owned draw surface
until a completed frame is submitted without a pending swap. APF PAD input
uses replaceable bindings and coherent type/button words. A bounded high/low/
high CPU-cycle reader uses the OS's live frequency to supply 64-bit time.

Sound service and Core stepping run every iteration, independently of the
5 ms publication/input interval and 33,333 us minimum frame interval. Pixel
work is limited to 256 per pump, with sound service between pumps. Maximum
service gaps and record/pump/present durations are retained for diagnostics.
The approved 1000 us sound mailbox watchdog is unchanged; transport preparation
and control deadlines are 5 s and 100 ms. These configured intervals are not
measured CPU timing guarantees. In particular, the pinned OS still cleans the
whole framebuffer during flip, and synchronous MDX admission/command recording
still need target measurements.

Every launch uses AlbumOrder / Default (two loops and five-second fade),
shuffle off and no autoplay. The composition provides no settings storage;
the published `kPlaybackSettings` bit and settings observation are absent.
In-session repeat/shuffle changes work through the existing command ingress.
No existing optional settings implementation was removed or weakened.

Executed checks:

- Focused MSVC build of `rpcmp_mdx_backend_tests` and the target-main compile
  check, then `ctest --preset host-msvc -R '^mdx_backend$'`: **1/1 PASS**, 0.26 s.
  New scenarios operate the actual application through copied sound mailboxes:
  album/track selection, A pause/resume, B stop/back, repeat and shuffle, two
  independent launches, and long display backpressure with retained surface
  and continuing input/mailbox service. Fatal canvas/display/reversed-clock
  cases request INHIBIT. Timer rollover/retry and 32 MiB loader bounds pass.
  This scripted device is not an RTL or audible-output simulation.
- `pwsh -File tools/host-verify.ps1` executed all **83** gates in **508.17 s**:
  **82 passed**, including formatting, architecture/rejection fixtures and
  the full clang-tidy gate (491.35 s). The sole failure was `harness_negative`:
  its isolated documentation tree did not contain the new README in `spikes/`.
  Moving that new document to `docs/development/pocket-player.md` and updating
  its link fixed the input layout without changing the test or checker.
  `ctest --preset host-msvc --rerun-failed --output-on-failure` then passed
  **1/1**, 0.95 s, and `ctest --preset host-msvc -R '^harness$'
  --output-on-failure` passed **1/1**, 0.07 s. Thus every gate passed, with the
  documentation-only correction checked separately; this was not a second
  full-suite run. Full log: `out/build/m6-player-app-host.log`.
- Changed-file clang-tidy passed before the full gate. Initial warnings in the
  new code concerned integer-width multiplication and use of `empty()`; both
  were corrected without suppressions. Log: `out/build/m6-player-app-tidy-final.log`.
- Docker `python3 out/build/m6-player-app-sanitize.py`: **PASS** with native
  GCC AddressSanitizer and UndefinedBehaviorSanitizer. This includes the
  existing backend tests and new real-application scenarios. Log:
  `out/build/m6-player-app-sanitize.log`.
- Docker `make -j4 -f spikes/pocket/openfpgaos/player.mk player-check`: **PASS**
  using GCC 14.2.0 and the pinned SDK. The complete RV32IMAFC / ILP32F app links
  without undefined symbols and with `-Werror`, no exceptions/RTTI. Text is
  **472036**, initialized data **228**, BSS **34888240**, total static
  **35360504 bytes**, leaving **21262600 bytes** in the 54 MiB app region.
  The conservative recorded stack sum is **120512 bytes**, largest frame
  **9600**, within the 512 KiB limit. BSS includes the full 32 MiB library
  reservation and actual application owners. OS framebuffers remain in their
  separate SDK regions. These are link budgets, not peak runtime measurements.
  Final log: `out/build/m6-player-app-cross-final.log`; budget:
  `out/build/pocket-m6-player/budget.json`; ELF SHA-256:
  `eed972b30445c6d2ce4afabc55b63fbb717fd72ce04940ac8adadda8452b295e`.

No RTL, FPGA fit or hardware checks were rerun: this unit changes CPU application
composition, SDK adaptation, build and host tests; the verified sound binding
is unchanged. There is still no coherent installable M6 package. Next: adapt
boot-failure handling to RSM1, pair the boot ROM/OS, assemble the matching FPGA
image and application, then measure service cadence and verify real Pocket
input, display and playback. Persistent storage remains deferred.

## First coherent Pocket candidate — 2026-09-20

M6 slice 5 now has an installable **local hardware candidate**, documented in
the [first hardware check](../development/pocket-player-hardware.md). This is
not M6 hardware acceptance or ADR-0008 production promotion. The user's deferred
persistence requirement remains in force: volatile AlbumOrder / Default,
shuffle off, no autoplay on each launch, and no nonvolatile APF slot.

The new `boot_m6_sound_reset.inc` replaces only the M6 preparation's old M5
sound include. Its ROM-only failure path requests RSM1 INHIBIT, waits with one
bounded poll budget, owns a Reset request through copied completion, validates
the full echo/result/epoch/prepared generation, releases only its completion,
and returns success only with clear fault/inhibit/diagnostic status. Failure
reasserts INHIBIT and the existing caller remains in ROM. It never starts or
feeds playback. The no-GPU OS hunk is selected from the already-approved CPU
video patch; the corresponding FPGA hunk is owned by the prior preparation.
Pinned upstream, firmware toolchain, musl and CRC/retry behavior are preserved.

Executed evidence for the firmware and candidate:

- `ctest --preset host-msvc -R '^(player_boot|firmware_pair|boot_crc)$'
  ran **2/2 PASS** (`player_boot`, `boot_crc`; the actual pair test has a
  `pocket_` prefix and was also run directly below). Log:
  `out/build/m6-player-boot-final.log`.
- `python -B tests/pocket/firmware_pair_tests.py`: **7/7 PASS**.
  `python -B tests/pocket/player_firmware_prepare_tests.py`: **2/2 PASS**.
  The latter applies the real filtered patch to an authored UTF-8 fixture and
  proves the FPGA file is untouched and baseline rejection is preserved.
- Compiling the same ROM test against `boot_sound_reset.inc` instead of the
  new RSM1 include produces **83 failed assertions / exit 1**, including the
  normal Reset case. This negative control shows the old MMIO protocol cannot
  serve M6. Log: `out/build/m6-player-boot-old.log`. Both new C++ files pass
  focused clang-tidy 22.1.8 without added suppressions.
- `python -B tools/pocket_player_firmware_prepare.py --repo . --upstream
  out/research/openfpgaCore-618a3eb --musl
  out/build/m5-firmware-fail-closed/src/firmware/musl --output
  out/build/m6-player-firmware-r2`, then Docker firmware image
  `sha256:98771990c464c0b3231d685de02579a25c6b5b0f952b68d5e573679664070e58`
  running `make -j4 TARGET=pocket` in `src/firmware/os`: **PASS**.
  The linked boot image is **16100 bytes** in its 16 KiB region; `os.bin` is
  **135768 bytes**. Log: `out/build/m6-player-firmware-build.log`.
  `pocket_firmware_pair.py` verifies the ELF sections, entire 8192-word MIF,
  OS payload, ABI, entry/BSS metadata and CRC. MIF SHA-256:
  `c479524bb380900a8eb4b0a4433b2513cb4c5e039568fef3d47776c2be002e60`;
  OS SHA-256: `fcd131fa43ca7f953576b69717c33f77cbcf07d8149e182a6e917066bf6f2080`.
- `quartus_sh --flow compile out/build/m6-player-finalbuild/ap_core`:
  **PASS, 0 errors / 967 warnings**, full fresh map/fit/assembly/STA in 10:09.
  The copied peripheral names the new MIF by absolute path; the map report
  confirms it. A copied-database `--update_mif` attempt did not list this ROM
  and was not used. No RTL logic or timing constraints changed. Resource
  results remain **16649/18480 ALMs, 27821 registers, 151 M10Ks, 8 DSPs**.
  All **136** clock/corner summary rows are nonnegative with zero TNS;
  setup/hold/recovery/removal/pulse minima are **0.349 / 0.110 / 3.178 / 0.268 /
  0.555 ns**. Log: `out/build/m6-player-finalbuild.log`.
- `quartus_sta -t ../m6-player-candidate-audit.tcl` on that new placement:
  **PASS** for all **32** mailbox/direction/corner cases, all bundle endpoints,
  synchronization paths and max-data bounds. Log and paths:
  `out/build/m6-player-candidate-audit.log` and
  `out/build/m6-player-candidate-cdc-paths.txt`. Existing shell external/legacy
  exceptions remain unproven; successful fit/CDC is not a Pocket measurement.
- `pocket_player_demo.py out/build/m6-player-demo-r2` and native
  `rpcmp_album_pack` create **2 albums / 3 tracks, 1712 bytes**, with no exclusions.
  `rpcmp_player_preflight` admits all three via the Core catalog and MDX path.
  The corpus is newly authored FM scale data, not copied music. The CLI test
  also rejects corrupted STRS content, empty input and input above 32 MiB.
  No audible result is inferred from parse/admission.
- `pocket_player_package.py` with the pinned SDK, prior app ELF, new FPGA/firmware,
  authored library and native preflight produces
  **`out/build/m6-player-candidate-r1.zip`**, **1322817 bytes**. ZIP SHA-256:
  `9341c4d21f1815f98ad2aeb590b3d812a5bbda38c7da159f40808fb5d127294d`.
  Native RBF SHA-256:
  `d5d6f7945fe69f17c56e1e9c15cf2d7ecb30fc4b4382e2b79c10d33575d95043`.
  The matching `.evidence.json` records member readback, identities, actual app
  section budget, firmware pair and Quartus report hashes. Dedicated core/platform
  IDs preserve every M5 control. Eight scaler entries preserve OS/RTL slot 7
  for 640x480; deferred slots contain OS/INI/ELF/library and no saves. Font
  notices are included. Packaging tests: **5/5 PASS**.

- With LLVM **22.1.8** prepended to PATH,
  `pwsh -File tools/host-verify.ps1`: **87/87 PASS**, including format, static
  analysis and architecture positive/negative checks. Total **512.06 s**;
  clang-tidy **494.47 s**. Log: `out/build/m6-player-candidate-host-r2.log`.
  The first invocation stopped at setup because this host's default LLVM was
  23.1.1; the version gate was preserved. After the documentation updates,
  `check_harness.py --root .` and the **7** harness tests also pass. The package
  identity checks and **5** packaging tests were repeated with the final RBF pin.

No new RTL simulation or Pocket run is claimed: logic is unchanged, while the full FPGA
build and CDC audit above were rerun for the new ROM. The next required input
is the first real Pocket boot/display/audio/operation report. Actual service
gaps, full-frame cache-clean duration, 100–300-track real-library behavior and
Japanese readability remain unmeasured. Persistent settings remain future work.

## First hardware report and r2 correction — 2026-09-20

The user reports r1 boots without autoplay, renders Japanese and all three main
views, and permits album/track selection. Before selecting any track it shows
`Playback failed; restart the player` and `ERROR`; no music is heard. Visible
input response takes roughly one second. Setting `core.json`'s minimum firmware
declaration to 2.2 is necessary on their Firmware 2.6 device. These are user
observations, not measured CPU timings or M6 audio acceptance.

The user replaces B panel navigation with D-pad focus movement and contextual
buttons. The adopted UI contract now permits Main focus in every view, including
terminal errors. Up from top-row controls or Left from either row's first icon
enters Main; Right returns to PlayPause. Main Tracker/Keyboard A plays/pauses;
Library A browses/selects and B returns from tracks to albums without stopping
audio. B elsewhere stops but never changes panels. The focus/binding tables
remain replaceable; Core APIs and playback engines are unchanged.

Two startup ordering bugs are reproduced with scripted copied mailboxes:

- The application's initial INHIBIT latches hardware fault before Reset.
  An immediate Capture response could turn this expected epoch-zero state
  into a new device failure before Core issued its first Reset.
- RTL increments `feed_epoch` on Reset acceptance, before RESET_WAIT clears
  its old fault. A query for the old epoch can return the new sampled epoch
  with old fault bits. Draining it alongside Reset Success incorrectly failed
  the recovered session because only the sampled epoch was checked.

The backend now requires an acknowledged nonzero epoch matching **both** the
query and captured epoch before interpreting health. Transfer/protocol failures
still fail, and a fresh established-epoch hardware fault still inhibits audio.
The corrected RTL-derived race fixture fails before the fix and passes after;
the immediate-reply application fixture separately fails before the epoch-zero
guard. The ROM Reset helper is only a failed-boot path, not a normal-boot caller;
an early investigation hypothesis about reused ROM operation IDs was rejected.
Hardware cause equivalence and restored audible playback remain to be checked.

The old frame took thousands of 256-pixel turns, each executing a complete
PlayerSession step. r2 uses bounded 4096-pixel pumps, services sound every turn,
steps Core at 1 ms intervals, and samples input every turn independently of the
5 ms publication interval. A host work-bound regression requires the idle frame
within 256 turns and fails with the old pump budget. This is not a target latency
measurement. Header F/S and error ERR/SND/REC/FLIP diagnostics expose frame time,
maximum service gap, error code, sound status and recording/presentation costs.

Executed verification and artifacts:

- Focused `ctest --preset host-msvc -R
  '^(mdx_backend|player_ui|player_render|bitmap_canvas|player_package)$'`:
  **5/5 PASS**. Tests cover D-pad focus during errors, context-dependent B,
  current-epoch fault preservation, startup without autoplay, retained display
  ownership and publication-independent feeding. UI expected-value changes
  follow the user's explicit revised operation requirement.
- Docker `make -j4 -f spikes/pocket/openfpgaos/player.mk
  PLAYER_OUT=/repo/out/build/pocket-m6-player-r2 player-check`: **PASS**.
  Text **472300**, data **228**, BSS **34888264**, static **35360792** bytes;
  conservative stack **120576**, largest frame **9680**, no dynamic frames or
  undefined symbols. Log: `out/build/m6-player-r2-cross.log`. ELF SHA-256:
  `d52320cef628952799a9c3fee15b5f50e57b4886d2d99534e67b73fdf1c7993a`.
- Docker `python3 out/build/m6-player-r2-sanitize.py`: **PASS** with native
  AddressSanitizer/UndefinedBehaviorSanitizer, leak detection and halt-on-error.
  It builds and runs the actual application/backend tests from current sources.
  Log: `out/build/m6-player-r2-sanitize.log`.
- `pocket_player_package.py` with that app and the unchanged r1 FPGA/ROM/OS,
  SDK and authored three-track library: **PASS**, exact member readback and
  all tracks admitted by Core preflight. `version_required` is now **2.2**;
  this follows the user's compatibility report, not a Firmware 2.2 test.
  `out/build/m6-player-candidate-r2.zip`: **1322912 bytes**, SHA-256
  `6d5c7e63216defcd5fab6284553c53e7f9cac5733d9b3488eac057f29a26fc00`.
  The matching `.evidence.json` records the identities and budgets. r1 is preserved.
- With LLVM 22.1.8 prepended, `pwsh -File tools/host-verify.ps1`:
  **87/87 PASS**, including architecture checks, format and clang-tidy.
  Total **538.49 s**, tidy **515.79 s**; log:
  `out/build/m6-player-r2-host.log`. After the final packaging pin and documentation
  edits, the **5** `player_package_tests.py` tests and
  `check_harness.py --root .` also pass. No failing check was disabled or relaxed.

No RTL, ROM/OS, timing constraints or FPGA image changed, so no RTL simulation
or synthesis rerun is claimed for this correction. The next task is the
[r2 hardware check](../development/pocket-player-hardware.md): startup error,
audible playback, D-pad panel navigation and actual input/frame timing. M6
remains open; persistent settings remain deferred.

## r2 hardware report and r3 CPU correction — 2026-09-20

The user confirms r2 boots without an error or autoplay, displays Japanese,
allows album/track selection and D-pad panel movement, and reports the requested
button operations as OK. Selecting a track repeatedly fails with
`Playback failed; select a track to retry` and
`ERR 5 SND 0800 REC 9178us FLIP 441us`. Reported header values are F206ms and
S84964us. Restart and power-off reproduce it. No music is heard, so audible
pause/resume, display switching during playback and automatic advancement
remain unverified. Repeat changes, but shuffle has no visible change.

ERR 5 maps to DeviceFault. The r2 SND field reads the client's current status
after the transport's recovery Reset, so 0800 cannot identify the original
failure. S is a maximum across the launch, including preparation; it does not
identify which operation consumed 84964 us. Source starvation from expensive
CPU work is a hypothesis, not a reproduced Pocket fault.

The MDX track/document/router/timeline/engine paths copied entire maximum-sized
output arrays every driver tick, including held/ended tracks and empty write
batches. r3 commits only the counted prefix after success, always updating its
count. Limits, failure rollback, timestamps, pending order and storage capacity
are unchanged. Dense-to-single-to-empty reuse and existing bounded/transactional
and exact-trace tests exercise the change.

An ignored local benchmark (`out/build/m6-mdx-bench.cpp` and `.py`) runs 10,000
ticks of an authored looping one-channel phrase, with a direct linked wrapper
around memcpy/memmove. Docker `rpcmp-openfpgaos-toolchain:14.2.0-3` runs
`python3 out/build/m6-mdx-bench.py before` before editing and `after` afterwards.
Both produce 1,278 writes, 104 completed loops and the same valid-prefix/time/
loop trace fingerprint `5da7c777d7ef05f3`. Wrapped copy-call volume falls from
12,508,500,000 to 465,356,770 bytes (96.28%). Three native x86-64 -O2 timings are
124888/133626/128937 us before and 7452/7911/7646 us after. These are host
measurements, not RV32 execution times; inline copies are outside that counter.
The speed ratio is not a Pocket service-deadline guarantee.

Target diagnostics now retain the first backend failure and sampled status
across recovery Reset, clearing only on accepted preparation for a new attempt.
ERR/AT/D/SND and REC/FLIP occupy two lines. AT uses `MdxBackendFailure`:
2/4/8 identify Control/Capture/Feed transfers (D: transfer result);
5 identifies captured health (D bit 0 fault, bit 1 inhibited, bits 8–15 media
failure, bits 16–23 queued count); 10 identifies MDX production (D low byte
source result, next byte decode error). Other cases name release, epoch,
generation, feed closure/reply, submission, ticket exhaustion or client health.
This is platform-only diagnostic data; generic Core/UI snapshots are unchanged.
The original failure policy, emergency silence and mailbox deadline remain intact.

Shuffle previously differed only in stroke color. Its icon now includes ON/OFF.
A focused rendering test fails before the change and passes after. The actual
application test changes shuffle on/off after a recoverable sound failure,
through input and command ingress, without restarting playback. It establishes
that this state accepts the command; it does not identify the exact reason the
user saw no change on r2.

Executed checks and candidate:

- `cmake --build --preset host-msvc` and
  `ctest --preset host-msvc -R 'mdx|pocket_player|player_render' --output-on-failure`:
  **23/23 PASS**, including existing MDX oracle/trace, render independence,
  producer and actual backend/application cases. New diagnostics tests cover
  original fault retention through Reset, clearing on retry, and distinct
  control/feed/capture timeouts.
- Docker `python3 out/build/m6-player-r3-sanitize.py`: **PASS** under native
  ASan/UBSan with leak detection and halt-on-error; log
  `out/build/m6-player-r3-sanitize.log`.
- Docker `make -j4 -f spikes/pocket/openfpgaos/player.mk
  PLAYER_OUT=/repo/out/build/pocket-m6-player-r3 player-check`: **PASS**.
  Text **472900**, data **228**, BSS **34888272**, static **35361400** bytes;
  conservative stack **120608**, largest frame **9696**, no dynamic frames or
  undefined symbols. Log `out/build/m6-player-r3-cross.log`. ELF SHA-256:
  `b6f8ac38edb6a3b1dfff651526c45652c4482d293215aa4179d5323437d0ca48`.
- `pocket_player_package.py` with that ELF, the unchanged verified r1 FPGA,
  r2 firmware directory, SDK and authored library: **PASS**. All three tracks
  pass Core preflight; member readback and original firmware/FPGA identity
  checks remain enforced. `out/build/m6-player-candidate-r3.zip` is
  **1323717 bytes**, SHA-256
  `37d1a92cda6d9802f43d14845973a3fbf9b732069ca61006e66d9de46f69d414`.
  Evidence and package log are adjacent; earlier candidates are preserved.
- With LLVM 22.1.8 prepended, `pwsh -File tools/host-verify.ps1`:
  **87/87 PASS**, including format, clang-tidy and architecture positive/negative
  checks. Total **534.86 s**, tidy **512.29 s**; log
  `out/build/m6-player-r3-host.log`. `check_harness.py --root .` also passes
  after navigation changes. No check, timeout or capacity bound was relaxed.
- An ignored host preview links the actual `BitmapCanvas`, pinned bitmap font
  and UI renderer with the authored rendering fixture. ON and OFF both fit
  within the selected 44-pixel icon. Images:
  `out/build/m6-r3-shuffle-{off,on}.png`. This verifies target glyph layout,
  not physical Pocket readability or the user's button report.

No RTL, ROM/OS, clock constraints or FPGA image changed, so RTL simulation and
synthesis are not rerun for r3. The next task is the
[r3 hardware check](../development/pocket-player-hardware.md): first establish
audible playback, then record F/S and shuffle ON/OFF; if playback fails, retain
both diagnostic lines. Hardware recovery and M6 completion remain pending.

## r3 hardware report and r4 performance display correction — 2026-09-21

The user's Firmware 2.6 r3 report confirms startup without an error/autoplay,
Japanese album/track selection, stereo playback for one minute in infinite
mode, D-pad focus, A pause/resume, B stop/list back, L/R, two loops plus fade,
next track, album-end stop, both setting icons and restart/power-off defaults.
The sound is quiet. Tracker and keyboard do not change during playback; the
keyboard continually says `Waiting for performance data`.
Reported header observations are idle library F110/S20493us, keyboard
F250 then F400/S10493us, and Tracker/album playback F250/S10593us. These are
user observations, not a controlled latency comparison or a service bound.

The 16-batch display history was smaller than the 64-item audio read-ahead
queue. During long held notes, empty MDX ticks still enqueue markers, so the
producer continuously evicted not-yet-output channel checkpoints before the
matching output record arrived. This was reproduced in the actual backend
with an authored 48-tick rest and a continuously replenished 64-item queue.
The regression observes the public SnapshotPublisher: every consumed marker
must publish Available/current key-on, with no fault/inhibit. It fails at 16
and passes at 128. The internal history contract now allows 128 batches,
covering 64 queued items, a pending producer and 32 output records awaiting
acquisition. Arbitrary overflow still reports display loss without blocking
audio; public snapshot capacity and schema are unchanged. The original device
symptom is consistent with this reproduction; r4 hardware motion is pending.

The larger target owner adds 616448 BSS bytes over r3. Host history fixtures
now allocate their large owners on the heap, matching the target's existing
static ownership rather than enlarging the host stack. Retention-overflow
assertions use 132 checkpoints (4 dropped, 128 retained); audio-independence
still crosses the new capacity, with the same independently authored 800
tokens and backpressure retry sequence under dense/sparse/absent reads.

CPU rendering changes preserve the same view contract:

- Axis-aligned lines process bounded spans; glyph rasterization consumes up
  to eight bits at a time. Transparent pixels still count against work budgets.
- Keyboard white-key backgrounds share one rectangle per channel; sounding
  white keys remain overlays and black keys/separators keep their order.
- Tracker projection overwrites its oldest row in place, then restores the
  newest sixteen rows' chronological order once. Tests check every retained
  row, two channels at one frame, sequence gaps and repeated-channel events.

Before/after actual bitmap fixture PPMs have identical SHA-256 in all views:
Tracker `41c763669e117da59ccaf66233b12cc24c9f87ceb5367784381cc00d401fe9f1`,
keyboard `b9854d45e098f7603cc11fec7eec11ed25afadd7a52adb9be63a5f0c72450aaf`,
library `78a81b489027ec054c862dbf50c355e68a11229c3751e3ee351fd901ba1e290d`.
Ignored files: `out/build/m6-r4-{before,after}-{0,1,2}.ppm`.
An ignored native GCC -Os benchmark (`m6-r4-canvas-bench.cpp/.py`) runs 500
frames per view: recording/raster us change from 3917/35545 to 3599/16847
(Tracker), 11363/107786 to 7497/40811 (keyboard), and 1613/25829 to 1600/12766
(library). Pixel fingerprints and total pixel work remain identical; idle
keyboard commands fall from 1763 to 1171. A separate
`m6-r4-ui-bench.cpp/.py` checks the final sixteen rows across 10,000 updates
of 256 events: 102198 us before, 37270 us after. These are single host runs,
not Pocket frame times or latency guarantees. Logs are in `out/build/`.

The original demo specified a keyed carrier TL of 30 with MDX volume 8,
giving effective TL51. r4 uses voice TL10 and volume15, giving TL12. The
ignored `m6-r4-tone-check.cpp/.py` runs actual parse/preparation/initial-tick
code and confirms TL51 for the old tone and TL12 for all three new tones,
with stereo pan3 and gate8 retained (`m6-r4-tone.log`). Player gain and
arbitrary library data are unchanged; perceptual volume remains a hardware
check. The fresh authored corpus is `out/build/m6-player-demo-r4`, packed as
`m6-player-demo-r4.rpcmlib`: 2 albums, 3 tracks, 1712 bytes, no exclusions.

Executed build/package evidence:

- Focused UI/history/backend/canvas/preflight checks pass. Two whole
  `pwsh -File tools/host-verify.ps1` runs each passed **86/87**; clang-tidy
  found offset arithmetic/unchecked test optionals, then the Tracker iterator's
  unsigned-to-signed offset. All were corrected with checked access or explicit
  bounded types, without disabling diagnostics. The final verification runs
  `cmake --build --preset host-msvc` and
  `ctest --preset host-msvc -E '^tidy$' --output-on-failure`: **86/86 PASS**,
  **18.65 s**, including format and architecture positive/negative cases.
  `python -B out/build/m6-r4-tidy-parallel.py` then extracts the registered
  tidy command through CTest JSON and checks **all 124 translation units** in
  four independent groups: **all PASS**. The source list, pinned LLVM 22.1.8,
  compilation database and `.clang-tidy` flags are identical to CTest's.
  Logs: `out/build/m6-player-r4-final-verification.log` and `m6-player-r4-tidy-*.log`.
  This is the final combined evidence, not an 87/87 result from either earlier run.
- Docker `python3 out/build/m6-player-r4-sanitize.py`: **PASS** for backend,
  history, bitmap canvas and UI with native ASan/UBSan, leak detection and
  halt-on-error. After the final iterator cast, `--reuse-unchanged` recompiles
  the changed implementation and reruns all four tests: **PASS**; headers and
  flags are unchanged. Logs: `out/build/m6-player-r4-final-sanitize-recheck.log`
  and `m6-player-r4-final-sanitize-signed-index.log`.
- Docker `python3 out/build/m6-r4-demo-history.py`: **PASS**, linking those
  sanitized objects with an ignored actual-demo integration fixture. Across
  600 consumed audio items, all 480 marker snapshots are Available; the
  packaged scale's relative intervals 0/4/7/12/7/4/0 reach the public snapshot
  and UI, and sixteen Tracker rows remain populated with the queue at 64.
  No inhibit or protocol violation is observed. This is scripted output-frame
  evidence, not actual FPGA/audio timing. Log: `out/build/m6-r4-demo-history.log`.
- Docker `make -j4 -f spikes/pocket/openfpgaos/player.mk
  PLAYER_OUT=/repo/out/build/pocket-m6-player-r4-final player-check`: **PASS**,
  including the final arithmetic/iterator corrections. Text **473932**, data **228**,
  BSS **35504720**, static **35978880** bytes, with **20644224** bytes headroom.
  Conservative stack **120880**, largest frame **9696**, no dynamic frames or
  undefined symbols. Log: `out/build/m6-player-r4-final-cross-signed-index.log`.
  ELF SHA-256:
  `3052cf02ebeba8d3feb64d6426f2ffd0d538c39d2cadd30f8b2d0ef98f75a3fc`.
- `python -B tools/pocket_player_package.py --repo .
  --sdk out/research/openfpgaSDK-a408ddc
  --elf out/build/pocket-m6-player-r4-final/rpcmp-player.elf
  --fpga out/build/m6-player-finalbuild
  --firmware out/build/m6-player-firmware-r2/src/firmware/os/bld/pocket
  --library out/build/m6-player-demo-r4.rpcmlib
  --preflight out/build/host-msvc/rpcmp_player_preflight.exe
  --name m6-player-candidate-r4-final`: **PASS**. Core preflight admits every
  track and exact member readback/firmware identities pass. The final
  arithmetic/iterator corrections yield the same ELF and budget hashes as the ZIP.
  ZIP **1324561 bytes**, SHA-256
  `97f0b2c686629b4c7966c38fd704c91ab0e4bdd72ce7132c87e433241f0b7557`.
  `.evidence.json` is adjacent; earlier candidates are preserved. Version
  `0.11.0-m6-preview-r4` keeps `version_required: 2.2`.

The FPGA, ROM/OS, sound registers, clocks and constraints are unchanged from
the verified candidate, so RTL simulation and synthesis are not rerun here.
The next task is the [r4 hardware check](../development/pocket-player-hardware.md):
moving Tracker/channel-A keys, clearing Waiting, audio level and F/S while
playing and switching views. Full M6 acceptance and 100–300 real-track load
remain open; this three-track host evidence does not complete either gate.

The user also supplied a private 12-file corpus for possible hardware testing.
`rpcmp_album_pack` excludes all twelve with `UnsupportedPcm8Layout`; the
structural audit finds sixteen-track tables and empty PDX references in every
file. All eight PCM regions contain only the two-byte TrackEnd instruction.
An ignored local conversion experiment preserves all retained FM/voice/P bytes
and the original files while rebuilding ordinary nine-track copies. All twelve
then fail admission with `UnsupportedOpcode` at the same first E9 instruction.
No command was ignored, no partial audio was packaged and no engine contract
was relaxed. This corpus is a compatibility target, not currently an admitted
hardware test library. Source identities, paths, transformed copies and
per-file results remain only in ignored local outputs.

### MDXPlayer reference and revised MVP — 2026-09-21

The user selected `asaday/MDXPlayer` as the compatibility reference and requires
every locally supplied file that played there, including PCM. PRD 0.5 and
CURRENT now prioritize selection/play/stop and the
[reference comparison](../research/mdxplayer-compatibility.md). That record
contains the Firmware 2.6/r4 report: display motion and volume pass, while
visual synchronization, view-dependent delay/audio cuts and clicks remain
unresolved. No M6 completion or new runtime capability is declared.

The reference source is pinned at `4076b91c7ced57bf6047f69b87c12a34bd99a438`.
An ignored native Linux build of its seven C++ engine/sound units succeeds
without source patches, with legacy warnings retained. Authored FM smoke
checks pass 6/6 at 44.1/48 kHz. A read-only, resource-bounded probe of the
previous twelve original sixteen-track files produces nonzero stereo PCM for
the first 102,400 frames in 12/12 cases. These are short host prefixes, not
full-track, PCM-track, iOS binary or Pocket acceptance; see the reference
record for commands, failed-wrapper correction and limitations.

`pwsh -File tools/host-verify.ps1` with pinned LLVM 22.1.8 passes **87/87** in
**528.48 s**, including format, tidy and architecture positive/negative checks.
Log: `out/build/mdxplayer-baseline-host-20260921.log`.
`python -B tests/harness/harness_tests.py`: **7/7 PASS**. Changed-document
local links and `python -B tools/check_harness.py --root .` pass; `git diff
--check` is clean. RPCMP code, hardware, clocks and package inputs did not
change. RTL simulation, synthesis, RV32 rebuilding and new hardware tests
were not run for this requirements/research documentation update.

## Minimal player implementation — 2026-09-21

The user's next request implements selection/play/stop over the accepted HYB1
engine. The [bounded design](../design/pocket-mdx-compatibility-plan.md#minimal-player-implementation-boundary)
adopts HPL1 for the prepared-input list: 1–300 tracks, a checked catalog and
per-payload IEEE CRC32, and loading only the selected MDX/PDX pair. Startup
stays silent. Up/down browse, left/right page, A starts from the beginning,
B stops, and A can select another song during playback. Browsing alone does
not change playback. Natural end stops without automatic next-track selection.

The small Core command/snapshot model uses an injected playback port. UI
depends on contracts only; it copies titles and submits PlayTrack/Stop.
The physical mapping remains replaceable. The display uses the existing
licensed Japanese bitmap font, drawing four scanlines between audio service
calls and presenting without a vsync wait. Rendering occurs only on selection
or transport changes. The accepted renderer's timing, loop behavior, FPGA,
ROM and OS are unchanged. Raw MDX/M3U ingestion, advanced visualization,
pause, shuffle and settings persistence are outside this candidate.

Changed production paths are the prepared playlist reader, minimal player
control, list input/view, bounded bitmap display and SDK application/feed
adapter. The build includes the same pinned Japanese font generation; the
packager produces one `Playlist.json` instead of one APF instance per song.
This deliberately replaces the old probe's slot-4 MDX/slot-5 PDX app profile;
it is not a compatibility wrapper. Existing r3 output packages are preserved.

Executed validation:

- `pwsh -File tools/host-verify.ps1 -Mode Fast` with LLVM 22.1.8: **88/88 PASS**,
  21.55 s. The native catalog/control/feed/display test covers startup silence,
  held/reconnected inputs, page/repeat boundaries, stop priority, reselection,
  loading failure/retry, renderer/audio/reset errors and timeout. The final
  additions cover pending-block retention, EOF ordering and direct PC-writer
  to C++-reader checks for two and 300 tracks, including payload corruption.
- Focused `clang-tidy -p out/build/host-msvc --config-file .clang-tidy` on the
  seven changed C++ translation units: **PASS** after making size/pointer
  arithmetic explicit. Six widening warnings were fixed, not suppressed.
  The earlier Full run was stopped after source changes; its interrupted
  analysis is not acceptance. Final Full status is recorded below.
- Network-disabled Docker image `rpcmp-cpu-budget-sim:20260921`,
  `python3 -B out/harness/minimal-player-native.py`: **PASS**, ASan/UBSan with
  leak detection and halt-on-error. Includes five package tests and native
  readback of 300 entries. Audio-write sequences are identical with rendering
  absent or deliberately deferred. This establishes scheduling/data behavior,
  not target CPU drawing latency.
- `python3 -B tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r1`
  in that same image: **PASS**. Final text **457000**, data **78988**, BSS
  **169344**, static total **705332** bytes; conservative stack **13248**,
  largest frame **4800**, no dynamic frames or undefined symbols. The initial
  build exceeded the initialized-data gate because a polymorphic catalog's
  zeroed arrays landed in `.data`; placement construction in BSS fixed it.
  The existing 128 KiB data, 54 MiB static and 512 KiB stack limits are unchanged.
  A prepared pair is at most 16 MiB; the loading buffers and renderer copies
  coexist temporarily, and source buffers are released after the copy.
- `tools/hybrid_renderer_verify.py --library out/build/minimal-player-cpu-r1/renderer.so
  --oracle out/build/hybrid-real-inputs-r2 --manifest out/build/hybrid-real-inputs-r2/private-inputs.json`
  after the final build: **PASS**, three authored 32-block cases and the same six
  real inputs at 960 blocks each (20.48 s). PCM, ordered/timed FM writes and
  chunk positions match the previously captured oracle. This is a focused
  regression of the supplied tracks, not a renewed whole-corpus campaign.
- Sequential `pwsh -File tools/rtl-hybrid-verify.ps1 -PreparedTree out/build/hybrid-candidate-r3`,
  `pwsh -File tools/rtl-verify.ps1`, and `pwsh -File tools/rtl-jt51-verify.ps1`:
  **PASS**. HYB1 actual-AXI phases 0/5555/81379 each decode 125 frames from
  295 writes. No RTL/ROM/OS source changed, so synthesis/STA was not repeated;
  the candidate reuses the r3 fit and scoped CDC evidence, with its inherited
  external timing limitations intact.
- The host bitmap preview was rendered and visually inspected for Japanese
  labels, selection and playback status. The tests check the literal font
  pixels, surface bounds and bounded pump completion. Pocket readability and
  responsiveness remain hardware observations.
- The command in the [Japanese procedure](../development/pocket-minimal-player.md#開発者向けパッケージ生成)
  creates `out/build/minimal-player-r1.zip`. The package's own readback and
  `python -B out/harness/minimal-player-readback.py` both **PASS**: every ZIP and
  directory byte/hash, APF slots, minimum framework 2.2, final ELF, all six
  original prepared pairs/reference WAVs, Japanese instructions and font notices.
  FPGA/OS/loader bytes equal the accepted r3 package.

Logs are `out/harness/minimal-player-*`; private music and generated artifacts
remain ignored. The final ELF SHA-256 is
`dd26563e3316a377e331ba521f873d2c87a9f10cc6f1fa7b371d11cac9f42f32`.
ZIP SHA-256 is
`a18ce9b1be2da3ac1c6699a7c9ade79652ef7bd9d5abbca5a7d85f4143ac78f5`.
Core version is `0.13.0-player-r1`, installed as `RPCMP.MinimalPlayer`.

Final `pwsh -File tools/host-verify.ps1 -Mode Full` with LLVM 22.1.8:
**89/89 PASS**, 543.90 s, including formatting, complete static analysis and
architecture/dependency guards. Log: `out/harness/minimal-player-full-final.log`.
The next check is the supplied Firmware 2.6 procedure: list input latency and uninterrupted FM/PCM playback
while drawing, plus switching/stopping/restarting songs. This implementation
does not claim that those new UI/audio integration observations have already
passed on Pocket or complete M6/production-substrate acceptance.

## Minimal Player r1 hardware result — 2026-09-21

The user reports the following results on **Firmware 2.6 / Minimal Player r1**,
following the [Japanese procedure](../development/pocket-minimal-player.md).
This is user-observed hardware evidence for the existing `0.13.0-player-r1`
package; no replacement build or new hardware run was performed by the agent.

| Check | User report |
| --- | --- |
| Startup, silence before A, no autoplay | No problem |
| Six-song list, Japanese titles, up/down and left/right | All OK |
| Each song's music, stereo and PCM | Plays normally |
| Browsing during playback, dropout/noise and input delay | No problem; normal playback |
| A switches to another song during playback, B stops, A restarts from the beginning | OK |
| Normal reboot and power-off startup | OK |
| Errors or other concerns | None reported |

After stopping the third song, the reported observations were **R 7818 us,
F 449 us, V 658 us, Q 1013 frames**. R is the maximum renderer duration, F the
maximum FIFO feed duration, V the maximum presentation-call duration, and Q
the sampled minimum queued native frames. **D (draw-piece duration) was not
reported**; V must not be relabeled as D. These observations cover that reported
run, not six per-song maxima or a proven worst-case service bound.

Together with the unchanged candidate's preceding Full **89/89**, sanitizer,
target-build, RTL and package evidence, this passes the minimal player's
selection/play/stop hardware slice. The missing D value does not contradict
the reported functional pass and does not require a repeat hardware run just
to fill the field. Continue diagnosing concrete playback defects when reported;
do not restart the accepted compatibility campaign. This result does not close
all historical M6/production-substrate timing or external-constraint gates.

The next recommended work is to define the practical M3U8/raw MDX/PDX library
workflow described in the [existing direction](../design/pocket-mdx-compatibility-plan.md#platform-and-library).
HPL1 prepared pairs remain the tested r1 input. Direct M3U loading, path/encoding
rules and dependency resolution have not been implemented or adopted by this
report. Keep advanced visualization and playback-policy expansion deferred.

This update changes progress records and current navigation only. Validation:
`python -B tools/check_harness.py --root .`, changed-document local links and
anchors, and `git diff --check`. No C++/RTL, contract, build, package input or
verification procedure changed; Full, cross-build, synthesis/STA and RTL
simulation are not repeated for this report. The existing Japanese procedure
and the six-song ZIP remain unchanged.

## M3U library import — 2026-09-21

The user confirmed and authorized **M3U on the PC -> resolve MDX/PDX -> generate
Pocket-specific data**. `tools/m3u_import.py` implements that boundary using
Python's standard library and the existing HPL1 writer. It does not change the
accepted Minimal Player r1 binary or require direct M3U parsing on Pocket.
The [Japanese guide](../development/m3u-library.md) explains authoring, conversion,
copying the Assets directory, exclusions and focused playback confirmation.

The importer preserves order and duplicates; supports UTF-8/BOM and explicit
CP932 lists, 9/16-track MDX layouts, MDX-title/filename fallback, local or
explicitly shared PDX directories and bounded LZX expansion. It validates
outer structure and offsets, confines music reads to the chosen root, rejects
case-ambiguous dependencies and lists every excluded entry with its M3U line.
Output I/O errors abort instead of being treated as invalid songs. A fresh
directory and data-only ZIP contain the generated collection, import reports
and Japanese instructions. No successful tracks means reports only and a
nonzero exit, without an installable asset or ZIP.

Validation executed:

- `python -B tests/utility/m3u_import_tests.py --checker
  out/build/host-msvc/rpcmp_minimal_player_tests.exe`: **17 tests**, 16 pass and
  one Linux-specific case/symlink test skipped on Windows. Coverage includes
  authored literal/short/long/overlapping LZX tokens, corrupt inputs, compressed
  MDX plus PDX, title fallback, partial/all-failed imports, size/count/root
  bounds, shared/local PDX precedence, CLI failure, output failure and 300-track
  native reader validation. The wire assertions independently read CRCs,
  offsets, titles and exact driver-header/payload bytes.
- `pwsh -File tools/host-verify.ps1 -Mode Fast`: **89/89 PASS**, 20.44 s.
  The final compressed-input integration case was added afterward and is
  included in the final focused, Linux and Full runs. Fast is not Full.
- Network-disabled `rpcmp-cpu-budget-sim:20260921`,
  `python3 -B tests/utility/m3u_import_tests.py --checker
  /repo/out/harness/minimal-player-sanitized`: **17/17 PASS** on Linux, including
  case collisions and a symlink outside the root. The unchanged native reader
  binary uses ASan/UBSan with leak detection and halt-on-error. No sanitizer
  diagnostic was reported. Log: `out/harness/m3u-import-linux.log`.
- `python -B tools/m3u_import.py out/build/m3u-input-r1/accepted-six.m3u8
  --root <local music root> --output out/build/m3u-library-r1-cli`: **6 imported,
  0 excluded**, using only the previously accepted real songs. Two source blobs
  are LZX-compressed. Each prepared MDX/PDX and title equals the earlier native
  reference preparation; the **entire HPL1 file is byte-identical** to accepted
  Minimal Player r1. ZIP/directory contents and the bundled Japanese guide were
  read back byte-for-byte, and `rpcmp_minimal_player_tests.exe --playlist` reads
  all six final payloads successfully. The root argument is redacted here;
  source identities and generated files remain under ignored local output.

The local deliverable is `out/build/m3u-library-r1-cli.zip` (collection update,
not a new core). ZIP SHA-256:
`0c9ca19d52b7416cae3e52785f1dd75ccce87752aea4f451131f5a461a43ffbd`.
HPL1 SHA-256:
`c37fd15bcff36a9c1d18373ed631a5ede236565fb3c1a51aa3a8e420e55e7ee4`.

Final `pwsh -File tools/host-verify.ps1 -Mode Full` with LLVM 22.1.8:
**90/90 PASS**, 546.61 s (tidy 526.82 s), including formatting, static analysis,
architecture/dependency guards and the new import test. Log:
`out/harness/m3u-import-full.log`. `python -B tools/check_harness.py --root .`,
five changed local links/anchors and `git diff --check` also pass.
C++/RTL, firmware, memory placement and runtime audio behavior are unchanged,
so target rebuilding, RTL simulation and synthesis/STA are not applicable to
this import slice. No new hardware result is claimed. The byte-identical six-song
asset does not need another compatibility campaign; the next useful check is
the user's own playlist and its reported exclusions. This does not certify
all sequence commands or all files in the collection. M6's remaining substrate
gates and optional album/visualization features remain separate.

## M3U 27-track hardware result — 2026-09-21

The user reports the following results for **Firmware 2.6 / Minimal Player r1 /
M3U import**, using their chosen playlist:

| Check | User-reported result |
| --- | --- |
| Imported / excluded tracks | 27 / 0 |
| M3U order / Japanese track names | OK |
| FM / PCM / stereo channels | OK |
| List navigation during playback / dropouts / noise | No problems |
| A switches tracks / B stops / A restarts from the beginning | OK |
| Errors or other observations | None reported |

This accepts the connected **PC M3U -> prepared MDX/PDX collection -> Pocket
selection/play/stop** slice on the existing r1 player. The report does not
provide per-track timings, playback durations or an asset hash, and does not
repeat the earlier reboot/power-cycle checks. It establishes the reported
playlist behavior, not exhaustive compatibility with every source file.
Keep the accepted player/importer as the baseline and choose the next small
requirement with the user; remaining M6 substrate gates and deferred features
are not completed by this report.

This update changes progress records and current navigation only. Validation:
`python -B tools/check_harness.py --root .`, changed-document local links and
anchors, and `git diff --check`. No code, contract, package input or verification
procedure changed. Full, cross-build, RTL simulation and synthesis/STA are not
repeated for this report; their inputs are unchanged.

## Continuous playback requirements — 2026-09-21

After the accepted 27-track M3U report, the user chose continuous playback as
the next feature and confirmed its manual-selection, error and browsing rules.
The [active boundary and acceptance checks](../design/pocket-mdx-compatibility-plan.md#continuous-playback-boundary)
are the implementation target: selected entry onward in M3U order, two loop
bodies plus five-second fade for looping songs, natural endings once, A to
restart the run from another selection, B to stop, recoverable failed-track
skipping, list-end stop and independent browsing cursor/playing marker.

This records approved requirements, not a new player build or hardware result.
The PRD and current navigation now point to this slice. It supersedes r1's
stop-on-natural-end policy while retaining HPL1 input and terminal failure
handling. Full is due when the connected transition behavior in the active
boundary works; implementation and hardware acceptance remain pending.

Requirement review checked the Q1–Q4 decisions against the existing r1 boundary,
including terminal failures and Core/UI ownership. Validation passed:
`python -B tools/check_harness.py --root .`, six changed local links/anchors,
`git diff --check`, and `python -B tests/architecture/check_dependencies.py`
with `--root .` and with `--root tests/architecture/fixtures/runtime_depends_ui
--expect-violation`. The latter detects the intentional Core-to-UI violation.
This adds prospective product requirements and acceptance checks; it does not
change executable code, test oracles, build settings or generated assets. Full
and hardware checks are not run at requirements completion and do not establish
the new behavior until implementation. RTL/cross-build/synthesis inputs are
unchanged in this update.

## Continuous playback implementation — 2026-09-21

Minimal Player r2 は、承認済みの連続再生を実装した実機確認候補です。
選択曲からM3U順に進み、ループ曲はイントロ1回＋2周後にFM・PCMを5秒で
フェードします。自然終了は1回、末尾は停止、曲単位の失敗は記録して飛ばします。
Aで新しい再生を開始し、Bで自動送りも解除します。一覧の位置と再生中の印・曲名・
曲番号は独立しています。読み込み中には曲間の無音があり、ギャップレス再生ではありません。

変更境界は [active design](../design/pocket-mdx-compatibility-plan.md#continuous-playback-boundary)
に先に記録しました。Coreの小さな状態機械で1回のserviceにつき最大1曲を開始し、
連続失敗中も入力を処理します。状態スナップショットはversion 2です。
音声の新しい制御マーカーに合わせ、MMIO IDをHYB2 (`0x48594232`) に更新しました。
5秒は62.5 kHzで312,500フレームです。HPL1、M3U取り込み、APF framework最低2.2は
変更していません。後方互換用の分岐や追加依存は導入していません。

今回実行した検証と、その限界:

- 変更前の自動送り試験は4個のassertionで失敗し、実装後に通りました。
  最終版のnative試験は300曲の順送り・全曲失敗・停止による取消・リセット失敗・
  空の自然終了・カーソル/ページ保持を確認しました。最終表示画像でも、選択曲1と
  再生曲2の印・曲名・番号が分離していることを確認しました。
- LLVM 22.1.8をこのプロセスのPATHに指定した
  `pwsh -File tools/host-verify.ps1 -CheckSetupOnly` はPASSです。
  途中の `pwsh -File tools/host-verify.ps1 -Mode Fast` は89/89、26.74秒でした。
  最終版の `pwsh -File tools/host-verify.ps1 -Mode Full` は**90/90 PASS**、
  681.37秒（tidy 654.60秒）でした。format、静的解析、Core→UI依存の拒否fixtureを
  含むアーキテクチャ検査も通っています。ログは `continuous-full-final.log` です。
- 最終ソースに対するLinuxのASAN/UBSAN試験はPASSです。
  `docker run --rm --network none --mount type=bind,source=F:/source/rpcmp,target=/repo
  -w /repo rpcmp-cpu-budget-sim:20260921 python3 -B out/harness/minimal-player-native.py`
  でcatalog/control/audio/displayと2/300曲のPC生成→native読込を確認し、
  パッケージ試験6件もPASS（0.105秒）でした。参照レンダラーの個別clang-tidyもPASSです。
- Linux上の `python3 -B tests/pocket/hybrid_renderer_tests.py --library
  out/build/minimal-player-cpu-r2/renderer.so` は自作MDXの3試験にPASSしました。
  曲中の独立したマーカーでイントロ1回・ループ本体2回を数え、終了位置が
  フェード開始＋312,500であること、自然終了と再開時の状態初期化を確認しました。
  この試験は準備済みの参照エンジンを使う任意実行試験で、標準CTestとは別です。
- 同じ6曲の既存参照データに対する `tools/hybrid_renderer_verify.py` はPASSです。
  `--library out/build/minimal-player-cpu-r2/renderer.so --oracle
  out/build/hybrid-real-inputs-r2 --manifest out/build/hybrid-real-inputs-r2/private-inputs.json`
  を指定し、Dockerで元データのルートを `/corpus` に読み取り専用でマウントしました。
  自作3ケース、および実曲6曲それぞれの冒頭20.48秒でPCM・FMイベント位置/順序が一致しました。
  期待値は再生成していません。別のローカル試験で6曲とも終了に到達し、4曲は
  フェード開始＋312,500フレーム、2曲はフェードなしの自然終了を確認しました。
  これはホスト上の結果であり、Pocketの音やタイミングの観測ではありません。
- `pwsh -File tools/rtl-hybrid-verify.ps1` はPASSです。既存ミキサー8ケースと
  フェード全312,501点、3位相のI2S境界・フレーム5開始のフェード・リセット解除・
  不正/重複制御・FIFO容量/待ち・供給枯渇・実8音FMの試験が通りました。
  `-PreparedTree out/build/continuous-candidate-r2` を指定する実AXI試験も3位相PASSです。
  `pwsh -File tools/rtl-verify.ps1` と `pwsh -File tools/rtl-jt51-verify.ps1` もPASSです。
  共有作業領域を使うRTLスイートは順番に実行しました。
- Linux上の `python3 -B tools/hybrid_renderer_build.py --output
  out/build/minimal-player-cpu-r2` は最終版でPASSです。text 457,616、data 79,012、
  BSS 169,344、合計705,972 bytes、保守的スタック13,424 bytes、最大フレーム4,880 bytes。
  動的スタックと未解決シンボルはありません。ELF SHA-256は
  `19312dffe64f9cb7eb042b27d60926d27cd068336215e43812bd4c70cbe7e137` です。
- 対応するROM/OSを `tools/pocket_hybrid_firmware_prepare.py` で準備し、SDKの
  `make -j2 CROSS=riscv-none-elf- bld/pocket/firmware.elf bld/pocket/os.bin` と
  `tools/pocket_hybrid_images.py` で生成・照合しました。OSのバイト列はr1と同じです。
  `tools/pocket_hybrid_prepare.py` で用意した `continuous-candidate-r2/hybrid-fit` の
  Quartus全体コンパイルは9分20秒、0 errors / 968 warningsでした。
  10,913/18,480 ALMs、12/66 DSP、最小setup 0.749 ns / hold 0.127 nsです。
  同じfitで `quartus_sta -t F:/source/rpcmp/tools/hybrid_cdc_audit.tcl` がPASSし、
  4コーナーの48 Grayポインタビットと同期段を確認しました。既存の外部I/O制約の
  未検証部分は残っており、これを基板全体のタイミング保証とはしていません。
- [日本語手順](../development/pocket-minimal-player.md#開発者向けパッケージ生成) の
  コマンドで全体ZIPと更新ZIPを生成しました。独立した読み戻しでも、全体29ファイルと
  更新21ファイルの内容、RBFのビット反転、最終ELF/OS、JSONのslot ID、6曲のHPL1、
  同梱手順の一致を確認しました。更新ZIPに曲集/WAV/曲目一覧は入りません。
  更新を既存曲集へ重ねる自動試験もPASSです。27曲の実機曲集自体を再読込した結果ではなく、
  更新物が `playlist.hpl` を置き換えないことの検証です。

ログはローカルの `out/harness/continuous-*.log`、fit/CDCは
`out/build/continuous-candidate-r2/hybrid-fit`、パッケージ証跡は
`out/build/minimal-player-r2.evidence.json` にあります。私有楽曲・曲名・波形はコミットしていません。
結果記録後は進捗・リンクのみを更新し、harness navigation、変更したローカルリンク/
アンカー12件、`git diff --check` を確認しました。Full以降のコード変更はありません。

- 推奨更新ZIP: `out/build/minimal-player-r2-update.zip`、SHA-256
  `2407ea920be5bd5b71e269b15f0fe866654ac820f1388bebf310cac974f66cda`。
- 6曲入り全体ZIP: `out/build/minimal-player-r2.zip`、SHA-256
  `2b0e4f3728b096491fb64ac78e2ae75ac1ad1b3207078dd68c01d835722791dd`。

**Firmware 2.6でのr2実機確認は未実施です。** 次は更新ZIPを使用し、日本語手順に沿って
自動送り、2周＋5秒フェード、FM/PCM、一覧位置保持、手動切替/停止、再起動を確認します。
互換性調査全体は再開せず、新しい不具合が出た場合に該当経路を解析します。

## Minimal Player r2 hardware result — 2026-09-22

ユーザーから **Firmware 2.6 / Minimal Player r2** の実機報告を受領しました。

| 確認項目 | ユーザー報告 |
| --- | --- |
| 曲数表示 / 起動時の無音 / 日本語の曲名 | OK、問題なし |
| 選択曲からM3U順の自動送り | OK、問題なし |
| ループ曲のイントロ1回＋2周 / 約5秒フェード / FM・PCM両方 | OK、問題なし |
| 自然終了の曲は1回 / 最後の曲で停止 | OK、問題なし |
| 自動送り時のカーソル・ページ保持 / 再生中の印・曲名 | OK、反映されている |
| 再生中・フェード中のA切替 / B停止 / Aで先頭から | OK、問題なし |
| FM / PCM / 左右 / 音切れ・ノイズ / 入力遅延 | OK、問題なし |
| エラー曲のスキップ | エラー未発生のため実機では未確認 |
| 通常再起動 / 電源OFF後 | OK、問題なし |
| エラー番号・その他の気になる点 | 特になし |

停止後の表示は **R 12755 us / F 478 us / D 1554 us / V 664 us /
Q 947 frames** でした。R/F/Qは直近の再生、D/Vは起動後の最大値で、
Qは62.5 kHzでの最小キュー残量です。今回の報告には数値としての曲数、
測定対象曲、再生時間、使用アセットのハッシュは含まれていません。

この報告で **M3U順の連続再生スライスを実機合格** とし、r2を受入済みの基準にします。
エラー曲のスキップは異常がなかったため実機合格とはせず、
[実装時のホスト試験](#continuous-playback-implementation--2026-09-21) と区別して記録します。
M6全体や残る外部I/Oタイミング・本番基盤のゲートを完了したという意味ではありません。
次はユーザーと次の小さな要件を決めます。追加機能はまだ選定していません。

今回は実機結果と進捗・ナビゲーションのみの更新です。
`python -B tools/check_harness.py --root .`、変更したローカルリンク/アンカーの検査、
`git diff --check` を実施しました。実装・契約の動作・受入条件・パッケージ入力・
確認手順は変更していません。実装時のFull 90/90、Core→UI依存拒否、RTL、
クロスビルド、合成/CDCの結果は上記の実装証跡を参照してください。
これらは入力が変わっていないため今回再実行していません。

## Pause and resume requirements — 2026-09-22

ユーザーは次の機能に一時停止・再開を選び、当面はXに割り当てると決定しました。
UI整備後にはボタン割り当てを変更する見込みです。
[今回の要件境界](../design/pocket-mdx-compatibility-plan.md#pause-and-resume-boundary) に、
FM/PCM・ループ/フェードの位置保持、一覧操作、Aで選択曲の先頭から再生、Bで停止、
入力と再生処理の分離を記録しました。既存の小さな入力割り当てを拡張する方針で、
ユーザー向けキー設定画面や汎用設定フレームワークは追加しません。

これは要件の決定であり、実装・実機合格ではありません。r2が受入済みの基準です。
新しい再生コマンドからFM/PCMの一時停止・再開・曲末尾への接続が動いた時点を
Fullの区切りとし、影響するRTL・クロスビルド・合成/CDC・パッケージも検証します。

今回の影響確認は要件と既存の入力/コマンド境界の照合、
`python -B tools/check_harness.py --root .`、変更したローカルリンク/アンカーの検査、
`git diff --check`、`python -B tests/architecture/check_dependencies.py --root .`、
同コマンドの `--root tests/architecture/fixtures/runtime_depends_ui --expect-violation`
です。実行コード・試験の期待値・生成入力は変更しておらず、Fullは新動作の実装時に
実行します。RTL・クロスビルド・合成は今回入力が変わらないため実行していません。

## Pause and resume implementation — 2026-09-22

Minimal Player r3 は、Xによる一時停止・同じ位置からの再開を実装した実機確認候補です。
FM・PCM、ループ/フェード位置、処理待ちの音声を保持し、一時停止中は無音にします。
一覧操作は継続でき、Aで選択曲の先頭から新しく再生、Bで停止します。
CoreのTogglePauseと物理入力は分離し、既存の小さなBindingsにX割り当てを追加しました。
表示は「一時停止中」とし、保持中の曲の印・曲名と一覧の選択位置を分けています。

[実装前に記録した境界](../design/pocket-mdx-compatibility-plan.md#pause-and-resume-boundary) に従い、
スナップショットをversion 3、音声インターフェースをHYB3 (`0x48594233`) にしました。
ステレオフレーム境界で音声側の状態を保持し、I2Sクロックは動かしたまま無音を出します。
既存の固定版JT51のhold変換とwide出力を組み合わせ、CPUの未転送ブロックと
タイムアウト計測も一時停止をまたいで保持します。新しい依存や設定フレームワークはありません。
HPL1、PCのM3U取り込み、APF framework最低2.2は維持しています。

実行した検証:

- 最初に追加したX操作の試験は、状態が再生中のまま・生成が進む、の2箇所で失敗しました。
  実装後は、起動時の押しっぱなし、長押し、一覧移動、再開、A/Bの優先、割り当て変更、
  起動前/転送待ち/EOF待ちの一時停止、長い待機、応答タイムアウト・音声エラーを確認しました。
- LLVM 22.1.8で `pwsh -File tools/host-verify.ps1 -CheckSetupOnly` はPASS。
  `-Mode Fast` は89/89、24.81秒。`-Mode Full` は **90/90 PASS**、815.53秒
  （tidy 790.51秒）でした。format、静的解析、Core→UI依存拒否fixtureを含む検査もPASSです。
  ログは `out/harness/pause-fast.log`、`pause-full.log` です。
- `docker run --rm --network none --mount type=bind,source=F:/source/rpcmp,target=/repo
  -w /repo rpcmp-cpu-budget-sim:20260921 python3 -B out/harness/minimal-player-native.py`
  は最終の表示試験を含めASAN/UBSANでPASSし、曲集パッケージ6試験もPASS（0.104秒）。
  同じDocker条件で `python3 -B tests/pocket/hybrid_tool_tests.py` は5/5 PASS（11.336秒）です。
  新しい生成物の再現性・固定版/範囲の検査、従来の比較用FM生成、HYB3 ROMの対応を確認しました。
- ホストの実描画から生成した `out/harness/pause-preview.png` で、Xの操作案内、
  「一時停止中」、保持曲2の印・番号と選択曲1の独立した表示を確認しました。
- `pwsh -File tools/rtl-hybrid-verify.ps1` は3位相（0/5555/81379 ps）でPASS。
  従来の符号境界・フェード・FIFO満杯/枯渇・実8音FMとPCMの検査に加え、
  通常再生の768ステレオフレームと10回の一時停止を挟んだ出力が、無音区間を除いて一致しました。
  生きている音声を一時停止したままClear/電源リセットする試験を追加し、
  `pwsh -File out/harness/pause-final-rtl.ps1` で最終fixtureを再コンパイルして3位相PASS。
  リセット時に受信器も再同期するよう直し、通常動作中のLRCK連続性の検査は維持しました。
- 同じ逐次ランナーから `tools/rtl-hybrid-verify.ps1 -PreparedTree out/build/pause-candidate-r3`
  を実行し、実AXIの一時停止・再開と125フレームの保持が3位相PASS。
  続けて `tools/rtl-verify.ps1`、`tools/rtl-jt51-verify.ps1`、
  `tools/rtl-jt51-hold-verify.ps1` がPASS。最後の固定版FM比較は256位相・855回の停止を含みます。
  RTLスイートは共有作業領域のため順番に実行しました。
- Dockerで `python3 -B tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r3`
  がPASS。text 458320 / data 79012 / BSS 169344、合計706676 bytes、
  保守的スタック13472 bytes、最大フレーム4912 bytes、動的スタックなし。
  ELF SHA-256: `e0d1e05a6789567ec6f61492a6a23223efa9eb1a88490252f34781006f9b41e7`。
- `tools/pocket_hybrid_firmware_prepare.py` で `pause-firmware-r3` を準備し、Docker内の
  `out/harness/pause-firmware-build.py` からSDK make、objcopy、
  `tools/pocket_hybrid_images.py` を実行しました。15524-byte ROM/MIF/OSの対応はPASS、
  OSはr2と同一です。`tools/pocket_hybrid_prepare.py` で対応する `pause-candidate-r3` を準備しました。
- `pause-candidate-r3/hybrid-fit` で `quartus_sh --flow compile ap_core` は9分26秒、
  0 errors / 968 warnings。10960/18480 ALMs、12/66 DSP、最小setup 0.766 ns / hold 0.058 ns。
  `quartus_sta -t F:/source/rpcmp/tools/hybrid_cdc_audit.tcl` は追加したpause/status同期段と
  全48 Grayポインタビットを4コーナーで検査してPASS。既存の外部I/O制約の未検証部分は残るため、
  基板全体の本番タイミング保証とは区別します。fit/CDCログは同じ候補のディレクトリにあります。
- [日本語手順](../development/pocket-minimal-player.md#開発者向けパッケージ生成) の
  `tools/pocket_hybrid_package.py` コマンドで全体ZIPと更新ZIPを生成しました。
  `python -B out/harness/pause-package-readback.py` はPASS。全体29・更新21ファイル、
  最終ELF/OS・RBFビット反転・Xの入力定義・framework 2.2・同梱手順を独立して読み戻しました。
  6曲のHPL1はr2と同一でnative readerもPASS。更新ZIPには曲集/WAV/曲目一覧を含めません。
  ユーザーのSD上の27曲を直接読み戻したという意味ではありません。

推奨更新物は `out/build/minimal-player-r3-update.zip`（1,184,730 bytes）、SHA-256:
`cc3997ee4a4366ad45140b51f753d70fc8e9d90dddcb2aeab3f13ed73eeedc29`。
6曲入り全体ZIPは `out/build/minimal-player-r3.zip`、SHA-256:
`ffb344b3ff7c5ad5956cca089498fbcb094e68f9a423328b548903c41d601658`。
詳細ログは `out/harness/pause-*.log`、パッケージ証跡は
`out/build/minimal-player-r3.evidence.json` に保存しています。

**r3の実機確認は未実施です。** 次は更新ZIPと日本語手順で、FM/PCMの一時停止・再開、
フェード途中の操作、停止・曲切替、再起動を確認します。r2を受入済みの基準に残します。
今回、参照レンダラーやMDX/PDX取り込みは変更しておらず、互換性調査全体は再開していません。
私有楽曲・曲名・波形はコミットしていません。Full後の本番コード変更はなく、追加したRTLの
取消試験は上記の最終ランナーで確認済みです。進捗・リンク更新後にharness navigation、
変更したローカルリンク/アンカー、`git diff --check` を確認しました。

## Minimal Player r3 hardware result — 2026-09-22

ユーザーから Firmware 2.6 / Minimal Player r3 の実機結果を受領しました。
27曲の一覧・起動時の無音・日本語表示に問題はなく、以下すべてOKとの報告です。

- FM曲のX一時停止、無音、状態表示、同じ位置からの再開。
- PCM曲の一時停止、約1分待ってからの再開、左右の音。
- 一時停止中の一覧移動と、Xで保持中の曲を再開する操作。
- 一時停止中のAによる選択曲再生、B停止、停止後のXで無音を維持、Aで先頭から再生。
- フェード途中の一時停止、残りのフェードの再開、その後の次曲への移行。
- X長押し・繰り返し操作、曲末尾付近の操作。
- 再開時の音切れ・ノイズ・位置の飛び・テンポ、入力遅延に問題なし。
- M3U順の自動送り、2周＋5秒フェード、自然終了、末尾で停止。
- 自動送り時のカーソル・ページ保持、再生中/一時停止中の印と曲名の表示。
- 一時停止からの通常再起動、電源OFF後の起動時の無音と再生。

停止後の観測値は **R 5421 us / F 494 us / D 1423 us / V 666 us / Q 960 frames**。
R/F/Qは直近の再生、D/Vは起動後の計測です。対象曲・条件の揃った比較ではないため、
r2からの性能改善率や全27曲の最大負荷を示す値とは扱いません。
エラー曲スキップの項目は今回未報告です。実機での異常曲試験の合格は追加せず、
既存のホスト試験をその経路の証拠として維持します。

この報告により、承認済みの一時停止・再開スライスを合格とし、**r3を受入済みの基準**に
更新します。[実装時のFull・RTL・ビルド・パッケージ検証](#pause-and-resume-implementation--2026-09-22)
と実機報告を合わせた判断です。M6全体、既存の外部I/Oタイミング制約や基板全体の
本番受入を完了したという意味ではありません。次は小さな追加機能の要件決めです。

今回の変更は結果と進捗リンクのみです。コード・仕様の動作・確認手順・配布物は変更せず、
`python -B tools/check_harness.py --root .`、
`python -B out/harness/continuous-document-links.py`（変更したリンク/アンカー5件）、
`git diff --check` はPASSです。Full・RTL・クロスビルド・合成は入力が変わらないため
再実行しません。実機結果はユーザーの報告であり、エージェントによる再測定ではありません。

## Loop and repeat implementation — 2026-09-22

ユーザーが承認した [ループ・リピート切り替え](../design/pocket-mdx-compatibility-plan.md#loop-and-repeat-switching-boundary)
をMinimal Player r4に実装しました。Yで2周→3周→5周→無限を切り替え、画面上部に
現在の設定を表示します。曲・停止をまたいで設定を維持し、毎起動2周・無音に戻します。
回数指定では自然終了曲を1回だけ再生し、無限では自然終了曲も先頭から繰り返します。
再生中・一時停止中に変更でき、回数到達済みならフェード、不要になったフェードは
最大20 msで音量を戻して取り消します。終了条件が続く変更では残りのフェードを維持します。
エラー曲は無限設定でも再試行せず次へ送り、Bは自動送り・リピートを取り消します。

Coreのsnapshot version 4にRepeatModeとCycleRepeatを追加しました。UIの既存Bindingsに
Yを加え、Core/音源側に物理ボタンを渡していません。CPUは各周回を通知し、音声側の
小さなenvelopeが再生済みの周回・フェード・終了を判断します。HYB4は`0x10001`を
周回通知に変更し、command 16の1ビット往復でモードを巡回します。Clear後は2周に戻し、
次曲のStart前にCoreの設定を再適用します。読み込み済みの音声を捨てて設定変更する
処理や互換経路は追加していません。M3U/HPL1、参照エンジンの音源生成、新規依存は変更なしです。

実行した検証（ログと生成物はignoredの`out/`）:

- LLVM 22.1.8で `pwsh -File tools/host-verify.ps1 -CheckSetupOnly` はPASS。
  `-Mode Fast` は89/89、26.89秒。最初のFullは不正enumの直接castを作るテストで
  静的解析が失敗しました。既存の境界試験と同じく不正なバイト表現をmemcpyで作る形にし、
  不正値の拒否確認は維持しました。対象テストの個別clang-tidyはPASS。
  修正後の `pwsh -File tools/host-verify.ps1 -Mode Full` は **90/90 PASS**、
  564.05秒（tidy 542.41秒）。`out/harness/repeat-full-final.log`に記録しました。
  書式・静的解析・Core/UI依存境界の検査も含みます。
- Dockerの `rpcmp-cpu-budget-sim:20260921`、network none、`/repo`マウントで
  `python3 -B out/harness/minimal-player-native.py` はASAN/UBSAN込みでPASS。
  Yの押下/長押し/再接続/割り当て変更、停止・選曲後の設定、末尾曲リピート、
  リピート解除、再読込失敗時のスキップ、300曲全件失敗の終了、応答タイムアウト、
  描画なし/遅延描画の音声制御列一致を確認しました。PC生成→native読込は2曲/300曲、
  パッケージ試験6件もPASS（0.099秒）。
- `tests/pocket/hybrid_renderer_tests.py --library out/build/minimal-player-cpu-r4/renderer.so`
  は自作MDXの3試験PASS。曲中の独立したイントロ/本体マーカーと各周回通知の位置が
  一致し、CPUが固定2周で打ち切らないこと、自然終了と再ロード初期化を確認しました。
  `tools/hybrid_renderer_verify.py` は既存参照データと自作3ケース・実曲6曲でPASS。
  実曲は各冒頭20.48秒でFMイベント位置/順序とPCMが一致。新しい周回通知は独立した
  上記試験で検証し、参照比較は従来のFM操作を比較します。期待音声・FM traceは再生成していません。
  最終バイナリのログは`repeat-renderer-final.log`、`repeat-reference-final.log`です。
- `pwsh -File tools/hybrid-renderer-tidy.ps1 -PreparedRenderer out/build/minimal-player-cpu-r4`
  はPASS。`tests/pocket/hybrid_tool_tests.py` は5件PASS（9.297秒）。対応ROM・MIFと
  HYB3不一致時の拒否、固定済みJT51生成/ライセンス記録を確認しました。
- `pwsh -File tools/rtl-hybrid-verify.ps1` は3位相（0/5555/81379 ps）でPASS。
  新しい`hybrid_envelope_tb`は2・3・5周それぞれ312,500サンプル全点のゲインと終了を
  独立の整数式で確認し、無限・回数到達後変更・フェード維持/解除/音量復帰・一時停止を検証。
  `out/harness/repeat-final-rtl.ps1`で追加した試験と実AXI・baseline・JT51を逐次実行してPASS。
  768ステレオフレームは10回の停止/再開・40回の設定応答を挟んでも連続再生と一致しました。
  自動終了後のdrain・無音・遅れて到着したCPU書込はenvelope完了信号を注入して確認しました。
  これは上記の全期間カウンター検証と組み合わせた境界試験であり、実音源を5秒間流した
  通しシミュレーションではありません。実AXIは3位相とも125 PCMフレーム、301書込でPASS。
- Dockerの `tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r4` はPASS。
  RV32はtext 459,280、data 79,020、BSS 169,344、static合計707,644 bytes。
  保守的stack上限13,600、最大frame 4,976 bytes、動的stack・未解決symbolなし。
  ELF SHA-256: `87114f67f474c9c131a6865abb6dd406a9843e25a3682a6ed0d9822f743b8416`。
- `tools/pocket_hybrid_firmware_prepare.py`と`out/harness/repeat-firmware-build.py`で
  `repeat-firmware-r4`を生成。boot 15,524 bytes、ROM/MIF/OSの対応確認はPASS、OSはr3と同一。
  `tools/pocket_hybrid_prepare.py`で`repeat-candidate-r4`を準備しました。
  `hybrid-fit`の `quartus_sh --flow compile ap_core` は9分25秒、0 errorsで成功。
  ALM 10,993/18,480（59%）、DSP 12/66、最小setup 0.749 ns・hold 0.087 ns。
  `quartus_sta -t F:/source/rpcmp/tools/hybrid_cdc_audit.tcl` は4条件のrepeat/pause/status同期段と
  全48 Gray-pointer bitでPASS。既存の外部I/O制約の未検証事項を解決した結果ではありません。
- [日本語手順](../development/pocket-minimal-player.md)の生成コマンドでパッケージ化し、
  `out/harness/repeat-package-readback.py`で全体29/更新21ファイルの完全一致、実行ファイル・OS・
  bit反転RBF・Y定義・framework.version_required=2.2・slot 1〜4を確認しました。
  HPL1はr3の6曲と同一で、更新ZIPにはHPL1・WAV・曲目一覧を含みません。
  これは生成したZIPの検査で、ユーザーのSD上の27曲を再測定したものではありません。
  描画コードから生成した`repeat-preview.png`も確認し、無限表示・一時停止・選択と再生印の
  分離・日本語表示に欠けや重なりはありませんでした。

候補は `out/build/minimal-player-r4-update.zip`（1,185,340 bytes、SHA-256
`dde4c77bbb36bc78b42fb74abf8e0270689e30352d09a6d5811a7f0ea4de126a`）。
6曲入り全体ZIPは `out/build/minimal-player-r4.zip`（16,831,818 bytes、SHA-256
`b5e6239b8c48e88a341c314fabc8c350b40bb614f3020310c6eb87a3f0b317ee`）です。
core `0.16.0-player-r4`、HYB4のFPGA/起動ROM/アプリを組み合わせて更新します。
私有楽曲・曲名・波形はコミットしません。実機でのr4確認は未実施で、r3が受入済みの基準です。
次は手順に沿ったループ回数・無限・一時停止/フェード中変更・音・再起動の実機確認です。
広範な互換性再調査、変更していないJT51 hold変換の専用回帰は再実行していません。

最終結果の進捗反映後、`python -B tools/check_harness.py --root .`、
`python -B out/harness/continuous-document-links.py`（HEADとの差分のリンク/アンカー7件）、
`git diff --check` はPASSです。この追記は結果・進捗だけで、仕様・確認手順・コード・
配布物は変えていません。入力が同一のFull・RTL・クロスビルド・合成は再実行しません。

## Minimal Player r4 hardware result — 2026-09-22

ユーザーの **Firmware 2.6 / Minimal Player r4** 報告を受領しました。
以下はユーザーによる実機確認で、エージェントの再測定ではありません。

- 曲一覧、起動時の無音・2周表示、日本語の曲名は問題なし。曲数の具体的な数字は未報告。
- Yで2→3→5→無限→2、停止中の無音、Y長押しは問題なし。
- ループ曲の2・3・5周、約5秒フェード、FM・PCM両方は問題なし。
- 無限で2周以上継続し、2周へ戻すと位置を保ってフェードする動作は問題なし。
- フェード中に3周・無限へ変更すると音量が戻り、その位置から継続する動作は問題なし。
- 一時停止中のY切替、無音・位置保持、新設定での再開は問題なし。
  フェード中のX一時停止・再開と残りのフェードも問題なし。
- 自然終了曲は回数設定で1回、無限で先頭から繰り返す動作は問題なし。
  M3U末尾では回数設定で停止し、無限で同じ曲を繰り返す動作も問題なし。
- Aで選択曲へ、B停止、停止後Xで無音、Aで先頭から再生は問題なし。
  停止・選曲・自動送りでの設定保持と、カーソル・ページ保持も問題なし。
- FM・PCM・左右、音切れ・ノイズ・位置の飛び・テンポ・入力遅延は問題なし。
- 通常再起動・電源OFF後の無音、2周への設定リセット、再生は問題なし。

停止後の観測値は **R 5161 us / F 434 us / D 1307 us / V 671 us / Q 1023 frames**。
R/F/Qは直近の再生、D/Vは起動後の計測です。曲・条件を揃えた比較ではないため、
r3からの性能改善や全曲の最大負荷を示す値とは扱いません。
エラー曲スキップは今回未報告です。実機での異常曲試験の合格は追加せず、
既存のホスト試験をその経路の証拠として維持します。

[実装時のFull・RTL・ビルド・パッケージ検証](#loop-and-repeat-implementation--2026-09-22)
と今回の実機報告を合わせ、承認済みのループ・リピート切り替えスライスを合格とし、
**r4を受入済みの基準**に更新します。M6全体や、既存の外部I/O制約を含む基板全体の
本番受入を完了したという意味ではありません。次は小さな追加機能の要件決めです。

今回の変更は実機結果と進捗リンクのみです。コード・仕様の動作・確認手順・配布物は
変更していません。Full・RTL・クロスビルド・合成は入力が変わらないため再実行せず、
`python -B tools/check_harness.py --root .`、
`python -B out/harness/continuous-document-links.py`（変更リンク/アンカー8件）、
`git diff --check` はPASSです。

## Multiple-playlist implementation — 2026-09-22

[Q1–Q6の複数プレイリスト要件](../design/pocket-mdx-compatibility-plan.md#multiple-playlist-requirements)
をMinimal Player r5に実装しました。複数M3Uの指定順取り込みとフォルダーの番号順取り込み、
日本語リスト名、リスト一覧→曲一覧、各ページの戻る行、リストごとの閲覧位置保持に対応します。
別リストの閲覧は再生を変更せず、曲をAで決定したときだけ再生元を切り替えます。
自動送り・失敗曲スキップは再生元リスト内に限定し、末尾で停止します。
B停止・X一時停止・Yリピート設定と、起動時の無音・2周設定を維持しています。

変更境界は事前に記録したHPL2です。最大100リスト×300登録のインデックスを1個だけ保持し、
閲覧中はストレージを読みません。曲のMDX/PDXだけを選択時に読みます。PCは同じ準備済み
データを共有し、ペイロードを逐次書き出します。1曲16 MiB、全体2 GiB未満の上限を維持し、
不正な個別曲は理由付きで除外、曲集全体の上限超過は失敗にします。
**HPL1とは非互換**で、既存M3Uはr5の取り込みツールで作り直します。Minimal snapshotは
version 5（再生元PlaylistId追加）、準備済みマニフェストはname/tracksを持つリストの配列です。
HYB4・FM/PCMレンダラー・FPGA・起動ROM・OS・依存ライブラリは変更していません。

実行した検証（ログ・私有入力・生成物はignoredの`out/`）:

- LLVM 22.1.8のPATHで `pwsh -File tools/host-verify.ps1 -CheckSetupOnly` はPASS。
  `-Mode Fast` は89/89 PASS。最初のFullは89/90で、配列オフセットとテストの画像サイズの
  乗算前に型を拡張するよう静的解析が指摘しました。警告抑止を追加せずsize_tで演算する形に
  修正し、対象2ファイルのclang-tidyはPASS。修正後の
  `pwsh -File tools/host-verify.ps1 -Mode Full` は **90/90 PASS**、690.17秒
  （tidy 644.70秒）。書式・静的解析・Core/UI依存境界の検査も含みます。
  最終ログは`out/harness/playlists-full-final.log`です。
- Docker `rpcmp-cpu-budget-sim:20260921`（network none、リポジトリを`/repo`へマウント）で
  `python3 -B out/harness/minimal-player-native.py` はASAN/UBSAN込みでPASS。
  100×300のID境界、再生・一時停止中の別リスト閲覧、元リスト内の自動送り、末尾停止、
  カーソル復元、再起動、既存の停止/一時停止/リピート回帰を確認しました。
  実際のHybridPlaybackを通る音声制御列は、描画なしと別リストの遅延描画で一致し、
  閲覧による曲データの追加読み込みはありません。パッケージ7件もPASS（0.145秒）。
  `tests/utility/m3u_import_tests.py` は実C++ readerを接続して21件PASS（8.221秒、skipなし）。
  個別/一括取り込み、順序・重複・共有、100×300、範囲外/壊れた入力、リンク脱出・
  大文字小文字衝突、出力失敗を確認しました。ログは`playlists-native-final.log`、
  `playlists-import-native-final.log`です。
- 最終ビルドの `tests/pocket/hybrid_renderer_tests.py --library out/build/minimal-player-cpu-r5/renderer.so`
  は3件PASS。`tools/hybrid_renderer_verify.py` は自作3ケースと既存実曲6曲でPASS。
  実曲は冒頭20.48秒のPCMとFM命令時刻・順序が既存参照と一致しました。参照データは
  再生成していません。最初の参照実行は私有入力のDockerマウント不足で停止したため、
  入力を読み取り専用で接続して再実行しました。ログは`playlists-renderer-final.log`、
  `playlists-reference-final.log`です。広範な互換性調査を再開したものではありません。
- `tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r5` はPASS。
  RV32はtext 462,200 / data 79,268 / BSS 3,944,944、static計 **4,486,412 bytes**。
  54-MiB領域内で、保守的stack上限15,344 / 最大frame 6,144 bytes、動的stack・未解決symbolなし。
  生インデックスの固定領域は3,851,200 bytesです。これはリンク時の資源検査であり、実機での
  heapピークや起動・描画時間の測定ではありません。ログは`playlists-cross-final.log`。
  ELF SHA-256: `ff7d7d57f10bccd006903c015ee6a80313414f33a70a17ddd83686f4c7517fa0`。
- [日本語手順](../development/pocket-minimal-player.md#開発者向けパッケージ生成)のコマンドで候補を生成。
  `python -B out/harness/playlists-readback.py` はZIPと展開物、最終ELF・手順、framework 2.2、
  deferred slot 4、曲集CRC・範囲・共有データを読み戻してPASS。FPGA/ROM/OSは受入済みr4と
  バイト一致、更新ZIPには曲集を含みません。実C++ readerのASAN/UBSAN検査でも、候補の
  42登録と規模確認用30,000登録の全ペイロード・選曲制御がPASSです。
  元の6曲の準備済みMDX/PDXは変更していません。実描画コードでリスト一覧・2ページ目・
  戻る行をPNGに出し、日本語・選択/再生印・再生情報に欠けや重なりがないことを確認しました。

配布物（core `0.17.0-player-r5`）:

| ローカル候補 | bytes | SHA-256 |
| --- | ---: | --- |
| `out/build/minimal-player-r5.zip` | 16,834,223 | `0a9bbca163f47f5a2c49882209635d04585e7923dadfcf1727e0de09c6d9bf38` |
| `out/build/minimal-player-r5-update.zip` | 1,187,278 | `3c7d98b8e3e587f80ac596a564fec59eb5919a90691c99acf343c0776666cf54` |
| `out/build/minimal-player-r5-scale-data.zip` | 531,004 | `7f82449d6c2efbdf56a3faaaf741af0cbbe20b3c342e17fb39cca6b3467a7bb0` |

全体ZIPは同じ受入済み6曲を3リスト・42登録に配置し、ページ移動とリスト切替を試せます。
規模確認用は同じ6曲を100リスト×300登録にしたもので、3万種類の楽曲の互換性検証ではありません。
そのHPL2は4,523,320 bytesです。私有楽曲・曲名・波形はコミットしていません。

**r5の実機確認は未実施**です。次は日本語手順に沿い、別リスト閲覧中の音声継続・操作反応、
自動送り先、位置復元、100×300での起動時間・先頭/中間/末尾の選曲を確認します。
画面のIは起動時のインデックス読み込み・検査時間（ms）です。r4を受入済み基準として残します。
今回はRTL・合成入力・ハードウェア音声動作が変わらないため、専用RTLシミュレーション、
合成・fit/CDCは再実行せず、r4の証拠とパッケージ一致を使用します。ホスト検証の合格は
この実機スライスやM6全体の合格を意味せず、既存の外部I/O制約の未検証事項も残ります。

最終Full後は結果と現在位置だけを文書へ反映し、本番コード・確認手順・配布物は変更していません。
`python -B tools/check_harness.py --root .`、
`python -B out/harness/continuous-document-links.py`（変更リンク/アンカー12件）、
`git diff --check` はPASS。最終のパッケージ読み戻しもPASSです。

## Minimal Player r5 hardware result — 2026-09-22

ユーザーの **Firmware 2.6 / Minimal Player r5** 実機報告を受領しました。
通常版は3プレイリスト・42登録です。以下はユーザーの報告であり、エージェントによる
再測定ではありません。

- 起動時・リストを開くときの無音、日本語表示、リスト順・曲順、上下・左右と各ページの戻る行はOK。
- 別リストを閲覧しても元の音楽が続き、入力遅延・音切れ・ノイズは問題なし。
- 別リスト閲覧中の自動送りと、閲覧中のカーソル・ページ保持はOK。
- Aで再生元リストを切り替え、各リスト末尾で停止する動作はOK。
- リストごとの曲・ページ位置復元はOK。
- 別リスト閲覧中のX一時停止・再開、一時停止中のA、B停止はOK。
- Yループ切替、フェード、無限、リスト切替後の設定保持はOK。
- FM・PCM・左右と約1分の安定再生はOK。
- 100×300登録の最初・中間・最後のリスト・曲の確認は「OK.問題なさそう」との報告。
  大規模版のIは **6,598 ms**。同じ6曲を繰り返した規模確認で、3万種類の曲の互換性確認ではありません。
- 通常再起動・電源OFF後の無音、2周設定、閲覧位置リセットはOK。

停止後の観測値は **R 12,691 us / F 473 us / D 1,594 us / V 669 us / I 20 ms**。
Qは未記載です。Iは曲集インデックスの読み込み・検査時間であり、通常版20 ms・大規模版
6,598 msを総起動時間とは扱いません。「起動所要時間 / I」欄は20とだけ記載されており、
総起動時間の独立した測定は未報告です。曲・条件を揃えていないため、r4比の性能変化や
全曲の最大負荷は推定しません。エラー曲スキップも今回未報告で、既存ホスト試験の証拠を維持します。

[実装時のFull 90/90・native/reference・RV32・パッケージ検証](#multiple-playlist-implementation--2026-09-22)
と今回の実機報告を合わせ、承認済みの複数プレイリストスライスを合格とし、
**r5を受入済みの基準**に更新します。今回は追加修正・最適化を開始せず、次は小さな機能の
要件決めです。M6全体や、既存の外部I/O制約を含む基板全体の本番受入を完了したものではありません。

今回の変更は実機結果と進捗リンクのみです。コード・仕様の動作・確認手順・配布物は変更せず、
Full・RTL・クロスビルド・合成は入力が変わらないため再実行しません。
`python -B tools/check_harness.py --root .`、
`python -B out/harness/continuous-document-links.py`（変更リンク/アンカー10件）、
`git diff --check` はPASSです。

## Minimal Player r6 UI implementation — 2026-09-22

ユーザーのQ1〜Q14と「実装して」に従い、最小プレイヤーの操作・表示を実装しました。
L/Rのどちらでも一覧と操作パネルを切り替え、十字キーとAで操作します。Bは一覧で戻る、
操作パネルで停止、X/Yは未割り当てです。戻る行を除き13曲／ページにし、一覧位置と
操作アイコンの記憶を分離しました。上段は前曲・再生／一時停止・次曲、下段は停止・ループです。

再生アイコンは停止後に最後に正常開始した曲を再生し、前後曲はその曲のリスト内で開始します。
閲覧先とは独立し、端では無効です。Coreのminimal command/snapshotを**version 6**へ更新し、
意味上のPlayPause／Previous／Next、最後の再生対象、経過秒を追加しました。
HPL2・HYB4、音源エンジン、FPGA、OSの仕様は変更していません。

経過時間は送信済みPCMからFIFO残量を引いた消費位置を62,500 frames/sで秒に変換し、
Coreからsnapshotに公開します。描画時計には依存せず、一時停止で保持、停止・選曲で0、
自然終了で最終値を保持します。既存CDC・音声パイプラインの短い遅延を含むFIFO消費位置であり、
新しい厳密な音声出力タイムスタンプではありません。

実描画は既存640×480・日本語ビットマップ・4走査線ずつの処理を使います。下部左に曲名・
リスト名・曲番号／曲数・時間・状態、右に操作アイコンを置きました。明るい操作枠、控えめな
非操作パネルの選択、独立した再生印、無効色、日本語の非モーダルエラーを表示します。
選択中の長い名前は1秒待って32px/sでスクロールし、他は省略します。依存追加はありません。
APFのControls表示もA/B/L/Rへ更新し、mappingをSDKの定義どおり`key`で記述しました。

変更した境界と実装前のFullチェックポイントは
[設計](../design/pocket-mdx-compatibility-plan.md#ui-implementation-boundary)に記録しています。
主な実装はminimalのcontracts/player/ui/display、`hybrid_playback`、`hybrid_main`です。
旧UI試験の期待値は、13実曲行・文脈依存B・アイコン操作へ変更し、再生／エラー／ループの
既存の保証は維持しました。境界の省略表示で幅ぴったりの名前が欠けるケースも修正しました。

実施済みの検証:

- `pwsh -File tools/host-verify.ps1`: 最終コードで**Full 90/90 PASS**。
  format・tidy、Core/UI依存の正・負検査、ヘッドレス／mock、影響するツール試験を含みます。
  CTest計585.83秒、うちtidy 551.79秒。ログは`out/harness/ui-r6-full-accepted.log`です。
- `pwsh -File tools/host-verify.ps1 -Mode Fast`: 89/89 PASS。最終の省略表示境界修正前の
  チェックポイントであり、最終受入は別のFull結果を使います。
- 初回の全体静的解析で描画と試験の型変換・未使用初期化4件を検出して修正しました。
  `clang-tidy -p out/build/host-msvc --config-file .clang-tidy core/platform/pocket/src/minimal_display.cpp tests/pocket/minimal_player_tests.cpp`
  は修正後PASS。指定LLVM 22.1.8を使い、警告抑制・検査除外は追加していません。
- `docker run --rm --network none -v F:/source/rpcmp:/repo -w /repo rpcmp-cpu-budget-sim:20260921 python3 -B out/harness/minimal-player-native.py`:
  最終コードのASan/UBSan付き操作・音声・描画試験とパッケージ試験7件PASS。
  リマップ、長押し・同時押し、100×300の閲覧、端の無効操作、別リスト閲覧中の操作、
  停止後の再生対象、読込失敗後の対象保持、一時停止中の時間保持を検証しました。
- 経過時間は63,000送信済み−501待機＝62,499では0秒、−500＝62,500で1秒という
  独立に定めた境界で確認。描画遅延・別リスト閲覧の有無で音声MMIO書込列が一致しました。
  描画ガード領域、実フォントの番号表示、幅ぴったりの文字列、スクロールの待機・クリップもPASS。
- `out/build/host-msvc/rpcmp_minimal_player_tests.exe out/harness/ui-r6-preview.ppm`: PASS。
  PNGへ変換した実描画を目視確認しました。これは公開mock曲名のホスト画像で、Pocket写真ではありません。
- 同Dockerで `tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r6`: PASS。
  static 4,490,636 bytes、data 79,292 bytes、最大stack frame 6,352 bytes、保守的stack上限
  15,856 bytes、動的stack frameなし。アプリの資源検査であり実機の描画時間測定ではありません。
  静的解析修正後も再ビルドし、ELFが配布物とbyte単位で同一であることを再確認しました。
- `tests/pocket/hybrid_renderer_tests.py --library out/build/minimal-player-cpu-r6/renderer.so`:
  ループ／自然終了／再オープン3件PASS。
- 同Dockerに私有曲フォルダーを`/corpus:ro`でマウントして
  `tools/hybrid_renderer_verify.py --library out/build/minimal-player-cpu-r6/renderer.so --oracle out/build/hybrid-real-inputs-r2 --manifest out/build/hybrid-real-inputs-r2/private-inputs.json`:
  合成3種と既存6曲の保存済み参照FM書込・wide PCMに完全一致。
  私有曲は各960 blocks／1,280,000 source framesの比較で、全曲全区間の互換性調査ではありません。
- [日本語手順の生成コマンド](../development/pocket-minimal-player.md#開発者向けパッケージ生成)と
  `python -B out/harness/ui-r6-readback.py`: PASS。ZIP全件、アプリELF、操作定義、framework 2.2、
  slot上限、r5と同一のHPL2曲集、参照prepared payload、更新ZIPに曲集がないことを確認しました。
  ASan/UBSanランタイムの`--playlist`でも同梱42登録を全件読み戻し、PASSでした。

配布物（core `0.18.0-player-r6`、ローカル確認用）:

| ファイル | bytes | SHA-256 |
| --- | ---: | --- |
| `out/build/minimal-player-r6.zip` | 16,839,241 | `390e8b06e1f2405263417518836de983bb109024b17ba9878378a3d048500169` |
| `out/build/minimal-player-r6-update.zip` | 1,192,218 | `0f917214432c3c2dc0c5b51265a6ef85652d66874672923736484a94c3584b92` |

RTLシミュレーションと合成は未実施です。RTL・制約・音源プロトコルに変更がなく、ZIP内の
FPGA・loader・OS・audio/video定義は合格済み組み合わせとbyte単位で同一と確認しました。
継承した外部I/O制約の未解決項目を今回のホスト検証で完了扱いにはしません。
実機では新UIの入力応答、連続描画・スクロール中のFM/PCM音声、一時停止・時間表示を
[日本語確認手順](../development/pocket-minimal-player.md)で確認します。**r6実機受入は未実施**です。
最終ゲート後の変更は進捗・リンク・検証記録のみです。文書参照と`git diff --check`を確認し、
入力が変わらないC++・RTL・クロスビルドは繰り返しません。

## Minimal Player r6 hardware result — 2026-09-22

The user reports all r6 procedure checks passing on Firmware 2.6 using the
scale collection. Numeric list/track counts were not repeated in this report;
the generated scale collection contains 100 lists × 300 entries repeating six
accepted songs, not 30,000 distinct songs.

- Silent/two-loop launch, Japanese text and initially disabled controls pass.
- Thirteen-row browsing, page changes, contextual B, per-list position retention,
  both L/R panel toggles, held-button behavior, icon memory and inactive X/Y pass.
- Playback metadata remains independent of browsing; elapsed time freezes through
  a roughly one-minute pause, resumes and resets on stop/selection as specified.
- Play/pause/restart, contextual stop, previous/next in the playback list and
  disabled endpoints pass, including while browsing another list or paused/stopped.
- Repeat switching, fade, infinite repeat, automatic advance, list-end stop,
  retained browsing cursor and playing marker pass.
- Selected-name scrolling stays inside its field. FM/PCM, stereo and approximately
  one minute of playback pass, with no reported dropout, noise or input delay.
- Normal restart and power-cycle silence/two-loop/position-reset defaults pass.

Reported R 5,337 / F 404 / D 2,874 / V 627 us, Q 987 frames, I 6,535 ms.
I is index load/validation, not total boot time. Failed-track skipping was not
reported in this check; its existing host evidence remains separate.

This accepts the r6 UI slice and makes r6 the hardware baseline, without closing
all M6/production-substrate gates. The user's requested follow-up changes are X
instead of L/R for panel switching, scrolling of overflowing lower information
fields, and a loop icon resembling the supplied two-arrow silhouette. These are
recorded in the [r7 boundary](../design/pocket-mdx-compatibility-plan.md#r7-ui-follow-up).

## Minimal Player r7 UI follow-up — 2026-09-22

Implements the user's three r6 follow-up requests within UI/platform: X toggles
panels (L/R/Y unassigned), overflowing playback title/list fields scroll, and
the loop icon uses two opposing bent arrows. Update on-screen help, APF Controls
and the Japanese package guide together. No new dependency or source image is
embedded. Commands/snapshots remain version 6; HPL2, HYB4 and playback/audio
implementation remain unchanged.

Information fields use the existing one-second dwell, 32 pixels/second and
one-second end dwell, independently of browsing/focus and pause/stop. Reset
their presentation clock on a changed last-played track ID. Each field uses its
own width; fitting fields remain stationary. Both text clocks share one animation
redraw interval of at least 50 ms; continue rendering four scanlines between
audio-service turns. This avoids doubling redraw requests when their phases differ.

Verification performed for this slice:

- An added X-toggle test fails against r6 at its two expected panel assertions
  (`out/harness/ui-r7-red.log`), then passes after the binding change. Existing
  transport/browsing tests use the newly approved input mapping; audio/status
  register expectations are unchanged.
- `pwsh -File tools/host-verify.ps1 -Mode Fast`: PASS 89/89 (29.97 s, tidy omitted),
  before the final common-cadence adjustment. Final Full evidence follows below.
- `docker run --rm --network none -v F:/source/rpcmp:/repo -w /repo
  rpcmp-cpu-budget-sim:20260921 python3 -B out/harness/minimal-player-native.py`:
  ASan/UBSan PASS and package tests 7/7. Tests cover X hold/rebinding/chords,
  unassigned L/R/Y, independent information clocks through pause/stop/browsing,
  400-pixel fitting bounds, exact 32-pixel motion after the dwell, separate end
  dwells and clipping to both information fields. The headless versus delayed
  rendering trace remains identical; it now renders through the information
  area, not just the upper list. See `out/harness/ui-r7-native.log`; the final
  integer-type correction below is also checked in `ui-r7-native-final.log`.
- Actual-renderer short/Japanese and long/Japanese+ASCII previews were inspected:
  `out/harness/ui-r7-preview.png`, `ui-r7-long-0.png`, `ui-r7-long-40.png`.
  These are generated mock views, not Pocket screenshots. The loop silhouette,
  adjacent setting, clipping and independent metadata are visible.
- RV32 build command: `docker run --rm --network none
  -v F:/source/rpcmp:/repo -w /repo rpcmp-cpu-budget-sim:20260921
  python3 -B tools/hybrid_renderer_build.py --output out/build/minimal-player-cpu-r7`.
  Final build/resources and package results are recorded below.

The first Full run was intentionally stopped when the common animation cadence
was corrected during review; it is not acceptance evidence. The next Full
(`ui-r7-full-final.log`) passed functional checks but failed six implicit
multiplication-widening diagnostics in the new test's iterator offsets. These
were corrected with explicit `std::ptrdiff_t` arithmetic without changing pixel
expectations or suppressions. Focused `clang-tidy -p out/build/host-msvc
--config-file .clang-tidy tests/pocket/minimal_player_tests.cpp` then passed
(`ui-r7-tidy-focused.log`). The correction affects tests only, not the packaged
application; its RV32 build/package evidence remains valid. Final Full evidence
is recorded below. Do not treat Fast or build success as Pocket acceptance.

The optional `minimal-player-r7-scroll-data.zip` contains two lists/four entries
with long Japanese/ASCII and short display names, reusing accepted audio payloads.
It adds `LongNames.json` and `longnames.hpl` without replacing the user's default
collection. Its independent header/index/CRC/name/payload/ZIP readback passes;
the actual sanitized reader also accepts all four payloads. Private audio and
generated artifacts remain under ignored `out/`, not committed fixtures.

RTL simulation, synthesis/fit and broad MDX reference comparisons are not rerun:
their inputs and playback behavior are unchanged. Package readback checks the
accepted FPGA/boot ROM/OS and existing HPL2 bytes directly. Existing M6 hardware
and external-I/O timing limitations remain; this slice introduces no claim that
they are resolved. r6 remains the accepted hardware baseline until the user
checks r7 using the [Japanese procedure](../development/pocket-minimal-player.md).

Final RV32 build passes (`out/harness/ui-r7-cross-final.log`): 4,490,924 static
bytes (288 more than r6), data 79,300 / text 466,680 / BSS 3,944,944 bytes.
The largest stack frame is 4,720 bytes, conservative bound 14,128 bytes, with
no dynamic frames. Existing third-party host pointer-size warnings and the
serial-LTO note remain; no warning suppression was added.

Package command uses `tools/pocket_hybrid_package.py` with shell
`out/build/repeat-candidate-r4`, CPU `out/build/minimal-player-cpu-r7`, firmware
`out/build/repeat-firmware-r4/src/firmware/os/bld/pocket`, the accepted local
three-list manifest and output `out/build/minimal-player-r7`. Final readback via
`python -B out/harness/ui-r7-readback.py` passes: exact final ELF and Japanese
guide, APF A/B/X keys, core `0.19.0-player-r7`, framework minimum 2.2, identical
accepted FPGA/loader/OS/audio/video and identical r5 HPL2 payload/index/order.
The full collection is three lists/42 entries/10 shared blobs. The update has
no HPL and all of its members match the full archive. See
`out/harness/ui-r7-package-final.log` and `ui-r7-readback-final.log`.

| Local artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `out/build/minimal-player-r7.zip` | 16,839,403 | `6d32d83d1194df425fcfddcf499d183f1c46234aa4127bb1f9ed80221783eb94` |
| `out/build/minimal-player-r7-update.zip` | 1,192,389 | `fcc604172eafd289d173588a797caf1f5ec71f7b7c144cbe9a467c3c44e91f4f` |
| `out/build/minimal-player-r7-scroll-data.zip` | 22,772 | `11d5303104e064fa97e3eaea8e71d02e7c52e8a5d0df657be8fc96b5e99acf00` |

Final `pwsh -File tools/host-verify.ps1` passes **90/90**, including format, tidy,
headless/mock and positive/negative architecture checks (636.97 s total, tidy
598.88 s; `out/harness/ui-r7-full-accepted.log`). Final native ASan/UBSan and
package tests also pass, 7/7 (`ui-r7-native-final.log`). No production source or
test changed after these passes; the remaining edits record progress/navigation
only. Document links and `git diff --check` are checked before committing.

The connected r7 UI slice is ready for hardware verification. r7 Pocket audio
continuity, input feel and scrolling legibility are not yet hardware-accepted.

## Minimal Player r7 hardware result — 2026-09-22

ユーザーの **Firmware 2.6 / Minimal Player r7** 実機報告を受領しました。
使用曲集は規模確認用データです。今回の報告にはプレイリスト数・曲数の数値はなく、
追加の長い名前用データを使用したかも明示されていません。以下はユーザーの報告であり、
エージェントによる再測定や、全楽曲の網羅的な互換性検証ではありません。

- 起動時の無音・2周設定・日本語表示はOK。
- Xでのパネル往復、長押し、行・ページ・アイコンの保持、L/R/Yの無操作はOK。
- A/B/十字キー、再生・一時停止・再開・停止・前後曲への移動はOK。
- 下部の長い曲名・リスト名は末尾まで読め、枠外にはみ出さずスクロールすることを確認。
  閲覧中・操作パネル中・一時停止中も継続し、別曲で先頭へ戻り、短い名前は静止する動作もOK。
- 一覧の選択行スクロール、カーソル・再生印はOK。
- ループアイコンの形・読みやすさ、2→3→5→無限、フェード・自動送り・末尾停止はOK。
- FM/PCM・左右・約1分の再生はOK。音切れ・ノイズ・入力遅延の問題は報告されていません。
- 通常再起動・電源OFF後の無音・2周設定・位置リセットはOK。エラーや気になる点はなし。

停止後の報告値は **R 5,256 / F 403 / D 2,712 / V 633 us、Q 971 frames、I 6,633 ms**。
Iは起動時のインデックス読み込み・検査時間で、起動全体の所要時間ではありません。
エラー曲のスキップは今回の確認項目に含まれず、その既存ホスト検証とは区別します。

この報告でr7のUI変更スライスを実機合格とし、受入済み基準をr6からr7へ更新します。
上記のr7実装記録にあるFull 90/90・サニタイザー・RV32・パッケージ検証と、今回の実機報告を
合わせた受入です。既存のM6全体・production substrate・外部I/Oタイミングの残件は別です。
次はユーザーと次の要件を選びます。未選択の機能を今回の報告から実装開始扱いにはしません。

今回の変更は受入記録と現在位置・参照リンクのみで、コード・契約・検証手順・配布物は変更しません。
`python -B tools/check_harness.py`、`python -B out/harness/continuous-document-links.py`
（変更リンク・アンカー8件）、`git diff --check` はPASS。
入力が変わらないFull・クロスビルド・RTL/合成は再実行していません。
