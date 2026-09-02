# M4S — Pocket Stereo Channel Mapping Probe

Status: implementation complete; Pocket firmware 2.6 hardware acceptance is
pending.

Build evidence on 2026-09-02: Quartus 25.1std.0 Build 1129 used official
template `da3a021b1eaf742604d86d8dc9b33a6666263e6a` and JT51
`985a573dcfc1ff135553a39f7eae21d18ba57cbe`. The fit used 2,007/18,480
ALMs, 13/308 RAM blocks, one DSP, and one PLL. Minimum reported slack was
`+0.096 ns`; there were no unconstrained external ports and 207 synchronizer
chains were recognized. The 891,072-byte native RBF SHA-256 is
`74ACEB4CCDA14DD907AC7B002603F1CC46EA1FF47A531B216EEB5A62E836089F`.

The local-only Pocket package is
`out/pocket-stereo/package/alsterium.RPCMP-Stereo_0.7.1-stereo-local_2026-09-02.zip`.
Its SHA-256 is
`D38C0B82E4EC7D8C94F0D09C11436EBE3531BA46820F5DCB79D0DD50D66CFC6B`;
the reversed RBF SHA-256 is
`D78DC2A76945F66EF91AD823A4434E873524A432D7F84BF0DEDF8FB22BF5E603`.

## Objective

Resolve the remaining M4 observation that its deliberately single-sided final
pan write was heard only from the physical left output. Prove the complete
logical YM2151 left/right to Pocket physical left/right mapping before M5 uses
real MDX files.

This is a bounded M4 diagnostic, not PCM playback and not a new production
runtime. It changes no public contract. The same stereo boundary is retained
for the mandatory future MSM6258/PCM8 mixer.

## Stimulus

The project-authored channel-0 voice and pitch from the M2 probe are reused.
JT51 documents register `$20` bits 7:6 as right/left enable, and its accumulator
maps `rl_I[0]` to `left` and `rl_I[1]` to `right`. The Pocket adapter passes
those outputs unchanged and serializes logical left while LRCK is low, then
logical right while LRCK is high.

The three-second probe is:

| Time | Expected audio |
|---:|---|
| 0.00–0.25 s | silence while the voice is configured with `$20=$47` |
| 0.25–1.25 s | logical left only |
| 1.25–1.75 s | silence; at 1.50 s pan changes to `$20=$87` |
| 1.75–2.75 s | logical right only |
| 2.75–3.00 s | silence |

The screen remains blue while the sequence runs, becomes green after the final
reset and queue drain, and becomes red on a queue or audio diagnostic fault.

## Acceptance

1. Behavioral and pinned real-JT51 simulations observe all 34 address/data bus
   writes in order, including `$47` before the first note and `$87` before the
   second note, with source samples and no queue/audio fault.
2. Quartus and package validation pass under the same pinned versions and
   timing/CDC checks as M4.
3. On Pocket firmware 2.6, the first tone is heard only on physical left and
   the second only on physical right, separated by silence. The screen reaches
   green without red.
4. A warm relaunch and a full power-off cold launch reproduce the mapping.

## Pocket procedure

1. Extract the generated ZIP at the SD-card root.
2. Launch `RPCMP Stereo` from Developer > Builds using headphones or another
   output whose physical left/right channels are known.
3. Confirm: left-only tone, silence, right-only tone, then green screen.
4. Record whether red appeared, repeat once by relaunching, then repeat after a
   full power-off.
