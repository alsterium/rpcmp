# Pocket M2 fixed-sequence package

This local-only package integrates the project-owned M2 fixed sequencer, device
queue, pinned JT51 revision, and exact 48 kHz Pocket AUDIO adapter into official
openFPGA core-template revision
`da3a021b1eaf742604d86d8dc9b33a6666263e6a`. It uses JT51 revision
`985a573dcfc1ff135553a39f7eae21d18ba57cbe`. ADR-0006 prohibits redistribution
until the repository licensing and corresponding-source process is approved.

## Build evidence — 2026-09-01

`pwsh -File tools/pocket-m2-build.ps1` passed a full Quartus Prime Lite
25.1std.0 Build 1129 compile for Cyclone V `5CEBA4F23C8`. All six discovered
clocks were constrained. Every PVT corner had zero TNS; the global minimum
setup slack was `+3.358 ns`, minimum hold slack was `+0.087 ns`, and minimum
pulse-width slack was `+0.830 ns`. The design was fully constrained for setup
and hold with zero unconstrained input/output ports. The RPCMP synchronizers
were recognized and reported a worst-case MTBF of `1e+09 years`.

The fit used 2,016/18,480 ALMs (11%), 2,543 registers, 11,496 block-memory bits,
13/308 RAM blocks (4%), 1/66 DSP blocks (2%), and 1/4 PLLs (25%). Quartus
reported 181 warnings and zero errors. The remaining baseline is inherited from
the official APF shell/template: unused and stuck safe peripheral pins,
incomplete I/O assignments, a non-dedicated bridge SPI clock route, the
template PLL reset/compensation warnings, missing optional SignalTap payload,
and the Lite-edition LogicLock notice. There are no RPCMP unknown-attribute,
missing-clock, negative-slack, or unconstrained-path warnings.

- Native RBF: 890,632 bytes,
  SHA-256 `0451BA7948876DB13DC6E26F60C7AB3E0F58DBF40FE71EFA163B4A7A8A23B9CD`
- SOF: 2,441,493 bytes,
  SHA-256 `B769CB4CB2EAF40383B3900E02606BEC4F1EFF1E33FE1D3BA3246B1594BF5431`
- Pocket-reversed RBF SHA-256:
  `98ABC1A7AD5EF03A73C4A99C5DA6A43234C41D8FF7D34CE8456C7F743404EE15`
- Validated ZIP SHA-256:
  `E690E705708A9489F43A622354C089BEBAE35DC1832D2D25B09722931F5B65F0`

The package contains only the seven APF JSON definitions and
`m2fixed.rbf_r`; it has no data slots, controller mapping, Interact variables,
music, samples, or ROM data. `tools/pocket-m2-package.ps1` validates JSON roots,
metadata bounds, the exact allowlist, and byte-wise RBF reversal round trip.

## Hardware observation

Install the generated tree under `out/pocket-m2/package/sd` or extract
`out/pocket-m2/package/alsterium.RPCMP-M2_0.0.0-m2-local_2026-09-01.zip` at the
SD-card root. Start `RPCMP M2` from openFPGA. The full-screen color is the only
visual diagnostic:

- blue: the fixed sequence is still being queued or played;
- green: all 17 operations reached the device queue without a queue/audio
  fault;
- red: queue overflow/invalid input or AUDIO underflow/overflow/clipping.

Record Pocket firmware, whether the complete bounded tone/pitch-change/key-off
sequence is audible, final color, three warm relaunches, one power-off restart,
and at least a ten-minute terminal-state run. Green proves queue completion,
not audio fidelity by itself; acceptance requires both audible completion and
no red terminal state.

### Accepted result — 2026-09-01

The checksum-recorded package above passed on Pocket firmware 2.6. The initial
launch displayed blue and then green, and the complete fixed sequence was
audible. Three warm relaunches and one restart after powering the Pocket off
also completed successfully. The final green state remained stable for ten
minutes without a red fault indication. This satisfies the M2 hardware audio,
terminal-state, relaunch/reset, power-cycle, and sustained-run acceptance
criteria.
