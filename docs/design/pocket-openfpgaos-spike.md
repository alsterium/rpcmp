# Pocket Execution-Substrate Comparison Spike

## 1. Status and scope

This document refines the next evidence step from `pocket-execution-candidates.md`. It compares openfpgaOS with a project-owned minimal soft-CPU shell using one common RPCMP probe. It does not adopt either substrate, add upstream code to RPCMP, approve redistribution, or authorize installation of a new toolchain.

The common probe is non-production feasibility code under `spikes/pocket`. It may link the real contracts and Mock Core, but it must not become a dependency of `core/runtime`, `core/ui`, or either public contract. `core/platform/pocket` remains a compile-free production placeholder until a superseding ADR selects a substrate.

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

### 2.3 Project-owned minimal SoC profile

The comparison profile is informed by HarpMudd.mp3player but does not fork or copy that player. Its allowed functional surface is:

- one reviewed RV32 CPU configuration and the smallest measured instruction/data memory that runs the common probe;
- APF baseline boot/reset/heartbeat handling inherited from the pinned official template;
- PAD input translated into a probe command outside Core;
- Target dataslot read support and only the data-table/parameter RAM needed by the documented command structs;
- a bounded fake-device queue with explicit full/overflow behavior;
- the minimum video path needed to display the observation record, with rendering skippable.

MP3/FLAC decoders, PCM playback, EQ, album art, playlists, existing player firmware/UI, and copied release media are outside the comparison. HarpMudd's empirically observed stale-completion and data-table-collision hazards become focused tests, not undocumented protocol requirements.

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

The project-owned minimal-SoC package uses the same logical slot purposes and synthetic payload. It may omit `os.bin` and the app configuration when firmware is loaded directly as its reviewed design specifies, but the resulting difference must be recorded rather than hidden in a nominally identical tree.

## 4. Common probe contract

The host-first probe uses the real `PlayerCommand`, `PlayerSnapshot`, and deterministic Mock Core. Its result is a spike-local value record, not a public wire format or C ABI. A run must cover:

1. generated synthetic bytes whose value at offset `n` is `(n * 37 + 11) mod 256`;
2. bounded reads at the beginning, a middle offset, and the final readable range, plus rejection of an out-of-range read;
3. open-library, load-track, play, pause, resume, and stop commands with strictly increasing IDs;
4. a recent duplicate, a stale ID, and a full command queue with one rejected overflow;
5. at least 120 published snapshots and the complete fake-device event trace;
6. a second run that omits or delays all rendering observation while applying the identical clock/command trace.

Both runs must yield value-identical command outcomes, snapshots, fake-device events, and storage-read checks. Target adapters may change how bytes, input, and observations are transported, but they may not change the contract values or transitions.

## 5. Bounded execution plan

1. **Host gate:** build and run the common probe under the existing M0 verification workflow, including observed and renderer-free traces.
2. **Dependency gate:** pin all candidate/reference revisions and record file-level licenses before importing any target source into ignored research output.
3. **Toolchain gate:** use Docker Desktop's WSL 2 Linux engine as the leading openfpgaOS experiment. Record the container runtime, pinned base image, xPack 14.2.0-3, and all relevant licenses before first image use.
4. **openfpgaOS C++ gate:** compile/link the unchanged common probe. Stop if the candidate requires a public contract change or an unreviewed C++ runtime.
5. **openfpgaOS host/package gate:** run through the desktop shim and generate only the minimal tree above, mechanically rejecting proprietary media, unknown files, mixed runtime revisions, and invalid APF definitions.
6. **minimal-SoC build gate:** construct the allowed profile from the pinned official template and independently reviewed source patterns, then compile the common probe or a documented semantic facade.
7. **Pocket gate:** on both candidates, verify build identity, input-to-command, immutable snapshot display, repeated Target commands, and beginning/middle/end reads from `synthetic.bin`.
8. **resource gate:** capture equivalent fit/timing and runtime measurements, then compare remaining resources before evaluating JT51.

The current Windows host uses Docker Desktop 4.88.1, Engine 29.7.2, and the WSL 2 `docker-desktop` environment. The spike image pins the Ubuntu 24.04 OCI index to `sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517` and verifies the architecture-specific xPack archive before extraction. Linux/GCC CI remains separately deferred and must not be reported as restored merely because this local target toolchain is available.

