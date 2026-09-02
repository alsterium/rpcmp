# M4 — MDX FM Audio Integration

Status: complete on 2026-09-02. All host, architectural, behavioral RTL,
pinned real-JT51, Quartus, packaging, and Pocket hardware acceptance gates
pass.

Build evidence on 2026-09-02: Quartus 25.1std.0 Build 1129 used official
template `da3a021b1eaf742604d86d8dc9b33a6666263e6a` and JT51
`985a573dcfc1ff135553a39f7eae21d18ba57cbe`. The fit used 1,986/18,480
ALMs, 12/308 RAM blocks, one DSP, and one PLL. Minimum reported slack was
`+0.073 ns`; the build recorded no unconstrained external ports and recognized
207 synchronizer chains. The 884,496-byte native RBF SHA-256 is
`4FFB82D464BA7E0A75B5A0842EF732624BF4EDF1BA51EB67A44A15BB1FA6DC3E`.

The local-only package is
`out/pocket-m4/package/alsterium.RPCMP-M4_0.7.0-m4-local_2026-09-02.zip`
(223,039 bytes, SHA-256
`CAA31934F2B2B1EB1E2622AF67B4E5BB191DA8BF6C06F3C9E8AD9C680F944890`).
Its reversed RBF SHA-256 is
`D3DE6CBF4CCFFEF94D9ABEF576B547E1B1E2E30B6ADF2F9E53F5A8A941CBD03F`.

## Objective

Carry the exact ordered YM2151 writes produced from the M3 self-authored MDX FM
fixture through Core-to-RTL Device Queue v1, the pinned JT51 adapter, and
Pocket AUDIO at 48 kHz, then prove the result on Pocket without selecting the
still-gated general-purpose execution substrate.

## Scope and boundaries

- The source fixture remains the 93-byte project-authored MDX used by the M3
  cross-target probe. No local user music or copyrighted bytes are packaged.
- `specs/fixtures/mdx-fm-probe-trace-v1.csv` freezes the 33-write, 48 kHz trace
  with end tick 2,752 and digest `326b24326bc7b40f`.
- A host test compares that CSV byte-for-byte with fresh parser-to-scheduler
  output. A second test compares its entries with the RTL sequencer literals.
- The M4 validation sequencer uses the M2 project-owned APF template shell,
  v1 queue, pinned JT51 adapter, and Pocket AUDIO adapter. It is a hardware
  validation substrate, not the future production MDX runtime.
- PDX/MSM6258/PCM8 playback and mixing are out of scope. The typed MDX target,
  semantic sink, `DeviceOp`, and device adapter boundaries remain unchanged so
  a later PCM device and mixer can be added without translating PCM into
  YM2151 writes.
- The package is local-only. The user has waived GPL process work for personal
  use; this does not change ADR-0006's restriction on distribution.

## Acceptance criteria

1. Fresh host execution produces exactly 33 entries matching the frozen CSV,
   including equal-timestamp order, tick 2,064 key-off, and the approved digest.
2. Behavioral and pinned real-JT51 simulations observe all 66 address/data bus
   writes in order, produce source samples, and report no queue or audio fault.
3. Quartus 25.1std.0 Build 1129 completes the pinned template/JT51 integration
   with non-negative timing slack, recognized CDC synchronizers, and no
   unconstrained external ports under the accepted M2 constraint policy.
4. APF JSON roots, metadata, paths, bit reversal, package allowlist, and ZIP
   roots validate; hashes and sizes are recorded before SD installation.
5. On Pocket firmware 2.6, the screen starts blue, the expected periodic FM
   sound is audible, and the screen reaches green rather than red. One warm
   relaunch and one power-off cold start reproduce the result.
6. Host tests, formatting, static analysis, architecture checks, and all
   affected RTL tests remain green.

## Hardware procedure

1. Extract the generated ZIP at the SD root, preserving its `Cores` directory.
2. Launch `RPCMP M4` from Developer > Builds.
3. Record `blue -> green`, whether a brief FM sound was heard, and whether a
   red screen appeared. Green proves queue drain/device idle; red is any queue
   or audio diagnostic fault.
4. Relaunch once and repeat the observation.
5. Power Pocket fully off, start it again, relaunch `RPCMP M4`, and repeat.
6. Report firmware, initial result, relaunch result, and power-off result.

## Pocket hardware result

Pocket firmware 2.6 passed on 2026-09-02. The initial launch reached blue then
green, produced the expected periodic FM sound, and showed no red fault screen.
Both a warm relaunch and a launch after full power-off also passed.

The sound was heard from the physical left output only. This is expected to be
single-sided for this fixture: after the driver's initial both-output channel
control write (`YM2151 $20=$c0`), the self-authored MDX deliberately performs a
direct `YM2151 $20=$55` write, which leaves only one YM2151 output-enable bit
set. This result proves audible trace playback but does not prove Pocket
left/right channel mapping. A later dedicated stereo probe must drive distinct
signals to both logical outputs and verify their physical mapping.
