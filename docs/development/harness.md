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
pwsh -File tools/host-verify.ps1 -Mode Fast
```

The first command checks tool versions and the VS compiler/Ninja environment
without building. The second configures, builds, and runs all registered host
tests, including format, tidy, architecture rejection fixtures, and harness
checks. Omitting `-Mode` means `Full`; `-Mode Full` is equivalent. The third
command uses the same configure/build and toolchain, excluding exactly the
test named `tidy`. Format, architecture and all other host tests remain enabled.
Fast reports `tidy NOT RUN` and never establishes Full acceptance.
All commands resolve the repository from the script location. Nonzero native
exit codes are failures. Setup success alone is not acceptance.

Select GPT-6 Astra in the session's model selector. Preserve the effective
reasoning effort initially and compare actual task outcomes before tuning it.
This repository has no model configuration; these rules do not change the
running session, personal skills, permissions, or global Codex settings.

## Work loop

Apply the [project charter](../../AGENTS.md#project-charter-2026-09-21): choose
the smallest connected behavior that advances the current MVP, implement it
directly, and measure it. Previous RPCMP API/format compatibility is optional;
MDXPlayer music compatibility remains the product goal. Do not add compatibility
adapters, generalized infrastructure or another proposal/approval cycle for an
already authorized direction. Describe changed boundaries briefly in the active
design before implementation and keep the affected contracts accurate.

For a bounded experiment, keep its runner and assertions small and reproducible;
generated binaries, private music, traces and audio stay in ignored `out/`.
Report the experiment's limits before promoting it to a target implementation.
Use the existing Fast/Full workflow below; a new framework or verification cache
is not needed to implement this charter.

The charter update passed `python -B tests/harness/harness_tests.py HarnessTests`
(9/9) and `pwsh -File tools/host-verify.ps1 -Mode Full` (87/87, 537.36 s;
tidy 519.38 s). The navigation fixture now includes `AGENTS.md` because the
workflow links to its charter. Runner/preset behavior is unchanged.

Read `AGENTS.md`, PRD, architecture, [current work](../CURRENT.md), and relevant
contracts. Briefly identify the requested outcome and the smallest slice that
achieves it. Reproduce a behavioral failure, implement that slice, run focused
checks, review the diff, then run the completion gate. A maintenance request
does not authorize implementing the next product milestone.

After configuration, discover tests with `ctest --preset host-msvc -N` and run
a relevant subset with `ctest --preset host-msvc -R '<test-name-pattern>'`.
Use Fast at coherent iteration checkpoints. Build changes must be rebuilt before
testing. Never treat a filtered run as the full suite or a listing as execution.
The preset rejects empty test selections. The `incremental_build` test compiles
an isolated fixture, changes only its header, and requires changed executable
output on the next build. It catches broken compiler include-dependency parsing.

### Verification milestones

Before implementing a connected feature, identify its observable behavior and
Full checkpoint (for example, a CPU audio request receiving its matching
completion). Keep that checkpoint bounded; expanding scope is not a reason to
postpone it indefinitely. Files, helpers and commits are not automatic Full
checkpoints. Small verified commits and Full frequency are separate decisions.

| Work stage or change | Required verification |
| --- | --- |
| Implementation iteration | Focused tests, then Fast at a coherent checkpoint |
| Approved connected behavior works | Full and affected RTL, cross-build, synthesis and other slice checks |
| Progress entries or link fixes only | Document/navigation checks (`python -B tools/check_harness.py`, review changed links and `git diff --check`) |
| Harness, build settings or verification scripts | Affected tooling tests and Full |
| Hardware candidate or milestone acceptance | All applicable acceptance checks; Fast cannot replace Full |

The document-only shortcut is limited to progress/link edits. Changes to product
specs, public contracts, acceptance criteria, build/verification procedures,
agent authority or stop rules, or documents consumed by tests/generators require
impact-based verification and Full when affected. File extension is not the rule.

Rerun a check only when its inputs change, a failure is fixed, a new concern
arises or integration scope expands. A progress-only addition after a pass needs
document checks, not another identical C++ analysis or RTL regression. A pass
never covers code changed after that run. Keep RTL suites sequential because
they share simulator work directories.

### Harness acceptance and measurement

`harness_negative` compares actual `ctest --show-only=json-v1` selections:
Fast = Full minus `{tidy}`, including tests added in the future. The configuration
guard rejects Full filtering and Fast overrides, retaining LLVM pinning, enabled
testing/clang tools and errors on empty selections. The configured build tree
is required for this selection check; a listing proves selection, not execution.

For wrapper/preset changes, also run the isolated Windows acceptance fixtures
with the pinned tools on PATH:

```powershell
python -B tests/harness/harness_tests.py --integration --evidence out/harness/workflow-acceptance.log
```

This explicitly exercises the real wrapper and a small temporary C++ project:
default/explicit Full, Fast, functional failure, forbidden Core/UI dependency,
tidy violation, failed build and empty selections. Intentional defects never
modify product sources or existing test oracles. These compiler/workflow probes
are opt-in rather than repeated inside every ordinary host loop.

Compare Full and Fast on the same source, machine and toolchain. Record configure,
build, test and tidy time separately under ignored `out/`; separate initial build
from incremental runs. Summarize measured results here, without adding an evidence
service. Track whether unchanged C++ analysis and duplicate runs disappear from
RTL/document iterations, and whether integration still has Full/RTL evidence.
One timing comparison measures command cost, not long-term development behavior.
Diff-based tidy, dependency graphs and persistent caches are deferred. If Fast
causes trouble, stop using it and run the unchanged default Full command.

### Initial measurement (2026-09-21)

Implemented on `eab0667`, preserving the intervening product work; the original
harness files were unchanged from the requested `55738ea` baseline. On the same
Windows machine with MSVC 14.51.36231, CMake 4.4.3 and LLVM 22.1.8, the default
Full wrapper passed 87/87 and `-Mode Fast` passed 86/86. Actual JSON selections
confirmed that the sole difference was `tidy`.

| Incremental measurement, seconds | Full | Fast |
| --- | ---: | ---: |
| Configure workflow step | 0.56 | 0.56 |
| Build workflow step | 0.16 | 0.17 |
| CTest total, including tidy when selected | 518.39 | 17.02 |
| tidy, included in the CTest total | 501.01 | Not run |
| Wrapper wall time, including setup | 521.63 | 20.09 |

Both builds reported `ninja: no work to do`; a separate Fast warmup took 20.71 s.
This is an incremental comparison, not a clean-build benchmark. Workflow step
times came from timestamped output boundaries; CTest reported test/tidy times.
The same source state was used for the compared runs. Logs, source hashes and
the measurement script are under `out/harness/`, with results in `timings.json`.
The observed wrapper reduction is about 96.1%; long-term duplicate-run reduction
and future integration discipline have not yet been measured.

`python -B tests/harness/harness_tests.py --integration --evidence
out/harness/workflow-acceptance.log` passed all 10 tests, including intentional
functional, dependency, tidy, build and empty-selection failures. After timing,
a selection-assertion fix preserved existing auxiliary builds without clang
tools: `python -B tests/harness/harness_tests.py HarnessTests` passed 9/9, and
the isolated auxiliary-build selection test passed. Standard Full/Fast still
require clang tools ON. The unchanged C++/tidy inputs were not reanalyzed after
that focused fix or this evidence-only addition. RTL simulation, cross-build,
synthesis and hardware checks were not run: their inputs/behavior did not change.
M6 acceptance remains pending; resume its current compatibility reassessment.

### Existing host environment notes

On 2026-09-13, a minimal compiled fixture reproduced stale consumers after
header changes: MSVC emitted UTF-8 Japanese include messages while CMake/Ninja
stored a misdecoded include prefix. Quoting the old trailing-space `VSLANG`
assignment alone did not fix it (only the Japanese compiler resources were
installed). Running the child command with `chcp 65001` made the fixture pass.
The wrapper now sets that code page and quotes the language assignment. This
does not install language packs or change global machine settings.

Existing build trees created under the old environment need a one-time
`cmake --fresh --preset host-msvc`, followed by
`cmake --build --preset host-msvc --clean-first`, in the VS developer environment
after `chcp 65001` and `set "VSLANG=1033"`. Then run the full wrapper. This repairs
cached prefix detection and rebuilds objects whose dependencies were missing.

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
