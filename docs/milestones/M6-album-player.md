# M6 — Album Player

Status: active on 2026-09-13, following the user's instruction to continue until
hardware evidence or a new product decision is needed.

## Objective and adoption

Implement the accepted Q1–Q24 [album-player requirements](../design/pocket-library-player-spec-draft.md):
100–300 FM-only tracks, album browsing, transport/loop/shuffle controls,
Tracker/keyboard/library views and persistent playback settings. Audio remains
independent of UI. Work proceeds one slice at a time.

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
4. **Sound and storage feasibility:** prove state-preserving pause, stereo
   frame boundaries, cancellable fade reservations and audible commit in RTL;
   derive the sound-control port/CDC/ACK contract from those results. Implement
   asynchronous APF RAM-slot flush/readback and timeout ownership. Use sequential
   RTL suites and fault injection before target integration.
5. **Pocket UI and integration:** generate licensed bitmap fonts, connect the
   real input/framebuffer/catalog/player adapters, cross-build and measure ELF,
   stack and framebuffer/font allocation. Review exact official APF mappings
   when implementing them. Run synthesis/timing/CDC and package-coherence checks.
6. **Hardware acceptance:** firmware 2.6 verifies playback, pause/resume, fade,
   navigation while playing, layout/readability, settings persistence and
   failure silence across relaunch/power cycles. Stop for the user once a
   concrete checked candidate and focused hardware checklist are ready.

Run focused host tests while iterating and `pwsh -File tools/host-verify.ps1`
for each stable unit. RTL, cross-build, fit and hardware checks are required
when those paths change; host-only results cannot establish their acceptance.

## Carried integration gates

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
