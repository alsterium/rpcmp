# RPCMP Agent Guide

This repository is a specification-first handoff for Retro PC Music Player (RPCMP).

## Project charter (2026-09-21)

- Deliver the current user outcome by the shortest practical, simple implementation.
  For this MVP that means MDXPlayer-compatible FM/PCM playback, selection and stop.
- Backward compatibility with RPCMP's previous APIs, file formats and experimental
  implementations is not a requirement. Do not add compatibility wrappers, dual
  paths or migration machinery unless the user requests them. This does not waive
  compatibility with the user's MDX/PDX music and selected playback reference.
- Keep only the separation needed for correct audio, headless verification and
  likely changes. Add an abstraction when a concrete use needs it; do not build
  speculative extension frameworks or preserve layers merely because they exist.
- Record the small behavior/boundary being changed, implement it end to end, and
  measure it. Prefer a short contract in the active design over additional layers
  of proposals. Within the approved direction, update superseded contracts in the
  same change without asking for repeated compatibility approval.
- Simplicity does not waive input bounds, licensing, audio/UI independence or
  truthful verification. Focus checks on changed inputs and observable behavior.

## Read first

1. `docs/PRD.md`
2. `docs/ARCHITECTURE.md`
3. `docs/CURRENT.md`, then the active milestone it links
4. Relevant contracts in `specs/`

The documents above are the system of record. If implementation and documentation disagree, identify the exact conflict. Resolve it from the user's latest approved direction and update the affected documents; stop only when an unresolved product decision is needed. Continue independent authorized work. Correct stale navigation against milestone evidence; record public-contract changes explicitly.

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
- Describe changed boundaries before coding them and update affected contracts. Version breaking serialized/hardware formats so mismatches fail clearly; backward-compatible implementations are not required by default.
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
- During iteration, run focused checks first, then `pwsh -File tools/host-verify.ps1 -Mode Fast` at a coherent checkpoint. Fast omits only tidy; it is not Full acceptance.
- Before implementation, name the approved connected behavior that will trigger Full (for example, sending a CPU audio request and receiving its matching completion). Run Full when that behavior works, with affected RTL/cross-build/synthesis checks. A file, helper or commit alone is not a Full milestone; do not defer the milestone indefinitely as scope grows. Small commits do not each require Full.
- Repeat checks only after their inputs change, a failure is fixed, a new concern arises or integration scope expands. After a pass, progress/link-only edits need document checks, not identical C++/RTL reruns; never reuse a pass for subsequently changed code. Run RTL suites sequentially because they use shared simulator work directories.
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
- Run formatting, linting, static analysis, and relevant RTL simulation at the milestones defined in `docs/development/harness.md`. Hardware candidate submission and milestone acceptance require all applicable gates; Fast never substitutes for Full there.
- Verify architecture/dependency checks still prevent Core-to-UI coupling.
- Update docs only when behavior or an approved contract changed.
- Report files changed, decisions made, tests run, tests not run, known risks, and the next recommended task.
- Harness/build/verification-script changes require affected tooling tests and Full. Progress entries and link fixes alone require document/navigation checks, without full C++ analysis. Product specs, public contracts, acceptance criteria, build/verification procedures, agent authority/stop rules, and documents used as test/generation inputs require impact-based checks, including Full when affected; Markdown is not itself grounds for lighter checks. RTL and hardware checks are not applicable unless their inputs or behavior changed. Explicitly report that distinction. Passing host checks does not complete a hardware milestone.
