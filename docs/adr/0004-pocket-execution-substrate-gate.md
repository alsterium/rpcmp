# ADR-0004: Gate the Pocket execution substrate decision

- Status: accepted
- Date: 2026-08-25
- Deciders: RPCMP maintainers

## Context

RPCMP requires library access, sequencing, a replaceable UI, and FPGA sound hardware on Analogue Pocket. The official openFPGA template establishes an FPGA bitstream/APF JSON integration surface. It does not, by itself, establish where RPCMP's general-purpose C++ runtime executes.

Possible architectures include a project-owned soft CPU and firmware runtime, a reviewed third-party FPGA OS/SDK, a hardware state machine with a much smaller software surface, or another documented bridge. These choices materially affect FPGA resources, memory, storage access, UI rendering, audio timing, licenses, and the host/target code-sharing claim.

The initialized `analogue-openfpga-skill` snapshot confirms the APF hardware boundary: a 32-bit BRIDGE bus with `0xF8xxxxxx` reserved for framework traffic, mandatory baseline Host/Target boot commands and heartbeat, up to 32 data slots, core-generated VIDEO, and signed 16-bit stereo AUDIO at exactly 48 kHz. It still does not provide a general-purpose runtime for RPCMP application code.

Quartus Prime Lite and Questa Starter are now available locally. The toolchain smoke test below proves that the unmodified official template can target Pocket's FPGA with Quartus 25.1, but it does not select the execution substrate or prove RPCMP logic on hardware.

## Decision

M0 may implement the host architectural proof described by ADR-0001, but it must label Pocket deployment as unverified. No production Pocket adapter, third-party OS, soft CPU, or cross-toolchain dependency is added until a focused target spike records:

1. the official APF/openFPGA interface and supported Quartus/tool versions;
2. the execution location of Core, UI, filesystem/container reader, and device scheduler;
3. available FPGA resources and memory after the minimum platform shell;
4. storage/data-slot capabilities and maximum practical `.rpcmlib` access pattern;
5. the Core-to-RTL FIFO/register protocol, clocks, backpressure, reset, and overflow behavior;
6. input and video/framebuffer capabilities needed by the replaceable UI;
7. exact third-party source revisions, licenses, notices, modifications, and distribution implications;
8. a minimal target experiment that advances an injected counter, accepts one command, publishes one snapshot-equivalent record, and emits one fake device event.

The spike must also satisfy the concrete APF boundary checklist in `docs/design/pocket-platform-boundary.md` and produce a superseding accepted ADR. Until then, `core/platform/pocket` is a compile-free placeholder and Pocket checks are listed as unavailable, not green.

## Toolchain smoke evidence

On 2026-08-25, the official `open-fpga/core-template` v1.3.0 at commit `da3a021b1eaf742604d86d8dc9b33a6666263e6a` was cloned into ignored build output and compiled without source edits using:

```text
C:\altera_lite\25.1std\quartus\bin64\quartus_sh.exe --flow compile ap_core
```

The source project records Quartus Lite 18.1.1. Quartus Prime Lite 25.1std.0 Build 1129 generated a compatibility defaults file, updated only the temporary clone's `LAST_QUARTUS_VERSION`, selected Cyclone V device `5CEBA4F23C8`, and completed analysis, synthesis, fitting, assembly, and timing analysis with zero errors.

The baseline fit used 413 of 18,480 ALMs (2%), 726 registers, 8,192 block-memory bits in two RAM blocks, one of four PLLs, and all 224 pins. Generated artifacts were:

- `ap_core.sof`: 2,441,493 bytes, SHA-256 `B1406802EA0864186347957FA3F04E00D49E0F44D068E0FEBFEAD4AA8FB27B1D`;
- `ap_core.rbf`: 788,264 bytes, SHA-256 `60E4DC8986EF636CDC14F1104DC9D6677F988E8F621F4FFC9CD245C0ACC95FC7`.

All reported setup, hold, and minimum-pulse-width slacks were positive. The worst reported setup slack was 5.822 ns, hold slack was 0.154 ns, and minimum-pulse-width slack was 0.830 ns across the analyzed corners. These figures apply only to the template and are not an RPCMP timing budget.

The compile produced 192 warnings, including the template's unused/stuck signals, PLL reset warning, unmatched generated-clock groups, and unconstrained audio divider clocks. Timing Analyzer explicitly reported that the design was not fully constrained. RPCMP must establish its own warning baseline and complete clock/reset/CDC constraints; a zero-error template build is not sufficient for RTL acceptance.

