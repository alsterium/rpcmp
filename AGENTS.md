# RPCMP Agent Guide

This repository is a specification-first handoff for Retro PC Music Player (RPCMP).

## Read first

1. `docs/PRD.md`
2. `docs/ARCHITECTURE.md`
3. The active milestone in `docs/milestones/`
4. Relevant contracts in `specs/`

The documents above are the system of record. If implementation and documentation disagree, stop, identify the conflict, and resolve it explicitly rather than silently changing a public contract.

## Architectural invariants

1. `core/runtime` must never depend on `core/ui`.
2. UI reads Core only through immutable/versioned state snapshots.
3. UI changes Core only through `PlayerCommand`; it never mutates runtime state.
4. Audio and sequencing timing must not depend on rendering, frame rate, or UI presence.
5. Playback engines must not render, navigate, or interpret physical UI controls.
6. Core playback must be runnable and testable headlessly.
7. UI must be runnable against recorded or generated mock snapshots.
8. Format-specific data must not leak into generic UI state except through a typed, optional extension.
9. Library consumers use IDs and logical blobs; they do not depend on container offsets.
10. No copyrighted music or proprietary ROM data may be committed as test data.

## Working rules

- Implement one milestone at a time. Do not implement future milestones speculatively.
- Preserve public API compatibility within a spec version. Propose spec changes before coding them.
- Prefer deterministic, host-runnable tests. Keep hardware-only verification as a final integration layer.
- Isolate platform, filesystem, clock, input, audio sink, and FPGA device access behind ports/adapters.
- Use fixed-width integer types and explicit endianness for serialized or hardware-facing data.
- Validate all external lengths, counts, offsets, IDs, and checksums before allocation or access.
- Do not add a dependency without recording its license, purpose, and platform implications.
- Keep commits small and reviewable; do not reformat unrelated files.

## Completion gate

Before declaring work complete:

- Run the milestone's acceptance tests and all affected unit/integration tests.
- Run formatting, linting, static analysis, and relevant RTL simulation when configured.
- Verify architecture/dependency checks still prevent Core-to-UI coupling.
- Update docs only when behavior or an approved contract changed.
- Report files changed, decisions made, tests run, tests not run, known risks, and the next recommended task.
