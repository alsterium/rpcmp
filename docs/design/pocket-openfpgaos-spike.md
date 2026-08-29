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

The minimal-SoC row is not satisfied by synthesizing a CPU alone. It must use
the pinned official Pocket shell, run the same target facade and synthetic
fixture, and account for every byte of executable memory. The linker map must
record all loadable segments, BSS, stack, bounded heap, queue storage, and
alignment loss; at least 20% of the selected instruction/data-memory capacity
must remain after those declared maxima. If code or mutable state is placed in
SDRAM, the comparison must also implement and test instruction/data arbitration,
Target-DMA visibility or cache invalidation, and reset/reload behavior. An
opaque pre-generated CPU without its exact generator revision and configuration
does not pass the dependency gate.

### 2.4 Comparison pass criteria and provisional decision rule

Each candidate must pass the same semantic, package, reset, input, bounded-read,
queue-overflow, and renderer-free checks. Its final comparison fit must include
JT51 and the actual bounded register/FIFO endpoint, not an arithmetic estimate,
and meet all of these provisional engineering limits:

- no more than 80% of ALMs, M10Ks, or DSP blocks, with at least one PLL free;
- nonnegative setup, hold, recovery/removal, and minimum-pulse-width slack at
  every supported corner, with no unconstrained functional clock or path;
- an accepted warning baseline in which every critical warning is resolved or
  explicitly proven irrelevant to the supported configuration;
- a repeatable build from pinned corresponding source and an approved release
  license/notice plan.

The current leading proposal is the stripped openfpgaOS profile, not stock
`os25`, because it has already run the unchanged probe and Target reads on
Pocket and its conservative resource budget including JT51 is below the 80%
limits. It is not yet selected: its 100 MHz timing fails, JT51 is not integrated,
the smaller-cache profile has not run the target ELF, and redistribution is not
approved. The next openfpgaOS experiment should integrate the queue endpoint
and JT51 under a fully constrained clock profile that actually closes timing;
90 MHz is a measured starting hypothesis, not an accepted frequency.

The project-owned minimal SoC remains the required control. It should be chosen
only if it passes the same hardware checks and materially improves at least one
binding concern—timing margin, resource headroom, redistribution simplicity, or
maintenance surface—without weakening the public contracts or moving storage
or rendering into the audio-critical path. If it cannot run the common probe
within the limits above, the comparison records that failure rather than
keeping the substrate decision open indefinitely.

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
| JT51 | YM2151-compatible RTL candidate | GPL-3.0-or-later in every reviewed HDL header; repository license text is GPL-3.0 | Measured separately at pinned revision; no source imported and release policy unresolved |
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
- Result: ELF32 RISC-V, RVC, single-float ABI; text 155,915 bytes, data 304
  bytes, BSS 3,628 bytes, total 159,847 bytes; artifact SHA-256
  `8caec346268d5ebe8a85247209819d05f6ebca059119a17fcc8b6263030edf6b`.
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
  gate. The target ELF subsequently passed the Pocket execution record below.

### Target adapter gate record — 2026-08-27

- The target ELF now links the pinned SDK's `of_init.c`, keeping capability
  and service-table initialization in the SDK-defined constructor path.
- A spike-local C adapter opens `slot:4` through the SDK-documented stdio
  path, validates its measured size, and performs bounded offset reads. SDK
  headers and service-table details remain outside Core and the C++ probe.
- The adapter selects the 40x30 terminal before the probe starts. After both
  observed and renderer-free runs finish, it prints all four storage checks,
  golden counts and digests, final sequence, and `RESULT: PASS` or `RESULT:
  FAIL`. It then waits on the SDK vblank service so rendering cannot affect
  the completed trace.
- Static ELF inspection confirms the slot, terminal, and result-hold adapter
  symbols and their `slot:%lu`/result strings are linked. The Pocket execution
  record below supplies the corresponding on-device evidence.

### Package gate record — 2026-08-26, updated 2026-08-27

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
- Two consecutive builds of the target-adapter package produced ZIP SHA-256
  `6c261468641a4afd00fc5865c9bbd9e80d0f6593a2b66d849f135d4ca7a45fdb`
  (1,138,231 bytes).
- The ZIP is a local experiment only. This gate mechanically excludes the
  proprietary sound bank and demo media, but it does not approve
  redistribution of the prebuilt bitstream or other upstream runtime files.

### Pocket core-setup correction record — 2026-08-27

- The first Pocket trial of the earlier package (ZIP SHA-256
  `27810cfd3fecf50b1fc85f454d71c2dac054e114b3fbec3ec12b6479399d5d7b`)
  stopped with `Load error in core: General error` and `Error in core setup`.
  The failure happened during Pocket core setup, before there was evidence that
  the target ELF ran.
- The package used `RPCMP.openfpgaOSProbe` as its core folder while declaring
  `RPCMP Probe` as `metadata.shortname`. This violated the documented
  `AuthorName.CoreName` folder-to-metadata correspondence and could prevent the
  core-specific instance path from resolving. This mismatch is confirmed; its
  responsibility for the observed error remains an inference until Pocket is
  retested.