The template declares `APF_VER_1`, framework requirement 1.1, top-level `apf_top`, and user module `core_top`. A subsequent clean integration overlay connected the project-owned register spike to `clk_74a`, APF `reset_n`, and BRIDGE address range `0x10xxxxxx`; the spike also observes the two exact action addresses used by the official Interact example. Quartus evidence for the current `0.0.0-m0.4` integrated flow is recorded in `docs/design/pocket-template-integration.md`.

Questa Altera Starter FPGA Edition 2025.2 completed the expanded project-owned self-checking test at 394 ns with zero errors and warnings. This proves the standalone register-spike semantics, including APF-style pre-strobe read data, staged commands, official-pattern one-write Interact aliases, write diagnostics, and fake-device events, but does not simulate the complete official APF shell or Analogue OS BRIDGE controller.

The integrated RBF was converted to equal-length `.rbf_r` output and packaged with project-owned `APF_VER_1` definitions. Automated checks passed for JSON roots and bounds, Interact/register mapping, byte-reversal round trip, exact SD tree, and ZIP base folders. On 2026-08-25, package version `0.0.0-m0` was installed and executed on Pocket: the Interact action advanced the visible value to 1000, relaunching the core restored the first-command behavior, and repeating the action with the same command ID caused no change. This is partial hardware evidence for the integrated BRIDGE path and duplicate rejection. Version `0.0.0-m0.1` was rejected because most labels remained truncated and its separate ID/advance actions produced no apparent readout changes. Version `0.0.0-m0.2` was also rejected because confirming either one-write Run action left all readouts unchanged. Version m0.3 added official-pattern actions and diagnostics, but its build signature read zero; m0.3.1 forced a unique bitstream name and produced the same result. Inspection of the official bridge then identified the defect: APF buffers read data before pulsing `bridge_rd`, while RPCMP emitted values only during that later strobe.

On 2026-08-31, the openfpgaOS workload probe established a hardware-stable
normal-OS/RBF pair, including repeated warm starts and a cold power cycle on
Pocket firmware 2.6. Later diagnostics showed that moving subsequent code and
BSS by only `0x40` caused a repeatable blackout even when the added terminal
call was skipped. The content-specific hypotheses were excluded; the remaining
issue is placement sensitivity in this substrate configuration. M0 work is
therefore restricted to the checksum- and size-pinned `--safe-layout` package.
This evidence does not yet approve a general openfpgaOS substrate or arbitrary
firmware growth; such approval requires explaining or eliminating the placement
sensitivity.

The resulting `0.5.38-safe` package (SHA-256
`1F677C0AC2C1C266D55447538DC61DC2D0EE5EB87B01164A2C4C2406B9202FEB`)
was then installed on Pocket firmware 2.6 and reached the workload-probe screen
through normal startup. This confirms the pinned workaround as the continuing
M0 hardware baseline; it does not broaden the decision to unpinned layouts or
supersede this gate.

Version m0.4 continuously decodes the read address and exposes signature `M004`. On 2026-08-26, Pocket returned that signature and the expected results for a direct `Run 1` then `Run 2` sequence: exact writes `0x00F00010=0x40` and `0x00F00018=0`, command/snapshot/event sequences 1 then 2, and counter/event values 1,000 then 2,000. This closes the bounded minimum target experiment in decision item 8 at the register-spike level and physically validates its integrated APF read/write path. It does not select the execution substrate or close the broader gate: JTAG enumeration, detailed APF boot/status and extended heartbeat observation, data slots, video/audio, runtime resources, licenses, and production protocol decisions remain open.

A source-level candidate survey is recorded in `docs/design/pocket-execution-candidates.md`, with exact compatibility gates in `docs/design/pocket-openfpgaos-spike.md`. It identifies openfpgaOS as the leading subject for the next bounded spike because it exposes a RISC-V app runtime and APF-backed platform services. No dependency or substrate is selected: the default SDK C++ profile lacks the standard library required by RPCMP's current types, stock packaging automatically includes prohibited proprietary sample data, upstream-reported Pocket utilization is already 87–89%, the RPCMP/JT51 fit is unmeasured, and the Windows build path and mixed-license distribution posture remain unresolved. JTFRAME is retained as an HDL/build-organization reference rather than a runtime candidate because its Pocket target is not public.

