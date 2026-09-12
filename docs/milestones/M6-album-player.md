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
