# RPCMP development harness

## Setup

Use PowerShell 7 on Windows, Git for Windows, Visual Studio C++ Build Tools with
the x64 C++ tools and CMake/Ninja component, CMake >= 3.25, Python >= 3.11,
and LLVM **22.1.8** (`clang-format` and `clang-tidy`). The version pin lives in
`CMakePresets.json`; installing an unpinned latest LLVM is not sufficient.
Install missing tools through the workstation's normal package/tool installer;
the verification script does not install software or change machine settings.

Clone with `--recurse-submodules`, or run
`git submodule update --init --recursive` for an existing checkout. Keep the
recorded skill revision; do not use `--remote` during setup. If Git reports
missing `basename`, `sed`, or `git-sh-setup`, repair the Git for Windows shell
environment or run the command in Git Bash. Do not replace the pinned submodule
with a copied skill. Host compilation does not require this documentation skill;
APF tasks do require reading its instructions.

```powershell
pwsh -File tools/host-verify.ps1 -CheckSetupOnly
pwsh -File tools/host-verify.ps1
```

The first command checks tool versions and the VS compiler/Ninja environment
without building. The second configures, builds, and runs all registered host
tests, including format, tidy, architecture rejection fixtures, and harness
checks. Both resolve the repository from the script location. Nonzero native
exit codes are failures. Setup success alone is not acceptance.

Select GPT-6 Astra in the session's model selector. Preserve the effective
reasoning effort initially and compare actual task outcomes before tuning it.
This repository has no model configuration; these rules do not change the
running session, personal skills, permissions, or global Codex settings.

## Work loop

Read `AGENTS.md`, PRD, architecture, [current work](../CURRENT.md), and relevant
contracts. Briefly identify the requested outcome and the smallest slice that
achieves it. Reproduce a behavioral failure, implement that slice, run focused
checks, review the diff, then run the completion gate. A maintenance request
does not authorize implementing the next product milestone.

After configuration, discover tests with `ctest --preset host-msvc -N` and run
a relevant subset with `ctest --preset host-msvc -R '<test-name-pattern>'`.
Use the full wrapper for completion. Build changes must be rebuilt before
testing. Never treat a filtered run as the full suite or a listing as execution.
The preset rejects empty test selections.

For RTL/audio integration changes, also run `tools/rtl-verify.ps1` followed by
`tools/rtl-jt51-verify.ps1`; see [test setup](../../tests/README.md) for simulator
and licensing prerequisites. Packaging, cross-build, synthesis, and hardware
requirements come from the active slice. An unavailable required tool is a
reported coverage gap, not permission to weaken the gate.

## Review evidence

| Observable failure | Prevention / evidence |
| --- | --- |
| Stale milestone/start prompt | One current pointer; `harness` CTest checks its target and entry links |
| Tests or developer tools silently disabled | Checked host preset; empty test selection fails |
| Core/UI coupling | Positive and synthetic negative architecture tests, separate headless/mock targets |
| Test made green by changing its oracle | Review expected-value changes against contract or independent trace |
| Speculative helpers, duplicate layers, swallowed failures | Review each added production path against the requested outcome |
| Unsupported completion claim | Report executed command/result and distinguish host, RTL, fit, package, hardware evidence |

The last three checks require judgment; there is no reliable lexical detector
for all slop. Do not ban arbitrary words or score quality by added test count.
The harness checker is a narrow navigation/configuration guard, not a semantic
proof of architecture or implementation correctness.

Finish with a concise report of changed files, decisions, checks passed/failed,
checks not run with reasons, remaining risks, and the next task. Keep generated
logs in ignored `out/` when needed; do not duplicate milestone histories in
new status reports. Preserve hardware acceptance as pending until observed.

## Audit basis (2026-09-06)

Observed before this change: README pointed to M2 and an M0 implementation
prompt despite M5 progress; test instructions claimed seven host gates and
three RTL markers; the host wrapper depended on the caller's directory;
setup omitted Python/Ninja and recommended unpinned LLVM. These are concrete
drift/setup failures. No project-wide claim that the source is slop was made.

The [official Astra guidance](https://developers.openai.com/api/docs/guides/latest-model)
recommends auditing loaded instructions, making initiative expectations explicit,
and calibrating verification. Applied here with RPCMP's existing contract and
hardware gates retained. Compare future tasks by unnecessary clarification
stops, unrelated diff, weakened assertions, missed checks, and unsupported
claims; model-quality improvement has not yet been measured in an A/B run.
