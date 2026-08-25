# M0 — Architecture Skeleton

## Design baseline

The executable M0 design is recorded in `docs/design/m0-executable-design.md` and ADR-0001 through ADR-0004. The host skeleton now implements the public contract values, deterministic Mock Core, mock-only text UI, independent link targets, and architecture checks. Run it with `pwsh -File tools/host-verify.ps1`.

Windows host verification pins LLVM 22.1.8 and treats `clang-format` and `clang-tidy` findings as failures. Linux/GCC CI is deferred and remains an explicit completion-gap item.

Pocket execution remains an explicit feasibility gate. The official openFPGA template does not by itself establish a general-purpose runtime for RPCMP, so a host-green M0 must not be reported as Pocket-compatible until ADR-0004 is superseded by target-spike evidence.

## Objective

Prove the Core/UI boundary before implementing music formats, synthesis, or final graphics.

## In scope

- Select and document runtime language/build/test tooling compatible with the intended target and host tests.
- Create module/directory structure for contracts, runtime, UI, platform adapters, RTL placeholders, utility, and tests.
- Define concrete v1 types for snapshots and commands from `specs/`.
- Implement a deterministic mock Core with eight fake YM2151 channels.
- Implement a minimal host UI or renderer that consumes only public snapshots.
- Implement deterministic clock, library, device, and snapshot fixtures.
- Add mechanical dependency checks protecting architecture invariants.
- Add CI-ready format, lint/static-analysis, unit, integration, and architecture commands.

## Explicitly out of scope

- MDX parsing or sequencing
- YM2151 synthesis/RTL integration
- PDX, MSM6258, PCM8
- Real `.rpcmlib` binary reader/writer
- Final Pocket graphics, fonts, or visual design
- Performance claims not backed by measurement

## Required behavior

The mock Core advances only when its deterministic test clock advances. It publishes at least 120 snapshots whose eight channel notes/activity follow a documented repeatable pattern. Commands drive a tested transport state machine. The minimal UI displays transport, track, and all eight channels and can switch between at least two simple views using UI-local navigation state.

## Deliverables

- Buildable skeleton and module manifests
- Public contract types and API documentation
- Mock Core and headless executable/test
- Mock-snapshot UI target/test
- Dependency/architecture test
- ADRs for language/build system, state transport, and test strategy
- CI configuration or a single documented local verification command

## Acceptance criteria

1. A clean checkout can run the documented host verification command.
2. Core builds and a headless test completes without UI linked.
3. UI builds/tests with contracts + mock data and without runtime/engine linked.
4. A mechanical check fails if runtime imports UI.
5. Eight deterministic channels are represented for at least 120 snapshots.
6. Replaying an identical clock/command trace yields identical snapshots.
7. Pausing freezes media position; rendering delay/absence does not change device-event or state progression for the same clock trace.
8. Unknown optional extension data and missing optional values are handled safely by UI.
9. Command state transitions, invalid commands, duplicates, stale commands, and bounded queue overflow are tested.
10. No production source contains an MDX parser, YM2151 synthesis implementation, PDX/PCM8 implementation, or final graphical assets.
11. Static checks and tests are green, or unavailable target-only checks are listed with exact reason and follow-up.
12. Documentation describes how to add a new UI without modifying Core.

## Codex guardrails

- Do not guess the Pocket toolchain: inspect repository/environment evidence and record assumptions.
- Do not add a YM2151 third-party core in M0.
- Do not weaken boundaries to make the demo easier.
- Do not make snapshots mutable or expose runtime pointers.
- Do not use wall-clock sleeps in deterministic tests.
- If a contract is ambiguous, record the question and choose the smallest reversible M0 behavior in an ADR.

## Completion report

Report files changed, ADRs/decisions, verification commands and results, unavailable checks, boundary evidence, known risks, unresolved questions, and the recommended next milestone (normally M1 container or YM2151 test-sequence bring-up, depending on project priority).