- The corrected package declares `RPCMP.openfpgaOSProbe` consistently, makes
  openfpgaOS slots 1 through 4 optional and deferred as in the pinned SDK custom
  core template, supplies the template's non-empty controller mappings, and
  removes the unused `variant_select` member from the single-bitstream instance.
- Generator validation and regression tests now reject the folder/metadata
  mismatch, eager openfpgaOS application slots, empty controller mappings, and
  `variant_select` in this instance. The corrected package requires Pocket
  firmware 2.2 or later and passed the on-device probe below. Because several
  setup fields changed together, the original error cannot be attributed to
  only one field.

### Pocket execution gate record — 2026-08-28

- Before the run, the SD-card core files were read back and matched the local
  corrected package byte-for-byte by SHA-256. The required Core, Assets, and
  Platform paths were present and every APF JSON file parsed successfully.
- The core appears in Developer Builds under its metadata shortname
  `openfpgaOSProbe`, rather than under `RPCMP`; the initially reported missing
  entry was a list-position/display-name misunderstanding, not a package loss.
- The user ran the corrected package on Pocket and observed `RESULT: PASS` on
  its terminal. The target prints PASS only after all four bounded slot 4 reads,
  the command outcomes, 151 snapshots, 154 fake-device events, and the observed
  versus renderer-free golden digest comparisons succeed. This closes the
  target ELF execution, deferred-slot read, and semantic-equivalence portions
  of the openfpgaOS Pocket gate.
- On Pocket firmware 2.6, the user then observed PASS on three consecutive core
  relaunches and once more after a complete power-off/start cycle. Together with
  the initial run, all five observed executions passed; this closes the
  reset/relaunch repetition evidence for this package and firmware.
- The physical input-to-command path, Target-command latency, and data-read
  latency were not captured by these runs and remain open acceptance evidence.

### Interactive input and read-latency package — 2026-08-28

- Package version `0.2.0-spike` retains the automatic golden comparison and
  adds a platform adapter that maps Pocket A, B, and START press edges to the
  abstract Play, TogglePause, and Stop probe actions. Pocket button names and
  SDK types do not enter Core, its contracts, or the common probe.
- The interactive sequence is Play, TogglePause, TogglePause, Stop. Each action
  is submitted as a `PlayerCommand`, Core advances independently of rendering,
  and the immutable snapshots must report playing, paused, playing, and stopped
  before the terminal prints `INPUT: PASS` and `OVERALL: PASS`.
- Before accepting input, the target performs 32 rotating 16-byte reads from
  synthetic slot 4, verifies every byte, and reports minimum, integer-average,
  and maximum microseconds. Unsigned 32-bit subtraction makes each duration
  valid across the SDK timer's approximately 71-minute wrap. This is the
  application-visible `fopen`/`fseek`/`fread`/`fclose` logical read path, not a
  raw APF Target-command measurement.
- Host tests cover the exact command/snapshot sequence, unexpected-action
  rejection, all latency aggregates, synthetic content, and timer wrap. The
  fixed Docker build produced ELF SHA-256
  `d90e211ea252274473ccabc1319be30048a166336a94bab293c86bc26a79b150`
  (232,536 bytes; loadable sections total 164,831 bytes).
- Two complete builds in the fixed Docker environment produced the same ZIP
  SHA-256
  `ccd6d4ef253861e82d49df2c3bfdd84e04405b073a6d5c4c6c1cf8a57132aa82`
  (1,142,498 bytes). Cross-environment ZIP compression is not the stated
  reproducibility boundary; the fixed container is.
- The user exercised `0.2.0-spike` on Pocket and observed `INPUT: PASS` and
  `OVERALL: PASS`. Those terminal results are emitted only after the automatic
  golden gate passes, all 32 reads return matching content, and the physical
  Play, Pause, Resume, and Stop sequence produces the expected immutable Core
  snapshots. The most recently reported firmware for this Pocket is 2.6.
- The displayed end-to-end logical read times were minimum 1,215 us, integer
  average 1,254 us, and maximum 1,310 us for 32 reads of 16 bytes. This closes
  the repeated application-visible data-slot read measurement for the bounded
  probe. It does not establish throughput for larger transfers or isolate the
  underlying APF command, filesystem, seek, and close components. Raw
  Target-command latency remains open.

### Target dataslot-read and bounded device-queue package — 2026-08-28

- Package version `0.3.0-spike` preserves every earlier automatic and input
  gate. It adds a spike-local fixed-capacity queue for timestamped fake device
  register writes. The queue stores eight writes in `std::array`, rejects the
  ninth without mutation, drains in FIFO order across ring wrap, and produces
  the same digest with and without an observation callback. It is feasibility
  code, not the production Core-to-RTL protocol or an RTL FIFO.
- The Pocket adapter allocates a 64-byte-aligned, 16-byte CRAM staging buffer
  through the pinned SDK and issues 32 rotating slot 4 reads with
  `of_file_read_async`. Each accepted trial is timed from immediately before
  the SDK call until its data-slot completion callback is observed. A bounded
  pre-trial retry permits the preceding command's bridge state to become idle
  and is deliberately excluded from the reported duration.