## 6. Dependency screen

| Component | Candidate purpose | Upstream license evidence | Spike status |
|---|---|---|---|
| openfpgaSDK authored sources/config | App ABI, headers, packaging, desktop shim | Apache-2.0 with REUSE annotations | Inspected, not added |
| Bundled musl 1.2.5 | C library and static startup | MIT | Inspected, not added |
| xPack RISC-V GCC 14.2.0-3 C++ runtime | Contract/runtime compatibility probe | xPack packaging is MIT; the archive retains component notices under `distro-info/licenses`, including GCC/libstdc++ runtime exception and the licenses for Binutils, Newlib, and GDB/Python payloads | Approved for build tooling only; archive SHA-256 is pinned and checked, redistribution in an RPCMP release is not required |
| Ubuntu 24.04 container base and APT tools | Reproducible Linux build environment | Ubuntu package metadata/notices remain in the image; base OCI index is digest-pinned | Approved for local/CI build tooling only; installed package versions must be captured with the evidence record |
| openfpgaCore authored RTL/firmware | RISC-V runtime and APF services | Apache-2.0 with REUSE annotations | Inspected, not added |
| Analogue APF source | Pocket shell integration | `LicenseRef-Analogue-Pocket-Framework` | Terms must be retained and reviewed |
| Intel/Altera generated IP | PLL/memory integration | Intel FPGA IP terms | Generated/distribution terms must be reviewed |
| Analogizer/MiSTer-derived RTL | Optional video/output functions in the prebuilt bitstream | GPL-2.0-or-later in the reviewed manifest | Project license policy unresolved |
| `bank.ofsf` and demo music | Unneeded MIDI/demo media | Proprietary or unverified third-party media | Must be excluded |
| JT51 | Later YM2151-compatible RTL candidate | GPL-3.0 | Not part of this spike; separate license/resource gate |
| ModPlayer_openfpgaos | API and application-shape evidence only | No root license file found at reviewed revision; bundled runtime provenance requires separate review | Reference only; no import |
| HarpMudd project-authored source | Minimal-SoC and APF command patterns | MIT root, with separately licensed bundled components | Reference only; no wholesale import |
| VexRiscv configuration | Candidate soft CPU | MIT in the reviewed HarpMudd credits/upstream source; exact generated revision must be pinned | Not added |

### Toolchain gate record — 2026-08-26

- Runtime: Docker Desktop 4.88.1, Engine 29.7.2, Linux `amd64` through WSL 2.
- Base: Ubuntu 24.04 OCI index
  `sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517`.
- Cross compiler: xPack GNU RISC-V Embedded GCC 14.2.0-3; Linux x64
  archive SHA-256
  `f574415b63f12b09bdd3475223ab492a465d23810646c90c13a4c3b676c83503`.
- Container packages observed: `ca-certificates 20260601~24.04.1`,
  `curl 8.5.0-2ubuntu10.13`, `g++ 4:13.2.0-7ubuntu1` (using
  `g++-13 13.3.0-6ubuntu2~24.04.1`), `git 1:2.43.0-1ubuntu7.3`, `libsdl2-dev
  2.30.0+dfsg-1ubuntu3.1`, `make 4.3-4.1build2`, and `pkg-config
  1.8.1-2build1`, and `python3 3.12.3-0ubuntu2.1`. These APT package versions
  are evidence from this build, not an additional floating contract. G++,
  Git, pkg-config, and Python are build tools; SDL2 is used only by the SDK
  desktop shim. Their package notices remain in the build image and none are
  RPCMP release artifacts.
- The xPack archive retained notices for GCC 14.2.0, Binutils 2.43.1,
  Newlib 4.4.0.20231231, GDB 15.1, Python 3.12.2, and their bundled support
  libraries under `/opt/xpack/distro-info/licenses`. The tool image is not an
  RPCMP release artifact.
- The unchanged RPCMP contract, Mock Core, and common probe sources compiled
  and statically linked with the pinned SDK musl at
  `a408ddc12aed0dfaa4aa22c06af82f829db77126`.
