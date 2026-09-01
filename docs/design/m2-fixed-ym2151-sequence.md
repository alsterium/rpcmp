# M2 Fixed YM2151 Sequence

## Purpose and provenance

This is a project-authored hardware-path stimulus, not copied music or a
fidelity reference. It drives YM2151-compatible device 1, channel 0, for 3.5
seconds. The sequence is intentionally small enough to inspect byte for byte.

## Timeline

The stream uses exactly 1,000 integer ticks per second and 17 ordered
operations:

| Tick | Operation |
|---:|---|
| 0 | Reset device 1 |
| 0 | Configure channel 0 stereo output, algorithm, pitch, operator envelope, and silence the unused operators |
| 1,000 | Key on channel 0 operator M1 |
| 2,000 | Change channel 0 key code |
| 3,000 | Key off channel 0 |
| 3,500 | Reset device 1 and end |

The exact addresses and values live in
`core/runtime/src/fixed_ym2151_sequence.cpp`. Their submission order at tick 0
is part of the contract fixture. The sequence does not promise a musical note
name, tuning, loudness, or cross-implementation fidelity; those require the
later device-clock and audio-adapter decisions.

## Golden test encoding

`fixed_ym2151_sequence_tests.cpp` encodes a comparison-only byte stream:

```text
header:
  magic[4] = "RDO1"
  fixture_version u16le = 1
  stream_version u16le
  tick_rate u32le
  operation_count u32le

operation[operation_count]:
  at_tick u64le
  device_id u16le
  kind u8                 # 0 reset, 1 register write
  address u8              # zero for reset
  value u8                # zero for reset
  reserved u8 = 0
```

The 17-operation fixture is exactly 254 bytes. Its complete lowercase hex is a
literal in the test, so a changed timestamp, order, address, value, width, or
endianness fails without relying on a serializer-generated expectation. This
encoding is not a public file format or the future Core-to-RTL protocol.