- The staging buffer uses the runtime's direct CRAM path. The reported Target
  value therefore includes syscall/command issue, APF host service, the
  16-byte transfer, completion IRQ, and callback observation. It excludes
  stdio open/seek/close and the SDK's non-CRAM bounce copy. It is not an
  isolated command-status handshake latency or a large-transfer throughput
  result.
- Host tests cover queue capacity, overflow rejection, FIFO wrap, observer-free
  equivalence, timed-read aggregation, content validation, and the existing
  architecture guards. The fixed Docker build produced ELF SHA-256
  `846b0e0b3652adc3dbd115a6cbf7eb72b0d949c1672c5b89232598f171f50a63`
  (loadable sections total 166,651 bytes).
- Two complete fixed-Docker builds produced ZIP SHA-256
  `4a99a5f43f5f156879d033c6bb855f3b21b88da61ec7275fbc34cfc6c0e1d15c`
  (1,144,018 bytes). The APF JSON, boot/reset/heartbeat implementation,
  synthetic asset, video, audio, bitstream, and package paths are unchanged.
- The user exercised `0.3.0-spike` on Pocket and observed `QUEUE: PASS`,
  `INPUT: PASS`, and `OVERALL: PASS`. The logical read row was minimum 1,214
  us, integer average 1,253 us, and maximum 1,300 us. The asynchronous Target
  dataslot-read row was minimum 354 us, integer average 695 us, and maximum
  4,665 us. The most recently reported firmware for this Pocket remains 2.6.
- This closes on-device execution of the bounded software queue and the first
  32-sample Target-read timing observation. The Target minimum is below the
  logical-read range, but its 4,665 us maximum shows a substantial tail event.
  Minimum/average/maximum alone cannot identify its frequency, percentile, or
  cause, so these values are not yet a scheduling bound or throughput claim.
  The `0.2.0-spike` observations above remain historical evidence for that
  exact earlier package.

### Target-read tail distribution package — 2026-08-28

- Package version `0.4.0-spike` preserves the automatic semantic, logical-read,
  bounded-queue, and physical-input gates. It replaces the 32-sample Target
  min/average/max display with four distribution profiles: 256 fixed-offset
  16-byte reads (`F16`), 256 rotating-offset 16-byte reads (`R16`), 64
  fixed-offset 256-byte reads (`F256`), and 64 fixed-offset 4,096-byte reads
  (`F4096`). The rotating offset remains `(iteration * 127) mod valid_range`.
- Each profile validates every returned synthetic byte and reports integer
  average, nearest-rank p50, p90, p95, p99, maximum, and counts at or above
  1,000 us and 2,000 us. The fixed sample arrays are bounded at 256 entries;
  invalid zero, oversized, or out-of-range profiles fail before issuing reads.
  No percentile or latency value is itself used as a pass threshold.
- The measurement boundary remains immediately before an accepted
  `of_file_read_async` call through completion-callback observation. It includes
  syscall/command issue, APF host service, payload transfer, completion IRQ, and
  callback observation, while excluding stdio, bounce copying, and bounded
  inter-command ready retries. Comparing `F16` with `R16` tests offset effects;
  comparing `F16`, `F256`, and `F4096` tests transfer-size effects.
- Host tests cover nearest-rank percentile selection, averages, threshold
  counts, invalid sample counts, content validation, and all prior gates. The
  fixed Docker build produced ELF SHA-256
  `e9a3bd68bb6d03761bdf26b01bc09412304c5fdcb6a20e773318ef857b9a709d`
  (238,116 bytes; loadable sections total 168,267 bytes).
- Two complete fixed-Docker builds produced ZIP SHA-256
  `7ea69a01f9ade454102c5795809419010859b5eef6ef4be02ca1ada702739d1f`
  (1,145,764 bytes). The APF JSON, boot/reset/heartbeat implementation,
  synthetic asset, video, audio, bitstream, and package paths are unchanged.
- The user exercised `0.4.0-spike` on Pocket and observed `INPUT: PASS` and
  `OVERALL: PASS`; those results follow the automatic semantic, read-content,
  and bounded-queue gates. Firmware was not repeated with this run; the most
  recently reported Pocket firmware remains 2.6.
- `F16` reported average/p50/p90 `450/359/365` us and p95/p99/maximum
  `366/3206/6182` us, with 7 of 256 samples at or above both 1 ms and 2 ms.
  `R16` reported `675/362/974` us and `982/3920/10754` us, with 11 of 256
  samples at or above both thresholds.
- `F256` reported average/p50/p90 `638/498/504` us and p95/p99/maximum
  `1083/3447/3447` us, with 4 of 64 samples at or above 1 ms and 3 at or above
  2 ms. `F4096` reported `4728/3943/5846` us and `5915/8901/8901` us, with all
  64 samples at or above both thresholds.
- The similar 16-byte medians establish a roughly 360 us small-read baseline
  for this bounded run. Rotating offsets increase the upper distribution and
  observed maximum, while the 4,096-byte profile shows that payload size is the
  dominant sustained cost. The rare small-read tail is real rather than a
  single unexplained maximum: 7/256 fixed and 11/256 rotating reads exceeded
  2 ms.
