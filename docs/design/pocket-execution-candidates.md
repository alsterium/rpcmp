# Pocket Execution-Substrate Candidates

## 1. Status and decision boundary

This document records a source-level candidate survey for ADR-0004. It does not select a runtime, add a dependency, approve a license, or claim that RPCMP fits or runs on a candidate platform.

The survey was performed on 2026-08-26 against these exact upstream revisions:

- [openfpgaOS/openfpgaCore `453a283`](https://github.com/openfpgaOS/openfpgaCore/tree/453a28350dab333b3afd520f8f8ac4508641bb3a);
- [openfpgaOS/openfpgaSDK `a408ddc`](https://github.com/openfpgaOS/openfpgaSDK/tree/a408ddc12aed0dfaa4aa22c06af82f829db77126);
- [jotego/jtcores `c96437c`](https://github.com/jotego/jtcores/tree/c96437c1eb2e2e99dfe6f523d9b6bfc45a74689f);
- [jotego/jt51 `985a573`](https://github.com/jotego/jt51/tree/985a573dcfc1ff135553a39f7eae21d18ba57cbe).

Upstream `main` and `master` branches are moving inputs. Any later spike must pin revisions again rather than assuming the observations below still apply.

## 2. Candidate comparison

| Candidate | What it provides | Main fit for RPCMP | Blocking evidence |
|---|---|---|---|
| Project-owned soft CPU and firmware | Maximum control over CPU, memory, device queue, and APF integration | Can preserve the current contracts with a purpose-built minimal target | CPU/toolchain selection, storage stack, C/C++ runtime, video, and resource cost are all unproven; this is the largest implementation scope |
| openfpgaOS runtime and SDK | RISC-V C/C++ apps, APF data-slot files, input, framebuffer video, 48 kHz audio, a PC shim, and a Pocket package/runtime | Closest existing match for running the portable Core and replaceable UI as software while keeping sound hardware in RTL | A custom RPCMP variant, direct device-register/FIFO extension, C++ compatibility, timing, resource headroom, and distribution obligations must be measured |
| JTFRAME/JTCORES | Mature declarative multi-core HDL generation, target wrappers, generated memory-mapped registers, and shared synthesis/simulation projections | Strong reference for organizing target-specific HDL and for later JT51 integration | It is not a general-purpose C++ runtime, and the JTFRAME Pocket target is a non-public submodule, so it cannot supply RPCMP's reproducible Pocket substrate |
| RTL-only player | No software CPU/runtime dependency | Small boot surface in a narrowly fixed player | Conflicts with reuse of the host C++ parser/library/UI implementation and makes variable-sized library, metadata, and UI work substantially harder; retain only as a fallback if software substrates cannot fit |

openfpgaOS is the leading candidate for the next bounded compatibility spike. This is a prioritization for gathering evidence, not an architecture decision.

## 3. openfpgaOS observations

At the reviewed revision, openfpgaOS documents a VexiiRiscv RV32IMAFC CPU, a 64 MiB SDRAM map, APF data-slot file access, framebuffer video, exact 48 kHz audio output, and a static ELF app ABI. Its SDK advertises C and C++ support, desktop testing through an SDL2 shim, and Pocket packaging. This is directionally compatible with RPCMP's platform-adapter boundary: storage, input, rendering, and time can remain behind ports while the public snapshot and command values stay platform-neutral.

Important constraints remain:

- the firmware is single-process with no MMU or scheduler, so RPCMP must own deterministic sequencing and explicitly isolate rendering work;
- the default SDK C++ ABI excludes exceptions, RTTI, and the C++ standard library. RPCMP's current contract types depend on string, vector, optional, and variant, so compatibility requires a separately reviewed runtime configuration or target facade rather than a plain SDK build;
- the documented Pocket variants already use 87–89% of the 18.5K-ALM device; this is upstream-reported utilization, not an RPCMP measurement;
- the variant/addon mechanism can remove features and add custom RTL, but a stripped RPCMP variant must be built and fitted before assuming that a YM2151 core, command queue, and audio adapter fit;
- the standard 32-voice PCM mixer is not a substitute for the required YM2151-compatible RTL interface;
- the upstream core-build workflow documents Linux/macOS container hosts, while the SDK documents a Windows MSYS2 RISC-V toolchain. RPCMP must verify a supported, repeatable Windows path or deliberately add a separate target-build environment.

The current RPCMP Windows host has no `riscv64-unknown-elf-gcc`, `riscv64-elf-gcc`, `make`, `bash`, or Docker command. The `wsl.exe` launcher is present but reports no installed Linux environment. Installing any of these is a separate toolchain decision, not an implicit consequence of this survey.

## 4. License and distribution screen

No surveyed source is added to RPCMP by this document.

- openfpgaCore and openfpgaSDK use Apache-2.0 for project-authored code, but their REUSE manifests identify separately licensed content. The reviewed core includes Analogue APF files, GPL-licensed optional target code, Intel-generated IP, and a proprietary sample-data asset. A candidate spike must inventory the exact files and generated artifacts it distributes, exclude unrelated proprietary sample data, retain required notices, and resolve every included component rather than relying on the repository-level license alone.
- JTCORES and JT51 identify GPL-3.0 licensing. Reusing JTFRAME or synthesizing JT51 into a distributed bitstream therefore requires a deliberate source/distribution policy and notices before either becomes a dependency.
- The private JTFRAME Pocket target cannot be the basis of a reproducible open-source RPCMP build even if other JTFRAME patterns are adopted independently.

The exact openfpgaOS file/package blockers and the proposed compatibility gates are recorded in `pocket-openfpgaos-spike.md`.

## 5. Next bounded spike

The next substrate experiment should be split into reviewable gates:

1. pin candidate revisions and produce a file-level dependency/license manifest without importing upstream code into production paths;
2. cross-build a minimal C++ app that reuses RPCMP contract values and deterministic mock-runtime logic with exceptions and RTTI disabled;
3. run the same app through the candidate's desktop shim and on Pocket, proving input-to-command, immutable snapshot rendering, and a synthetic data-slot read;
4. create a stripped RPCMP-specific FPGA variant with unused game GPU/mixer features removed, then record ALM, register, RAM, DSP, warning, and timing deltas;
5. add only a fake bounded device queue/register endpoint, verify that sequencing continues while rendering is skipped, and measure bridge/storage latency;
6. compare the measured remaining resources with a separately synthesized YM2151 candidate before proposing a superseding ADR.

Failure at any gate must leave the current host architecture and public contracts intact. No production Pocket adapter or sound-core dependency should be added until the spike records its exact source revisions, license implications, resource report, timing result, and on-device evidence.

## 6. Sources

- [openfpgaOS architecture](https://github.com/openfpgaOS/openfpgaCore/blob/453a28350dab333b3afd520f8f8ac4508641bb3a/docs/ARCHITECTURE.md)
- [openfpgaOS variants and addons](https://github.com/openfpgaOS/openfpgaCore/blob/453a28350dab333b3afd520f8f8ac4508641bb3a/docs/VARIANTS_AND_ADDONS.md)
- [openfpgaOS licensing annotations](https://github.com/openfpgaOS/openfpgaCore/blob/453a28350dab333b3afd520f8f8ac4508641bb3a/REUSE.toml)
- [openfpgaOS SDK getting started](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/GETTING_STARTED.md)
- [JTFRAME command-line and generated-project model](https://github.com/jotego/jtcores/blob/c96437c1eb2e2e99dfe6f523d9b6bfc45a74689f/modules/jtframe/doc/jtframe.md)
- [JTFRAME compilation prerequisites and Pocket-target limitation](https://github.com/jotego/jtcores/blob/c96437c1eb2e2e99dfe6f523d9b6bfc45a74689f/modules/jtframe/doc/compilation.md)
- [JT51 source and usage](https://github.com/jotego/jt51/tree/985a573dcfc1ff135553a39f7eae21d18ba57cbe)
