# MDXPlayer compatibility baseline

Status: user-selected product target on 2026-09-21; implementation and full
corpus comparison pending. This records the target and research evidence,
not a replacement engine/device contract or new runtime dependency.

## Accepted target

Support every locally supplied file that the user could play with
[asaday/MDXPlayer](https://github.com/asaday/MDXPlayer), including its required
PDX data and PCM parts. Track selection, play and stop are the MVP controls.
Tracker/keyboard views, shuffle, counted-loop fades and persistence are deferred
as completion requirements. Existing work need not be deleted.

Successful parsing, skipping unsupported commands, dropping PCM or removing
unimplemented files from the denominator does not satisfy compatibility.
Preserve note/timing/loop behavior and FM/PCM parts. Distinguish playable
compatibility from bit-identical software-emulator audio: the current JT51
hardware route is not the reference application's FMGEN waveform generator.
Exact comparison tolerances remain to be established with reference evidence.

This user decision supersedes the FM-only release goal in PRD 0.4 and the
earlier M6 feature priority. It does not silently modify `mdx-v1`, enable PCM
in that API, authorize unsafe reads, or approve a particular third-party port.

## Pinned source evidence

The repository was inspected at
[`4076b91c7ced57bf6047f69b87c12a34bd99a438`](https://github.com/asaday/MDXPlayer/tree/4076b91c7ced57bf6047f69b87c12a34bd99a438),
dated 2025-11-11. Its Xcode project declares marketing version **2.1.0** and
build version **2.1.0.0**. This pins the research source; the user's installed
binary version/build and playback settings have not been identified. Do not
describe the checkout as a verified reproduction of that installed app.

| Area | Observed implementation |
| --- | --- |
| Driver | `gamdx/jni/mxdrvg/mxdrvg_core.h` identifies MXDRV 2.06+17 Rel.X5-S, converted as fMXDRVg V3.00a; `PCM8_ENABLE` is 1 |
| FM | FMGEN `FM::OPM`, initialized at a 4 MHz chip clock |
| Samples | `X68K::X68PCM8`; both PCM8 commands and legacy ADPCM calls route to it |
| Timing | `MXDRVG_GetPCM` advances the OPM timer and invokes the driver interrupt while producing FM/PCM blocks |
| Load | `classes/objc/Player.m` extracts the MDX body and PDX name, adds driver headers and calls `MXDRVG_SetData` |
| Compression | The loader calls `lzx042check`/`lzx042decode` for both MDX bodies and PDX data |
| PDX resolution | The loader tries the referenced file beside the MDX, appends `.pdx` when needed, and tries several case variants |

Primary code: [driver and sound composition](https://github.com/asaday/MDXPlayer/blob/4076b91c7ced57bf6047f69b87c12a34bd99a438/gamdx/jni/mxdrvg/mxdrvg_core.h),
[file loading and playback](https://github.com/asaday/MDXPlayer/blob/4076b91c7ced57bf6047f69b87c12a34bd99a438/classes/objc/Player.m),
[PCM8 implementation](https://github.com/asaday/MDXPlayer/tree/4076b91c7ced57bf6047f69b87c12a34bd99a438/gamdx/jni/pcm8).

Use this bundled implementation as the first comparison reference. A current
GAMDX or portable_mdx checkout is related but not automatically equivalent.
In particular, replacing `OPM.SetReg` with hardware writes alone would lose
the timer-driven execution relationship unless that timing owner is retained
or deliberately replaced and compared. The iOS UI/AudioQueue are platform
adapters and are not necessary Pocket dependencies.

The loader can pass a null PDX pointer when loading fails. A file opening in
the reference therefore does not establish that its expected PCM was audible.
Record missing dependencies separately and compare the actual source behavior;
do not infer either full PCM success or permanent exclusion from that branch.

## Local coverage and r4 evidence

Read-only whole-corpus checks on 2026-09-21 used the existing
`tools/mdx_corpus_audit.py` and `rpcmp_mdx_admission` at RPCMP `dd68405`.
Both exited 0. Of **13,140** MDX files, **2,521** pass current preparation,
**4,351** fail the 16-track gate, **5,071** first fail an unsupported opcode,
**19** an unsupported extension, **432** PCM admission and **746** other parser
checks. The 19.2% preparation pass rate is not an audible playback pass rate.
The reference has not been run against every file, and parser rejections must
not be described as proven broken source data.

PDX inventory: **1,637** files. MDX plus PDX is about **179.7 MiB** including
duplicates, beyond the current all-in-memory 32 MiB library profile. A simple
name/case/suffix candidate lookup leaves **273** nonempty PDX references
unmatched; archives, aliases and content identity remain unverified. The
counts cover loose `.mdx` files, not all archive contents or other formats.
Private paths, names and bytes remain outside committed evidence.

Firmware 2.6/r4 hardware report: volume/stereo/one-minute infinite playback
pass; Tracker A and keyboard A move and Waiting clears. Tracker does not
synchronize with audio. View-dependent input delay/audio cuts and clicks
between notes since M6 remain reported. F values are Tracker **300 ms**,
keyboard **360 ms**, library **250 ms**; reported S is **8870 us**.
F is complete-frame work time, not FPS; S is the maximum observed service gap.
The performance-display pause/resume/stop/next field was not answered.
Noise cause and exact visual/audio offset are not established by these values.

## Next bounded work

The headless reference, CPU comparison and initial PCM/split experiments below
are complete within their stated limits. Follow the
[consolidated design proposal](../design/pocket-mdx-compatibility-plan.md):

1. The offline shared-timeline mix below now works. Connect the split renderer
   to a minimal target PCM/FM transport and test start/stop/reset/underrun.
   The user approved this direction and waived previous RPCMP compatibility;
   document the concrete changed boundary without adding compatibility layers.
2. Measure the composed CPU/transport/memory deadline and compare actual FM/PCM
   timing and sound with the pinned reference, including dense writes, stops,
   loops and faults. Keep per-file time/memory limits around the legacy decoder.
3. Extend the private reference manifest beyond prefixes to complete songs or
   loop boundaries and the whole collection. Resolve dependency failures
   separately; do not turn successful loading or a prefix into a playback pass.

No production RPCMP implementation change, all-file reference playback,
composed PCM Pocket benchmark or new hardware pass is claimed here.

## Host reference feasibility check

The seven C++ decoder/sound translation units listed by the Xcode project
build as an ignored Linux shared library without source patches:
`mxdrvg/so.cpp`, `fmgen/fmgen.cpp`, `fmgen/fmtimer.cpp`, `fmgen/opm.cpp`,
`pcm8/pcm8.cpp`, `pcm8/x68pcm8.cpp`, `downsample/downsample.cpp`.
The existing `rpcmp-openfpgaos-toolchain:14.2.0-3` Docker image runs native
`g++ -std=c++17 -O2 -fPIC -shared -include cstdint <these sources> -o
out/build/mdxplayer-reference-20260921.so`. The forced standard integer header
replaces a transitive platform include; no decoder behavior is patched.
Compilation exits 0 with legacy warnings, including integer/pointer-size
conversions. This is not a warning-clean production integration or proof of
all-path 64-bit portability. The research checkout remains clean.

`python3 -B out/build/mdxplayer-reference-smoke-20260921.py` runs the three
original RPCMP r4 FM demo files at 44,100 and 48,000 Hz in separate processes
with 15-second per-case limits. It creates the same ten-byte driver header as
the app, initializes total volume, and requests 100 blocks of 1,024 stereo
frames. All **6/6** cases return the requested frame counts, advance playback,
produce nonzero left/right samples and preserve output-buffer canaries.
This is about 2.32/2.13 seconds of generated FM per case, with no listening,
PCM track, private corpus, sanitizer or Pocket timing validation.

The first smoke invocation failed its nonzero-output assertion because the
new wrapper omitted the app's explicit `MXDRVG_TotalVolume` initialization.
Adding that call fixes the wrapper; the upstream decoder is unchanged.
Logs are ignored: `mdxplayer-reference-20260921-compile.log`,
`mdxplayer-reference-smoke-20260921.log` and
`mdxplayer-reference-smoke-20260921-final.log` under `out/build`.
The next comparison tool still needs faithful file/dependency loading,
legacy-input isolation and per-file reference evidence.

A separate `python3 -B out/build/mdxplayer-private-prefix-20260921.py` probe
uses the previously supplied twelve-file set whose original sixteen-track
layouts and E9 commands the RPCMP subset rejected. Source directories are
mounted read-only in a network-disabled Docker invocation. Each file runs in
a fresh process with a 15-second wall limit, 10-second CPU limit and 512 MiB
address-space limit. All **12/12** generate 102,400 frames at 44,100 Hz, advance
playback, produce nonzero stereo samples and preserve output-buffer canaries;
there are no failed cases or timeouts. Source bytes are not normalized or edited.
Log: `out/build/mdxplayer-private-prefix-20260921.log` (aggregate only).
These files have empty PDX references and inert PCM tracks; this establishes
only an approximately 2.32-second FM prefix, not full-song fidelity, PCM support,
app-binary equivalence or Pocket execution. No generated private audio is saved.

## Software-synthesis execution budget

On 2026-09-21, a separate ignored probe compiled the same seven translation
units for native Linux with GCC 13.3.0 and RV32IMAFC/ILP32F with GCC 14.2.0.
It includes the reference core in place of the small `so.cpp` wrapper, directly sets
authored FM registers/sample buffers, and calls `MXDRVG_GetPCM`. No upstream
synthesis source is changed for these builds. At 48,000 Hz output the
reference internally generates 62,500 Hz, with the application's filter mode 0.

This is a synthesis-budget experiment, not playback of MDX or PDX files. Cases
use zero/one/eight FM channels and zero/eight PCM channels. FM uses four active
operators per channel, algorithm 7 and sustained tones; sample modes cover
15,625 Hz ADPCM, 16-bit PCM and 8-bit PCM. Generated sample patterns include
clipping in the eight-voice cases; this is not an audio-quality acceptance test.
Each case warms up 256 frames, then measures 128 blocks of 256 stereo frames.
Allocation warm-up, input, rendering, storage, driver sequencing and output
transport are outside the measurement. Other algorithms, LFO, note/envelope
transitions, sample rates, DMA chains and entire songs are not covered.

Instruction execution uses the MIT-licensed research tool
[rv32emu](https://github.com/sysprog21/rv32emu/tree/61b1a194f808e1a47aa7c2ed1ddae20d8bcff36b),
with JIT, macro-operation fusion and block chaining disabled. Its interpreter
increments the counter per guest instruction; `INSTRET` reads that counter.
An independently counted eight-NOP sequence between CSR reads returns the
expected delta of nine. This is **retired ISA instructions, not Pocket CPU
cycles, wall-clock performance, cache simulation or a hardware test**.

| Authored active channels | `-O2`: million instructions/audio second | `-O3 -flto`: million instructions/audio second |
| --- | ---: | ---: |
| Silent synthesis path | 17.16 | 13.50 |
| FM 1 | 33.82 | 28.79 |
| FM 8 | 103.71 | 99.18 |
| ADPCM 8, FM silent | 53.79 | 49.26 |
| 16-bit PCM 8, FM silent | 53.14 | 47.48 |
| 8-bit PCM 8, FM silent | 49.62 | 44.71 |
| FM 8 + ADPCM 8 | 140.36 | 134.95 |
| FM 8 + 16-bit PCM 8 | 139.69 | 133.16 |
| FM 8 + 8-bit PCM 8 | 136.17 | 130.39 |

The rate is measured instructions times 48,000 / 32,768. Counters include the
few instructions needed to delimit calls. All **9 cases x 3 builds** exit 0,
preserve output canaries and match the native per-case output hash after the
signed-character correction below. This establishes limited cross-target
agreement, not equivalence to the iOS binary or complete corpus compatibility.

The current fitted `VexiiRiscv_rpcmp` configuration has one decoder/issue lane
and runs at 90 MHz. Its optimistic ceiling is 90 million instructions/second
before memory waits, multicycle instructions or OS work. The measured FM-8 and
combined cases exceed that ceiling even with `-O3 -flto`. Therefore **an unchanged
reference synthesizer on the unchanged 90 MHz CPU cannot satisfy these cases**.
Removing rendering or openfpgaOS overhead alone cannot bridge this gap.

This does not eliminate all software sound. The pinned upstream `os20.cfg`
describes a 96 MHz dual-issue variant with 32 KiB instruction / 64 KiB data
caches. That is source evidence, not an RPCMP fit or performance result. Two
issue lanes do not imply twice the throughput: the combined ADPCM case alone
would require about 1.41 instructions/cycle before other work. Compare that
candidate, or a lean CPU platform with measured throughput, against FM hardware
plus CPU PCM. The PCM-only rows leave a plausible budget for the latter but do
not include a hardware FM transport or prove that composition works. Do not
retain or discard openfpgaOS/JT51 merely because they already exist.

### Portability and reproducibility

The first cross-target comparison found an 8-bit PCM output mismatch. The
reference casts sample bytes through plain `char` in `pcm8.cpp`; RV32 GCC
defines `__CHAR_UNSIGNED__`, while the native comparison compiler does not.
Explicit `-fsigned-char` restores the intended signed conversion and all nine
case hashes agree at both optimization levels. Preserve this regression case;
a successful cross-link alone would not have caught it.

A separate Pocket SDK link succeeds with no undefined symbols: text 109,584,
data 77,428, BSS 148,044 bytes (335,056 static bytes, excluding heap and stack).
The SDK's C++ `NULL` is `nullptr`, exposing one upstream assignment to a byte;
only the copied SDK experiment changes `DisposeStack_L00122e = NULL` to `= 0`.
The original pinned checkout remains clean. This ELF has not been loaded on
Pocket and exceeds the existing player's 4 KiB initialized-data budget; it is
not a packaged candidate or evidence that the current loader/profile accepts it.
Native builds still report legacy pointer-width warnings.

Commands in the existing `rpcmp-openfpgaos-toolchain:14.2.0-3` image:

```sh
python3 -B out/build/mdxplayer-synthesis-budget-20260921-build.py
python3 -B out/build/mdxplayer-synthesis-budget-20260921-run.py
```

Owned probe sources, ELF files, calibration, final build/results logs and the
machine-readable `results.json` remain in ignored `out/build`; they contain
only authored signals. The earlier failed build/comparison logs are retained.
The emulator is research-only: MIT rv32emu, BSD-3-Clause Berkeley SoftFloat
at `3b70b5d8147675932c38b36cd09af6df4eedd919`, and ISC Kconfiglib at
`7a57bfc7adac8a7567805ac5097c706b824aeb28` provide a Linux-hosted ISA interpreter
and its build configuration, not player dependencies or shipped assets.
The additionally fetched ieeelib test source is GPL-2.0-or-later with its
stated GCC linking exception; it is not part of this interpreter build.
The interpreter's 256 MiB address-space build
uses the explicit hexadecimal `compute_size=10000000` because the build image
lacks `bc`. No production code, public engine/device contract or package changes.

## CPU RTL comparison

A second 2026-09-21 experiment executes the reference synthesis workload on
Verilator CPU models, rather than using ISA instruction counts as cycles.
The current saved `VexiiRiscv_rpcmp.v` has one issue lane, 16 KiB instruction
and 32 KiB data caches. A separately generated `rpcmp_budget_dual` follows
the pinned upstream `os20` settings: two issue lanes, 32 KiB instruction and
64 KiB data caches, a four-slot/32-operation store buffer and reduced-accuracy
FMA. This compares complete configurations, not the isolated benefit of a
second issue lane. No dual-issue FPGA fit or clock frequency is established.

Both models use a research-only bare-metal entry point and independent
instruction/data AXI memories. They accept requests without SDRAM arbitration,
return the first read beat one cycle later, and sustain one 32-bit beat per
cycle per port. CPU pipelines and caches are active. There is no OS, rendering,
display DMA, real SDRAM controller, storage, MDX sequencing or audio transport.
This deliberately optimistic memory model can reject a throughput candidate;
passing it would not establish Pocket real-time performance.

The sound source, optimization, authored signals and 48,000/62,500 Hz rates
match the instruction experiment. Each case warms up 256 frames and measures
eight blocks of 256 frames: **2,048 output frames, about 42.67 ms**, not an
entire song or a sustained hardware run. Buffer canaries, stereo peaks and
output hashes are compared with independent native processes. The simulator
supplies its cycle count through an uncached research MMIO register. ELF
disassembly confirms clock read / synthesis call / clock read ordering;
reported cycles include the small measurement overhead. Initialization and
hashing are outside the measured interval.

Both CPU models complete all nine corrected cases with native-matching
hashes, peaks and intact canaries (**18/18** comparisons). An additional
fresh-process dual-issue FM+ADPCM run also matches. Both final model batches
use the same ELF, SHA-256
`c9e3e1b35157487ed5a203d116b17cc4842f74ab63e33ce6edc843381120c160`.
Representative results, in million CPU cycles per audio second:

| Authored active channels | Current single-issue configuration | Dual-issue configuration |
| --- | ---: | ---: |
| FM 8 | 131.74 | 96.93 |
| ADPCM 8, FM silent | 56.33 | 46.37 |
| 16-bit PCM 8, FM silent | 57.86 | 48.03 |
| 8-bit PCM 8, FM silent | 51.93 | 42.28 |
| FM 8 + ADPCM 8 | 172.11 | 130.90 |

The separate isolated dual-issue FM+ADPCM case uses 5,594,148 measured cycles
(131.11 million cycles per audio second); its
largest 256-frame block uses 699,969 cycles. Even at the upstream candidate's
claimed 96 MHz, its average compute demand is about **137%** of the available
time, before the excluded work. An unchanged reference renderer therefore
does not become viable for this load merely by choosing this stronger CPU.
This does not rule out a different renderer, major optimization, a different
CPU or a different device.

The PCM-only measurements make **reference-driver CPU + FPGA FM + software
PCM8** the leading next experiment. The current CPU's tested PCM paths consume
roughly 52–58 million cycles per audio second under optimistic memory. That is
plausible at 90 MHz, but still needs driver, FM command transport, actual memory,
audio buffering and worst-case corpus measurements. It is not a PCM hardware
pass or a promise that every file meets deadlines. Retain the reference timer
owner and compare FM/PCM event positions on one audio timeline when testing
this split; merely forwarding FM writes loses the original timing relationship.

### Resource evidence and simplification

Inspection of the existing `m6-player-finalbuild/output_files/ap_core.fit.rpt`
finds these inclusive hierarchy estimates; this is **historical fitted
evidence, not a new synthesis result**:

| Existing hierarchy | Estimated ALMs needed |
| --- | ---: |
| Whole core | 16,648.9 |
| CPU system, including the CPU | 5,404.9 |
| VexiiRiscv CPU | 4,631.0 |
| RPCMP sound MMIO hierarchy, including sound/session processing | 7,683.1 |
| Hold-capable JT51 itself | 1,186.2 |
| Media envelope hierarchy | 2,123.9 |

Parent and child rows overlap and must not be added. Fitter estimates are not
guaranteed savings after removal or redesign. Nevertheless, most of this
sound hierarchy's area is outside the FM synthesizer. Removing FM hardware
alone would surrender substantial synthesis throughput for a comparatively
small area saving. Evaluate a thin, timed FM-command/PCM-sample output path
for the select/play/stop MVP; the historical fade, progress and completion
machinery is not automatically part of the replacement design. Do not simply
delete current paths or weaken their contracts.

The design shortlist is therefore:

1. Reference MXDRV interpretation on CPU, FPGA FM and reference software PCM8,
   with a bounded audio buffer and minimal selection UI. First establish the
   execution/timing boundary headlessly; select the smallest CPU that passes.
2. Add narrowly scoped PCM acceleration only if measured worst-case PCM or
   memory traffic leaves insufficient margin. Preserve a software oracle.
3. Full software synthesis remains attractive on a sufficiently capable host,
   but the tested Pocket CPU configurations do not justify adopting it as-is.

Keep openfpgaOS as a useful integration comparison. Its existing loader,
memory and APF work may be reused independently; the full OS and current
RPCMP sound stack are not mandatory. SDL is a presentation choice, not a
solution to the measured synthesis budget. No replacement public contract,
production substrate, library schema or new package is adopted here.

### Research tooling and failed attempts

Ignored artifacts live under `out/build/cpu-budget-sim-20260921`: `Dockerfile`,
`build-firmware.py`, `start.S`, `bare.c`, `bare.ld`, `sim.cpp`, `compare.py`,
ELFs, native executables, disassembly, logs and `validated-results.json`.
The Linux-only research image adds Verilator **5.020**, licensed
Artistic-2.0 or LGPL-3, to the existing cross-toolchain image. It is not a
player dependency or shipped asset. CPU generation uses the existing cached
SpinalHDL/VexiiRiscv environment; the VexiiRiscv checkout is
`580b76c3868512c8316bb7a3d3add81cad49a0dc` (MIT).

The first generation attempt encountered an absolute path embedded in the
Scala build cache; mounting the source at its original container path fixed
generation. The generated CPU emits width-truncation and constant-comparison
warnings. Their expressions were inspected and the simulator build retains
them with `-Wno-fatal`; this is not a clean RTL-lint claim. Production gates
and source are unchanged. Early startup probes trap on counter CSRs; the
exposed TIME input is unused in these generated models, so the final harness
uses the external MMIO clock instead of adding CPU performance-counter logic.

An initial multi-case run failed the three combined FM/PCM hash comparisons.
The authored probe had omitted key-off between cases: `Operator::Reset` does
not clear its `keyon_` latch, so repeated direct register test setup did not
start all voices. Those low cycle counts are rejected. Explicit key-off after
each measurement fixes the probe's case isolation without changing the
reference renderer. A fresh-process combined case independently matches the
native oracle. This is not evidence of an MDXPlayer application lifecycle bug.

Reproduction uses the research image for `python3 -B
out/build/cpu-budget-sim-20260921/build-firmware.py`, Verilator builds of the
two named CPU netlists with `sim.cpp`, and each model executing `bare-8.elf
9 0 4` for the corrected batch. `python3 -B
out/build/cpu-budget-sim-20260921/compare.py` checks recorded results against
native runs. These tests establish only the stated authored synthesis cases;
full-corpus compatibility, final timing, sound quality and hardware acceptance
remain outstanding.

## Private corpus loading and split feasibility

Further 2026-09-21 research uses the same pinned source, 48,000 Hz output,
62,500 Hz internal synthesis and filter mode 0. Source files are mounted
read-only in network-disabled Docker runs. Per-file processes have a
15-second CPU limit, 25-second wall limit and 512 MiB address-space limit.
Only aggregate evidence is committed; paths, private hashes, manifests and
test artifacts remain in ignored `out/build/mdx-corpus-reference-20260921`.
No generated private audio is saved or distributed.

### Loader and PCM evidence

An app-derived loader lookup over 13,140 loose MDX files finds 8,901 empty
PDX references, 3,703 resolved dependencies, 535 unresolved beside the MDX
and one invalid outer MDX envelope. A second same-directory case-folded
lookup resolves none of those 535. These are lookup results, not reference
playability classifications. The earlier inventory's 273 unmatched names
used a broader cross-directory/name-candidate search; the resolution rules
and count meanings differ. Do not select an unrelated PDX merely because its basename
matches. Archives and other file formats remain outside this survey.

A deterministic sample covers 24 tracks using 24 distinct PDX files. All
24 full-mix and 24 FM-muted runs produce the requested 491,520 frames
(10.24 seconds), advance playback and retain output canaries. Full mixes
are nonzero in all 24; PCM alone is nonzero in 20. Extending the other four
PCM-only runs to 20.48 seconds produces PCM in three more. The remaining
silent prefix does not establish a broken or PCM-free whole song.

The loader also finds three compressed MDX bodies and 54 compressed-PDX
references. Of these 57 cases, 56 produce nonzero 10.24-second prefixes:
two compressed MDX bodies and all 54 compressed-PDX references. The original
LZX decoder returns zero for the remaining MDX body; the pinned app's loader
would reject that result. This is not proof about the user's installed app
binary. Seven authored LZX controls pass: literal `ABC`, raw data, two
declared-size mismatches and three short headers.

The wrapper adds the app's ten-byte driver headers and uses its 64 KiB MDX /
1 MiB PDX logical allocations. Source buffers have 16 trailing zero bytes,
and inputs within 16 bytes of those allocation limits are rejected by this
research wrapper: the legacy driver copy routine transfers trailing words.
LZX decoded size must equal the declared size; a mismatch is reported as
unresolved, whereas the app checks only for nonzero. These explicit guards
do not certify the legacy decoder's memory safety or define production limits.

### Separating FM synthesis from the reference clock

Eighty tracks (the initial 24 plus 56 successfully decoded compressed cases)
run in three modes: full reference, FM-muted reference, and a research copy
with only `OPM.Mix` omitted. `OPM.SetReg`, `OPM.Count`, the driver timer,
PCM8 and downsampling remain present. All **240 runs** complete their
10.24-second prefixes; there are no comparison failures.

The modes agree on ordered FM writes and their outer-frame/internal-sample
positions, signed PCM contributions, observed PCM channel mask and playback
clock/end flag. The two PCM-only modes additionally have identical output
hashes, peaks and intact canaries. The hooks are checked against **104**
earlier unmodified-source outputs. Sampling active PCM channels after each
1,024-frame block observes up to five simultaneous voices; this is a lower
bound, not a full-song channel maximum. One rejected compressed reference
case remains explicitly outside these 80, not silently converted to success.

This establishes a useful engine split, not a hybrid output pass: the
candidate has no FPGA FM, new mixer, target memory or real-time transport.
Source observations preserve the existing 48 kHz chunk/timer behavior and
count actual internal samples; changing the driver to a nominal 62.5 kHz
output API without comparison is not equivalent.

The PCM mixer contributes signed values before saturating the FM+PCM sum.
One tested prefix reaches magnitude **33,697**, above int16. No audible-sample
difference from early PCM clipping was observed in these prefixes, but it is
not algebraically safe: FM=-10,000 and PCM=40,000 should sum to 30,000;
clipping PCM first instead gives 22,767. The proposed boundary therefore
preserves signed 32-bit PCM contributions until mixing. Stereo 62.5 kHz
int32 transport is 500,000 bytes/second by arithmetic, not a bus benchmark.

### FM burst service experiment

An additional observed-reference run over all 80 prefixes matches their
previous full-output and event/PCM hashes. Its largest nonzero-time burst
has **242 writes at internal sample 897** (about 14.35 ms): 219 distinct
registers, 206 operator writes and eight key writes. Initialization at
sample zero is counted separately. Software register updates at one logical
sample are not instantaneous physical YM2151 bus transactions.

A hypothetical serial server applied to these traces, excluding sample-zero
preload, has maximum completion lag 3,872 / 4,356 / 4,840 microseconds at
16 / 18 / 20 microseconds per write, respectively. Maximum outstanding work
is 242 writes in each model. These are finite-prefix queue calculations;
buffering preserves order but cannot remove within-burst timing differences.

A separate Questa 2025.2 experiment uses unmodified pinned JT51 `985a573`,
continuously enabled, with a research copy of the current bus adapter changed
from 3,579,545 Hz to the reference's **4 MHz**. Across 32 start offsets it
passes **3,072 issued/received writes and 2,048 native operator-bank scans**.
Back-to-back receipts are 202–203 audio-clock edges apart: about 16.44–16.52
microseconds at 12.288 MHz. The simulation counts edges; it is not a new
FPGA fit, complete command FIFO or Pocket timing result. Only the research
copy changes its clock/hold connection. Production RTL remains unchanged.

This makes ordinary ordered bus service plausible, while leaving the
acceptable FM/PCM skew and dense-burst sound comparison for the prototype.
The existing 64-entry source queue is not thereby proven sufficient for
the replacement. Do not drop writes, coalesce key/timer operations, pause
the audio timeline to drain a queue, or label a desired timestamp an actual
receipt time.

Reproduction scripts in that ignored directory are `build.py`, `survey.py`,
`lzx-controls.py`, `followups.py`, `build-split.py`, `compare-split.py` and
`schedules.py`, run with `python3 -B` in the recorded cross-toolchain Docker
image. The last four native-comparison outputs are `followups-aggregate.json`,
`split-aggregate.json`, `schedule-aggregate.json` and `lzx-controls-final.log`.
`pwsh -NoProfile -File out/build/mdx-corpus-reference-20260921/bus-budget.ps1`
produces `bus-budget.log`. No production runtime or dependency was added.

## Offline hybrid audio experiment

On 2026-09-21 the user approved implementation and the simple-development
charter. The committed runner `tools/mdx_hybrid_probe.py` and native bus adapter
`tools/mdx_hybrid_replay.cpp` connect the pinned reference to native JT51.
This is offline replay; it adds no production dependency, MMIO or FPGA package.

The runner makes two disposable reference builds. The candidate omits only
`OPM.Mix`; both retain the timer, original 48 kHz rendering chunks, PCM8 and
instrumented FM/PCM outputs. It compares their ordered sample-timestamped FM
events, signed int32 PCM and chunk records byte for byte. JT51 receives the
captured writes at a 4 MHz simulated clock, with busy polling and data held
through `cen_p1`. A generated JT51 copy adds two registered 19-bit accumulator
observation ports at the same edge as its existing `xleft`/`xright`; operator,
timer and native DAC outputs remain unchanged. Apply the reference FM limiter
(-65,536 to 65,535), Q14 gain and int16 FM saturation, then add wide PCM and
saturate the final 62,500 Hz mix. Use the reference's fast 48 kHz resampler with
the recorded chunk sizes.
Native FM advances continuously while writes wait. No hardware IRQ calls the
driver a second time; no UI participates in this path.

Results:

- Eight authored 2.048-second controls (silence, FM 1/8, loud FM 8, ADPCM 8,
  16-bit PCM 8, 8-bit PCM 8, FM 8 + ADPCM 8) and six private 10.24-second prefixes complete.
  Private selection includes two compressed MDX cases, compressed PDX, the
  previously observed 242-write burst and wide PCM/five active PCM channels.
  These are representatives, not a random or whole-corpus coverage claim.
- All 14 cases preserve candidate/reference FM events, PCM and resampler chunks.
  Recombining captured software FM and PCM reproduces the full reference output
  exactly. All six private reference outputs also match their earlier unmodified
  source results. Silence and authored PCM-only hybrid output are byte-identical
  to the reference. Native replay accounts for all 79,034 in-prefix writes and
  all requested samples. FM key-off/re-key-on controls and a repeated clean start
  pass. These are not streamed player stop/restart tests.
  All 14 original native int16 streams also match the saved unmodified-JT51
  baseline byte for byte after adding the observation ports; the pinned source
  checkouts remain clean. The fix uses the extra wide stream only at the mixer.
- Raw JT51 FM is about twice the reference amplitude in the authored controls.
  FMGEN's `SetVolume(-12)` computes `int(16384 * 10^(-12/40)) = 8211`, rather than
  treating that parameter as a conventional -12 dB amplitude setting. However,
  applying gain after native int16/DAC clipping drops the loud eight-channel
  control to **0.70863** of reference RMS. Fixing the gain/limiter order using
  the wide accumulator gives RMS ratios **0.99972** (one channel), **1.00329**
  (eight channels) and **0.99833** (loud eight channels). The loud accumulator
  reaches 211,066 before limiting. The authored check rejects deviations above
  5% to catch gross level/clipping regressions; that is a local control bound,
  not an accepted whole-corpus fidelity tolerance or waveform-identity claim.
- Preserve wide PCM until the final sum. Clipping it early changes **2,386**
  scalar samples in the authored FM/ADPCM mix. The private wide-PCM case reaches
  **33,697**; its final output saturates on 20 scalar samples, with reference FM
  and JT51 both silent in that prefix. That existing saturation is not a newly
  introduced noise failure. Click-free output or the r4 noise cause is unproven.
- The largest runtime bus delay is **4,098.25 us**, including the C++ adapter's
  transfer completion overhead. This uses native busy at a direct 4 MHz clock;
  it is separate from the earlier 12.288 MHz RTL adapter measurement. Desired
  timestamp, bus transfer and audible effect are not interchangeable. Buffering
  CPU supply cannot eliminate this serial burst delay.

Reproduce with the existing Linux image `rpcmp-cpu-budget-sim:20260921`
(the image ID recorded above), Verilator 5.020, and the two pinned clean
checkouts. Bind the repository at `/repo` and the private corpus read-only at
`/corpus`; run with networking disabled. The reference build retains its legacy
pointer-conversion warnings; this is not production decoder hardening.

```text
python3 -B tools/mdx_hybrid_probe.py --out /repo/out/build/mdx-hybrid-20260921 --build --manifest /repo/out/build/mdx-hybrid-20260921/private-inputs.json
python3 -B tests/rtl/mdx_hybrid_probe_tests.py --out /repo/out/build/mdx-hybrid-20260921
```

Omit `--manifest` for authored-only reproduction and omit `--build` after a
build when sources have not changed. A private manifest is a JSON list of
`mdx`, optional resolved `pdx`, `blocks` (1–960, default 480), and optional
`reference_sha256` from an independent prior run. Capture uses isolated child
processes, time/memory limits, bounded LZX sizes and output canaries. Inputs
remain read-only. This is a research guard, not proof that every legacy decoder
path safely handles arbitrary untrusted input. Private paths, captures, hashes
and generated stereo WAVs stay under ignored `out/build/mdx-hybrid-20260921/`.

The focused test command passes **4/4**, including malformed/truncated/oversized
event records, zero/oversized duration, endpoint handling, clean-start audio
repeatability, inconsistent resampler lengths and analytically derived fractional
resampler positions. A final replay of the bounded converter preserves all 14
reference and 14 hybrid outputs (`hybrid-resampler-final.log`). The
charter/harness Full run passes **87/87** (537.36 s, tidy 519.38 s); Core/UI
dependency tests remain enabled. Logs are `out/harness/charter-full.log`,
`hybrid-headroom.log`, `hybrid-wide.log` and `hybrid-focused.log`.
The probe compiles its C++ adapter with `-Wall -Wextra -Werror` and has a separate
clang-format check; its optional Linux/native-source checks are not in CTest.
The adapter also passes `g++ -std=c++17 -O0 -Wall -Wextra -Werror -fanalyzer`
with the generated-model include path and installed Verilator headers passed
with `-isystem` (`hybrid-analyzer-system.log`). An initial ordinary `-I` compile
failed on unused parameters in those installed headers; only that external
header classification changed. Adapter warnings and analyzer checks stayed on.

No production RTL was edited, so the unchanged production RTL regressions,
cross-build, synthesis/fit, APF/package and hardware gates were not rerun for
this offline experiment. Live target refill, queue capacity, CDC, underrun,
streamed stop/reset, full songs/loops and listening acceptance remain open.
Next work is that minimal target audio connection, not another architecture
comparison or richer UI. Research dependencies retain the earlier component
licenses (JT51 GPL-3.0-or-later; Verilator Artistic-2.0/LGPL-3.0; mixed reference
driver/sound notices below); generated binaries and copied sources are not
distributed by this change.

## Target streaming transport

The next M6 slice implements `rpcmp_hybrid_mmio.sv`, `rpcmp_hybrid_audio.sv`
and `rpcmp_hybrid_mixer.sv`, using the short HYB1 contract in the active design.
It connects full-word CPU-side requests to actual Intel dual-clock FIFOs,
native 4 MHz JT51, 62.5 kHz wide PCM mixing and externally decoded 48 kHz
stereo pins. No playback history, rich UI or previous RSM1 compatibility
machinery is in this path. An explicit end record distinguishes EOF from
starvation; Clear flushes and acknowledges both domains before restart.

Executed checks:

- `pwsh -File tools/rtl-hybrid-verify.ps1`: final transport checks pass at
  CPU/audio phases 0, 5,555 and 81,379 ps. Each checks 5,569 stereo frames,
  including irregular live refill beyond the 4,096-frame FIFO, exact fractional
  sample selection, future/dense ordered FM commands, full/retry on both FIFOs,
  malformed requests, staged-write invalidation, EOF, stop during a serial word,
  repeated starts and sticky starvation. Eight analytical mixer vectors also
  cover pre-gain FM limiting and signed-int32 PCM extrema. All simulator
  summaries have zero errors/warnings (`out/harness/hybrid-stream-final.log`).
- Four-state simulation exposed an uninitialized first native FM accumulator
  sample. Resetting the two pre-limit sums fixes it; this is not evidence that
  the r4 hardware clicks had that cause. The rebuilt offline experiment passes
  all 14 authored/private cases. Its 42 native/wide/mixed output files match
  saved pre-change hashes exactly. The focused probe tests now pass **5/5**,
  including generated-output scope and source-pin rejection
  (`hybrid-reset-regression.log`). Private identities/hashes remain local.
- `pwsh -File tools/host-verify.ps1 -Mode Full`: **87/87**, 587.42 s, including
  tidy 568.70 s and Core/UI rejection fixtures (`hybrid-stream-full.log`). The
  initial Fast run rejected the three new RTL paths; the approved slice was
  added individually, with an independent fixture retaining unknown-RTL rejection.
- Quartus 25.1std Build 1129 standalone compilation for 5CEBA4F23C8 fits at
  **1,689 ALMs, 47 RAM blocks, 5 DSP blocks** (`hybrid-synth-width-fix.log`).
  The first elaboration exposed different simulation/synthesis FIFO defaults;
  both read widths are now explicit. The scoped CDC constraints identify only
  first-stage control/status and vendor write-reset synchronizers; later stages
  remain timed. Resource fit is **not timing acceptance**: the standalone
  virtual-port experiment still violates its assumed bus I/O timing. Actual
  shell timing/CDC and pin constraints must be checked on the integrated design.

`pwsh -File tools/rtl-hybrid-verify.ps1 -VoiceOnly` also passes all three phases:
768 stereo frames and 216 writes each, with a native FM peak of 209,834.
It exercises eight real FM channels mixed with PCM values outside int16,
comparing source-domain arithmetic against decoded pins
(`hybrid-stream-voice.log`). It does not turn same-chip observation into an
independent FM waveform oracle; the separate software-reference comparison
remains necessary.

These are local-bus authored tests, not execution of the MXDRV renderer on the
target CPU. The former production shell/firmware/package inputs are unchanged,
so their cross-build and APF/package suites were not rerun for this slice.
The next connected checkpoint is actual AXI plus CPU rendering, refill budget,
whole-shell fit/timing/CDC, then a matched minimal hardware candidate. No
whole-corpus, whole-song, listening or hardware acceptance is claimed here.

## CPU renderer and actual shell

The next HYB1 connection uses the pinned MXDRV timer/sequencer and wide PCM8
output in `hybrid_renderer.cpp`, with bounded copied blocks. It omits software
FM mixing and sends the same timestamped register operations to the existing
FPGA FM path. The minimal SDK application preloads prepared files, starts only
on A, stops on B and freezes terminal rendering while audio runs.

`tools/hybrid_renderer_build.py --output out/build/hybrid-cpu-20260921` was run in
the existing network-disabled `rpcmp-cpu-budget-sim:20260921` image. The final SDK
link has 131,664 bytes text, 77,532 data and 159,288 BSS: 368,484 bytes total.
The generated owned/reference-source stack records sum to 8,208 bytes, with a
608-byte largest frame and no dynamic frames. This is section/compiler evidence,
not proof of every library call chain or arbitrary input's memory safety.

`tools/hybrid_renderer_verify.py` compares the built native renderer against the
previously captured independent streams. Three authored eight-PCM formats pass
32 blocks each, and six private inputs pass 480 blocks each (10.24 seconds).
Wide PCM, ordered/timed FM writes and native-frame positions match exactly.
The maximum observed writes in a block are 484. A zero-length PDX with a non-null
buffer initially emitted 40 extra reset writes; passing null for absent PDX
fixed that observed discrepancy without changing the reference oracle.
Prepared-input malformed-file safety and larger/corpus-wide capacity remain open.

The actual current single-issue CPU RTL also ran the application's renderer and
FIFO write loop. The experiment retained the earlier ideal independent AXI RAMs,
modeled FIFO consumption at 62.5 kHz, native FM busy at 64 chip clocks and a
16-CPU-cycle peripheral response delay. It ran short authored prefixes of each
format, then one real input with the final platform-port implementation.

| CPU experiment | Maximum render, us | Maximum feed, us | Underflows |
| --- | ---: | ---: | ---: |
| Eight ADPCM voices | 13,869 | 809 | 0 |
| Eight 16-bit PCM voices | 14,332 | 809 | 0 |
| Eight 8-bit PCM voices | 12,879 | 809 | 0 |
| Real MDX/PDX prefix | 4,487 | 880 | 0 |

Each render produces about 21.333 ms of source audio. The first two runs preceded
the added post-render queue observation; their old queue minima are not used as
render-time headroom evidence. The 8-bit run observed 535 frames after rendering;
the real-input run observed 1,066. Accepted PCM from the 16-bit and 8-bit runs
(17,333 frames each) and final real-input run (18,666) matched the independent
native PCM prefixes byte for byte. The real-input run consumed 15,626 source
frames before an explicit Stop. These are CPU/modeled-peripheral measurements,
not Pocket SDRAM/OS deadlines; short prefixes do not prove sustained whole-song
performance. Experiment sources, logs and private data remain under ignored
`out/build/hybrid-cpu-20260921` and `out/harness/hybrid-cpu-*`.

The new shell prep binds HYB1 directly into the actual AXI peripheral. The
three-phase `rtl-hybrid-verify.ps1 -PreparedTree out/build/hybrid-candidate-20260921`
passes reads before any WSTRB, held R/B responses, concurrent AW/W while B is
held, malformed masks/addresses/bursts, ordered PCM/EOF, clear and restart.
Each phase consumes 125 checked frames, with zero simulator errors/warnings.
An initial one-line dummy MIF caused a simulator `ERROR:` despite its zero error
summary; the fixture is corrected and the runner now rejects such messages.
The unchanged default player fixture is byte-identical to HEAD's extractor.

The matched HYB1 ROM/OS was built from the pinned firmware with the new reset
include. The missing `hexdump` in the image initially produced an empty MIF while
make returned success. `pocket_hybrid_images.py` rebuilds it from the boot binary
and requires exact linked-ELF/MIF/OS matching; that check passes. Four focused
tool/ROM tests cover source pin/output scope, profile provenance, MIF content and
pair rejection, plus successful/delayed/timed-out/mismatched ROM reset.

Quartus 25.1std Build 1129 compiled the full no-GPU shell with the candidate ROM
at an explicitly mapped absolute path. The fit is 10,161/18,480 ALMs (55%),
164/308 RAM blocks, 10/66 DSPs and 2/4 PLLs. Reported worst setup/hold slack is
0.638/0.097 ns. `hybrid_cdc_audit.tcl` checks all four operating corners: control
synchronizer later stages retain positive setup/hold slack, and all 48 Gray
pointer bits (including wrap bits and fitted source replicas) have complete
coverage. Maximum observed Gray-route data delay is 2.781 ns, below the 11.111 ns
fast-clock period. The inherited shell still has six unconstrained input ports
and 28 output ports, including audio/video external pins; this is not complete
platform timing sign-off. No blanket CPU/audio clock cut was introduced.

The [local hardware candidate](../development/pocket-hybrid-hardware.md) combines
the checked app/ROM/OS/bitstream and a prepared private input. Packaging checks
the mapped MIF, linked pair and app section budget, then reads back every ZIP and
directory member. Negative checks reject an existing destination, corrupted OS
and stale ELF budget before writes. The candidate's hash and the pending
Firmware 2.6 checklist are on that hardware page. No listening or hardware result
is claimed, and M6 remains open.

The connected slice's Fast gate passed 86/86. The first Full run exposed two
intentional-address dereferences in the app and an index-widening warning.
MMIO was moved behind the existing SDK platform boundary and the PCM index made
`size_t`; no warning suppression was added. The final
`pwsh -File tools/host-verify.ps1 -Mode Full` passes 87/87 in 535.99 s
(tidy 517.37 s), including architecture rejection fixtures. The optional
reference-backed renderer also passes its dedicated LLVM 22.1.8 tidy command.
Logs are `out/harness/hybrid-connected-full-final.log` and
`hybrid-renderer-tidy.log`. A negative CDC audit cuts a later synchronization
stage and is rejected; the unchanged positive audit is then rerun successfully.
The baseline `pwsh -File tools/rtl-verify.ps1` and
`pwsh -File tools/rtl-jt51-verify.ps1` regressions were run sequentially and pass
with zero simulator errors/warnings. Their logs are
`out/harness/hybrid-baseline-rtl.log` and `hybrid-baseline-jt51.log`.
No source RTL changed after the earlier streaming transport acceptance; the
actual-shell/ROM binding and application were the new connected inputs here.

## 実曲6曲の確認パッケージ（HYB1 r2）

ユーザーの指定フォルダーから、FMのみ2曲とPDXを伴う4曲を選び、曲ごとに
APFのinstance JSONを生成した。4曲のうち2曲は圧縮PDXをPCで展開している。
PDX参照名と同梱するファイルを照合し、元のMDX/PDXが変更されていないことを
SHA-256で確認した。個別の名前・ハッシュ・音声はGitに登録していない。

`out/build/hybrid-real-inputs-r2/prepare.py` を既存Dockerイメージ内で実行し、
指定フォルダーを読み取り専用でマウントした。各曲の冒頭960ブロック（20.48秒）で
参照側とFM合成を除いた側の時刻付きFM書き込み、PCM、ブロック区切りが一致した。
同じ入力で `tools/hybrid_renderer_verify.py` も通り、既存の合成3条件と実曲6条件で
最大484書き込み/ブロックだった。FMのみ2曲のPCM出力はゼロ、残る4曲では非ゼロで、
参照出力の6つのWAVはいずれも無音ではなかった。これはPC出力の検証で、実機の試聴結果ではない。
実行記録は `out/harness/hybrid-real-inputs-r2.log` にある。

`tools/pocket_hybrid_package.py --shell out/build/hybrid-candidate-20260921
--cpu out/build/hybrid-cpu-20260921
--firmware out/build/hybrid-firmware-20260921/src/firmware/os/bld/pocket
--tracks out/build/hybrid-real-inputs-r2/prepared/tracks.json
--output out/build/hybrid-real-mdx-r2` で実機用ZIPを生成し、各メンバーを読み戻した。
日本語の手順・曲目一覧と比較用WAVを同梱し、FMのみのinstanceからPDXスロットを省いた。
`python -B tests/pocket/hybrid_package_tests.py` は4件合格し、曲別スロット、PDX不一致、
重複ID、入力範囲外パス、不正な準備済みデータ・WAVを確認した。
`python -B out/harness/hybrid-real-r2-readback.py` でも、実際のZIPに6つのinstanceと
対応する全入力があること、6つのWAVが参照出力と一致すること、日本語手順の同梱、
r1との実行バイナリ一致、全メンバーの長さ・ハッシュを確認した。既存候補の保護、
不一致OSと古いELF容量記録を拒否する確認も通った。実行記録は
`out/harness/hybrid-real-r2-readback.log` にある。

初回のFast起動はPATH上のLLVM 23.1.1を検出して検証前に停止した。
インストール済みの固定版22.1.8をプロセス内のPATH先頭へ置き、
`pwsh -File tools/host-verify.ps1 -CheckSetupOnly` と
`pwsh -File tools/host-verify.ps1 -Mode Fast` が合格した（Fastは87/87、19.17秒）。
システム設定やツールの固定条件は変更していない。
続く `pwsh -File tools/host-verify.ps1 -Mode Full` も88/88で合格した
（558.51秒、静的解析540.38秒）。アーキテクチャの依存違反検出も含む。
実行記録は `out/harness/hybrid-real-r2-full.log` にある。

アプリELF・ROM/OS・音源RTL・FPGAビットストリームはr1と同一であり、クロスビルド、
RTLシミュレーション、合成は再実行していない。既存バイナリの組み合わせと検証記録を
パッケージ生成時に再確認した。実機の音・処理時間・全曲全区間の互換性は未確認で、M6は継続中。

## HYB1 r2実機報告とI2S出力の不具合

2026-09-21、ユーザーがFirmware 2.6で実曲6曲を確認した。6曲とも旋律・テンポ、
左右、B停止とAでの先頭からの再生はOK。PCMありの曲でもサンプル音を確認できた。
通常再起動と電源OFF後の再生もOK（確認に使った曲の番号は未指定）。
曲01・02は準備時にPDX参照がなく、PCM出力ゼロだったためFMのみの確認である。
曲04の「起動／A前は無音」欄にはファイル名が入り、この項目の明示的結果は未記入。
他の5曲では起動とA前の無音もOKだった。

| 曲番号 | Render max (us) | Feed max (us) | Queue minimum (frames) | 約1分の試聴結果 |
| --- | ---: | ---: | ---: | --- |
| 01 | 7643 | 460 | 1030 | 問題なし |
| 02 | 7763 | 403 | 1030 | 問題なし |
| 03 | 7587 | 435 | 1028 | 問題なし |
| 04 | 11669 | 452 | 808 | 問題なし |
| 05 | 14539 | 451 | 945 | 一部のサンプル末尾でプチプチ音 |
| 06 | 12666 | 448 | 960 | アタックのピークでびりびり音。音切れやテンポ乱れはなし |

いずれもエラー表示なし。1ブロック約21.33msに対し観測最大renderは14.539ms、
観測最小キュー808フレームは62.5kHz換算で約12.93msであり、この報告は単純な
供給不足を示していない。ただしキューは定期観測で、全区間の最小値保証ではない。
ユーザーは追加確認で「05・06のノイズはPocketだけで聞こえる」と回答した。
元データ側の問題として処理せず、実機固有の不具合として調査した。

[公式APF AUDIO](https://www.analogue.co/developer/docs/bus-communication) はLRCK切り替えから
1 SCLK後にMSBを出すI2Sを要求する。r2の `rpcmp_hybrid_audio.sv` は待ち時間がなく、
テスト側も同じ位置から読んでいた。受信側だけを公式タイミングに直した
`pwsh -File tools/rtl-hybrid-verify.ps1 -PhasePs 0` は、送信値16383と1を32766と2として
受信して失敗した（`out/harness/hybrid-i2s-red.log`）。期待する音声値は変更していない。
この1ビット左ずれは、正側16384以上・負側-16384未満で符号を反転させる。

`python -B out/harness/hybrid-i2s-reference-peaks.py` で比較WAVの冒頭20.48秒を調べた。
曲01〜04の最大絶対値は2817／2670／15345／15786で、この符号反転域に入らない。
曲05は最大20073で36チャンネルサンプル（最初は約3.677秒）、曲06は最大32768で
9063チャンネルサンプル（最初は約0.066秒）が該当した。これはPC参照波形の計数で、
Pocket録音との一致証明ではないが、報告された曲と症状を説明する具体的な根拠となる。

修正はDACのビット出力を4 MCLK遅らせるだけで、CPUレンダラー・PCMの音量・曲データ・
48kHzフレーム周期は変更しない。テストは外部ピンから受信し、両符号の16384付近、
フルスケール、最下位ビット、飽和を含む768フレームを追加した。
修正版の実機でノイズが消えることを確認するまでは、症状の解消を確定しない。

### HYB1 r3の出力・配置・パッケージ検証

`pwsh -File tools/rtl-hybrid-verify.ps1` は3位相（0／5555／81379ps）で合格した。
各位相で符号境界768フレーム、通常動作5569フレーム、停止・EOF・飢餓検出、
native FM＋wide PCMの768フレーム／216書き込みを確認した。ミキサー8条件も合格した。
ログは `out/harness/hybrid-i2s-green.log`。さらに
`pwsh -File tools/rtl-hybrid-verify.ps1 -PreparedTree out/build/hybrid-candidate-r3` で
実際のCPU AXI接続を3位相、各125フレーム／295書き込みで確認した
（`out/harness/hybrid-r3-axi.log`）。いずれもシミュレーターのErrors／Warningsは0。
`pwsh -File tools/rtl-verify.ps1`、続けて `pwsh -File tools/rtl-jt51-verify.ps1` も合格した。
ログは `out/harness/hybrid-r3-baseline-rtl.log` と `hybrid-r3-baseline-jt51.log`。

`tools/pocket_hybrid_prepare.py` で同じ固定済みopenfpgaOS／JT51／CPU netlistと
既存ROM/OSから `out/build/hybrid-candidate-r3` を作成した。その `hybrid-fit` で
Quartus 25.1stdの `quartus_sh --flow compile ap_core` が合格した（7分6秒、
0 errors／968 warnings）。使用量は10,064/18,480 ALMs、15,398 registers、
1,190,249 memory bits、10 DSP blocks。4条件の最小setup slackは0.689ns、
holdは0.130nsだった。既存基盤の外部入力6端子・出力28端子は未制約のままで、
プラットフォーム全体のタイミング完了を意味しない。

`quartus_sta -t F:/source/rpcmp/tools/hybrid_cdc_audit.tcl` は最初、
配置ツールが `status_sync[4]` を複製したため、旧スクリプトの最大6レジスタ条件で停止した。
配置後の経路を調べ、6つの論理ビットと追加の複製先1つを確認した。監査を各ビット単位にし、
元の経路と複製先のsetup／holdをすべて要求するよう修正した。総数の上限だけを広げず、
欠落ビットを検出する条件を保っている。この監査修正に伴う音声回路やSDCの例外指定の変更はない。
`python -B out/harness/hybrid-cdc-negative.py` でビット欠落と複製先の負slackを注入し、
両方の拒否を確認した。再度実回路を監査し、4条件で同期経路と全48 Gray-pointer bitが
合格した。Gray経路の最大遅延は2.482ns（上限11.111ns）。実記録は
`out/build/hybrid-candidate-r3/hybrid-fit/hybrid-cdc-audit.log` と `hybrid-cdc-paths.txt`。

`python -B tools/pocket_hybrid_package.py --shell out/build/hybrid-candidate-r3
--cpu out/build/hybrid-cpu-20260921
--firmware out/build/hybrid-firmware-20260921/src/firmware/os/bld/pocket
--tracks out/build/hybrid-real-inputs-r2/prepared/tracks.json
--output out/build/hybrid-real-mdx-r3` でr3を生成した。
`python -B out/harness/hybrid-real-r3-readback.py` は全メンバーとハッシュを読み戻し、
新しいRBFのビット反転、6曲の入力・比較WAV、framework最低版2.2、日本語手順を確認した。
r2との違いはFPGA画像・core版番号・手順だけで、r2 ZIPも保存時のハッシュと一致した。
CPUアプリ・ROM/OS・準備済み曲は変更せず、クロスビルドやnative参照生成は再実行していない。
ROM/OSの対応とアプリ容量はパッケージ生成時に再確認した。実機のノイズ解消は未確認。

固定LLVM 22.1.8をプロセス内PATHで選び、
`pwsh -File tools/host-verify.ps1 -Mode Fast` は87/87、33.54秒で合格した。
監査スクリプト修正後の最終 `pwsh -File tools/host-verify.ps1 -Mode Full` は
88/88、534.89秒（静的解析517.14秒）で合格した。整形とCore/UI依存境界の
正負チェックも含む。ログは `out/harness/hybrid-r3-fast.log` と
`out/harness/hybrid-r3-final-full.log`。その後の結果・進行リンクだけの更新には
`python -B tools/check_harness.py` と `git diff --check` を用いる。

### HYB1 r3 hardware follow-up

2026-09-21、r3の再確認依頼に対し、ユーザーは曲05・06の前回のノイズがともに
「消えた」と報告した。両曲とも起動とAを押す前の無音、比較WAVとの旋律・テンポ、
左右とPCMサンプル、約1分の再生での音切れ・プチプチ音・テンポ乱れ、B停止と
Aでの先頭からの再生が問題なしだった。PCMありの1曲で通常再起動と電源OFF後の
起動・再生も問題なし（その確認に使った曲番号は未指定）。曲05はエラーや気になる表示も
「特になし」と報告された。今回の回答ではFirmwareとcore版番号の再記入はなく、
直前のFirmware 2.6／HYB1 r3確認依頼への結果として記録する。

| 曲番号 | Render max (us) | Feed max (us) | Queue minimum (frames) | 前回のノイズ |
| --- | ---: | ---: | ---: | --- |
| 05 | 13144 | 500 | 943 | 消失 |
| 06 | 12619 | 445 | 956 | 消失 |

曲06の数値欄は見出しがRender／Feedの2項目で値が3つあり、従来テンプレートの
順序に従って3つ目の956framesをQueue minimumとして記録した。
今回の観測最小キュー943フレームは62.5kHz換算で約15.09ms。最大renderは13.144ms、
最大feedは0.500msだった。いずれも報告された区間の観測値であり、全瞬間・全曲の
最悪値やUI・ストレージ処理を追加した場合の余裕を保証するものではない。

受信仕様からの失敗再現、出力だけを変えた候補、同じ2曲での症状消失がそろったため、
報告されたピーク時ノイズの修正は実機確認済みとする。新しい回路変更や追加候補は作らない。
FMのみの01／02のr3再確認は今回未報告であり、r2の確認結果をr3の結果として扱わない。
全曲全区間の互換性、外部端子のタイミング制約、UIやストレージとの同時動作、
M6全体の受け入れは未完了。次は既存の計画どおり、私有コーパスの参照比較を広げる。

この更新はユーザー実機結果と進行リンクの追記のみ。
`python -B tools/check_harness.py` と `git diff --check` で文書を検査する。
製品コード・RTL・検証手順・生成入力は変更せず、Full、RTL、クロスビルド、合成、
パッケージ生成は再実行しない。前節の実行済み結果と今回のユーザー報告を区別する。

## Dependency conditions

The [MDXPlayer README](https://github.com/asaday/MDXPlayer/blob/4076b91c7ced57bf6047f69b87c12a34bd99a438/README.md)
distinguishes the BSD application from the GAMDX decoder's terms.
[GAMDX's author](https://gorry.haun.org/android/gamdx/) further distinguishes
GORRY-authored Apache-2.0 files, original MXDRV-derived material and sound-library
components. The pinned MDXPlayer also bundles FMGEN's own notice. Do not label
the entire decoder BSD or Apache-2.0 based on the application license.
The local hardware experiment now builds those sources in ignored output, with
component notices retained in its private package. Public redistribution still
needs component-specific review; it is not authorized by the app's BSD badge.
