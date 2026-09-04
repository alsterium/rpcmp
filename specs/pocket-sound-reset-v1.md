# Pocket Sound Reset Control v1

This integration-only control supplies the bounded common-reset operation that
Core-to-RTL Device Queue v1 cannot express through its flag-only `CLEAR`
register. It resets the queue, an in-flight CDC transfer, JT51, and the Pocket
AUDIO adapter together without resetting the CPU that issues the request.

The CPU-local, little-endian block begins at `0x4000_0240`, outside APF's
reserved `0xF8xx_xxxx` region. It is a separate block: no Queue v1 offset or
reserved-bit behavior changes.

| Offset | Name | Access | Meaning |
|---:|---|---|---|
| `0x00` | ID | R | `0x52534331` (`RSC1`) |
| `0x04` | CAPABILITY | R | version 1 in bits 31:16; minimum audio reset cycles (256) in bits 15:0 |
| `0x08` | STATUS | R | completed generation in bits 31:16; invalid sticky in bit 1; busy in bit 0 |
| `0x0C` | COMMAND | W | bit 0 requests reset; bit 1 clears invalid |

A reset request while busy is an idempotent no-op; it never shortens the active
reset. Unknown offsets, misalignment, reads of undefined offsets, and reserved
`COMMAND` bits set `invalid`. Reset requests and invalid-clear may be combined.

## Clock and failure semantics

The controller asserts reset in the CPU domain before accepting more queue
work. A toggle request crosses to the 12.288 MHz audio domain through two
synchronizer stages. The audio-domain reset remains asserted for at least 256
audio clocks, then deasserts on an audio-clock edge. Its acknowledgement crosses
back through two synchronizer stages; only then does CPU reset deassert on a
CPU-clock edge and `busy` clear. Each completed request increments generation.

Software must verify both control and Queue v1 IDs/capabilities, request a reset,
wait for `busy=0` with a bounded timeout, require generation to advance, and
then establish a new Queue `NOW` epoch. Timeout, `invalid`, Queue overflow,
Queue invalid, or Pocket AUDIO diagnostics is a device fault: software stops
submitting work and requests another bounded reset. If that reset also fails,
the application remains silent and reports the fault outside the audio path.

The two locally synchronized reset outputs represent one logical common reset.
Platform integration connects them to the corresponding CPU and audio portions
of the queue, JT51 adapter, and audio adapter. No pre-reset operation or sample
may reappear after both outputs deassert.
