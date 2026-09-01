# M1 — Library Container

Status: active

## Objective

Freeze and implement `.rpcmlib` v1 so untrusted libraries can be validated once
and then accessed by stable `TrackId` and `BlobId`, without exposing container
offsets to UI or playback code.

## In scope

- Freeze the byte layout defined by `specs/rpcmlib-v1.md` in reviewable slices.
- Implement a dependency-free C++17 reader in `core/library`.
- Implement a deterministic host-side writer in `utility`.
- Validate all sizes, counts, offsets, alignments, overlaps, IDs, references,
  UTF-8 strings, codecs, and CRC-32 values before exposing logical records.
- Add golden minimal and multi-track fixtures plus bounded corruption tests.
- Prove random lookup in a 1,000-track synthetic library.
- Preserve the M0 Core/UI dependency boundary.
- Keep Pocket experiments on the checksum- and size-pinned `0.5.38-safe`
  OS/RBF pair; M1 does not modify or rebuild that pair.

## Explicitly out of scope

- MDX parsing or sequencing
- YM2151 synthesis or RTL integration
- Compression codecs other than `none`
- Search, artwork, or analysis sections
- Final Pocket storage integration or performance claims
- Resolving the general execution-substrate gate in ADR-0004

## Implementation slices

1. Freeze and validate the header and section-directory envelope.
2. Freeze required-section payload records and expose typed logical views.
3. Add the deterministic writer, stable-ID rules, and golden fixtures.
4. Add corruption coverage and the 1,000-track random-access proof.
5. Exercise bounded logical blob reads in the safe Pocket application without
   changing the pinned OS/RBF pair.

Each slice must keep the repository verifiable. A partial reader must not claim
full `.rpcmlib` v1 compatibility until all required payload layouts are frozen
and all acceptance criteria pass.

Progress: slices 1 through 4 are complete. The reader validates all required
payload layouts before exposing typed track, blob, string, and dependency views.
The deterministic writer, stable SHA-256 IDs, and byte-exact minimal and
multi-track golden fixtures are verified on the host. Bounded corruption tests
cover the v1 envelope, record identities, references, UTF-8/NUL handling,
checksums, codecs, and admission limits. A synthetic 1,000-track library opens
and resolves arbitrary tracks and their blobs through the binary-search index.
Slice 5 is host- and cross-build complete. The `0.5.39-m1` Pocket application
admits at most 1,024 bytes from dataslot 5, opens the packaged 656-byte minimal
golden library with Pocket-specific count and decoded-size limits, resolves its
known `BlobId`, verifies the three logical bytes, and rejects a missing ID. The
package retains the pinned safe OS/RBF pair byte-for-byte. Firmware 2.6 hardware
confirmation of `LIBRARY: PASS` and `RESULT: PASS` remains before M1 completion.

## Acceptance criteria

1. Golden fixtures define every byte of a minimal and multi-track file.
2. Truncation, arithmetic overflow, overlap, duplicate IDs, missing sections,
   bad UTF-8, invalid references, unsupported codecs, and checksum failures are
   rejected with stable typed errors.
3. A 1,000-track synthetic library opens and arbitrary tracks and blobs are
   retrieved by ID without a per-lookup full-file scan.
4. Rebuilding unchanged normalized input produces byte-identical output.
5. Unknown optional sections are skipped; unknown required features and
   unsupported major versions are rejected.
6. UI and playback engines receive IDs and bounded logical byte views only;
   raw container offsets never cross the library boundary.
7. Host verification and architecture dependency checks remain green.
8. Pocket-only verification, if performed, uses the pinned safe OS/RBF pair and
   records firmware, package checksum, and observed result.

## Verification

Run `pwsh -File tools/host-verify.ps1`. Add focused container tests to the same
CTest workflow. Pocket verification is a final integration layer, not a
substitute for deterministic host tests.

## Known constraints

- Pocket memory budgets will be justified before final admission limits freeze.
- CRC-32 is the v1 corruption detector, not an authenticity mechanism.
- The safe Pocket baseline permits bounded application experiments but does not
  prove arbitrary firmware layout changes safe.
