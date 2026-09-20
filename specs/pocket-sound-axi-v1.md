# Pocket sound AXI binding — v1

Status: adopted internal M6 slice 5 binding, 2026-09-20, under the approved
[CPU sound connection](../docs/design/pocket-cpu-sound-connection.md).
It connects [sound MMIO v1](pocket-sound-mmio-v1.md) to the pinned openfpgaOS
peripheral and audio pins. It does not add a public command or enable settings
persistence, which the user deferred on 2026-09-20.

## Build and compatibility

Source preparation opts in with `--player-sound`, requiring the existing
`--apf-lifecycle --apf-flush` base (including its delayed-first-write fix).
The `INCLUDE_RPCMP_PLAYER` build selects sound MMIO at CPU address
`0x40000400..0x400007ff`; the M5 queue/reset area is absent in this profile.
Default preparation keeps M5's queue/reset and audio owner unchanged. Software
must check the respective IDs/capabilities rather than infer support from a
successful AXI transaction. Flush transport alone is not settings capability.

The player profile generates the pinned, already-tested hold-capable JT51
namespace and records its source/recipe hashes and license. Generated vendor
HDL remains in `out/`. No settings RAM or storage controller is instantiated.
This source profile alone is not a coherent ROM/OS/application package.

The first integrated fit exceeds the device capacity. The player build uses
the shell's existing `EXCLUDE_GPU` branch: framebuffer scanout and CPU drawing
remain, and GPU DMA/flip masters are inactive. All GPU feature bits (4, 10..23,
25..30) must be clear, with `gpu_base=0` in the OS capability descriptor.
CPU framebuffer presentation uses the video service, never GPU fence/flip
commands. M5's default GPU and descriptor remain unchanged. The eventual
renderer must bound drawing work so sound service remains independent of it;
this configuration does not establish measured rendering or service cadence.

## Transactions

The actual peripheral interface carries 32-bit addresses/data and four write
strobes; it has no ARSIZE/AWSIZE input. CPU software uses aligned 32-bit loads
and stores. This binding accepts only single-beat sound transactions (`LEN=0`),
and FIXED/INCR write types. It rejects sound bursts and reserved/WRAP writes
without issuing MMIO strobes; reads return zero/SLVERR and writes drain the
declared beats before returning SLVERR. A single write also requires WLAST.

For valid shapes the exact address and byte enables reach the MMIO owner.
Its rejection (including partial/zero-mask writes, reserved words, invalid
values and busy submission) becomes AXI SLVERR with no accepted side effect.
The sound block's combinational read strobe is asserted during the actual
peripheral read-latch cycle. Data and error are captured together, then held
on AXI until RREADY. A held B response cannot be overwritten by another AW.
Audio mailbox completion is independent of AXI response acceptance: the bus
acknowledges a local register access, never an unobserved audio operation.

Other peripheral regions retain their existing mappings/semantics. Accesses
outside the sound range do not alias it. The M5 profile's decode and response
behavior are unchanged when the player macro is absent.

## Clock and reset

The sound owner uses the actual 90 MHz CPU and 12.288 MHz audio domains. Both
reset inputs share the shell's APF/BCR reset; its existing local synchronizers
release each domain. The output uses the new owner's I2S pins; the legacy PCM
path does not drive those pins in this profile. Ordinary player Stop/Reset is
the mailbox protocol, not a physical reset of the CPU or serializer.

The player-only preparation removes the inherited blanket CPU/audio clock-group
cut. `player-sound.sdc` constrains all four retained request/response bundles to
81.380/11.111 ns, respectively; only their asynchronous hold relationship and
the first toggle/status synchronizer stages are excepted. Second stages and
local reset release remain timed. Missing required endpoints fail constraint
loading. The final fit must audit every optimized bundle destination, route
delay and second-stage setup/hold path; resource-only fits using the inherited
clock cuts cannot establish this guarantee.

The inherited exclusion for the separate legacy PCM serializer/FIFO is retained
only on CPU/audio crossings through that instance. It does not drive the AUDIO
pins in this profile. Same-domain paths remain timed, and no path through the
new sound owner may be covered by this exception. This preserves an existing
shell limitation rather than proving that legacy reset/FIFO CDC.

## Acceptance

Test the actual prepared AXI module and exact top-level sound/reset port
connections with the real sound owner and generated JT51, across independent
clock phases. Exercise bundled/delayed W, held R/B, full-address bounds,
invalid masks/shapes, retained mailbox responses, all four mailbox channels
and APF reset. Negative controls must catch the wrong read-strobe edge and
overwriting a pending B response. Run disabled-profile APF/legacy regressions,
sound RTL tests, host checks and whole-core synthesis/placement/timing.

Whole-core resource and inherited-constraint reports are evidence with their
stated limits, not proof of firmware service cadence, reviewed external I/O
timing, live UI behavior or Pocket playback. Those remain slice 5/6 gates.
