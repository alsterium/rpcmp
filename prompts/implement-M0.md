# Copy-ready Codex task: Implement M0

Historical bootstrap prompt. For current work use `prompts/work-current.md`;
do not apply the scope restrictions below to later milestones.

Read `AGENTS.md` and every document it requires. Implement only `docs/milestones/M0-architecture.md`.

Before coding, inspect the repository and available toolchain, then create ADRs for the language/build system, state transport, and test strategy. Preserve the architecture contracts: Core publishes immutable snapshots; UI sends commands; Core never depends on UI; playback time never depends on rendering.

Do not implement MDX parsing, YM2151 synthesis, PDX/MSM6258, PCM8, the real `.rpcmlib` binary format, or final graphics.

Run all relevant host checks. At completion report:

- files changed
- architecture decisions and ADRs
- tests/checks run with results
- tests not run and why
- evidence that headless Core and mock-only UI work independently
- unresolved questions and risks
- recommended next task