- Result: ELF32 RISC-V, RVC, single-float ABI; text 152,367 bytes, data 136
  bytes, BSS 2,600 bytes, total 155,103 bytes; artifact SHA-256
  `16106536a5c3011f727d5d9396513dcf8d48a62b9cce616fd7f418985951c0bf`.
- Compatibility note: the SDK's nine-byte `libm.a` is an empty archive with a
  CRLF marker that xPack `ld` rejects. The probe references no math symbols,
  so its focused link recipe omits `-lm`; production code requiring libm must
  resolve this separately.
- The SDK PC backend read the generated fixture through provisional data slot
  4 and reproduced the host golden record: 9 command outcomes, 151 snapshots,
  and 154 fake-device events with digests `2446879228733133299`,
  `6832192089657554689`, and `16311210033269188847`, respectively. Its
  SDK-backed file operations used the same entry points as the target adapter.
- This closes the compile/link and desktop-shim portions of the openfpgaOS C++
  gate. The target ELF has not run on Pocket.

### Package gate record — 2026-08-26

- The generator accepts only SDK revision
  `a408ddc12aed0dfaa4aa22c06af82f829db77126` and runtime manifest source
  `618a3eb`; it verifies the manifest MD5 values for `loader.bin`, `os.bin`,
  and `os25.rbf_r` before copying them.
- The exact output allowlist contains 15 files. Its data definition contains
  only slots 0 through 4, with a 4,096-byte project-generated slot 4 fixture;
  nonvolatile slots and `.ofsf`, `.mod`, and `.mid` files are rejected.
- Every APF JSON file has the expected root and `APF_VER_1` magic where the
  definition requires it. The generated evidence JSON records each file's
  byte count and SHA-256 separately from the Pocket tree.
- Two consecutive builds produced ZIP SHA-256
  `8fa4580bd3c1e3d513e6e3ea46a09dfa0643a1d4f761cdf673aaa56287ca2c19`
  (1,134,397 bytes).
- The ZIP is a local experiment only. This gate mechanically excludes the
  proprietary sound bank and demo media, but it does not approve
  redistribution of the prebuilt bitstream or other upstream runtime files.

## 7. Acceptance record

Each candidate must produce one reviewable record containing:

- exact source/tool revisions and license inventory;
- hashes and byte sizes of firmware, bitstream, loader/runtime, synthetic asset, and package;
- ALMs, registers, block-memory bits/blocks, DSPs, PLLs, warnings, all declared clocks, and worst setup/hold/pulse-width slack;
- probe result values, storage-read offsets/checks, command outcomes, snapshot/event counts, queue-overflow result, reset/relaunch result, and renderer-free trace comparison;
- measured Target-command and data-read latency, including repeated reads;
- unavailable or failed checks with exact reason.

Selection requires a superseding ADR. A candidate does not pass merely because it boots or plays audio.

## 8. Evidence sources

- [SDK build rules](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/sdk.mk)
- [SDK minimal C++ ABI](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/of_cxxabi.cpp)
- [SDK Pocket image assembly](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/platforms/pocket/image.sh)
- [SDK instance template](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/src/sdk/platforms/pocket/templates/instance.json)
- [SDK licensing annotations](https://github.com/openfpgaOS/openfpgaSDK/blob/a408ddc12aed0dfaa4aa22c06af82f829db77126/REUSE.toml)
- [Pinned xPack firmware container](https://github.com/openfpgaOS/openfpgaCore/blob/453a28350dab333b3afd520f8f8ac4508641bb3a/tools/docker/Dockerfile.firmware)
- [Runtime-producing core licensing annotations](https://github.com/openfpgaOS/openfpgaCore/blob/618a3eb985759a4154115109c2c8036271252888/REUSE.toml)
- [Diablo C++/musl link recipe](https://github.com/openfpgaOS/Diablo/blob/c4f1d24ad9db011dcfd5f3b1d90423ceb28cfd17/src/diablo/Makefile)
- [ModPlayer openfpgaOS application](https://github.com/RndMnkIII/ModPlayer_openfpgaos/tree/544e7fb569eeffc38525ed409464f59bcd932108)
- [HarpMudd architecture notes](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/docs/HOW_IT_WORKS.md)
- [HarpMudd feasibility measurements](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/STAGE0_RESULTS.md)
- [HarpMudd APF Target-command crossing](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/src/fpga/core/tgt_cmd.v)
