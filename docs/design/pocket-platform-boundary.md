# Pocket Platform Boundary

## 1. Status and purpose

This document records the APF constraints that RPCMP can design against before selecting its Pocket execution substrate. It is based on the initialized `analogue-openfpga-skill` documentation snapshot dated 2026-08-22 and the official openFPGA core template.

It does not select a soft CPU, firmware runtime, third-party OS, Quartus version, or hardware protocol owned by RPCMP. Those choices remain gated by ADR-0004.

## 2. Proven APF boundary

```text
Analogue OS / APF
  |-- Host/Target command BRAM and data-slot services
  |-- PAD reports and heartbeat supervision
  |-- video scaler/display modes
  `-- 48 kHz stereo audio consumer
             |
          BRIDGE/PAD/VIDEO/AUDIO
             |
RPCMP Pocket core
  |-- mandatory APF boot/reset/heartbeat adapter
  |-- unresolved Core/UI execution substrate
  |-- RPCMP device scheduler and bounded hardware queue
  |-- sound-device RTL and 48 kHz output adapter
  `-- RPCMP-generated video when a rich player UI is present
```

Known constraints:

| Surface | Constraint relevant to RPCMP |
|---|---|
| BRIDGE | 32-bit memory-mapped interface; `0xF8xxxxxx` is reserved for APF and cannot be assigned to RPCMP registers. |
| Host/Target commands | Core must implement the baseline status, reset, data-transfer completion, RTC, and ready-to-run boot sequence. |
| Heartbeat | Must run continuously after bitstream start; loss causes Pocket to terminate and reload the core. It cannot depend on UI rendering or music sequencing. |
| Data slots | At most 32 slots. `deferload` exposes an asset's size while allowing bounded Target-command reads instead of loading the entire file at boot. |
| PAD | Up to four report sources. The controller type nibble must be checked before interpreting button, analog, keyboard, or mouse fields. |
| VIDEO | The core generates RGB video and timing. APF scales it; UI rendering must remain outside audio/sequencing authority. |
| AUDIO | Signed 16-bit stereo I2S-compatible output at exactly 48 kHz with 12.288 MHz MCLK. Internal device rates must convert explicitly to this boundary. |
| Interact UI | Intended for bounded Core Settings controls, normally up to 16 entries. It is not assumed to be RPCMP's track browser or visualization surface. |

The last Interact conclusion is an architectural inference from the documented control types and limits. The Pocket spike must confirm the intended UI path on current firmware.

## 3. RPCMP ownership mapping

The platform adapter may translate APF mechanisms, but it must preserve the repository invariants:

- PAD reports become UI-local actions or `PlayerCommand` values. Physical button names do not cross into runtime contracts.
- Data-slot offsets remain inside the Pocket storage adapter. Library and playback consumers receive `TrackId`, `BlobId`, and bounded logical byte views.
- APF reset stops Core scheduling and resets device queues. Reset/heartbeat handling must remain operational even when Core or UI is stalled.
- The Pocket VIDEO path consumes immutable snapshots. A late frame may be dropped; it cannot delay clock advancement or hardware writes.
- The 48 kHz AUDIO adapter consumes a bounded FIFO or sample stream with explicit underflow/overflow behavior. It never asks the renderer for timing.
- RPCMP-specific BRIDGE addresses, if any, are allocated outside `0xF8xxxxxx` and documented with endianness, reset values, access widths, and ownership.

## 4. Library loading hypothesis

For a potentially large `.rpcmlib`, the leading candidate is one required, read-only, `deferload` data slot. The Pocket storage adapter would use Target data-slot reads and expose only validated logical records upward.

This is not yet a contract. M1 must measure request overhead and throughput, validate 48-bit file offsets and bounded chunk sizes, and decide whether indices or hot metadata need a boot-time cache. A full-file automatic load must not be assumed to fit memory.

## 5. Boot and shutdown state mapping

The Pocket adapter must be designed as a separate state machine from `TransportState`:

1. bitstream start: initialize safe outputs, clocks, command channels, and heartbeat; report APF `booting`;
2. setup: accept variant/instance writes and data-slot metadata; Core remains stopped;
3. data complete/RTC: validate required slot availability, then issue Target `Ready to run` and report `idle`;
4. reset exit: permit Core execution and report `running`;
5. reset enter/shutdown: stop scheduling, reset device state and queues, finish required nonvolatile operations, and return hardware outputs to safe state.

APF boot status is platform lifecycle state and must not be encoded as a new `TransportState`. A failure that affects playback is translated to the existing typed `PlayerError` boundary.

## 6. Required Pocket spike evidence

Before implementing a production Pocket adapter, capture:

- official template revision, APF framework/firmware requirement, Quartus edition/version, and exact top-level HDL;
- successful baseline Host/Target status, reset, `Ready to run`, and continuous heartbeat behavior;
- one read-only `deferload` synthetic asset read at beginning, middle, and end with checked lengths and offsets;
- measured BRIDGE/data-slot latency and throughput sufficient to choose cache/chunk bounds;
- PAD type discrimination and mapping through an adapter into one generic command;
- stable VIDEO output while Core snapshot reads are deliberately skipped;
- exact 48 kHz AUDIO output with defined reset, silence, underflow, and overflow behavior;
- one bounded fake-device event crossing the proposed Core-to-RTL boundary without using the UI clock;
- valid APF JSON roots and `magic: "APF_VER_1"`, matching SD paths, reversed-bit `.rbf_r`, and release package layout;
- third-party license and resource reports for any proposed CPU, runtime, memory controller, or framework.

Until this evidence exists, host M0 tests may prove RPCMP contract semantics but not Pocket deployability.

### Evidence captured so far

ADR-0004 records a successful zero-error compile of official template v1.3.0 commit `da3a021b1eaf742604d86d8dc9b33a6666263e6a` with Quartus Prime Lite 25.1 for `5CEBA4F23C8`, including generated SOF/RBF hashes and resource/timing reports. This satisfies only the tool availability and baseline template-compile portion of the first evidence item.

The baseline template is not fully timing constrained and produced 192 warnings. JTAG/Pocket execution, APF boot/heartbeat behavior, data-slot access, RPCMP video/audio, the fake-device crossing, `.rbf_r` packaging, proposed-runtime resources/licenses, and all remaining evidence items are still open.

## 7. Sources

- Analogue Developer Docs: <https://www.analogue.co/developer/docs/>
- Official core template: <https://github.com/open-fpga/core-template>
- Local skill references: `.codex/skills/analogue-openfpga/references/runtime-hardware.md`, `json-definitions.md`, and `workflows.md`
