# Device Operation API v1

## Purpose and boundary

This contract carries deterministic sound-device work from a format engine or
test-sequence producer to the Core scheduler and then to a device port. It does
not describe a hardware MMIO layout. Platform adapters may translate accepted
operations to a bounded FIFO or simulator, but engines never address that
transport directly.

The v1 operation set is intentionally limited to the YM2151 fixed-sequence
milestone. Additions require defaults or a new contract version; MDX-specific
commands and physical Pocket controls are not valid operations.

## Time domain

An operation stream declares one unsigned 32-bit `tick_rate` in ticks per
second. `tick_rate` must be nonzero. Each operation carries an unsigned 64-bit
absolute `at_tick` in that domain.

Operations must be submitted in nondecreasing `at_tick` order. Operations with
the same timestamp preserve submission order. Integer arithmetic used to
compare, advance, or convert time must reject overflow; floating-point time and
wall-clock sleeps are not scheduling authorities.

The scheduler dispatches an operation when media time is at least `at_tick` and
the device port accepts it. Backpressure retains the operation without changing
its timestamp or relative order. A queue-full submission is explicitly
rejected; it is never treated as success and never overwrites pending work.

## Value model

The portable C++ representation uses fixed-width values equivalent to:

```text
DeviceOpStream {
  version: u16 = 1
  tick_rate: u32
  operations: bounded sequence<DeviceOp>
}

DeviceOp {
  at_tick: u64
  device_id: DeviceId(u16)
  operation: Reset | WriteRegister
}

Reset {}

WriteRegister {
  address: u8
  value: u8
}
```

`DeviceId` uses the public contract type. The receiving device registry must
contain the ID and must identify it as YM2151-compatible before either v1
operation is accepted. No pointer, container offset, platform address, dynamic
type name, or engine-owned object crosses this boundary.

## Validation and results

The complete fixed sequence is validated before scheduling. Validation rejects:

- an unsupported stream version or zero tick rate;
- more operations than the caller's explicit admission limit;
- an unknown or incompatible device ID;
- timestamps that decrease;
- a last timestamp beyond the caller's declared media-duration limit; or
- any time conversion that cannot be represented exactly or by the separately
  specified deterministic rounding rule of its adapter.

Submission and dispatch results are typed and distinguish at least `accepted`,
`queue_full`, `invalid_device`, `invalid_operation`, `time_overflow`, and
`device_fault`. Diagnostic text is optional and is not a stable API value.

## Reset semantics

`Reset` is an ordered device operation, not a scheduler reset. It reaches the
device only after all earlier operations and before all later operations with
the same timestamp.

Resetting the scheduler is a separate control action: it clears every pending
operation, clears scheduler overflow/error state, resets media time to zero,
and resets each attached device through its port. After reset, no operation
from the previous generation may be dispatched.

## Determinism requirements

For the same validated stream, device registry, admission limits, and sequence
of injected media-time advances and port-ready states, the accepted/rejected
results and dispatched operation bytes must be identical. Snapshot publication,
rendering delay, UI absence, host locale, filesystem order, and wall time must
not alter the trace.

The M2 golden fixture will serialize only for comparison, using little-endian
integers and an explicitly versioned fixture header. That test encoding is not
a runtime interchange format or the Core-to-RTL protocol.
