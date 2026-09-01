# Pocket AUDIO Adapter v1

This contract converts one signed stereo device stream into Analogue Pocket's
AUDIO boundary. It is platform RTL: sequencing, UI, rendering, and wall-clock
service are never timing authorities.

## Clocks and rates

The adapter runs entirely on the Pocket AUDIO master clock, exactly
12,288,000 Hz. `audio_mclk` is that clock. One output frame is exactly 256
master-clock cycles, so the frame rate is exactly 48,000 Hz.

The selected M2 source is JT51 revision
`985a573dcfc1ff135553a39f7eae21d18ba57cbe`. Its native sample strobe is one
sample per 64 effective 3,579,545 Hz clock enables. For every asserted source
sample the converter adds `48,000 * 64` to an integer phase accumulator. When
the sum reaches `3,579,545`, it subtracts that threshold and selects the sample;
otherwise it drops it. This deterministic nearest-previous-sample conversion
has no floating-point state and no cumulative rate drift. It is an M2 transport
proof, not the fidelity oracle for later MDX playback.

## Sample and serial format

The input is signed 18-bit two's-complement stereo. Each selected channel is
saturated independently to signed 16-bit: values above 32,767 become 32,767
and values below -32,768 become -32,768. Either saturation sets sticky
`clipped`.

The serial frame has 64 bit slots. `audio_lrck` is low for the first 32 slots
(left) and high for the second 32 (right). Each slot is four master-clock
cycles. The first 16 slots of each channel carry the signed word MSB first; the
remaining 16 slots are zero. `audio_dac` is zero while reset is asserted.

## Buffer and fault behavior

One selected stereo sample may wait for the next frame boundary. The boundary
atomically consumes it. Before the first selected sample, frames are silent and
do not count as underflow. Afterwards, a boundary without a pending sample
emits silence and sets sticky `underflow`. Selecting another sample while one is
already pending replaces the pending sample with the newer value and sets
sticky `overflow`; this is bounded and observable rather than a silent queue
overrun.

Common active-low reset clears phase, pending and output samples, serial phase,
diagnostic counters, and all sticky flags. `clear_flags` clears the three sticky
flags without disturbing conversion or serial timing. The integration must
synchronize reset deassertion to the 12.288 MHz clock.