Subsequent review of ModPlayer_openfpgaos and HarpMudd.mp3player added working-player evidence without changing this decision. ModPlayer corroborates that openfpgaOS can host an application-shaped C music player, but it does not prove RPCMP's C++ contracts or architectural boundaries. HarpMudd demonstrates an independent VexRiscv/firmware/APF path and useful Target-command, CDC, audio-queue, and rendering-isolation patterns, but its integrated firmware is not snapshot/command separated and its completed player is block-RAM constrained. The next spike therefore uses one host-first RPCMP probe to compare openfpgaOS with a stripped project-owned minimal SoC before selecting either substrate.

On 2026-08-28, a full Quartus 25.1 fit of the SDK-manifest runtime revision established that stock openfpgaOS `os25` uses 16,007 ALMs and 301/308 M10K blocks and misses 100 MHz setup by 0.805 ns. A separately synthesized pinned JT51 uses 1,240 ALMs, nine M10K blocks, and one DSP, so it cannot fit in the seven remaining stock M10Ks. A spike-only openfpgaOS profile with no optional feature macros and 16 KiB/32 KiB instruction/data caches fit at 12,540 ALMs and 163 M10Ks. The conservative stripped-plus-JT51 arithmetic budget is about 75% ALM and 56% M10K, so capacity is feasible only after stripping; timing closure, integrated fit, smaller-cache application performance, redistribution policy, and the project-owned minimal-SoC comparison remain open. Detailed revisions, hashes, warnings, timing caveats, and license findings are recorded in `docs/design/pocket-openfpgaos-spike.md`. This evidence narrows the gate but does not supersede this ADR or select a substrate.

On 2026-08-29, a research-only integrated fit added an eight-entry timestamped
register queue and the pinned JT51 to that stripped profile. Running the CPU at
90 MHz and JT51 from the constrained 12.288 MHz audio clock with fractional
enables produced 13,774 ALMs, 172/308 M10Ks, 13/66 DSPs, setup slack +0.327 ns,
and hold slack at least +0.110 ns across reported corners. A self-checking RTL
test passed full/reject/overflow-clear, due-time, ordering, wrap/reuse, YM2151
address/data, and sample-diagnostic CDC cases. Thus integrated capacity and
internal clock timing are no longer open for this bounded profile. The APF
shell retains the same unconstrained external-I/O counts as the stripped
baseline, and the spike does not implement 48 kHz audio output or exercise
hardware playback. Smaller-cache application performance, external-I/O
constraints, redistribution policy, the project-owned minimal-SoC comparison,
and a superseding ADR remain open; this result still does not select a
substrate. Full evidence is recorded in
`docs/design/pocket-openfpgaos-spike.md`.

## Alternatives considered

- **Assume C++ runs directly under openFPGA:** unsupported by the official template evidence reviewed.
- **Adopt an available community FPGA OS immediately:** potentially viable, but it is a major dependency requiring license, resource, ABI, and maintenance review.
- **Move all runtime behavior into RTL now:** prematurely couples format/library/UI work to hardware and weakens host testability.
- **Block all M0 work:** unnecessary; the host proof still retires Core/UI and determinism risks if its scope is labeled accurately.

## Consequences

- M0 can progress without embedding an unverified hardware premise.
- The product schedule contains an explicit target-feasibility gate. It should run before M1 choices depend on storage or memory budgets and before YM2151 integration.
- A substantial target mismatch may require reimplementing adapters or runtime internals, but the snapshot/command semantics and deterministic tests remain reusable.

## Evidence reviewed

- Official openFPGA core template: <https://github.com/open-fpga/core-template> (accessed 2026-08-25)
- Official template v1.3.0 commit `da3a021b1eaf742604d86d8dc9b33a6666263e6a` and local Quartus 25.1 compile reports (reviewed 2026-08-25)
- Official openFPGA examples organization: <https://github.com/open-fpga> (accessed 2026-08-25)
- Initialized `analogue-openfpga-skill` snapshot at commit `81da4c1fac028da594cc2a602ce0ef47a45b64e5`
- Candidate survey revisions and primary-source links recorded in `docs/design/pocket-execution-candidates.md` (reviewed 2026-08-26)
- Repository and local tool availability (inspected 2026-08-25)