- This closes the bounded Target-tail distribution experiment, but does not
  establish a hard worst-case latency or approve synchronous storage access in
  playback timing. A production openfpgaOS design must keep Target reads outside
  the device-scheduling/audio-critical path and use bounded prefetch, cache, or
  double buffering. Substrate selection still requires the resource/license
  gate and a superseding ADR; this result alone does not select openfpgaOS.

### Runtime resource and license gate — 2026-08-28

The resource comparison used the runtime-producing openfpgaCore revision
`618a3eb985759a4154115109c2c8036271252888` named by the pinned SDK manifest,
not the older revision used only to build the probe toolchain image. Its VexiiRiscv
submodule was fixed at `580b76c3868512c8316bb7a3d3add81cad49a0dc` and nested
SpinalHDL at `6f8510cdbb8ad7b8bcc0f6d58395669c4c4d7e2d`. CPU Verilog was
generated in an `amd64` Docker image built from that revision's Vexii Dockerfile
(image ID `sha256:b3abc03a15e60e2b3e80abfdde7476abda1a420518c1e36d0063c634af1053aa`,
Docker Engine 29.7.2). Quartus Prime Lite 25.1std.0 Build 1129 then performed a
full compile for Pocket device `5CEBA4F23C8`.

Three measurements establish the resource boundary:

| Measurement | ALMs | Registers | Block bits | M10K | DSP | PLL | Worst setup | Worst hold | Warnings |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Stock `os25`, seed 40 | 16,007 (87%) | 24,228 | 2,396,121 (76%) | 301 (98%) | 24 (36%) | 2 (50%) | -0.805 ns | 0.045 ns | 969 |
| Stripped `rpcmp`, seed 1 | 12,540 (68%) | 18,845 | 1,213,529 (38%) | 163 (53%) | 12 (18%) | 2 (50%) | -0.785 ns | 0.124 ns | 1,061 |
| Standalone JT51, seed 1 | 1,240 (7%) | 1,034 | 3,019 (<1%) | 9 (3%) | 1 (2%) | 0 | -8.683 ns | 0.101 ns | 22 |

The stock row uses the upstream `os25` feature list and its 32 KiB instruction
cache, 128 KiB data cache, 1 KiB GShare, and 256-entry BTB. Its generated CPU
Verilog is 1,720,844 bytes with SHA-256
`29B628B540953F35574F6A67CF0A622A62EDBCBE615BDCF820EB9CAF1E24BFB8`.
The resulting RBF is 2,146,496 bytes with SHA-256
`502B60DE887CC48600275D3516A8ABF633CD77753BE86A844F8900527A6BEF6F`.
Only seven M10K blocks remain. The separately synthesized JT51 at revision
`985a573dcfc1ff135553a39f7eae21d18ba57cbe` requires nine, so stock `os25`
cannot be the RPCMP base even before integration overhead.

The spike-only `rpcmp` profile enables no optional openfpgaOS feature macros,
leaving the upstream BASE SoC, SDRAM, scanout, and software-audio path. Its
single-issue CPU keeps the same ISA/FPU and predictor choices but reduces the
instruction cache to 16 KiB and data cache to 32 KiB. Its generated CPU Verilog
is 1,720,822 bytes with SHA-256
`266242A20FB65D91B674920DB869201D100FBC31721E8DF9B4DC383D24CC019B`;
the RBF is 1,716,968 bytes with SHA-256
`05D0F642A7611E440B1BB3A45F2A4F28F3E56F60B40EB3A11AE8588D1018E342`.
A conservative arithmetic budget for stripped openfpgaOS plus the standalone
JT51 is 13,780 ALMs (about 75%), 172 M10K (about 56%), and 13 DSP blocks
(about 20%). This passes the provisional capacity gate, but it is not an
integrated fit and does not include the production FIFO/register endpoint.

Both openfpgaOS fits miss the 100 MHz setup requirement and both reports say
the design is not fully constrained. Cache and feature removal therefore solve
the capacity problem, not timing closure. The standalone JT51 test deliberately
constrained all logic to a 100 MHz top-level clock and also missed timing; its
real integration requires a documented master clock, clock-enable behavior,
and valid multicycle constraints before its slack is meaningful. The warning
totals are recorded observations rather than accepted baselines: `os25` was
955/12/0/2 and `rpcmp` 1,045/13/0/3 across synthesis, fitter, assembler, and
timing. No warning class was waived for production.

The reviewed openfpgaCore manifest assigns project-authored work Apache-2.0,
VexiiRiscv/SpinalHDL MIT, APF files to the Analogue Pocket Framework license,
Intel-generated IP to its FPGA IP terms, and the optional Analogizer directory
GPL-2.0-or-later. The APF license text says applicable MIT or GNU GPL terms
prevail where they conflict with the APF agreement, consistent with Analogue's
firmware 1.1-beta-2 clarification. The stripped profile does not define
`INCLUDE_ANALOGIZER`, but a release review must still prove which source and
notices correspond to the conveyed bitstream rather than inferring that from
resource pruning alone.

