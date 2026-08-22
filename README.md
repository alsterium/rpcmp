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

This package deliberately does not select C++, Rust, a build system, or a concrete YM2151 core. M0 must record those choices in an ADR after checking the target toolchain and licenses.

## Recommended execution

1. Initialize a repository with this package at its root.
2. Ask Codex to read `AGENTS.md` and implement M0 only.
3. Review the public contracts and architecture tests before accepting implementation.
4. Continue milestone-by-milestone; do not jump directly to full MDX playback.

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
