# Pocket Execution-Substrate Comparison Probe

This directory contains non-production feasibility code for ADR-0004. It links the real v1 contracts and deterministic Mock Core so candidate Pocket substrates must compile and run representative RPCMP values rather than a simplified hello-world type.

It is intentionally outside `core/`. Production contracts, runtime, and UI may not depend on this directory, and the architecture test enforces that direction.

## Host gate

Run the complete M0 workflow:

```powershell
pwsh -File tools/host-verify.ps1
```

Run the probe observation record directly after the build:

```powershell
out/build/host-msvc/rpcmp_pocket_comparison_probe_host.exe
```

The host executable runs the same trace with and without a renderer and fails if their semantic records differ. The golden host record is:

| Observation | Value |
|---|---:|
| snapshots | 151 |
| snapshot digest | `6832192089657554689` |
| fake device events | 154 |
| event digest | `16311210033269188847` |
| accepted trace commands | 9 |
| command digest | `2446879228733133299` |
| final snapshot sequence | 151 |

The semantic digests are spike-local FNV-1a records over explicit contract fields in little-endian integer order. They are regression and cross-target comparison values, not a public serialization format.

## Candidate adapters

A candidate supplies only:

- `IProbeBlobReader`, backed by a generated 4,096-byte `synthetic.bin` or an equivalent target data slot;
- optionally `IProbeRenderer`, which observes immutable snapshots and cannot advance Core time.

The synthetic byte at offset `n` is `(n * 37 + 11) mod 256`. The probe checks the first 16 bytes, 16 bytes at offset 2,048, the final 16 readable bytes, and rejection of a 16-byte read beginning at offset 4,088.

The host executable uses `iostream` only for reporting. A target wrapper may report the returned `ProbeRunResult` through its platform console or framebuffer without linking the host executable. The common library itself remains the C++ compatibility input.

See `docs/design/pocket-openfpgaos-spike.md` for the openfpgaOS and minimal-SoC gates, package rules, resource evidence, and selection criteria.

## Docker toolchain gate

Docker Desktop with its Linux engine is the supported Windows path for the
openfpgaOS C++ compatibility gate. The script checks the pinned SDK revision,
builds the checksum-verified xPack 14.2.0-3 image, and links the unchanged
common probe against the SDK's musl runtime:

```powershell
pwsh -File tools/pocket-toolchain-verify.ps1
```

The outputs are:

- `out/build/pocket-openfpgaos/rpcmp-probe.elf`, which reads synthetic slot 4,
  compares both traces with the host golden record, and prints the result on
  the Pocket terminal;
- `out/build/pocket-openfpgaos-desktop/app_pc`, which reads synthetic slot 4
  through the SDK PC backend and must reproduce every host golden digest.
- `out/build/pocket-openfpgaos-package`, the exact allowlisted Pocket tree;
- `out/build/rpcmp-openfpgaos-probe.zip` and its adjacent evidence JSON.

The corrected `0.1.0-spike` target ELF produced `RESULT: PASS` on Pocket on
2026-08-28. This confirms the target slot reader and both deterministic semantic
traces. On firmware 2.6, three consecutive relaunches and a full power-off/start
cycle also passed. The `0.2.0-spike` package retains that automatic gate and
adds a physical input-to-command sequence plus 32 timed logical slot reads. It
subsequently reached `INPUT: PASS` and `OVERALL: PASS` on Pocket; its 16-byte
logical reads measured minimum 1,215 us, integer average 1,254 us, and maximum
1,310 us. The package is a local experiment only: its prebuilt bitstream is not
approved for RPCMP redistribution.

The `0.3.0-spike` package adds a fixed-capacity fake device-write queue and a
zero-copy asynchronous Target dataslot-read measurement. It reached `QUEUE:
PASS`, `INPUT: PASS`, and `OVERALL: PASS` on Pocket. The logical read row was
`1214/1253/1300` us and the Target row was `354/695/4665` us for
minimum/integer-average/maximum. The Target maximum is a tail observation, not
an approved scheduling bound.

The validated `0.4.0-spike` package investigates that tail with four Target
profiles. `F16` and `R16` perform 256 reads of 16 bytes at a fixed and rotating
offset respectively; `F256` and `F4096` perform 64 fixed-offset reads of 256
and 4,096 bytes. Each profile reports integer average, nearest-rank p50, p90,
p95, p99, maximum, and the counts at or above 1 ms and 2 ms. This separates
offset effects from transfer-size effects; it is still a feasibility probe,
not a production scheduling benchmark.

On Pocket, all automatic, queue, input, and overall gates passed. The measured
Target profiles, in microseconds except for the final two count columns, were:

