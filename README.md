# Retro PC Music Player (RPCMP)

Codex handoff package for a portable retro-computer music player targeting Analogue Pocket/openFPGA.

RPCMP reconstructs period sound hardware in FPGA logic and plays source music data through a modular runtime. The first functional target is X68000 MDX driving YM2151-compatible FM synthesis. PDX/MSM6258 and PCM8 follow after FM-only playback is proven.

## Product shape

```text
Desktop Utility -> .rpcmlib -> Player Core -> sound-device/audio adapters
                                  |   ^
                           snapshots commands
                                  v   |
                           Replaceable UI
```

The defining rule is `Core -> UI = State` and `UI -> Core = Command`. Playback must continue correctly with no UI attached, and a UI must be developable with mock state and no playback engine.

## Start here

- Product scope: `docs/PRD.md`
- Architecture and dependency rules: `docs/ARCHITECTURE.md`
- Public contracts: `specs/`
- First implementation task: `docs/milestones/M0-architecture.md`
- M0 executable design: `docs/design/m0-executable-design.md`
- Pocket platform boundary: `docs/design/pocket-platform-boundary.md`
- Pocket execution candidates: `docs/design/pocket-execution-candidates.md`
- Architecture decisions: `docs/adr/`
- Copy-ready Codex prompt: `prompts/implement-M0.md`

## Intended repository layout

```text
utility/                 host-side library builder
core/runtime/            platform-neutral player core
core/ui/                 replaceable presentation layer
core/rtl/                sound devices and audio hardware
tests/                   cross-module fixtures and integration tests
third_party/             vendored dependencies and notices
```

M0 selects C++17 and CMake for the host architecture skeleton. Pocket execution and a concrete YM2151 core remain gated by target-toolchain and license evidence; see the M0 ADRs.

## Recommended execution

1. Initialize a repository with this package at its root.
2. Ask Codex to read `AGENTS.md` and implement M0 only.
3. Review the public contracts and architecture tests before accepting implementation.
4. Continue milestone-by-milestone; do not jump directly to full MDX playback.

## M0 host verification

On Windows, install Visual Studio C++ Build Tools, CMake, and the pinned LLVM development tools:

```powershell
scoop install llvm
```

The `host-msvc` preset requires LLVM 22.1.8. Run the complete build, format, static-analysis, architecture, and test workflow with:

```powershell
pwsh -File tools/host-verify.ps1
```

After configuring the preset, the developer checks are also available individually as `format`, `format-check`, and `tidy-check` CMake targets. Linux/GCC CI is currently deferred and is not part of the reported host coverage.

## Local Codex Skill

The Analogue Pocket/openFPGA development guidance is tracked as a Git submodule at
`.codex/skills/analogue-openfpga`. Clone this repository with submodules enabled:

```sh
git clone --recurse-submodules https://github.com/alsterium/rpcmp.git
```

For an existing checkout, initialize or update it with:

```sh
git submodule update --init --recursive
```