JT51 is GPL-3.0-or-later. A release containing a JT51-derived bitstream must be
planned as a GPL-covered non-source conveyance with complete corresponding
source and build material available under GPLv3 section 6; this record is an
engineering gate, not legal advice. RPCMP still has no repository-level
`LICENSE`, `NOTICE`, or approved corresponding-source process, so no measured
bitstream is approved for redistribution. The proprietary sample bank remains
excluded. Resource feasibility is now positive only for the stripped profile;
the project-owned minimal-SoC comparison, integrated JT51 timing, application
performance with smaller caches, and the superseding ADR remain open.

### Integrated JT51 queue and 90 MHz timing gate — 2026-08-29

A research-only overlay on the same pinned openfpgaCore, VexiiRiscv, SpinalHDL,
and JT51 revisions integrated the stripped `rpcmp` profile with JT51 and a
bounded register endpoint. It is not production RTL and does not change the M0
hardware contract. The CPU runs from the existing 90 MHz PLL profile. JT51 runs
on the existing constrained 12.288 MHz audio PLL output and receives fractional
clock enables averaging 3.579545 MHz and half that rate. No generated fabric
clock or multicycle exception was added.

The endpoint stores eight absolute 32-bit CPU timestamps plus YM2151 address and
data bytes, preserves FIFO order across pointer wrap, rejects a ninth resident
write, latches overflow until explicit clear, and transfers one stable command
at a time across a toggle handshake. A spike-only MMIO window at
`0x40000184-0x40000194` stages timestamps and commands and reports queue state,
the CPU cycle counter, and sample diagnostics. JT51 audio is deliberately not
connected to the Pocket output: native-rate samples cross back only as a count
and rolling hash so synthesis cannot discard the sound path. A 48 kHz output
adapter/resampler remains a separate gate.

Questa Altera Starter FPGA Edition 2025.2 compiled the endpoint testbench with
zero errors and warnings. Its self-check completed with `PASS commands=16
writes=32 samples=5 hash=8f5d7bfe`, covering future-timestamp hold, eight-entry
full state, ninth-write rejection, sticky overflow clear, two bus writes per
YM2151 command, FIFO order, complete pointer wrap/reuse, and bundled sample CDC.
Quartus Prime Lite 25.1std.0 Build 1129 then completed synthesis, fit, assembly,
and timing for `5CEBA4F23C8` with seed 1:

| Measurement | ALMs | Registers | Block bits | M10K | DSP | PLL | Worst setup | Worst hold | Warnings |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Integrated `rpcmp` + queue + JT51, 90 MHz | 13,774 (75%) | 20,735 | 1,216,297 (39%) | 172 (56%) | 13 (20%) | 2 (50%) | 0.327 ns | 0.110 ns | 1,072 |

Relative to the stripped baseline, integration adds 1,234 ALMs, 1,890
registers, 2,768 block-memory bits, nine M10Ks, and one DSP. The resulting ALM,
M10K, and DSP totals differ from the earlier arithmetic estimate by only six
ALMs, zero M10Ks, and zero DSPs. The RBF is 1,778,024 bytes with SHA-256
`161750411A11CD7C9B77EE745847190A85FCD2E0B212C02555C9F6D53A861C6C`.

All analyzed clock-domain setup and hold totals are non-negative. At the slow
1.1 V 85 C corner, the 90 MHz domain has setup slack 0.327 ns and the overall
hold minimum is 0.296 ns; across reported corners, the overall minimum hold is
0.110 ns. Minimum pulse-width slack is at least 0.555 ns. Timing Analyzer found
zero illegal or unconstrained clocks. It still reports the design as not fully
constrained because six APF-shell input ports and 28 APF-shell output ports lack
I/O delays. The stripped baseline has the exact same 6/32/28/31 input-port,
input-path, output-port, and output-path counts, so the integration introduced
no new unconstrained category or path count. This supports the internal
clock/timing comparison but does not waive the inherited external-I/O gap.

The integrated capacity and internal timing gates are therefore positive for
the stripped 90 MHz openfpgaOS profile. Substrate selection remains open:
representative application performance with the smaller caches, a comparable
project-owned minimal-SoC fit, actual 48 kHz audio adaptation and hardware
playback, external-I/O constraints, GPL corresponding-source process, and a
superseding ADR are still required.

### Reduced-cache runtime workload package — 2026-08-29

The next local-only `0.5.0-spike` package binds the unchanged SDK OS and loader
to the integrated 90 MHz `rpcmp` + queue + JT51 RBF above rather than the stock
`os25` bitstream. The package builder accepts this path only when its native RBF
matches SHA-256
`161750411A11CD7C9B77EE745847190A85FCD2E0B212C02555C9F6D53A861C6C`,
then performs the Pocket bit reversal. The packaged `os25.rbf_r` is 1,778,024
bytes with SHA-256
`A7ED6A86977A9848AACBF72B028B939B7182C970544DF5CB9E4EF2B416D1B13E`.

One application-workload sample advances the real M0 Mock Core through 120
snapshot cadences while hashing the immutable eight-channel snapshots and fake
device events. At every cadence it also pushes and drains eight timestamped
writes through the same spike-local bounded queue, for 960 writes per sample.
The package measures 32 headless samples (`WH`) and 32 samples with a snapshot
observer (`WO`), reporting average, nearest-rank p50/p90/p95/p99, and maximum.
Every sample must complete with identical snapshot, event, and write digests;
the timing values themselves are observations rather than hardcoded pass
thresholds. This intentionally measures the current C++ contract/state/queue
shape, including its allocations, but does not claim to measure the future MDX
parser or a production scheduler.