| Profile | Average | p50 | p90 | p95 | p99 | Maximum | >=1 ms | >=2 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `F16` | 450 | 359 | 365 | 366 | 3,206 | 6,182 | 7/256 | 7/256 |
| `R16` | 675 | 362 | 974 | 982 | 3,920 | 10,754 | 11/256 | 11/256 |
| `F256` | 638 | 498 | 504 | 1,083 | 3,447 | 3,447 | 4/64 | 3/64 |
| `F4096` | 4,728 | 3,943 | 5,846 | 5,915 | 8,901 | 8,901 | 64/64 | 64/64 |

The similar `F16`/`R16` medians show a roughly 360 us small-read baseline, but
rotating offsets increase the upper distribution and observed maximum. Transfer
size dominates at 4,096 bytes. Target reads therefore must remain outside the
real-time device scheduling path and use bounded prefetch/cache buffering.

The `0.5.0-spike` package adds the reduced-cache application-performance gate.
Unlike earlier packages, it contains the checksum-pinned integrated fit with a
90 MHz CPU, 16 KiB instruction cache, 32 KiB data cache, the eight-entry JT51
command endpoint, and JT51 itself. One workload sample runs 120 M0 Core snapshot
updates, hashes immutable eight-channel snapshots and fake device events, and
pushes/drains 960 timestamped device writes through the bounded queue. `WH`
measures that workload headlessly and `WO` adds a snapshot observer. Each profile
contains 32 samples and reports integer average, nearest-rank p50/p90/p95/p99,
and maximum in microseconds. Passing proves semantic determinism and completion;
the measured distribution is evidence for judging cache headroom, not a generic
MDX-performance guarantee.

## Pocket installation and retest

### M5 BSS placement diagnostic

Extract `out/build/rpcmp-m5-bss-probe.zip` at the SD-card root. This package
uses the separate paths below and can coexist with the existing safe-layout
probe:

```text
/Cores/RPCMP.M5BssProbe
/Assets/rpcmp_m5bss
/Platforms/rpcmp_m5bss.json
```

From Developer Builds, open `M5BssProbe` and select `m5-bss` if the asset
picker is shown. A pass reaches this terminal result:

```text
RPCMP M5 BSS probe

M5 BSS: PASS
W=33 D=f1f04f5a8695a112
Open Pocket menu to exit.
```

Record the firmware version and the initial result, then relaunch it three
times and perform one full power-off/cold-start run. A blackout, reload loop,
or numbered `M5 BSS: FAIL` is a failure; record the last visible state. This
diagnostic deliberately uses the checksum-pinned safe-layout OS/RBF and does
not contain the JT51/MMIO overlay, so no audio is expected.

Use Pocket firmware 2.2 or later. Replace the earlier experiment by removing
these exact paths from the SD card, then extract
`out/build/rpcmp-openfpgaos-probe.zip` at the SD-card root:

```text
/Cores/RPCMP.openfpgaOSProbe
/Assets/rpcmp_probe
/Platforms/rpcmp_probe.json
```

The current M3 cross-target package is `0.6.0-m3`. Its ZIP is 1,070,541 bytes
with SHA-256
`C3B595D29A6D6D4FB08A207B86CE6505E88238C6FAE373B755A5A8B2CE30EE1D`.
It embeds a 93-byte self-authored FM-only MDX and runs it through structural
parsing, playback preparation, four driver ticks, rational 48 kHz timing, the
fixed-capacity scheduler bridge, and the device scheduler. No local corpus MDX
or copyrighted music is included. The hardware-proven safe OS and RBF remain
byte-identical to the M1 package.

On launch, confirm these additional rows before completing the unchanged input
sequence below:

```text
MDX: PASS W=33 T=2752
MDX D=326b24326bc7b40f
```

The digest is FNV-1a over each dispatched scheduler tick in little-endian form,
followed by its YM2151 address and value. Any parser, preparation, sequencing,
timing, queueing, or dispatch mismatch stops before the interactive input
probe and displays `MDX: FAIL`. For acceptance, record the firmware version,
MDX rows, final `OVERALL: PASS`, normal-restart result, and one power-off cold
start. This gate does not exercise YM2151 audio RTL.

The M3 hardware gate passed on Pocket firmware 2.6: the automatic MDX and
interactive checks passed, and the same package passed both a normal restart
and a power-off cold start. M3 FM sequencing is therefore complete; YM2151
audio RTL integration remains a later milestone.

The current M4 local audio package is
`out/pocket-m4/package/alsterium.RPCMP-M4_0.7.0-m4-local_2026-09-02.zip`
(223,039 bytes, SHA-256
`CAA31934F2B2B1EB1E2622AF67B4E5BB191DA8BF6C06F3C9E8AD9C680F944890`).

