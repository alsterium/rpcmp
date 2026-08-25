# Pocket Template Integration Spike

## 1. Scope

This M0 target-spike step connects the project-owned register experiment to the exact official openFPGA template revision already accepted by ADR-0004. It proves tool-level compatibility and obtains resource/timing evidence. It does not claim a production protocol or successful execution on Pocket.

The official template is not copied into this repository. `tools/pocket-spike-build.ps1` exports the pinned revision from a local clone into ignored build output, verifies every overlay marker, copies the project-owned RTL, and compiles the generated project with the pinned Quartus version.

## 2. Pinned inputs

| Input | Required value |
|---|---|
| Official template | `open-fpga/core-template` v1.3.0 commit `da3a021b1eaf742604d86d8dc9b33a6666263e6a` |
| Quartus | Prime Lite `25.1std.0 Build 1129` |
| FPGA | Cyclone V `5CEBA4F23C8` inherited from the official template |
| RPCMP RTL | `core/rtl/pocket/rpcmp_spike_regs.sv` from the current checkout |

The default template location is `out/toolchain-smoke/core-template`. Set `RPCMP_OPENFPGA_TEMPLATE` to use another local clone and `RPCMP_QUARTUS_ROOT` to select another installation directory. Version and revision mismatches fail before generation.

## 3. Generated integration

The generated, uncommitted project makes only three source-level changes:

1. adds `rpcmp_spike_regs.sv` to the Quartus project as SystemVerilog;
2. instantiates it on the template's `clk_74a`, APF-controlled `reset_n`, and 32-bit BRIDGE signals;
3. routes reads in `0x10xxxxxx` to the RPCMP register block while retaining the template Host/Target command handler for reserved `0xF8xxxxxx` reads.

The block observes all writes but mutates only for its exact aligned register addresses. It does not drive VIDEO, AUDIO, PAD, storage, APF command status, or physical pins. The template remains responsible for safe tie-offs, boot/reset commands, video, silence audio, PLLs, and real platform liveness.

## 4. Build command and evidence boundary

```powershell
pwsh -File tools/pocket-spike-build.ps1
```

The script recreates only `out/pocket-spike/core-template`, compiles `ap_core`, verifies SOF/RBF creation, and prints SHA-256 hashes. Generated template source, Quartus databases, reports, and bitstreams remain ignored build output.

The pinned warning baseline is 159 Analysis & Synthesis, 16 Fitter, 2 Assembler, and 15 Timing Analyzer warnings: 192 in total. Any phase error or warning-count drift fails the script so that new warnings require explicit review rather than blending into template noise.

A successful build proves that the module elaborates, synthesizes, fits, and assembles under the pinned shell and device. It does not prove BRIDGE transactions from Analogue OS, behavioral test assertions, JTAG programming, heartbeat survival, timing completeness, bit reversal, APF JSON/package validity, or hardware execution.

## 5. Evidence captured on 2026-08-25

The clean generated integration completed in 6 minutes 8 seconds with zero errors and the accepted 192-warning baseline. Quartus elaborated `rpcmp_spike_regs` below the official `core_top`, then completed synthesis, fitting, assembly, and timing analysis for `5CEBA4F23C8`.

Final fit used 656 of 18,480 ALMs (4%), 975 registers, 8,192 block-memory bits in two RAM blocks, one of four PLLs, and all 224 pins. Compared with the clean template evidence in ADR-0004, the whole-design delta is 243 ALMs and 249 fitted registers; RAM, PLL, DSP, and pin counts are unchanged. Analysis & Synthesis reported 834 registers versus the template's 660, matching the 174 project-owned state bits before fitter retiming and duplication. These deltas are integration evidence, not an isolated production area estimate.

All reported slack values were positive, with worst setup slack 4.429 ns, hold slack 0.167 ns, and minimum-pulse-width slack 0.830 ns across the analyzed corners. The template still reports that setup and hold are not fully constrained, including its known derived audio-clock and unmatched clock-group warnings, so the positive values do not establish timing closure for a production core.

Generated artifacts were:

- `ap_core.sof`: 2,441,493 bytes, SHA-256 `E667786AD501D21D9AD7A2EDA8775DEAB2BBA7F3F4A4C8008ACDBBC14544B2F1`;
- `ap_core.rbf`: 794,760 bytes, SHA-256 `CAEB5E867AD27E416227BB4967EB0EDEED9D0AADA8B942FB416A2490BF527DEB`.

The official build-ID generator embeds compile time, so these hashes identify this evidence run and are not expected to be reproducible across later successful builds.
