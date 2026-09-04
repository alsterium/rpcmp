# Core-to-RTL Device Queue v1

This contract transports scheduled YM2151 operations over a CPU-local 32-bit
little-endian MMIO block. The default base `0x4000_0200` is not APF BRIDGE and
is outside APF's reserved `0xF8xx_xxxx` region. Relocation may change the base,
but not the offsets or semantics. Version 1 has one eight-entry FIFO and does
not select a CPU, runtime, or YM2151 core.

## Register map

| Offset | Name | Access | Meaning |
|---:|---|---|---|
| `0x00` | ID | R | `0x52514D31` (`RQM1`) |
| `0x04` | CAPABILITY | R | version 1 in bits 31:16; depth 8 in bits 7:0 |
| `0x08` | STATUS | R | count 3:0, full 8, overflow 9, invalid 10, in-flight 11 |
| `0x0C` | CLEAR | W | bit 0 clears overflow; bit 1 clears invalid |
| `0x10/14` | NOW_LO/HI | R | free-running 64-bit CPU tick |
| `0x18/1C` | DUE_LO/HI | R/W | staged absolute due tick |
| `0x20` | PUSH | W | push 31, kind 17:16, address 15:8, value 7:0 |

Kind 0 is reset and requires zero address/value. Kind 1 is register write.
Other kinds, reserved PUSH bits, missing push, reset payload, unknown offsets,
and misalignment set sticky `invalid` without enqueueing. A valid push while
full sets sticky `overflow` and never overwrites data. Software reads NOW high,
low, then high again and retries on rollover. Media-time conversion is the
software adapter's explicit rational conversion responsibility.

## Dispatch, CDC, and reset

FIFO order is preserved. Only a due head dispatches. One stable bundle crosses
from CPU to device clock with a request toggle and remains stable until
`dev_valid && dev_ready`; acknowledgement returns through two synchronizer
stages. Backpressure cannot drop or reorder work. Integration must mark the
toggle synchronizers and constrain/review the bundled-data CDC path.

One logical common active-low reset clears FIFO, staging, time, flags,
request/ack state, and device valid even with queued or in-flight work. The RTL
receives this as `cpu_reset_n` and `dev_reset_n` so platform integration can
synchronize deassertion independently to both clocks. Both must be asserted for
every common reset. No pre-reset operation may reappear.
Version 1 has no device-fault wire; the selected sound adapter must define that
mapping before integration.
