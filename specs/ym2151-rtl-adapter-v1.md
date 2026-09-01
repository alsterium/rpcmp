# YM2151 RTL Adapter v1

This adapter binds Core-to-RTL Device Queue v1 to the JT51 revision selected by
ADR-0006, then binds JT51's native stereo sample strobe to Pocket AUDIO Adapter
v1. It owns sound-implementation details; neither the scheduler nor the queue
depends on JT51 signal names or timing.

## Device operation handshake

The adapter accepts one operation when `dev_valid && dev_ready`. Kind 0 starts
a 256-cycle JT51 reset hold and requires zero address/value as already enforced
by the queue. Kind 1 performs an address write (`a0=0`) followed by a data write
(`a0=1`). Each active-low write pulse is established before and held through a
3,579,545 Hz effective clock-enable edge. The adapter waits for JT51 busy to
clear before each half. It does not accept another operation until the reset or
both writes complete. Other kinds are never acknowledged.

The effective JT51 enables are generated in the 12,288,000 Hz AUDIO domain by
an integer accumulator: add 3,579,545 every master-clock cycle, subtract
12,288,000 on carry, and alternate carried enables to produce `cen_p1` at half
rate. No fabric-generated clock or floating-point timing is used.

## Audio and reset

JT51 `left`, `right`, and `sample` connect to Pocket AUDIO Adapter v1 in the
same clock domain. The signed 16-bit samples are sign-extended to its signed
18-bit input. Global reset clears command, enable, sound, rational-conversion,
serial-output, counters, and sticky fault state. A device Reset also resets
JT51 and clears the audio adapter so no pre-reset sample survives.

`clear_audio_flags` clears only the Pocket adapter's underflow, overflow, and
clipped diagnostics. Queue flags retain their own v1 clear semantics.
