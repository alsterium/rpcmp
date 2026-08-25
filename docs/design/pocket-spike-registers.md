# Pocket Register-Spike Contract

## 1. Purpose and status

This M0 contract defines the smallest project-owned RTL experiment required by ADR-0004. It is a simulation boundary for proving that a 32-bit host-facing register interface can advance an injected counter, accept one command, publish a snapshot-equivalent record, and emit a fake device event without any UI or rendering clock.

It is not the production Core-to-RTL protocol, an APF boot implementation, or evidence of execution on Pocket. A later target-spike step must connect this block through the official `core_top` boundary and validate the real Host/Target command and heartbeat behavior.

## 2. Bus and ownership

- The interface uses one 32-bit address, read-data, and write-data word per access.
- All registers are naturally aligned 32-bit words. Unmapped or misaligned reads return zero. Writes in the diagnostic observation window may update only the write-observation registers even when they do not affect command, snapshot, counter, or event state.
- Register values are unsigned little-endian words at the future byte-addressed adapter boundary. This M0 module has no byte enables, so partial writes are unsupported.
- RPCMP owns `0x1000_0000` through `0x1000_003C` for this experiment. It also recognizes `0x00F0_0010` and `0x00F0_0018` as diagnostic Interact-action aliases. These addresses are outside the APF-reserved `0xF8xxxxxx` region.
- `reset_n` is asynchronous and active low. Reset clears all mutable register state.

## 3. Register map

| Address | Name | Access | Reset | Meaning |
|---|---|---:|---:|---|
| `0x1000_0000` | `STATUS` | R | bit 0 set | bit 0 ready, bit 1 local liveness, bit 2 one-cycle device-event valid |
| `0x1000_0004` | `COMMAND_ID` | R/W | 0 | staged monotonically increasing command ID |
| `0x1000_0008` | `COMMAND` | W | — | opcode `1` advances the injected counter; all other opcodes are ignored |
| `0x1000_000C` | `SNAPSHOT_SEQUENCE` | R | 0 | increments once per accepted command |
| `0x1000_0010` | `COUNTER_LO` | R | 0 | low word of the injected 64-bit tick counter |
| `0x1000_0014` | `COUNTER_HI` | R | 0 | high word of the injected 64-bit tick counter |
| `0x1000_0018` | `LAST_COMMAND_ID` | R | 0 | most recently accepted command ID |
| `0x1000_001C` | `EVENT_SEQUENCE` | R | 0 | increments once per emitted fake device event |
| `0x1000_0020` | `EVENT_VALUE_LO` | R | 0 | low word of the last emitted counter value |
| `0x1000_0024` | `EVENT_VALUE_HI` | R | 0 | high word of the last emitted counter value |
| `0x1000_0028` | `RUN_ID_1` | W | — | opcode `1` atomically stages command ID 1 and advances if it is newer |
| `0x1000_002C` | `RUN_ID_2` | W | — | opcode `1` atomically stages command ID 2 and advances if it is newer |
| `0x1000_0030` | `BUILD_SIGNATURE` | R | `0x4D303033` | ASCII-like signature `M003`, proving that the m0.3 bitstream is loaded |
| `0x1000_0034` | `WRITE_SEQUENCE` | R | 0 | increments on each observed write to this spike or either diagnostic action alias |
| `0x1000_0038` | `LAST_WRITE_ADDR` | R | 0 | address of the last observed spike/action write |
| `0x1000_003C` | `LAST_WRITE_DATA` | R | 0 | raw data of the last observed spike/action write |
| `0x00F0_0010` | `INTERACT_RUN_ID_1` | W | — | any write atomically stages command ID 1 and advances if it is newer |
| `0x00F0_0018` | `INTERACT_RUN_ID_2` | W | — | any write atomically stages command ID 2 and advances if it is newer |

The first accepted command ID is 1. An advance command is accepted only when its staged ID is greater than `LAST_COMMAND_ID`. Duplicate and stale IDs do not mutate snapshot, counter, or event state. An unsupported opcode is ignored and does not consume its staged ID. ID wraparound is deliberately outside this bounded M0 experiment.

Each accepted advance adds 1,000 ticks, updates the snapshot and event records atomically on the rising clock edge, and asserts `device_event_valid` for one clock. `device_event_value` exposes the same 64-bit value retained by the event registers.

`RUN_ID_1` and `RUN_ID_2` are retained M0 compatibility conveniences. Each converts one opcode-`1` write into the same state transition as a staged ID followed by `COMMAND=1`.

The m0.3 Interact menu instead uses `INTERACT_RUN_ID_1` and `INTERACT_RUN_ID_2`. Their addresses and action values mirror Analogue's official `core-example-interact` `Reset Square` (`0x00F0_0010`, value 64) and `Increment Frame` (`0x00F0_0018`, value 0) entries. Like the official RTL, RPCMP treats the address write strobe as the action and does not compare its data. This removes the undocumented m0.2 assumption that Analogue OS must present action data as raw word value `1` at project-specific action addresses.

The build and write-observation registers are diagnostic only. They distinguish a stale bitstream (`BUILD_SIGNATURE` mismatch), a UI action that emitted no BRIDGE write (`WRITE_SEQUENCE` unchanged), and a write whose address/data differed from the JSON contract. They are not a proposed production telemetry interface.

Because every M0 fake event corresponds exactly to one accepted snapshot advance, `EVENT_SEQUENCE` aliases `SNAPSHOT_SEQUENCE` and the event value aliases the injected counter. The logical records remain distinct at the register boundary without duplicating 96 flip-flops. A future event queue must introduce independent storage only when its ordering and backpressure contract is approved.

## 4. Timing boundary

The local liveness bit is driven solely by the RTL clock and continues to toggle when no command is received. It is a simulation observation point, not the mandatory APF heartbeat. The real heartbeat remains owned by the Pocket platform shell and must run independently of this block, Core sequencing, and UI rendering.

This block has no UI, video, audio, storage, format, or physical-control dependency. Its self-checking test uses clock edges rather than wall-clock delays to determine behavior.

## 5. Acceptance and remaining work

Run the simulation with:

```powershell
pwsh -File tools/rtl-verify.ps1
```

The test must prove reset values, the build signature, write observations, command-independent liveness, staged and one-write Interact advances, action-data independence on the official aliases, atomic snapshot/event publication, duplicate rejection, unsupported-opcode rejection, and a later accepted advance.

On 2026-08-25, Questa Altera Starter 2025.2 completed the m0.3 self-checking test at 352 ns with the `pocket_spike_tb: PASS` marker and zero errors or warnings. This is behavioral evidence for the standalone project-owned register boundary, including both official-pattern Interact aliases and retained compatibility aliases. It is not evidence that Analogue OS can access the generated integration on Pocket.

Still required before ADR-0004 can be superseded:

- behavioral or physical validation of the integrated APF-to-register path;
- complete timing constraints and CDC/reset review beyond the successful Quartus integration build;
- real APF boot, reset, Host/Target commands, and heartbeat on Pocket;
- a bounded asynchronous queue or handshake if Core and device logic use different clocks;
- measured register/data-slot latency and the remaining evidence listed in `pocket-platform-boundary.md`.
