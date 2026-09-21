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
The [Firmware 2.6 hardware report](../milestones/M6-album-player.md#minimal-player-r2-hardware-result--2026-09-22)
passed the continuous-playback slice and established r2 as the hardware baseline.
No errors occurred, so failed-track skipping was not exercised on hardware;
its authored host checks remain the evidence for that path. The user selected
the [pause/resume boundary](#pause-and-resume-boundary) on 2026-09-22 as the next
small slice, with provisional X input. Minimal Player r3 implements it; see the
[implementation evidence](../milestones/M6-album-player.md#pause-and-resume-implementation--2026-09-22).
The subsequent [Firmware 2.6 / r3 report](../milestones/M6-album-player.md#minimal-player-r3-hardware-result--2026-09-22)
passes pause/resume, browsing and transport controls, fade/automatic advance,
audio continuity and restart/power-cycle recovery. r3 became the accepted
hardware baseline. The user then approved [loop/repeat switching](#loop-and-repeat-switching-boundary)
with provisional Y input. The [r4 candidate](../development/pocket-minimal-player.md)
passes its local verification gates and the subsequent
[Firmware 2.6 / r4 hardware report](../milestones/M6-album-player.md#minimal-player-r4-hardware-result--2026-09-22).
This accepts loop/repeat switching and establishes r4 as the hardware baseline.
Failed-track skipping was not reported in that hardware check. The user selected
[multiple-playlist support](#multiple-playlist-requirements) as the next slice
and settled its Q1–Q6 requirements. The subsequent instruction to continue until
hardware verification is needed authorizes implementation and its handoff.
The [r5 implementation](../milestones/M6-album-player.md#multiple-playlist-implementation--2026-09-22)
passes the subsequent [Firmware 2.6 / r5 hardware report](../milestones/M6-album-player.md#minimal-player-r5-hardware-result--2026-09-22).
r5 is now the accepted hardware baseline; the next small feature is not yet selected.
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
repeat-one, shuffle, pause, extra views and saved settings were outside r2;
the next pause/resume slice is defined below.

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

実装後の確認項目（ホスト試験の結果は実装証跡、r2の実機結果は
[2026-09-22の報告](../milestones/M6-album-player.md#minimal-player-r2-hardware-result--2026-09-22) を参照）:

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

### Pause and resume boundary

Approved on 2026-09-22 after r2 hardware acceptance. The user chose pause/resume
and X as its provisional button because the final UI is not yet developed.
Keep the slice small. The [r3 hardware report](../milestones/M6-album-player.md#minimal-player-r3-hardware-result--2026-09-22)
now accepts this slice on Pocket; the requirements below remain its contract.

- X toggles pause/resume for the current song, regardless of browsing selection.
  Pause silences both FM and PCM and holds their musical position, loop progress
  and any in-progress fade. Resume continues that same song and fade from the
  held position. Muting while the song continues to advance does not satisfy pause.
- Paused playback does not reach EOF, advance to another entry or count paused
  wall-clock time toward the loop/fade. Once resumed, r2's M3U ordering and
  natural-end/two-loop/five-second-fade behavior continue normally.
- Browsing remains available while paused and does not change the held song.
  A always starts the selected entry from its beginning, including the same
  entry, and starts a new continuous run. B stops and cancels automatic advance;
  X after Stop does not resume the abandoned song or start playback.
- Treat X as a press, not a repeating action on hold. Preserve startup/reconnect
  held-button suppression. For simultaneous transport presses, use B, then A,
  then X. X without a playing/paused song does not start one.
- Show the paused state and retain the playing entry/title and browsing position.
  A core restart or power cycle still starts silently; paused position is not saved.

Extend the existing small UI input bindings rather than adding a remapping
framework or settings screen. Translate the physical X binding to a playback
command at that boundary; Core/engine/device code must not interpret Pocket
buttons. Core owns pause state and UI reads a copied/versioned snapshot. Preserve
audio state and buffered work as needed to resume without lost/duplicated musical
progress. Rendering and snapshot cadence never determine pause/audio timing.
Describe the concrete port/hardware changes before implementation and version any
breaking snapshot/hardware boundary; no compatibility wrapper is required.
M3U/HPL1 and deferred UI/policy/settings features stay outside this slice.

Implementation boundary (2026-09-22): snapshot version 3 adds Paused and a
TogglePause command. The playback port accepts a logical paused value. HYB3
(`0x48594233`) adds control values 4 (pause) and 8 (resume) and status bit 8
(acknowledged paused). Clear has priority and releases pause. Apply transitions
only at a stereo frame boundary; keep I2S clocks running and emit whole silent
frames while holding the native FM clock/state, register bus, PCM/event FIFO
consumption, resampler, mixer pipeline and output queue. Reuse the pinned JT51
hold transformation with the existing wide mixer taps. On the CPU, retain
pending rendered blocks and exclude paused time from the feed/EOF deadline.
An X before the first device start holds preparation locally; X with an already
ended device can hold the completed song until resume observes EOF. Pause/control
failures use the existing reset/error path. FPGA, boot ROM and application must
all use HYB3; the package supplies the matching set. No new dependency is added.

The Full checkpoint is **play -> pause to silence -> browse -> resume at the
held FM/PCM position -> normal fade/end and next-track advance**, including A/B
interruption. Run focused checks and Fast during iteration, then Full plus
affected RTL, target build, synthesis/CDC and package checks before hardware
submission. Requirements approval alone does not establish this behavior.

実装後の確認項目:

1. FM・PCMの曲をXで一時停止すると無音になり、再度Xで同じ位置から再開する。
2. 一時停止したまま待っても曲・ループ回数・フェードは進まず、次曲へ送られない。
   フェード途中でも残りのフェードを再開できる。
3. 一時停止中に一覧を移動しても保持曲は変わらず、Xでその曲を再開できる。
   Aなら選択曲の先頭から再生し、Bなら停止する。停止後のXでは再生しない。
4. X長押し・連打・A/Bとの同時押し、曲末尾付近の操作でも状態が矛盾しない。
   ホストでは描画遅延/無効化と入力割り当て変更を確認し、Coreの動作が変わらないことを確かめる。
5. 実機でFM・PCM・左右、再開時の音切れ・ノイズ・テンポ、入力反映、通常再起動・
   電源OFF後を確認する。ホスト/RTLの結果と実機報告は区別して記録する。

### Loop and repeat switching boundary

Approved on 2026-09-22 after r3 acceptance: Y cycles **2 -> 3 -> 5 -> repeat
one -> 2**. The existing small bindings keep Y replaceable; A/B/X retain their
r3 meanings. Simultaneous-input priority is B, A, X, then Y. A held Y at
startup/reconnect or during normal input does not repeat the action. Display
the Core-owned setting. Each application launch starts silently with two loops;
stop, manual track selection and automatic advance retain the session setting.

Counted modes play one intro and the chosen number of loop bodies, then fade
FM and PCM together for five seconds and continue in M3U order. Natural endings
play once in counted modes. Repeat one lets looping music continue without a
fade and restarts naturally ending music from its beginning, including the last
entry. Recoverable errors still skip forward, even in repeat one; never retry a
failed entry automatically or wrap the list. B cancels all automatic continuation.

Changes apply to the current song without restarting or moving its position.
Loop counts are measured since this song started, not since the setting changed.
If the new count has already been reached, begin a five-second fade now. If an
existing fade is still required, retain its progress. If increasing the count
or choosing repeat one removes the end condition, cancel the fade and restore
gain smoothly over at most 20 ms. Paused changes retain silence and position;
the new condition takes effect on resume. A completed audio end cannot be undone;
the current setting chooses repeat/advance at the next Core service boundary.

Implementation boundary: snapshot version 4 adds RepeatMode and CycleRepeat.
The existing playback port accepts a logical repeat mode; the renderer reports
each completed loop rather than hard-coding a two-loop fade/EOF. HYB4
(`0x48594234`) gives event `0x10001` the meaning "one completed loop". Natural
EOF remains `0x10000`. The audio owner counts loops (saturating at five) and
owns the live fade/end, independent of rendering and CPU read-ahead. A single
bit request/acknowledgement crosses the clock boundary for command 16 (cycle
mode); status bit 9 is its acknowledgement toggle. Clear resets mode to two and
both toggles to zero; the CPU reapplies the session mode before starting a song.
This avoids crossing a mutable multi-bit setting. Commands are acknowledged
within a bounded CPU wait, including while paused. On autonomous fade completion,
already-admitted FIFO writes may finish; no further audio is played, and Core
clears queued data before the next song. FPGA/boot/application update together.
M3U/HPL1 and the reference sequencer/PCM synthesis remain unchanged.

The Full checkpoint is **Y -> current-song loop/fade change -> counted advance
or natural-end repeat -> stop**, including paused/fading changes and errors.
Run focused host/native/RTL checks and Fast, then Full, RV32 link/budget,
affected RTL/CDC/synthesis and package readback before hardware submission.
Pocket acceptance still requires the user's Japanese hardware report.

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

### Multiple-playlist requirements

Status: Q1–Q6 requirements agreed on 2026-09-22, implemented in r5 and accepted
through the [Firmware 2.6 hardware report](../milestones/M6-album-player.md#minimal-player-r5-hardware-result--2026-09-22).
See the [implementation evidence](../milestones/M6-album-player.md#multiple-playlist-implementation--2026-09-22).
This extends the single installed collection described above. Keep PC-side M3U
authoring and MDX/PDX preparation; Pocket consumes generated data. The current
HPL1 limits remain r4 implementation facts, not the new feature's capacity.

| 決定 | 確定した動作 |
| --- | --- |
| 基本構成 | M3Uごとに1プレイリストを作り、Pocketでプレイリスト一覧 → 曲一覧を選ぶ |
| Q1 閲覧と再生 | 別リストを開くだけでは再生を止めず、元のリストで連続再生を続ける。曲をAで決定したとき、その曲の先頭から再生し、以後は選んだリストの順に進む |
| Q2 戻る操作 | 曲一覧の先頭に「プレイリスト一覧へ」を置き、Aで戻る。戻る操作は再生を止めない。SELECTへの新規割り当ては不要 |
| Q3 PC取り込み | M3Uファイルの複数指定を基本とし、指定フォルダー内のM3Uを一括取り込みするオプションも用意する |
| Q4 規模 | 100プレイリスト程度、1リスト最大300曲、最大30,000登録を第一目標とする。メモリ容量上で非現実的だと判明した場合は、ユーザーは全体100〜300曲への縮小も許容している |
| Q5 名前と順序 | 表示名はM3Uファイル名から拡張子を除き、日本語に対応。複数指定時は指定順、一括時はファイル名の番号順。曲順は各M3Uの記載順 |
| Q6 閲覧位置 | カーソルとページをリストごとに保持し、戻ったとき復元する。起動中だけ記憶し、再起動・電源OFF後はリセットする |

Keep the accepted r4 transport semantics: B stops, X pauses/resumes, Y changes
the session-wide loop/repeat setting, and startup is silent with two loops.
Opening a list does not start or resume a song. Automatic advancement follows
the active playback list, without moving the browsing cursor/page even when
another list is being viewed. Counted modes stop at that list's end; repeat one
continues the same song. Existing recoverable failed-track skipping remains
within the active list. Opening another list does not reset the repeat setting.
The back item is navigation, never a track or an automatic-playback entry.

Measured implementation resources: HPL2 keeps one raw index of at most
3,851,200 bytes (100 × 112 + 30,000 × 128), without a decoded copy. The RV32
build has 4,486,412 static bytes against its 54-MiB region, with a conservative
15,344-byte stack bound. The loader reads only the selected MDX/PDX pair and
browsing performs no storage reads. Host checks exercise 100 × 300 entries;
these are not measurements of runtime peak heap or total Pocket startup time.
The subsequent user hardware report passes the normal collection and reports
the 100 × 300 entry check as OK. Index load/validation I is 20 ms normally and
6,598 ms at scale; no input delay, dropout or noise was reported. The scale
collection repeats six songs, not 30,000 distinct songs. No smaller fallback
was needed. The storage and pair limits
below still apply; 30,000 maximum-size pairs cannot fit in a 2-GiB file.

The connected implementation checkpoint for Full is **multiple M3Us → generated
collection → browse another list while audio continues → select a track and
advance within its list**. Cover both import modes, deterministic names/order,
duplicate track entries, Japanese text, malformed counts/offsets and input
containment; test navigation versus playback ownership, pause/stop/repeat,
per-list cursor/page restoration and restart defaults. Check 100 × 300 authored
entries without copyrighted fixtures, target link/memory/stack and measured
loading/browsing costs. Keep headless and delayed-display audio independence
checks. Run affected tooling tests and Full before a hardware candidate, with
RTL/synthesis only when their inputs change, then provide Japanese hardware
steps for playback continuity while browsing and switching lists. Persistent
settings, shuffle, rich visualization and broad MDX compatibility revalidation
are outside this slice.

#### HPL2 implementation boundary

Use one deferred APF slot and one generated HPL2 file. Keep all validated index
records in RAM once, then read music only when starting a selected song after
silencing the preceding song. Browsing performs no storage I/O. Keep at most
100 lists and 300 entries per list (30,000 total). A list owns a contiguous range
of global one-based TrackIds; duplicate song occurrences retain separate IDs.
Use session-local one-based PlaylistIds. Core owns the active playback list;
UI owns a separate browsing list and a bounded selection record per list.
Minimal snapshot version 5 adds the active PlaylistId. Keep all physical inputs
in the replaceable UI bindings and preserve HYB4/audio behavior.

HPL2 is little-endian. Its 48-byte header contains magic `HPL2`, version 2,
playlist record width 112, track record width 128, playlist count, track count,
file byte count, index CRC32, CRC32 of header bytes 0–31, then 12 zero bytes.
The index is all playlist records followed by all track records. A playlist
record contains first TrackId, count, name byte length, 96 UTF-8 name bytes and
a zero word. Playlist ranges partition all entries in order with no gaps and
contain 1–300 entries each. Track records retain the HPL1 field layout but their
blobs may be shared; each present range must lie wholly after the index and
within the declared file. Preserve UTF-8/padding/reserved-field checks, the
16-MiB combined prepared pair bound, selected-blob CRCs and driver wrappers.
Reject HPL1 explicitly; rebuild existing music collections with the new importer.
Do not change or duplicate the audio protocol to version this library format.

Bound the file to 2,147,483,647 bytes: the current RV32 SDK size adapter uses
signed 32-bit `ftell`. This is a storage limit, not an in-RAM allocation or a
claim that all 30,000 maximum-size pairs fit. The APF
[data.json definition](https://www.analogue.co/developer/docs/core-definition-files/data-json)
allows an unsigned 32-bit `size_maximum`; use the smaller adapter limit.
The PC writer streams payloads to disk and shares identical prepared MDX/PDX
blobs using SHA-256 identities, preserving all occurrence/order metadata.
Oversize collections fail clearly instead of truncating the requested lists.
No new dependency is required. The read-only source tree is never rewritten.

The importer accepts positional M3Us in the supplied order and an optional
folder of M3Us (immediate files only), appended in natural filename-number
order with deterministic path tie breaks. Relative music/PDX references remain
relative to each source M3U and subject to the existing explicit root boundary.
Invalid whole M3Us fail the import; invalid individual songs are reported and
excluded as before. Omit an all-excluded list with a report; if nothing remains,
produce the report without an installable collection. Duplicate list names do
not merge lists. Preserve each list's identity/order even when songs repeat.

Start at the playlist list with no playback. A opens a list; it starts music
only on an actual track row. Track pages have one fixed first row for
「プレイリスト一覧へ」 and 12 track rows. Up from a page's first track focuses that
back row while retaining the track/page position; A returns to the playlist
list. Down from the back row restores track focus, and left/right change track
pages. Opening that list again restores its track/page, not the back row.
The playlist list keeps its own selection/page. Show the current browsing list,
and the playing list/title separately; mark the playing list/track without
moving the cursor. Copy all of these display values into the UI view.

### UI interaction requirements

On 2026-09-22, after accepting r5, the user selected UI interaction organization
as the next requirements task. Settle the remaining choices through numbered
questions. Keep the accepted playback behavior as the baseline and keep physical
bindings in UI/platform rather than in the playback engine.

**Q1 — decided:** B depends on the focused panel. In the track list, it returns
to the playlist list without stopping or changing the playing track. In the
control panel, it stops playback using the existing stop semantics, including
cancelling automatic advancement. B never moves focus between panels.
This replaces r5's global B-to-stop binding in the next UI; r5 itself still has
the documented global stop behavior. At the playlist root there is no parent;
B does nothing and does not stop playback. Q9 removes the existing back row.

**Q2 — decided:** L/R switches focus between the list and control panels. The
D-pad operates within the focused panel. In lists, up/down selects entries and
left/right retains page navigation. Moving focus does not start, stop or switch
music. This supersedes the earlier D-pad-between-panels preference for this UI.
The specific icon arrangement/navigation remains to be defined; Q8 settles
control focus restoration. L/R is not a previous/next-track shortcut in this UI.

**Q3 — decided:** Do not assign X/Y shortcuts. Select playback/repeat control
icons with the D-pad and activate them with A. Q1's contextual B stop/return
remains available; this decision does not remove that stop action or track-list
A playback. The next UI replaces r5's X pause/resume and Y repeat bindings.

**Q4 — decided:** The play/pause icon acts on the playback track, independently
of the browsing cursor. While playing it pauses; while paused it resumes at
the same position. While stopped it starts the last-played track from its
beginning, retaining that track's playback playlist. It does not start the
track currently being browsed in another list. Before the first playback,
start a track with A in the track list; the control icon cannot start music
without a last-played track. This does not change startup silence or the
session's repeat setting.

**Q5 — decided:** Provide five control icons: play/pause, stop, repeat count,
previous track and next track. Previous/next operates within the playback
playlist, independently of the list currently being browsed. Adding these icons
does not authorize cross-playlist navigation. Q6–Q7 settle their playback-state
and list-boundary behavior; the exact layout remains to be defined.

**Q6 — decided:** A previous/next action with an available target switches to
that track and starts playback from its beginning, including when the original
track is paused or stopped. Do not carry the paused/stopped state to the target.
Keep the session's repeat setting and the existing separation between browsing
and playback.

**Q7 — decided:** Disable previous on the playback playlist's first entry and
next on its last entry. Show the unavailable action as disabled; activating it
does not change the track, position or playback state. Do not wrap within the
playlist or cross into another playlist. This applies while playing, paused and
stopped, including repeat-one mode; both directions are unavailable for a
one-entry list. It does not change automatic list-end/repeat behavior.

**Q8 — decided:** On entering the control panel, restore the previously focused
icon. Returning to the list retains the existing cursor/page position. The
remembered icon is session-local, consistent with the current no-persistence
scope. Use play/pause as the initial icon when there is no prior control focus.
An icon becoming unavailable does not activate another action; Q7's disabled
behavior still applies when the remembered previous/next icon is unavailable.

**Q9 — decided:** Remove the 「プレイリスト一覧へ」 row from track pages. Show
only tracks in the list and use B to return to the playlist list. Preserve each
playlist's track/page position on return and reopening; returning is browsing,
not a stop or a change of playback playlist.

#### Information display requirements

After Q9, the user selected information organization as the next part of the
same UI requirements task. Keep browsing information distinct from the playback
track's information; browsing another list does not change the playback target.

**Q10 — decided:** Display elapsed playback time only, for example `01:23`.
The value freezes while paused. Do not add total-duration analysis, remaining
time or a duration/progress bar as part of this choice. The current minimal
snapshot has no elapsed-time field; implementation must publish playback-owned
time through the snapshot, rather than infer it from UI frames or text updates.

**Q11 — decided:** If a track or playlist name does not fit, horizontally
scroll only the selected name after a brief dwell so the full name can be read.
Other rows use a trailing ellipsis and remain stationary. This is UI-local
presentation timing and must not drive audio, change selection or start music.
The exact dwell, speed and fitting text width belong to the layout pass.

**Q12 — decided:** Continuously show the playback track's title, playback
playlist name, entry number / entry count, elapsed time and transport state in
the lower information area. The entry number/count belong to the playback
playlist, not the global collection or the currently browsed list. Browsing
another playlist does not replace these values with the highlighted row's
metadata. Show the repeat setting with its control icon.

| Information area | Content and source |
| --- | --- |
| Main browsing area | Browsed list heading, playlist/track entries and browsing position; retain the accepted separate playing marker. |
| Lower playback information | Playback track title, playback playlist name, entry number / count, elapsed time and transport state. |
| Control panel | Five action icons and the current repeat setting. |

These fields settle the information grouping, not exact sizes, line breaks or
icon placement. The working layout retains a large main area above the lower
playback information, with controls to its right. Next lay out these values
and the focus indicators together so browsing selection, playback identity,
control focus and disabled actions remain distinguishable.

#### Consolidated operation table

The table describes the next UI, not the installed r5 controls. Top-level B
being a no-op follows from having no parent and Q1's panel-specific stop action.
Physical bindings remain replaceable in UI/platform; playback commands and
audio timing remain independent of focus and rendering.

| Input | Playlist list | Track list | Control panel |
| --- | --- | --- | --- |
| D-pad up/down | Select list | Select track | Select an icon according to its placement |
| D-pad left/right | Change page | Change page | Select an icon according to its placement |
| A | Open selected list without changing playback | Start selected track from its beginning and use its playlist | Activate selected icon |
| B | No action | Return to playlist list without changing playback | Stop playback and cancel automatic advance |
| L/R | Switch panels without changing playback | Switch panels without changing playback | Switch panels without changing playback |
| X/Y | Unassigned | Unassigned | Unassigned |

| Control icon | Action |
| --- | --- |
| Play/pause | Playing: pause; paused: resume at the same position; stopped: restart the last-played track from its beginning. Unavailable before initial track-list playback. |
| Stop | Stop playback and cancel automatic advance; do not navigate. |
| Repeat count | Cycle 2 -> 3 -> 5 -> infinite -> 2 using the accepted live repeat behavior, including while stopped or paused. |
| Previous | Start the previous entry in the playback playlist from its beginning. Disabled at the first entry. |
| Next | Start the next entry in the playback playlist from its beginning. Disabled at the last entry. |

Before initial playback there is no playback playlist/track for previous/next;
those icons cannot start music. Browsing, returning and switching panels do not
alter playback, and automatic advance does not move the browsing cursor.
Remember control-icon focus and list positions for the session only. Launch
still starts silently at the playlist list with two loops; no setting or focus
persistence is added. Exact icon placement, D-pad adjacency and the directional
L/R mapping belong to the next layout pass, not new playback features.

The working layout proposal reuses the earlier preference for a large main
area, track information below it and control icons to the right of that
information. First organize the list, information and playback controls;
Tracker/keyboard reintroduction is not approved by Q1–Q12. The operation table
and information grouping are consolidated. Next settle layout and
placement-dependent focus navigation before implementation.

These are prospective product requirements, not a hardware candidate or a
claim of implemented behavior. No runtime, package, test/generator input or
hardware contract changes in this requirements record. Check document navigation
and consistency now; run the affected UI/command/audio-independence checks and
the harness integration gates when the settled interaction is implemented.

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
