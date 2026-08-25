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

The template declares `APF_VER_1`, framework requirement 1.1, top-level `apf_top`, and user module `core_top`. A subsequent clean integration overlay connected the project-owned register spike to `clk_74a`, APF `reset_n`, and BRIDGE address range `0x10xxxxxx`. Quartus completed the current `0.0.0-m0.2` integrated flow with zero errors and the same 192-warning baseline, using 686 ALMs and 1,068 fitted registers; full evidence is recorded in `docs/design/pocket-template-integration.md`.

Questa Altera Starter FPGA Edition 2025.2 completed the expanded project-owned self-checking test at 266 ns with zero errors and warnings. This proves the standalone register-spike semantics, including staged commands, one-write Interact actions, and fake-device events, but does not simulate the official APF shell or Analogue OS BRIDGE controller.

The integrated RBF was converted to equal-length `.rbf_r` output and packaged with project-owned `APF_VER_1` definitions. Automated checks passed for JSON roots and bounds, Interact/register mapping, byte-reversal round trip, exact SD tree, and ZIP base folders. On 2026-08-25, package version `0.0.0-m0` was installed and executed on Pocket: the Interact action advanced the visible value to 1000, relaunching the core restored the first-command behavior, and repeating the action with the same command ID caused no change. This is partial hardware evidence for the integrated BRIDGE path and duplicate rejection. Version `0.0.0-m0.1` was rejected because most labels remained truncated and its separate ID/advance actions produced no apparent readout changes. Version `0.0.0-m0.2` instead uses five-character labels and dedicated one-write Run-ID registers; its revised UI, JTAG enumeration, the command-ID-2 case, complete snapshot/event readouts, and continuous-heartbeat behavior remain unverified.

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
- Repository and local tool availability (inspected 2026-08-25)
