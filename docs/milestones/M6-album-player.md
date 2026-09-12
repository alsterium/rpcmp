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
Performance history/settings/UI and the real audio mapping/adapter remain subsequent work; this
does not advertise a new capability on the existing Pocket backend.
Hardware ACK deadlines and MMIO remain slice 4 decisions based on RTL evidence.

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
