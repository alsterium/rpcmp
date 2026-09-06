# RPCMP Agent Guide

This repository is a specification-first handoff for Retro PC Music Player (RPCMP).

## Read first

1. `docs/PRD.md`
2. `docs/ARCHITECTURE.md`
3. `docs/CURRENT.md`, then the active milestone it links
4. Relevant contracts in `specs/`

The documents above are the system of record. If implementation and documentation disagree, identify the exact conflict and stop the affected change until it is resolved explicitly. Continue independent authorized work. Correct stale navigation against milestone evidence; do not silently change a public contract.

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

## Setup and workflow

- Follow `docs/development/harness.md` for setup, verification commands, and review criteria. Run `pwsh -File tools/host-verify.ps1 -CheckSetupOnly` on a new or changed host environment.
- Inspect HEAD, branch, staged/unstaged changes, and applicable instructions before editing. Preserve pre-existing work; stage only owned paths or hunks.
- State the intended outcome, active milestone/slice, and relevant checks briefly. Read relevant files once; load additional contracts and skills only when needed by the task.
- Resolve routine, reversible implementation choices using existing contracts. Proceed through implementation and verification without repeated permission requests. Ask when an unresolved decision changes public behavior, scope, or authorization; identify the source of the blocker.
- Start with a focused failing case for behavioral bugs. Use contract-derived assertions, independently derived traces, boundary cases, and existing fixtures. Do not add tests that merely repeat implementation details for prose-only edits.
- Run focused checks during iteration and the completion gate once the change is stable. Repeat or broaden checks only after changes, failures, or unresolved concerns. Run RTL suites sequentially because they use shared simulator work directories.
- Keep subagent use bounded to independent tasks when explicitly requested. Do not create agents solely to repeat the same review.

## Observable quality rules

- Every changed production path must serve the requested outcome or an active acceptance criterion. Remove unused helpers, speculative abstractions, duplicate validation, and unrelated cleanup introduced by the change.
- Do not replace failures with success, defaults, ignored exceptions, disabled tests, relaxed bounds, or new warning suppressions just to pass a gate. A necessary exception needs a concrete reason and validation of the remaining guarantee.
- Do not regenerate golden traces, hashes, or expected values solely from the implementation under test. Explain the contract or independent evidence for an expected-value change.
- Keep one current-task pointer in `docs/CURRENT.md`; historical milestone evidence and prompts are not current instructions. Update navigation when a milestone advances, not historical results.
- Describe measured facts separately from inference. A build, host test, simulation, synthesis report, package hash, and user hardware report establish different things; never promote one into another.
- Cite actual commands and results from this task. Do not invent test counts, timings, hardware observations, or claim unrun checks passed. Keep private corpus identities and artifacts out of committed evidence.

## Completion gate

Before declaring work complete:

- Run the milestone's acceptance tests and all affected unit/integration tests.
- Run formatting, linting, static analysis, and relevant RTL simulation when configured.
- Verify architecture/dependency checks still prevent Core-to-UI coupling.
- Update docs only when behavior or an approved contract changed.
- Report files changed, decisions made, tests run, tests not run, known risks, and the next recommended task.
- For documentation/harness-only work, run the host gate and any affected tooling tests; RTL and hardware checks are not applicable unless their inputs or behavior changed. Explicitly report that distinction. Passing host checks does not complete a hardware milestone.
