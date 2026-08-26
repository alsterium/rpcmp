# Pocket openfpgaOS Compatibility Spike

## 1. Status and scope

This document refines the next evidence step from `pocket-execution-candidates.md`. It does not adopt openfpgaOS, add upstream code to RPCMP, approve redistribution, or authorize installation of a new toolchain.

The file-level inspection used openfpgaSDK commit `a408ddc12aed0dfaa4aa22c06af82f829db77126`. Its bundled `runtime/MANIFEST` identifies openfpgaCore commit `618a3eb985759a4154115109c2c8036271252888` as the producer of the prebuilt runtime. The current core candidate revision `453a28350dab333b3afd520f8f8ac4508641bb3a` is newer. These inputs must not be mixed: an experiment either uses the SDK's internally matched prebuilt runtime or publishes a complete new runtime set from one pinned core revision.

The upstream repositories were inspected only in ignored `out/research` paths. They are not Git dependencies or distributable RPCMP inputs.

## 2. Confirmed compatibility blockers

### 2.1 C++ runtime profile

The default SDK C++ path is not sufficient to compile RPCMP's current public value types:

- `sdk.mk` passes `-fno-exceptions`, `-fno-rtti`, and `-nostdinc`;
- it deliberately does not link the toolchain's `libsupc++` or C++ standard library;
- `of_cxxabi.cpp` provides allocation, static initialization, and a few ABI hooks, but explicitly excludes `std::vector`, `std::string`, streams, and the rest of the standard library;
- RPCMP contracts currently use `std::string`, `std::vector`, `std::optional`, and `std::variant`, while the M0 runtime also uses `std::deque` and algorithms.

Therefore a successful plain C or minimal-class SDK demo is not evidence that RPCMP can run on this substrate. The first compile gate must build and link a small source against the real RPCMP contract headers and representative runtime containers. Acceptable outcomes are:

1. prove a pinned, license-reviewed C++ standard-library configuration compatible with the SDK's musl and static ELF ABI;
2. propose a target representation/facade that preserves the versioned contract semantics without changing the host ABI; or
3. reject the candidate.

The spike must not weaken bounded values, replace immutable snapshots with shared mutable state, or silently change a public v1 field to make the toolchain pass.

There is a concrete upstream route to test rather than inventing one. The reviewed openfpgaCore firmware container pins xPack `riscv-none-elf-gcc` 14.2.0-3 alongside the ordinary firmware compiler. The openfpgaOS Diablo port at commit `c4f1d24ad9db011dcfd5f3b1d90423ceb28cfd17` uses that xPack toolchain's C++ headers, `libstdc++.a`, and `libsupc++.a` with the SDK's musl CRT/library and explicit compatibility glue. This proves that a larger standard-library C++ application has an upstream build recipe; it does not prove that RPCMP's types link cleanly, that the runtime combination is supportable, or that its licenses and memory cost are acceptable.

### 2.2 Package and license boundary

RPCMP currently has no repository-level `LICENSE`, `NOTICE`, or REUSE manifest, so it has no approved redistribution posture for copyleft or mixed-license bitstreams. No upstream binary may become a release dependency until maintainers make that policy decision.

The stock SDK packaging path also cannot be used unchanged for an RPCMP experiment:

- `runtime/bank.ofsf` is identified by the upstream NOTICE as an SC-55-derived proprietary sample bank with no upstream license;
- the Pocket instance template assigns that bank to slot 7;
- `src/sdk/platforms/pocket/image.sh` copies every `runtime/*.ofsf` file into each custom Pocket image automatically;
- the prebuilt Pocket bitstream embeds separately licensed APF, Intel-generated IP, and GPL-2.0-or-later Analogizer/MiSTer-derived RTL in addition to project-authored Apache-2.0 code;
- the SDK NOTICE requires consumers of the prebuilt runtime to consult the core NOTICE rather than treating the SDK's broad Apache annotation as the whole binary license.

The proprietary sound bank and all demo music/assets are prohibited from the RPCMP spike. The package generator must omit slot 7, refuse `.ofsf`, `.mod`, and `.mid` files, and check the exact output tree before any archive reaches Pocket. This restriction does not resolve the remaining bitstream licenses; those still require a reviewed notice set and an explicit project distribution policy.

## 3. Minimal experiment package

The first on-device compatibility package should contain only:

