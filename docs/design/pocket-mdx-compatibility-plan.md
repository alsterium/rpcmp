# MDX-compatible Pocket player design proposal

Status: prototype direction approved by the user, 2026-09-21. The user also
waived backward compatibility with earlier RPCMP implementations and requested
the simplest practical implementation under the [project charter](../../AGENTS.md#project-charter-2026-09-21).
The user subsequently accepted the six-song hardware verification and closed
the current compatibility investigation on the same date. Evidence and
reproduction details are in the [compatibility baseline](../research/mdxplayer-compatibility.md).
The [current acceptance and next MVP step](#current-acceptance-and-next-mvp-step)
supersede the earlier requirement to extend verification across the collection
before proceeding.

## Current acceptance and next MVP step

The user reports that all six songs play normally, continued playback remains
stable so far, and song endings and loops work satisfactorily. The current MDX
compatibility verification is accepted and complete. Continue with CPU MXDRV/
PCM8 plus FPGA FM; diagnose and fix engine defects as they are encountered.
Keep MDXPlayer-compatible FM/PCM playback as the product target. This decision
does not claim that every collection file or waveform has been compared.

The minimal M6 slice now has a Japanese-capable track list, selection, play and
stop using the accepted audio path. The user's Firmware 2.6 / Minimal Player r1
[hardware report](../milestones/M6-album-player.md#minimal-player-r1-hardware-result--2026-09-21)
confirms these controls, startup silence, six-song FM/PCM stereo playback,
responsive browsing during playback and restart/power-cycle recovery.
The prepared-input player below is the working baseline. The user approved
M3U as the PC-side authoring input: resolve MDX/PDX on the PC and generate the
Pocket-specific collection, as defined below. Pocket continues reading HPL1.
The subsequent [27-track hardware report](../milestones/M6-album-player.md#m3u-27-track-hardware-result--2026-09-21)
accepts this connected workflow: 27 imported, zero excluded, correct order and
Japanese titles, FM/PCM stereo playback and selection/stop controls, with no
reported playback or browsing problems. The user has now selected and approved
the [continuous-playback boundary](#continuous-playback-boundary) below as the
next slice. Minimal Player r2 now implements it; see the
[implementation evidence](../milestones/M6-album-player.md#continuous-playback-implementation--2026-09-21).
Its [hardware verification](../development/pocket-minimal-player.md) remains pending.
Tracker/keyboard expansion, shuffle and persistent settings remain deferred.
Keep relevant input bounds, focused regressions and integration checks; do not
restart a broad compatibility campaign as a prerequisite for this next step.

### Minimal player implementation boundary

This section records the accepted r1 baseline. The continuous-playback boundary
below supersedes its automatic-next, loop/end and per-track error behavior for
the r2 implementation; the input format and safety bounds remain applicable.

Implement the existing prepared-track workflow first: package 1–300 prepared
MDX/PDX pairs into one deferred APF slot, retain only its catalog in memory,
and read only the chosen pair while audio is stopped. This removes the need
to exit the core between songs. The PC M3U import boundary below now feeds this
same prepared format; Pocket does not need a direct M3U/raw-file adapter.
The prepared inputs retain the tested driver wrappers and LZX
preparation. Do not accept arbitrary raw MDX as this prepared format.

The small HPL1 file is little-endian: a 32-byte header contains magic `HPL1`,
version 1, entry size 128, count (1–300), total bytes (at most 512 MiB), index
CRC32, CRC32 of header bytes 0–23, and a zero reserved word. Each 128-byte entry
contains MDX offset/length/CRC32, PDX offset/length/CRC32, UTF-8 title length
(1–96), 96 zero-padded title bytes, and a zero reserved word. IDs are the
one-based entry ordinals. Payloads follow the index in entry order, MDX then
optional PDX, with no gaps; absent PDX fields are zero. A selected pair is at
most 16 MiB combined; each present blob is at least ten bytes. Validate the
complete index, bounds, UTF-8 and checksums before exposing titles or decoding
a selected pair. CRC32 uses the existing IEEE implementation. The loader alone
knows offsets; playback/UI use track IDs and copied metadata. Free source
buffers after the reference renderer copies them, and add its sixteen guard
bytes before opening. Corrupt data produces a visible, retryable error after
silencing; a reset failure is terminal.

Use a small versioned snapshot and `PlayerCommand` (PlayTrack/Stop), an injected
playback port and copied catalog metadata. Up/down select, left/right page,
A starts the selected song from its beginning, B stops; B wins simultaneous
A/B. Browsing does not stop the current song. No autoplay or automatic next
song: natural end stops, and the accepted engine's existing loop/end behavior
is unchanged. Keep physical mapping in the UI/platform boundary.

Use the existing licensed Japanese bitmap font with a simple list and status.
Draw only when selection/transport changes, in bounded scanline pieces between
audio service calls, and never wait for vsync. Keep rendering independent of
the audio timeline; collect render/feed/draw/flip maxima and queue minimum for
hardware verification. The r1 slice includes no tracker, keyboard, save or
playback-policy expansion.

Full is due when list selection connects through loading to HYB1 playback,
stop and reselection. Verify malformed catalogs, missing/corrupt payloads,
silence/error recovery, navigation, headless/delayed-draw equivalence, target
link/memory/stack, and package readback. Reuse the unchanged r3 FPGA/ROM/OS;
rerun affected transport integration checks, without resynthesizing identical
RTL. Provide a Japanese procedure and the same six private songs locally.

### Continuous playback boundary

Approved on 2026-09-21 through the user's continuous-playback Q1–Q4 answers
and the previously agreed loop/fade semantics. This is the r2 implementation
boundary on the accepted r1/HPL1 baseline; r1 itself does not implement it.

- Launch remains silent until A starts the selected entry from its beginning.
  Continue through subsequent HPL1 entry IDs in imported M3U order; duplicate
  entries remain separate positions. Stop after the last entry, without wrapping.
- A during playback starts the browsing selection from its beginning and
  continues from that entry in M3U order. B stops and cancels automatic advance,
  including pending error skips. The next A starts a new run from the selection.
  Keep the existing B-over-A priority for simultaneous input.
- A looping song plays its intro once and its loop body twice, then fades both
  FM and PCM to silence over five seconds before advancing. For an intro of
  10 seconds and a loop body of 30 seconds, fade starts at 70 seconds and ends
  at 75 seconds. A naturally ending song plays once and advances on completion;
  it is not restarted to reach a loop count or padded with a five-second wait.
- A recoverable per-track loading or playback failure silences/resets the old
  playback and advances to the next entry. Do not retry that entry or wrap in
  the same run. Consecutive failures, including all remaining entries failing,
  terminate at the list end. Preserve failure diagnostics instead of reporting
  failed tracks as successfully played. An invalid whole catalog or a terminal
  reset failure retains the existing stop/error behavior: playback cannot
  safely continue in those cases.
- Automatic advance and error skipping preserve the browsing cursor and scroll
  position. Show the playing entry with a separate marker derived from the Core
  snapshot. Merely browsing never changes the playback sequence; A does.

Core owns advancement and bounded recovery through the existing playback port.
UI submits PlayerCommand and reads copied/versioned snapshots; it owns only
browsing and presentation. Loop/fade/end timing follows audio progress, independent
of rendering or snapshot cadence. Finish silencing the old track before loading
the next pair; seamless/gapless loading is not an acceptance requirement. Keep
the PC import workflow and HPL1 input boundary. Configurable loop counts,
repeat-one, shuffle, pause, extra views and saved settings remain deferred.

The Full checkpoint is the connected behavior **select -> play -> natural end
or two-loop fade -> next entry -> list-end stop**, including recoverable-error
skipping and B cancellation. Run focused host checks then Fast during iteration;
run Full when this behavior works, with affected target link/memory, audio
integration and package checks before hardware submission. RTL/synthesis checks
apply when their inputs or behavior change. Host checks do not establish Pocket
acceptance. User-facing verification instructions and report templates are Japanese.

Implementation contract: snapshot version 2 adds Advancing and the last skipped
entry/error/count. Core attempts at most one new entry per service call, so Stop
can cancel a failure chain. A new PlayTrack clears the previous run's diagnostics.
The renderer observes reference loop/end state at native PCM chunk boundaries,
emits one timed fade marker after loop body 2 and trims output at its end. HYB2
(MMIO ID `0x48594232`) adds operation `0x10001` for that marker to the existing
17-bit event payload; `0x10000` still denotes EOF. Other control values, repeated
fade markers and overflowing fade start times are rejected. HYB2 applies the
same linear gain to the saturated stereo mix for 312500 native frames (five
seconds at 62500 Hz), then reaches zero. Gain at native offset n is
`max(312500 - n, 0) / 312500`, with signed division toward zero. An earlier
natural EOF ends normally. Clear resets the fade. CPU and boot ROM require
HYB2, so a mismatched old FPGA fails explicitly. HPL1 is unchanged.

実装後の確認項目（ホスト試験の結果は実装証跡を参照。r2実機確認は未実施）:

1. 起動直後は無音。途中の曲をAで選ぶと、その曲からM3U順に進み、末尾で停止する。
2. ループ曲はイントロ1回＋ループ部分2周の後、FM・PCMとも5秒でフェードする。
   自然に終わる曲は1回で次曲へ進む。ループ数と時間は表示更新回数で決めない。
3. 再生中にAで曲を切り替えると、その曲から順に進む。B停止後は次曲へ進まない。
   曲末尾・フェード終了・エラー送りと操作が重なった場合も確認する。
4. 読み込み失敗・再生失敗の曲を飛ばす。連続失敗・残り全曲失敗・末尾曲の失敗で
   先頭へ戻らず停止し、エラー送り中もBで止められる。異常な曲集やリセット失敗は停止する。
5. 自動送り時も一覧のカーソルとスクロール位置を保ち、再生中の印だけが変わる。
   描画を遅延・無効化しても曲順、ループ終了、フェード時間が変わらない。
6. 実機でFM・PCM・左右の音、一覧操作中の音切れ・ノイズ・入力遅延、停止・再選曲、
   通常再起動・電源OFF後を確認する。使用曲と観測結果はホスト試験の結果と分けて記録する。

### M3U import boundary

The approved connected behavior is **M3U -> prepared MDX/PDX -> HPL1 -> the
existing Pocket reader**. Implement a Python standard-library CLI; no Pocket,
engine, firmware or RTL change is required. Full is due when that round trip
works. Check authored malformed/boundary cases and byte equality with the same
six already accepted prepared pairs; do not reopen the compatibility campaign.

Accept UTF-8 (optional BOM) `.m3u`/`.m3u8`, with explicit CP932 input selection
for legacy lists. Ignore blank/comment lines, including EXTINF; preserve MDX
entry order and duplicates. Limit the list to 1 MiB and 300 entries. Resolve
local paths relative to the playlist; `--root` defaults to its directory and
confines all music reads, including symlinks. An explicit wider root permits
parent-relative or absolute paths within it. Reject network URLs and devices.
Treat slash/backslash as separators and reject ambiguous case-insensitive
matches instead of selecting an arbitrary file.

Decode the MDX outer title and PDX name as CP932. Normalize title whitespace;
use the filename when the title is empty or undecodable. PDX is a dependency,
not a separate playlist entry. Search the MDX directory first, then explicitly
supplied PDX directories under the root, with case-insensitive names and the
optional `.pdx` suffix. Missing/ambiguous dependencies exclude that track.
Bound raw reads and LZX expansion; validate MDX header/offset structure and PDX
sample ranges before wrapping. This structural import check is not a proof of
all sequence commands or every possible runtime behavior.

Use the unchanged ten-byte driver wrappers and HPL1 limits (16 MiB per pair,
512 MiB collection, 96 UTF-8 title bytes). The PC decoder handles the reference
LZX token format with explicit read, back-reference and output bounds; it adds
no native runtime dependency. Format provenance is the pinned MDXPlayer
`4076b91c7ced57bf6047f69b87c12a34bd99a438` `classes/objc/lzx042.c` and
`Player.m` loader already used by the accepted preparation. This Python
implementation does not vendor or link that C decoder. Hand-encoded literal,
short/long match, overlap and terminator cases provide independent expected
bytes; compare accepted real inputs with the earlier native-decoder output.

Write a fresh output directory containing an SD-layout `playlist.hpl`, Japanese
copy instructions and a per-entry import/exclusion report. A data-only ZIP is
convenient for the same copy operation. A partial import succeeds with visible
exclusions; an empty result writes its report, returns failure and creates no
installable collection. Invalid whole-list encoding/count/size is a fatal error.
Existing outputs and source music are never overwritten. The user copies the
generated Assets directory over the installed r1 collection while the core is
closed, then opens the existing Playlist.json. The original six-song package
remains available for recovery. This slice manages one installed collection;
album navigation and multiple named collections are subsequent work.

## Recommendation

Use the MDXPlayer-derived MXDRV interpreter and PCM8 behavior on a CPU, a
4 MHz YM2151-compatible FM implementation in FPGA, and a small timed audio
transport/mixer. Start with a headless select/play/stop engine and add a
minimal Japanese-capable track list. Choose retained code for measured value,
not because the current player already contains it.

| Candidate | Evidence and decision |
| --- | --- |
| Unchanged software FM+PCM on current CPU | Reject as the default: the authored eight-FM/eight-ADPCM workload needs about 172 million cycles/audio second against a 90 MHz single-issue CPU, with optimistic memory and no UI/OS. |
| Same renderer on the investigated dual-issue CPU | Still about 131 million cycles/audio second before integration. A different renderer/CPU may improve this, but this candidate does not justify full software adoption. |
| Reference interpreter + software PCM8 + FPGA FM | Continue. Tested PCM-only paths need roughly 52–58 million cycles/audio second on the current CPU; the 80-track split preserves reference event timing and PCM output. Six-song hardware playback is now accepted; measure added UI/storage interference during player integration. |
| Hardware PCM8 from the start | Defer. Software retains the reference sample formats, filters and channel semantics; accelerate a measured bottleneck only if the composed budget fails. |
| Recreate an X68000 CPU/OS or continue expanding the custom MDX subset | Adds a second compatibility implementation while the selected reference already interprets the required features. Keep a reference-based route first. |
| Pre-render music on the PC | Simplifies playback but changes the raw MDX/PDX workflow and the PRD's non-conversion goal; not the default solution to the current request. |

FPGA FM is not the dominant cost of the existing sound integration. Historical
fit estimates put JT51 itself at about 1,186 ALMs, while the inclusive sound
MMIO hierarchy needs about 7,683. These overlap; they are not guaranteed
removal savings. Simplify scheduling/control around FM rather than assuming
that deleting FM buys enough CPU throughput.

## Proposed boundaries

```text
PC: M3U -> bounded MDX + resolved PDX -> HPL1 collection
Pocket: HPL1 -> selected prepared MDX/PDX -> reference interpreter
                                                   |             |
                                      timed FM register writes   PCM8 software
                                                   |             |
                                                FPGA FM     wide PCM samples
                                                   \             /
                                             common-time mixer
                                                     |
                                             48 kHz stereo output

input -> replaceable mapping -> PlayerCommand -> Core -> snapshots -> small UI
```

The catalog line uses the adopted PC-side M3U importer and existing HPL1 reader.
IDs and logical blobs remain the consumer boundary. Core never depends on
UI, and delaying or disabling rendering must leave the audio event sequence
unchanged. Avoid connecting snapshots, input interpretation or drawing to
the synthesis/interrupt path.

Keep the reference driver timer as the sequencing authority and preserve its
tested chunk behavior. A shared integer audio timeline orders FM events and
PCM contributions. The hardware timer must not independently tick the CPU
driver a second time; timer/CSM/LFO/noise behavior needs explicit comparison.
Use 4 MHz FM for this reference profile. The current 3,579,545 Hz adapter
is not silently retuned under its existing contract.

Transport PCM contributions with headroom, proposed as signed int32 stereo
at the reference's 62.5 kHz internal rate. Match the FM/PCM level relationship,
combine before final saturation, then resample to the Pocket's
[fixed 48 kHz stereo output](https://www.analogue.co/developer/docs/bus-communication).
Source reference volume settings are FM -12 dB, PCM 0 dB and
global volume 256; a hardware FM gain must be calibrated rather than assuming
equal numeric amplitudes. The exact resampler and MMIO layout belong to the
prototype contract, not this high-level choice.

FM writes enter an ordered, bounded timed queue; PCM uses bounded buffering.
Specify copy/ownership, start/preload, generation changes, stop/reset, overflow,
underrun and CDC before implementing a production adapter. While playing,
audio time must continue independently of rendering and CPU submission.
An underrun is an explicit failed playback state with safe output, not an
unreported stretched note. Exact pause and the previous rich fade/progress
machinery remain deferred; the next continuous-playback slice requires only
the fixed two-loop, five-second fade described above.

The largest observed FM burst needs 242 writes. A 20 us/write model drains it
within 4.84 ms, and the separate bus experiment measures shorter back-to-back
service for its authored cases. This motivates a prototype queue with explicit
headroom, not an all-file queue bound. Desired event time, actual bus receipt
and output position must remain distinguishable. Prebuffering removes CPU
jitter; it cannot make serial chip writes simultaneous. Do not claim waveform
identity with FMGEN or choose a timing tolerance without listening/comparison
evidence. If bit-identical MDXPlayer audio becomes a requirement, reopen the
FM implementation choice.

## Platform and library

Use the existing openfpgaOS integration first as the measured comparison host.
Retain its APF boot, storage, memory and input facilities where they reduce
work. The complete OS, current framebuffer renderer and RPCMP sound-control
stack are not mandatory. Test audio with rendering disabled, then with a
bounded dirty-region list UI. SDL by itself does not change the synthesis
cycle budget. A platform rewrite is justified by a measured residual cost,
not needed before the first complete audio comparison.

Use UTF-8 M3U8 plus unchanged source MDX/PDX files on the PC. The user confirmed
that this playlist generates Pocket-specific data, rather than requiring the
Pocket to parse M3U or scan raw music folders. The [import boundary](#m3u-import-boundary)
preserves entry order, resolves dependencies and writes HPL1. Only the selected
prepared pair is loaded on Pocket. Album navigation is not added by this step.

The user's [HarpMudd example](https://github.com/harpmudd/HarpMudd.mp3player#playlists)
supports a plain line-per-track list with paths relative to the playlist.
That is useful workflow evidence, not a reason to inherit its 256-track /
12 KB limits or assume its MP3/FLAC decoder budget applies to FM synthesis.

Relative paths, Japanese encoding, MDX-title fallback, missing/ambiguous PDX
reporting and LZX bounds are defined in the import boundary. Explicit shared
PDX directories are supported; per-track dependency overrides are deferred.
M3U lists MDX tracks, and the PC importer resolves their PDX. HPL1 and the
Pocket consumer boundary stay unchanged; no new database or container is added.

## Next prototype and decision gates

### First connected experiment

The first implementation is an offline headless replay, not a new runtime API:
capture ordered `(internal_sample, register, value)` FM writes, signed int32
stereo PCM contributions and the original resampler chunk boundaries from the
pinned reference. Replay FM through pinned JT51 at 4 MHz. Observe its 19-bit
accumulator alongside the unchanged native output; apply the reference's 17-bit
FM limiter, Q14 gain and int16 FM limit before adding wide PCM and saturating
the mix. Then use the reference's 48 kHz resampler on the 62.5 kHz timeline.
Keep the driver timer in the reference process; JT51 IRQ never ticks it again.
Measure actual bus delay and audio levels instead of asserting waveform identity.
Keep all private captures and generated audio under ignored `out/`.

Use a small standalone runner with authored silence/FM/PCM/mixed controls and
representative private prefixes (including dense bursts and wide PCM). Check
event consumption, repeatability, unchanged PCM-only output and exact offline
reconstruction of the software oracle. No new MMIO, queue protocol, UI, catalog
or compatibility layer is needed for this first experiment. Its completion
triggers Full plus the native JT51 simulation checks. CPU refill, bounded target
queues, stop/reset during streaming, CDC and hardware acceptance follow in the
target connection; preloaded offline replay cannot establish those properties.

This experiment is implemented in `tools/mdx_hybrid_probe.py` and
`tools/mdx_hybrid_replay.cpp`. Its eight authored cases and six private prefixes
pass; [results and reproduction](../research/mdxplayer-compatibility.md#offline-hybrid-audio-experiment)
record FM gain, wide mixing and serial bus delay. Next implement the smallest
target streaming connection, keeping select/play/stop and explicit underrun
handling; do not rebuild the historical completion/history/policy stack first.

### Target streaming connection

Implement a distinct HYB1 MMIO owner in the existing CPU/audio domains. Keep
two bounded vendor dual-clock FIFOs: 4,096 signed int32 stereo PCM frames and
1,024 timed FM writes. The CPU stages a left PCM word then commits with the
right word, or stages a 32-bit internal-sample timestamp then commits an FM
address/value word. Full/invalid writes fail without losing staged data; the
CPU polls available space. Only aligned full-word MMIO accesses are accepted.
The identifier prevents old firmware from treating this as the RSM1 protocol.

Before publishing PCM for a render chunk, enqueue every FM event preceding
that chunk's end. The two FIFO write domains and read domains are shared, and
their synchronizer depths match. They are nevertheless separate CDC paths,
not an atomic publication: the producer must retain refill headroom and the
audio owner continuously consumes due FM writes. Measure lateness rather than
assuming simultaneous visibility. FM bus busy causes measurable write delay,
never a pause of native/audio time. The reference timer remains the sole CPU sequencer.
Start requires prefilled PCM; streaming starvation latches a fault and mutes
output until Clear. Counter exhaustion also fails closed.

Clear is Stop plus FIFO/device reset. It is acknowledged across clock domains
before accepting a new stream, with sufficient native reset time; CPU stages
are invalidated. Start and Clear use explicit state and synchronized control
levels, without the previous progress/history/policy mailboxes. Reset both
domains from the platform reset, synchronize release, and keep audio clock and
serializer phase continuous during ordinary stops. Do not advertise pause.

The prototype's single-word local bus uses offsets within the existing
`0x40000400..0x400007ff` AXI region; offsets above `0xff` reject rather than alias.
Reads and writes require full byte enables. Rejected commits preserve staged
values, and accepted commits consume them. The MMIO mapping is:

| Offset | Access | Meaning |
|---|---|---|
| `00` | R | `0x48594231` (HYB1) |
| `04` | R | bit 0 control-ready, 1 start accepted, 2 running, 3 ended, 7:4 faults |
| `08` | W | 1 Clear/Stop, 2 Start; other values reject |
| `10`, `14` | W | PCM left stage, right commit (signed int32) |
| `18` | R | PCM free frames, including an unambiguous zero when full |
| `20`, `24` | W | source-sample timestamp stage, FM operation commit |
| `28` | R | FM free records |

FM operation bits 15:8 hold address and 7:0 value; bits 31:17 are zero.
Bit 16 instead denotes an end marker and requires bits 15:0 zero. Timestamps
must be nondecreasing, including the end marker, after which no FM records
are accepted. Publish that marker before the final PCM chunk. At its sample
position the audio owner stops generating and drains already selected output
before reporting Ended; ordinary FIFO exhaustion remains an error. A new Start
requires Clear, even after Ended/fault. There is no implicit autoplay/retry.
Fault bits currently identify PCM starvation (bit 4), output-buffer timing
(bit 5), reserved (bit 6), and counter exhaustion (bit 7).

Clear holds native reset for at least 4,096 audio clocks and waits another
16 clocks after releasing the vendor FIFO resets before acknowledging readiness.
FIFO reset release is synchronized in both domains. The native accumulator's
two pre-limit sums are explicitly reset: four-state simulation found that the
first sample otherwise contains uninitialized data, invisible in the earlier
two-state replay. Wide taps remain needed for pre-clip FM gain. This is an
initial-state correction to pinned JT51, not a proven cause of the r4 clicks.

The audio clock owns 4 MHz FM, the 62.5 kHz PCM cursor, gain/limiting and fixed
48 kHz serialization. A start resets only media state and resampler selection;
PCM and FM sample indices share that start. The source selection must implement
the reference's `floor(output_index * 125 / 96)` sequence, with a fixed output
latency, rather than reusing a converter with a different initial phase.
The external serializer must send the signed word's MSB one SCLK (four MCLK
cycles) after each LRCK transition, as required by
[APF AUDIO](https://www.analogue.co/developer/docs/bus-communication).
The r2 serializer and its test receiver incorrectly used zero delay. Correct
the receiver from the external protocol before changing the sender, and cover
both signs around 16384, full scale and the low bit. Correct signed PCM decoded
at the external pins is this fix's connected Full checkpoint; rerun the HYB1
RTL, full-shell fit/timing/CDC and matched-package checks before hardware handoff.
Keep the CPU renderer and musical data unchanged to isolate the output fix.

Use the existing native model and offline captures to verify the composed
audio stream, independent CPU/audio phases, full/backpressure, clear during
traffic, repeated starts and explicit starvation. The connected CPU-write to
I2S behavior triggers Full plus affected RTL/cross-build and fit/timing gates.
Then connect the actual split CPU renderer and measure worst refill latency,
before preparing the minimal hardware candidate. Queue sizes are prototype
choices to test, not whole-corpus capacity proofs.

The next connection replaces the sound owner in a freshly prepared no-GPU
openfpgaOS shell, retaining its verified single-beat AXI/APF/framebuffer wiring.
Use the existing pinned preparer to assemble that shell, then remove its old
sound sources and bind HYB1; do not add a runtime protocol switch. AXI reads
have no byte strobes, so the binding supplies `0xf` for reads and actual WSTRB
for writes. Recheck held responses, malformed transactions and clear/start
through the real peripheral before measuring CPU rendering. A shell prepared
with the earlier boot ROM is only a synthesis/bus fixture: hardware packaging
requires the matching HYB1 boot reset and player firmware, not the r4 images.

The CPU renderer keeps the reference's 1,024-output-frame call size and timer,
recording at most 1,024 FM writes and 1,536 internal stereo PCM frames per call.
The block owns copied data until the next render call; overflow is an error,
never silent truncation. Decode the MDX outer wrapper/LZX and resolve PDX on the
PC for the first hardware fixture, then load bounded, padded reference-format
blobs before starting audio. This deliberately separates renderer/transport
timing from the later M3U/filesystem interface. The fixture must include actual
MDX sequencing as well as authored eight-FM/eight-PCM stress; it is not an
all-corpus capacity or malformed-input safety claim. Keep the reference source
in generated output with its existing component notices, not vendored music.

The first Pocket fixture uses a frozen terminal while audio runs: START selects
the prepared MDX/PDX pair or one of three authored eight-FM/eight-PCM cases,
A starts and B clears/stops. There is no autoplay. It loads the prepared blobs
before playback and reports maximum render/feed time and minimum queued frames
after stopping. The final block reserves one FM FIFO slot for EOF; 1,024 writes
plus EOF in one block is an explicit fixture capacity failure. Input decoding,
directory/M3U browsing, Japanese titles and full malformed-input hardening remain
outside this prepared-input timing fixture. The old 4 KiB initialized-data gate
was a player profile choice, not the SDK loader limit: this reference's tables
use about 76 KiB, so the fixture caps initialized data at 128 KiB while retaining
the actual 54 MiB static-region and 512 KiB stack bounds. It is not a general
relaxation of the old player's gate. Reference-backed static analysis runs via
`tools/hybrid-renderer-tidy.ps1` after source preparation; normal host builds do
not require the optional downloaded reference. The minimal app remains in the
ordinary host compile/format/tidy checks.

For the user's multi-song hardware check, package several locally prepared real
MDX/PDX pairs as separate APF instance JSON files. Select a song in Pocket's file
browser before launching the unchanged app; START continues to select synthetic
stress modes, not songs. Each instance names its own blobs, and FM-only instances
omit PDX. Include a Japanese procedure, a local song table and short PC reference
WAVs so the listener can identify melody, tempo and PCM parts. Keep private names,
blobs and audio under ignored output. This packaging slice does not change the
renderer, ROM, OS or FPGA; verify slot resolution/readback and reference streams,
then run Full when the multi-song package is complete. All user-facing hardware
verification procedures must be written in Japanese.

Implement one headless hybrid vertical slice before rebuilding the full
player. Start with authored eight-FM/eight-PCM loads and the existing private
comparison set; keep a native full-software oracle. The first slice must:

1. Preserve ordered FM events, PCM formats/pan/rates and the driver's timer;
   verify dense bursts, key transitions, loops/end and stop/restart. Compare
   the actual mixed output for clipping, levels, FM/PCM alignment and clicks.
2. Measure worst block completion, queue occupancy and underruns with the
   real CPU/memory/audio transport. Reject a candidate that misses a refill
   deadline; propose numerical margin from measured storage/UI interference
   before promotion, rather than treating a passing average as sufficient.
3. Show identical audio scheduling with UI absent and delayed; then demonstrate
   responsive select/play/stop and Japanese titles. Startup remains silent.
4. Pass the relevant host/architecture tests, RTL ordering/CDC/reset checks,
   fit/timing and final Firmware 2.6 hardware checks. No host result substitutes
   for these hardware integration observations.

The earlier plan to extend the private manifest across every reference-playable
MDX and compare whole songs/loop boundaries is superseded by the user's
[acceptance](#current-acceptance-and-next-mvp-step). Do not run that expansion as
the next task. When an actual playback defect appears, retain a focused
reproduction and distinguish dependency/reference failures from RPCMP failures;
do not silently exclude the file. The installed iOS build and settings remain
unverified. Review the mixed
driver/sound-library licensing before distributing a port, and harden input
copy/decompression boundaries before exposing the legacy decoder to files.

If the composed current CPU lacks margin, first measure where cycles go;
compare a fitting faster CPU or narrowly scoped PCM acceleration. Reopen
full software synthesis only with measured improvements that cover the
whole budget. The present investigation is sufficient to choose this prototype;
further abstract comparison should not delay testing its complete audio path.