The M4 hardware gate passed on Pocket firmware 2.6: the initial launch reached
blue then green with the expected periodic FM sound and no red fault screen;
warm restart and power-off cold start also passed. Audio was heard from the
physical left output only. The fixture intentionally writes `YM2151 $20=$55`
after its initial `$20=$c0`, leaving one output-enable bit set, so single-sided
audio is expected here. This probe does not establish the logical-to-physical
left/right mapping; that requires a dedicated stereo fixture.

It feeds the exact M3 self-authored 33-write trace through the v1 queue, pinned
JT51, and 48 kHz Pocket AUDIO path. Launch `RPCMP M4` from Developer Builds:
blue means running, green means the queue drained and JT51 became idle, and red
means a queue or audio fault. Record whether the brief FM sound is audible,
then repeat once after relaunch and once after a full power-off start. This is a
local-only package and contains no MDX file or user music.

The current M1 bounded-library package is `0.5.39-m1`. Its ZIP is 1,057,657
bytes with SHA-256
`548E38DE6C99511CE372E083B6CC24869520AD18702DFC0D3C65621150EB8E22`.
It adds the 656-byte minimal `.rpcmlib` golden fixture in dataslot 5 and a
bounded logical blob check in the application. The hardware-proven safe OS and
RBF are unchanged. On launch, confirm `LIBRARY: PASS` followed by
`RESULT: PASS`, then complete the existing input prompts through
`OVERALL: PASS`. Record the firmware version and exact last visible line if it
does not reach those results.

Firmware 2.6 completed all four interactive input steps with this exact
package. Because the interactive probe is entered only after every automatic
gate, including the bounded library read, succeeds, this is the M1 hardware
pass. The accumulated diagnostic redraw placed the literal `LIBRARY: PASS` and
final `OVERALL: PASS` rows outside the visible terminal area; future probe UI
work should clear or paginate the result screen.

The reduced-cache workload `0.5.0-spike` ZIP SHA-256 is
`6bfd479fa5375cb01abcf1f4b52bb0206db7a7ece9c8249b5d06d935829cb872`.
Developer Builds displays this package under its metadata shortname
`openfpgaOSProbe`. Run it and follow the prompts in this order:

1. Press A for Play; confirm `INPUT: 1/4` and `state=playing`.
2. Press B for Pause; confirm `INPUT: 2/4` and `state=paused`.
3. Press B for Resume; confirm `INPUT: 3/4` and `state=playing`.
4. Press START for Stop; confirm `INPUT: 4/4`, `INPUT: PASS`, and
   `OVERALL: PASS`.

Before the input prompts, confirm `AUTO/QUEUE: PASS`. Record the existing eight
Target-profile rows and these reduced-cache workload rows exactly as displayed:

```text
F16 a/50/90=.../.../...
95/99/M/1k/2k=.../.../.../.../...
R16 a/50/90=.../.../...
95/99/M/1k/2k=.../.../.../.../...
F256 a/50/90=.../.../...
95/99/M/1k/2k=.../.../.../.../...
F4096 a/50/90=.../.../...
95/99/M/1k/2k=.../.../.../.../...
WH a/50/90=.../.../...
95/99/M=.../.../...
WO a/50/90=.../.../...
95/99/M=.../.../...
WD s=...
WD e=...
WD w=...
```

`a/50/90` means integer average, p50, and p90 in microseconds.
`95/99/M/1k/2k` means p95, p99, maximum in microseconds, then counts at or above
1 ms and 2 ms. The four `F`/`R` profiles time an accepted zero-copy asynchronous
Target dataslot read from issue through completion callback; they exclude stdio
and inter-command ready waiting. If a run fails, record the Pocket firmware version
and exact on-screen state before changing the package. For `WH` and `WO`, the
second line contains p95, p99, and maximum; `WD` contains the snapshot, event,
and write semantic digests. The previously validated `0.4.0-spike` ZIP remains
identified by SHA-256
`7ea69a01f9ade454102c5795809419010859b5eef6ef4be02ca1ada702739d1f`;
the validated `0.3.0-spike` ZIP remains identified by SHA-256
`4a99a5f43f5f156879d033c6bb855f3b21b88da61ec7275fbc34cfc6c0e1d15c`;
the validated `0.2.0-spike` ZIP is
`ccd6d4ef253861e82d49df2c3bfdd84e04405b073a6d5c4c6c1cf8a57132aa82`;
the validated `0.1.0-spike` ZIP is
`6c261468641a4afd00fc5865c9bbd9e80d0f6593a2b66d849f135d4ca7a45fdb`.