The pinned RV32 toolchain produced a 241,444-byte ELF with SHA-256
`7018A1311D24888EB85C600FA61CF56F01E1DEDD14D6FABF02CF1AFABF5A25E4`.
The SDK PC backend reproduced the host semantic digests and passed. The final
allowlisted ZIP is 998,041 bytes with SHA-256
`6BFD479FA5375CB01ABCF1F4B52BB0206DB7A7ECE9C8249B5D06D935829CB872`.
No removable SD volume was attached at package time, so Pocket boot, memory
check, workload timings, and physical-input PASS remain unmeasured. The
smaller-cache application-performance gate therefore remains open until those
on-device results are recorded.

### Boot-ROM packaging correction — 2026-08-29

Pocket firmware 2.6 did not reach the openfpgaOS boot screen with the
`0.5.0-spike` package. Inspection of the preserved Quartus report found Critical
Warning 127003 for both `firmware.mif` and `apf/build_id.mif`; the 8,192-word
boot BRAM and APF build-ID RAM had therefore been synthesized with zero initial
contents. Bit reversal was independently reproduced with upstream
`reverse_bits.c`, so RBF conversion was not the failure.

The pinned openfpgaOS firmware sources were rebuilt for `rv32imafc/ilp32f` in
the firmware container. The resulting 65,962-byte `firmware.mif` has SHA-256
`0B33FC685721744F03D886986BFEEC875392FA534EF5A8EF62739CDE1BD46B37`.
A deterministic 1,024-word build-ID MIF was also supplied. A clean Quartus
compile, rather than a post-fit MIF update against the zero-initialized database,
was required to change the RBF.

The repaired JT51-integrated fit completed with zero errors, no MIF-not-found
warning, 13,774 ALMs (75%), and worst setup slack 0.327 ns at 90 MHz. Its native
RBF is 1,770,016 bytes with SHA-256
`F532DFE96F8563A14A0860FCC83A89B67CD03527D82C5A1A71C22A73091CC190`.
The package builder now rejects the earlier zero-boot-ROM RBF by pinning this
replacement checksum. On-device boot and workload results for the replacement
remain required. The resulting local-only `0.5.1-spike` package contains a
1,770,016-byte reversed bitstream with SHA-256
`D75CAE4D8F95908C0891D6E758BD91B95F5DF6FF5D84FD94545AA6D975AE86D4`.
The 1,055,907-byte ZIP has SHA-256
`FA74096C5A5C01B829E0093DC19FE5C3C5A5EF14334C4C6F680DA26A49B6CC69`.

Pocket firmware 2.6 subsequently displayed the bootloader's `Loading...`
message with this corrected package, cleared that message, and remained on a
black screen. In the pinned bootloader this transition occurs only after the
OS dataslot load, image CRC check, and boot-ABI check have succeeded, immediately
before control transfers to `os.bin`. This narrows the failure from APF/core
loading to early OS execution, but does not yet distinguish the reduced-cache
90 MHz substrate from the JT51/MMIO overlay.

The next `0.5.2-spike` diagnostic therefore uses the already compiled
`rpcmp90` isolation fit: the same 16 KiB instruction cache, 32 KiB data cache,
90 MHz clock, boot ROM, and base SoC, but no JT51 or RPCMP MMIO overlay. Its
native RBF is 1,736,972 bytes with SHA-256
`F131A5677389E1557DE863838C1289E20B8456B57DA5A93722D8E69977697D91`.
If this package reaches the workload screen, the integrated overlay is the
remaining changed block; if it fails at the same transition, investigation
must stay in the reduced-cache/90 MHz CPU, SDRAM, or early-OS path.
The reversed RBF has SHA-256
`CC18C0A05622C80930774F8859E1B1ED7FF0133ED25978B82D0AF11CA4EA1E8A`.
The resulting 1,025,226-byte local-only ZIP has SHA-256
`381123F797A07A7BC9327146BBE596C37CD6695D0BD31C9B156840414B82E584`.

Pocket firmware 2.6 produced the same `Loading...` then black-screen result
with this no-JT51/no-MMIO package. The integrated endpoint and sound RTL are
therefore excluded as the cause. Because the bootloader runs from BRAM and
the first OS instructions execute from SDRAM, the remaining leading boundary
is the 90 MHz clock/SDRAM path or the reduced instruction/data-cache netlist.

