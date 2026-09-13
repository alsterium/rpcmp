# JT51 hold experiment v1

Status: local M6 slice 4 feasibility fixture, 2026-09-13. This is a generated
sound-engine experiment, not a public MMIO protocol or an advertised Pocket
pause capability. [The adapter proposal](../docs/design/pocket-player-platform-adapters.md)
requires source state to survive a pause before queue, resampler, I2S and
control-ACK integration can be accepted.

## Source and generation boundary

Consume the clean JT51 checkout at
`985a573dcfc1ff135553a39f7eae21d18ba57cbe`, as selected by
[ADR-0006](../docs/adr/0006-m2-ym2151-rtl.md). Its GPL-3.0-or-later source and
generated derivatives stay under ignored `out/`; they are not vendored or
redistributed by this fixture. The existing sound path continues to use the
unmodified source. This introduces no additional third-party dependency.

`tools/jt51_hold_prepare.py` reads the pinned Git objects after revision/clean
checks, processes only 22 named HDL files, and refuses an existing output or
an output outside the repository's `out/`. Module names gain `rpcmp_hold_` so
original and candidate can coexist in one simulation. Only the seven module
types that need or forward hold gain an explicit `rpcmp_hold` port. Seven child
connections form a checked propagation graph.
Comments, including copyright/license headers and the commented-out phinc ROM
clock input, are preserved. A manifest records input/output hashes, recipe
hash and per-file port/process counts, alongside a copy of the upstream license.

The recipe is specific to this revision. Its inventory checks 44 textual
clocked processes (42 active and two disabled debug processes), including 14
asynchronous reset processes. At the top, eight native `cen_p1` connections
are qualified by `!rpcmp_hold || rst`. This freezes the 38 active processes
already governed by native enables, while preserving shift-register RAM
inference and the original per-stage clocked statement shapes.

Four processes also need explicit hold: the LFO update latch, the MMR register
block, channel register writes and the timer flag. The latter three retain
their leading asynchronous reset branch and gate only its ordinary-update
`else` arm, whose pinned shape is checked before editing. The LFO latch uses
an outer hold condition with reset priority. Child hold is cleared during reset.
ROM initialization and combinational logic are unchanged.
No fabric clock gate is introduced in the generated engine. Debug/test macros
and other source variants are outside this fixture's supported profile.

## Meaning and comparison

When `rpcmp_hold` is high, sequential sound state keeps its prior value,
including memory-mapped register handling, channel memory writes, LFO update
latches and timer flags that do not all obey the original `cen`. Reset remains
effective during a hold. The owner must retain its enable-generator phase and
in-flight bus signals with the engine. A held `sample` value is not a new event;
the consuming adapter must qualify progress with its media enable.

The testbench uses an unmodified JT51 oracle that receives only retained media
clock edges. This testbench-only clock is not target RTL. The candidate receives
every fabric edge plus hold. A shared rational enable trace and authored bus
operations provide the same retained inputs, and the comparison checks native
stereo and extended samples, sample strobe, status, IRQ and control outputs.
The selected-address register also checks the first in-flight-address hold.
Unknown output samples after setup and a vacuous silent comparison fail.

Stimuli configure all eight channels, both output sides, all four operators,
noise, four LFO waveforms, timers, pitch changes and key retriggers. Holds cover
all 256 media sub-tick phases, including bus work, with variable lengths.
Reset while held and subsequent retained execution use the same oracle.
The `-CenOnly` negative control runs the identical bench with an unmodified
candidate whose enables alone are masked. It must reproduce the specific
MMR hold failure; that expected failure is distinct from the positive result.

```powershell
pwsh -File tools/rtl-jt51-hold-verify.ps1 -CenOnly
pwsh -File tools/rtl-jt51-hold-verify.ps1
```

Both use the pinned Questa 2025.2 and configured Starter license. Run RTL suites
sequentially. This fixture proves native-engine equivalence only for the tested
trace. Frame-boundary pause requests, pending/resampled samples, stereo zero
insertion, gain reservations, audible commit, CDC/ACK bounds, integrated synthesis and
Pocket behavior still require the subsequent M6 integration checks.
