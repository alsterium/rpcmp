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
promotion has been established. Slice 2 is next.