```text
Cores/<rpcmp-spike-id>/
  audio.json
  core.json
  data.json
  input.json
  interact.json
  loader.bin
  <selected-variant>.rbf_r
  variants.json
  video.json
Assets/<rpcmp-spike-platform>/common/
  os.bin
  rpcmp-probe.ini
  rpcmp-probe.elf
  synthetic.bin
Assets/<rpcmp-spike-platform>/<rpcmp-spike-id>/
  rpcmp-probe.json
Platforms/
  <rpcmp-spike-platform>.json
```

The provisional slots are 1 for `os.bin`, 2 for the app configuration, 3 for the ELF, and 4 for a small project-authored synthetic file. Slot 7 and nonvolatile slots are omitted. This experiment does not introduce `.rpcmlib` bytes; it proves only the storage adapter needed before M1 chooses chunk and cache bounds.

Every runtime artifact must be matched to one manifest/source revision. The package evidence must record SHA-256 for the bitstream, loader, OS binary, ELF, synthetic file, and ZIP; validate JSON roots and paths; and reject unexpected files.

## 4. Bounded execution plan

1. **Toolchain gate:** obtain explicit approval for a WSL2 Linux and container environment as the leading Windows experiment. Upstream documents Linux and macOS container hosts, so WSL2 compatibility must itself be verified. Record WSL distribution, container runtime, xPack 14.2.0-3, and all relevant licenses before installation or first use.
2. **C++ gate:** compile/link a header-level contract probe using string, vector, optional, variant, and deterministic fixed-width values. Stop if the candidate requires a public contract change or an unreviewed C++ runtime.
3. **Host gate:** run the candidate's desktop shim with a generated snapshot and one command while retaining RPCMP's existing headless and architecture tests.
4. **Package gate:** generate only the minimal tree above and mechanically reject the proprietary bank, demo media, unknown files, mixed runtime revisions, and invalid APF definitions.
5. **Pocket gate:** verify build identity, input-to-command, immutable snapshot display, and beginning/middle/end reads from `synthetic.bin`.
6. **Custom-RTL gate:** only after the software path passes, build a stripped variant with a fake device queue and record fit/timing. Measure remaining resources before evaluating JT51.

The current Windows host is not ready for gate 1: `make`, `bash`, Docker, and both checked RISC-V GCC command names are absent; `wsl.exe` reports no installed Linux environment. Linux/GCC CI remains separately deferred and must not be reported as restored merely because a local target toolchain is later installed.

## 5. Dependency screen

| Component | Candidate purpose | Upstream license evidence | Spike status |
|---|---|---|---|
| openfpgaSDK authored sources/config | App ABI, headers, packaging, desktop shim | Apache-2.0 with REUSE annotations | Inspected, not added |
| Bundled musl 1.2.5 | C library and static startup | MIT | Inspected, not added |
| xPack RISC-V GCC 14.2.0-3 C++ runtime | Contract/runtime compatibility probe | GCC and bundled runtime licenses require a recorded tool/distribution review | Upstream recipe identified, not installed |
| openfpgaCore authored RTL/firmware | RISC-V runtime and APF services | Apache-2.0 with REUSE annotations | Inspected, not added |
| Analogue APF source | Pocket shell integration | `LicenseRef-Analogue-Pocket-Framework` | Terms must be retained and reviewed |
| Intel/Altera generated IP | PLL/memory integration | Intel FPGA IP terms | Generated/distribution terms must be reviewed |
| Analogizer/MiSTer-derived RTL | Optional video/output functions in the prebuilt bitstream | GPL-2.0-or-later in the reviewed manifest | Project license policy unresolved |
| `bank.ofsf` and demo music | Unneeded MIDI/demo media | Proprietary or unverified third-party media | Must be excluded |
| JT51 | Later YM2151-compatible RTL candidate | GPL-3.0 | Not part of this spike; separate license/resource gate |

## 6. Evidence sources

- [SDK build rules](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/sdk.mk)
- [SDK minimal C++ ABI](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/of_cxxabi.cpp)
- [SDK Pocket image assembly](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/platforms/pocket/image.sh)
- [SDK instance template](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/platforms/pocket/templates/instance.json)
- [SDK licensing annotations](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/REUSE.toml)
- [Pinned xPack firmware container](https://github.com/openfpgaOS/openfpgaCore/blob/453a28350dab333b3afd520f8f8ac4508641bb3a/tools/docker/Dockerfile.firmware)
- [Runtime-producing core licensing annotations](https://github.com/openfpgaOS/openfpgaCore/blob/618a3eb985759a4154115109c2c8036271252888/REUSE.toml)
- [Diablo C++/musl link recipe](https://github.com/openfpgaOS/Diablo/blob/c4f1d24ad9db011dcfd5f3b1d90423ceb28cfd17/src/diablo/Makefile)