The `0.5.3-spike` diagnostic keeps the 90 MHz clock and feature-stripped base
SoC but restores the stock os25 32 KiB instruction cache and 128 KiB data
cache. It contains no JT51 or RPCMP MMIO overlay. The standard-cache CPU
netlist is byte-identical to the pinned os25 netlist, with SHA-256
`29B628B540953F35574F6A67CF0A622A62EDBCBE615BDCF820EB9CAF1E24BFB8`.
A clean Quartus Prime Lite 25.1std.0 full compile completed with zero errors,
12,575 ALMs (68%), 18,711 registers, 2,164,953 block-memory bits, 273 M10Ks
(89%), 12 DSP blocks, and two PLLs. All reported setup and hold totals are
non-negative; worst setup slack is 0.597 ns and worst hold slack is 0.095 ns.
The inherited APF external-I/O constraint gap remains. Both `firmware.mif`
and `apf/build_id.mif` were found and used. The 1,983,580-byte native RBF has
SHA-256 `D8FCC136E09D21C62505330570F9D1D16011024799625AABFA94AC20A2D016F5`.
The reversed RBF has SHA-256
`F90FC325518AAD02999157FD39B7E15E46B68240C6841F9F8EE132D94D609027`.
The resulting 1,050,908-byte local-only ZIP has SHA-256
`BB09D649F7E5AEB314C92C6CDF8E4D8075AF081EC02723038EEEA80C6CE9D893`.

Pocket firmware 2.6 produced the same `Loading...` then black-screen result
with this standard-cache 90 MHz package. The reduced cache geometry is
therefore excluded alongside the JT51/MMIO overlay. Relative to the earlier
known-good stock-runtime probe, the leading remaining hardware difference is
the custom CPU/SDRAM clock path; a custom-fit or feature-pruning difference is
also still possible.

The `0.5.4-spike` frequency control keeps the same standard-cache CPU netlist,
feature-stripped base SoC, repaired boot ROM, build ID, OS, and application,
but removes `INCLUDE_CLK90` and returns the CPU/SDRAM domain, clock-frequency
register, SDRAM refresh interval, and UART divisor to the upstream 100 MHz
defaults. A clean Quartus Prime Lite 25.1std.0 full compile completed with zero
errors, 12,591 ALMs (68%), 18,794 registers, 2,164,953 block-memory bits, 273
M10Ks (89%), 12 DSP blocks, and two PLLs. Both MIFs were found and used. Across
all reported corners the worst setup slack is -0.745 ns, worst hold slack is
0.053 ns, and minimum pulse-width slack is 0.500 ns; the inherited external-I/O
constraint gap remains. The setup miss is comparable to the earlier stock
`os25` fit (-0.805 ns), which passed the prior hardware probe, but it makes a
black-screen result non-conclusive: successful boot identifies the 90 MHz path,
whereas failure still permits either a custom-fit/feature difference or the
100 MHz setup violation. The 1,975,140-byte native RBF has SHA-256
`E5728C288C5B013E4C0467F7D6A16CBF9C0A4A989CEC2DEED443F48D23E6FC8A`.
The reversed RBF has SHA-256
`79CF0A53681D9F6A33447D5B5199FA85ECE85B1DB860B33D981F00779681C9A7`.
The resulting 1,051,447-byte local-only ZIP has SHA-256
`00337451821454B66BC76687964975E50D4233A36C153E010B8113A4F88DDFA9`.

Pocket firmware 2.6 produced the same `Loading...` then black-screen result
with this custom-fit 100 MHz package. This excludes the 90 MHz clock path as
the sole cause, but does not distinguish the custom feature-pruned fit from
its setup violation. The next `0.5.5-spike` control therefore returns only the
bitstream to the SDK-manifest `os25.rbf_r`, which previously completed this
same workload on Pocket. It keeps the current loader, OS, ELF, synthetic data,
APF definitions, and SD paths. A successful control run will locate the
regression in the custom FPGA builds; a failed control run will instead show
that the currently packaged software or installation state has changed since
the earlier known-good run. The manifest-verified stock `os25.rbf_r` is
2,135,596 bytes with SHA-256
`584885133D6CF45BA35480CE8E8766150F67CA47F9019C6F771B12E89CCA12C1`.
The resulting 1,147,833-byte local-only ZIP has SHA-256
`98DB7FE24E36139E24EB39FE9E15E1D2AA61B2E77C0111AF411797D728334060`.

Pocket firmware 2.6 reached the workload screen with this stock-runtime
control. This proves that the current SD layout, APF loading, loader, OS,
application ELF, and synthetic data are operational. The black-screen
regression is therefore inside the locally generated FPGA bitstreams shared
by `0.5.1-spike` through `0.5.4-spike`, not the packaged software.

The next `0.5.6-spike` control uses the locally fitted stock `os25` feature
profile and its standard-cache CPU netlist. The fit predates the RPCMP JT51
RTL changes and contains the upstream os25 macros at the default 100 MHz. Its
original map reported missing `firmware.mif` and `build_id.mif`; both were
subsequently processed by `quartus_cdb --update_mif` with zero errors and zero
warnings, and `quartus_asm` regenerated the programming files with zero errors
and zero warnings. The fit uses 16,007 ALMs, 2,396,121 block-memory bits, 301
M10Ks, and 24 DSP blocks. Across all reported corners its worst setup slack is
-0.805 ns and worst hold slack is 0.045 ns. The 2,146,496-byte native RBF has
SHA-256 `502B60DE887CC48600275D3516A8ABF633CD77753BE86A844F8900527A6BEF6F`.
The reversed RBF has SHA-256
`9369E6BC993834291D7B85384E004D1972A4F9169DF9A7F2F0322BA5F88D6661`.
The resulting 1,094,466-byte local-only ZIP has SHA-256
`FDE0684BA7939E485020DF3972D08C1C8E1B62D11C1DBC5F81F2FAF45B106A10`.
If this control boots, feature pruning is the remaining common cause of the
custom-fit failures; if it blacks out, the local fit/tool/timing path remains
the fault boundary.

