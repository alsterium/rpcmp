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

1. Make the pinned bundled decoder runnable headlessly as a private comparison
   tool. Preserve timer, FM and PCM behavior; first check authored fixtures
   and representative user-confirmed files. Record any portability patches.
   Keep per-file time/memory limits around the legacy decoder.
2. Establish a private per-file reference manifest: dependencies, actual
   playback result, loops/end, sample use and unresolved cases. Compare FM
   event timing and PCM operations/audio, then extend to the whole collection.
3. Propose the engine/device contract from that evidence. Evaluate the existing
   openfpgaOS/JT51 substrate with a minimal selection/play/stop display and PCM;
   measure CPU work, transfer/buffer margins and FPGA resources before selecting
   the production integration. Isolate the existing clicks with identical
   source data and rendering disabled/enabled.

No RPCMP implementation change, all-file reference playback, PCM target
benchmark or new hardware pass is claimed here.

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

## Dependency conditions

The [MDXPlayer README](https://github.com/asaday/MDXPlayer/blob/4076b91c7ced57bf6047f69b87c12a34bd99a438/README.md)
distinguishes the BSD application from the GAMDX decoder's terms.
[GAMDX's author](https://gorry.haun.org/android/gamdx/) further distinguishes
GORRY-authored Apache-2.0 files, original MXDRV-derived material and sound-library
components. The pinned MDXPlayer also bundles FMGEN's own notice. Do not label
the entire decoder BSD or Apache-2.0 based on the application license.
Direct redistribution/integration needs component-specific review; reading
the ignored research checkout has not added a distributed dependency.
