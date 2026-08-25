# ADR-0003: M0 test and verification strategy

- Status: accepted
- Date: 2026-08-25
- Deciders: RPCMP maintainers

## Context

M0 is an architectural proof. Passing feature tests is insufficient if a target silently links forbidden modules, a renderer drives time, or tests depend on wall-clock behavior. The repository also needs one CI-ready verification entry point.

## Decision

Use CTest for compiled tests and Python for source/dependency policy checks. Once implemented, the single local/CI entry point is:

```text
pwsh -File tools/host-verify.ps1
```

The wrapper locates Visual Studio C++ Build Tools, initializes its x64 developer environment, and invokes `cmake --workflow --preset host-verify` with the bundled Ninja generator.

The workflow configures with warnings-as-errors and generated compile commands, builds all M0 targets, runs format verification, static analysis where configured, architecture checks, and CTest. No test may use wall-clock sleeps.

Required test groups are:

- `contracts`: bounds, optionals, enum coverage, value equality, and absence of platform/container fields.
- `command_state`: the full M0 transition table, projected FIFO state, validation precedence, unknown IDs, unsupported seek, mute/solo policy, duplicate replay, stale IDs/sequences, and queue capacity 32/33.
- `determinism`: run the same clock/command trace twice and compare every logical snapshot field and fake-device event.
- `headless`: link only contracts + runtime + fake ports; generate at least 120 cadence snapshots without UI.
- `mock_ui`: link only contracts + UI + generated fixture snapshots; render Overview and Channels views, missing optionals, and unknown extensions.
- `render_independence`: compare runs with no snapshot reads, every snapshot read, and deliberately sparse reads. Device events and snapshots at matching sequences must be equal.
- `architecture`: reject forbidden include paths and target dependencies. Run the checker against both the real tree and a committed synthetic failing fixture to prove the checker can fail.
- `scope`: reject production files or dependencies that introduce MDX parsing, YM2151 synthesis, PDX/PCM8 implementation, or final graphical assets during M0.

Architecture checking has two layers:

1. CMake target links encode allowed edges; independent executables prove link closure.
2. `tests/architecture/check_dependencies.py` scans production includes/imports and CMake dependency declarations against an allowlist. A synthetic `runtime -> ui` fixture must make it return non-zero.

The Windows host workflow requires LLVM 22.1.8 and runs `clang-format` and `clang-tidy` as CTest gates. LLVM is a development-only dependency; it does not link into or ship with RPCMP. The local wrapper uses Ninja bundled with Visual Studio Build Tools and does not require a separate Ninja installation.

Linux/GCC CI is deliberately deferred after the initial host implementation. Restoring it is a follow-up task before M0 is closed, not an implicit claim of current coverage. RTL simulation and Pocket builds are reported as unavailable in M0 until ADR-0004 closes their toolchain gates.

## Alternatives considered

- **Only unit tests:** cannot prove independent linkage or dependency direction.
- **Only source scanning:** can miss transitive link coupling and can pass if the scanner itself is broken.
- **Golden terminal screenshots:** couple M0 to host rendering and presentation details; semantic text/view-model assertions are sufficient.
- **Real-time integration tests:** nondeterministic and unable to prove clock/render independence.

## Consequences

- Verification is deterministic and suitable for clean-checkout CI.
- The synthetic negative architecture fixture is production guardrail data, not application code.
- Windows formatting and analysis are reproducible against the pinned LLVM 22.1.8 release.
- Linux/GCC behavior remains an explicit verification gap while its CI job is deferred.