Pocket firmware 2.6 produced a black screen without displaying `Loading...`
with this locally rebuilt stock control. Unlike the timing-clean custom fits,
this RBF therefore did not establish successful APF bootloader execution. Its
timing violation and near-full device utilization make it unsuitable as the
next software-side diagnostic, and this result does not overturn the known-good
manifest-runtime control.

The `0.5.7-spike` diagnostic instead reuses the `0.5.3-spike` 90 MHz,
stock-cache, no-JT51 native RBF. That fit has 0.597 ns worst setup slack and
previously displayed `Loading...` before blacking out. Its SDK-manifest OS is
replaced by a checksum-pinned diagnostic build from runtime revision
`618a3eb985759a4154115109c2c8036271252888`. At the first instruction of
`os_main`, before normal OS initialization, this build writes palette index 15
to a 320 by 16 pixel band at terminal framebuffer address `0x50300000`, issues
a memory fence, and halts. The source delta is preserved as
`spikes/pocket/openfpgaos/early-os-marker.patch` and is enabled with
`EXTRA_CFLAGS=-DRPCMP_EARLY_MARKER=1`. The generated 111,732-byte `os.bin` has SHA-256
`560FFE0ED43E85FD0FF2CF0C40EB73BE731A0639C0AE97181250940F8FD131C2`;
disassembly confirms the framebuffer stores and terminal loop at OS entry.
The resulting 1,037,872-byte local-only ZIP has SHA-256
`8C0162C5CC6F012518F12673C742DF04F3EE8187124ACF219EF4C03CA7604308`.

A white band after `Loading...` proves the bootloader transferred control to
the OS and that initial SDRAM instruction fetch and framebuffer stores work;
the failure is then later in normal OS initialization. `Loading...` followed
by black with no white band places the boundary at OS image transfer,
verification, jump, or the first SDRAM instructions. Failure to display even
`Loading...` is an unexpected regression in the already exercised RBF/APF
path and should trigger an installation/hash check before further diagnosis.

Pocket firmware 2.6 displayed `Loading...` and then the expected white band
with `0.5.7-spike`. This proves that the bootloader transferred and verified
the OS image, jumped to `os_main`, fetched its initial instructions from
SDRAM, and completed uncached terminal-framebuffer stores. The earlier black
screen is therefore inside normal OS initialization rather than the APF load,
OS-image transfer, or initial execution boundary.

The `0.5.8-spike` diagnostic advances the same marker to immediately after
`of_irq_init()`, `os_textguard_baseline()`, and `of_init_early()`. The last call
initializes the advertised CPU clock, cache, timer, video, and terminal. The
stage extension is preserved in
`spikes/pocket/openfpgaos/early-init-marker.patch`, applied after the entry
marker patch, and built with `EXTRA_CFLAGS=-DRPCMP_EARLY_MARKER=2`.
Disassembly confirms those calls precede
the framebuffer stores and halt loop. The resulting 112,164-byte `os.bin` has
SHA-256
`8F286D3F3AEC754277420333F6583BF80FA6DE008EE5AD90B6605F8E6ACCAB2B`.
The 1,038,279-byte local-only ZIP has SHA-256
`B821CE7D7FA385AD1AE8718A5D558F2840E909464254DD4D216D3D35CE7A21AD`.
A white band narrows the failure to contract checking, the boot memory test,
or later initialization. No white band narrows it to IRQ reset, textguard
baselining, or early HAL initialization, which must then be split further.

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
- [Runtime-producing core APF license text](https://github.com/openfpgaOS/openfpgaCore/blob/618a3eb985759a4154115109c2c8036271252888/LICENSES/LicenseRef-Analogue-Pocket-Framework.txt)
- [Analogue firmware 1.1-beta-2 license-compatibility note](https://www.analogue.co/support/pocket/firmware/1.1-beta-2)
- [GNU GPLv3 section 6](https://www.gnu.org/licenses/gpl-3.0.html#section6)
- [GNU GPL FAQ on corresponding source for binaries](https://www.gnu.org/licenses/gpl-faq.html#DistributeExtendedBinary)
- [Pinned JT51 source](https://github.com/jotego/jt51/tree/985a573dcfc1ff135553a39f7eae21d18ba57cbe)
- [Diablo C++/musl link recipe](https://github.com/openfpgaOS/Diablo/blob/c4f1d24ad9db011dcfd5f3b1d90423ceb28cfd17/src/diablo/Makefile)
- [ModPlayer openfpgaOS application](https://github.com/RndMnkIII/ModPlayer_openfpgaos/tree/544e7fb569eeffc38525ed409464f59bcd932108)
- [HarpMudd architecture notes](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/docs/HOW_IT_WORKS.md)
- [HarpMudd feasibility measurements](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/STAGE0_RESULTS.md)
- [HarpMudd APF Target-command crossing](https://github.com/harpmudd/HarpMudd.mp3player/blob/011077b6a1bb210ea14ea81ff7bca7974e200f79/src/fpga/core/tgt_cmd.v)
