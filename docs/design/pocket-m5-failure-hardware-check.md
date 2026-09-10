# M5 admission failure — firmware 2.6 hardware handoff

## 今回の実機確認

`0.10.3-m5-failure`。2026-09-10 にユーザーから３候補すべての
実機項目 **PASS / OK** と、追加確認で全候補の `RESET: OK` の報告を受けました。
実機合格済み M5 Boot の ROM・OS・アプリ・loader を固定して、入力だけを
変えた３候補です。M5 の再生開始前の失敗停止と無音を確認します。

ZIP を SD カードのルートへ展開してください。既存コアとは別の名前です。
楽曲ライブラリを含むため、ローカル確認専用です。

| ZIP（リポジトリ内） | 表示名 / core ID | 入力変更 | 期待する表示 |
| --- | --- | --- | --- |
| `out/build/rpcmp-m5-args-stop.zip` | RPCMP M5 ARGS STOP / `RPCMP.M5ArgStopProbe` | 曲 ID 引数を不正なゼロへ | `M5 FILE: FAIL 2` / `RESET: OK` |
| `out/build/rpcmp-m5-library-stop.zip` | RPCMP M5 LIBRARY STOP / `RPCMP.M5LibStopProbe` | ライブラリの magic 先頭を１ビット反転 | `M5 FILE: FAIL 3` / `RESET: OK` |
| `out/build/rpcmp-m5-track-stop.zip` | RPCMP M5 TRACK STOP / `RPCMP.M5TrackStopProbe` | 有効なライブラリに存在しない曲 ID を指定 | `M5 FILE: FAIL 4` / `RESET: OK` |

platform は順に `rpcmp_m5argstop`、`rpcmp_m5libstop`、`rpcmp_m5track`。
３候補とも故障時の `PLAYING` 表示や音楽再生は期待しません。
`RESET: FAILED`、無表示、リロードループは合格ではありません。

1. 正常版 **RPCMP M5 Boot** で同じ曲が左右から再生されることを確認します。
2. 各故障版で上表の `FAIL` 番号と `RESET: OK` が表示され、停止画面を
   維持して **１分間無音**、`PLAYING` へ進まないことを確認します。
3. 各故障版で通常再起動を **１回**、完全な電源 OFF 後の起動を **１回**
   行い、同じ表示・１分の無音になることを確認します。
4. 各故障版から正常版へ戻り、左右の再生が戻って１分安定することを確認します。

失敗時は最後の表示と音の有無を記録し、その候補を中断してください。

```text
Firmware: 2.6
正常版 音楽 / 左右:
ARGS STOP FAIL番号 / RESET:
ARGS STOP 無音1分 / PLAYINGへ進まない:
ARGS STOP 通常再起動 / 電源OFF後 / 正常版復帰:
LIBRARY STOP FAIL番号 / RESET:
LIBRARY STOP 無音1分 / PLAYINGへ進まない:
LIBRARY STOP 通常再起動 / 電源OFF後 / 正常版復帰:
TRACK STOP FAIL番号 / RESET:
TRACK STOP 無音1分 / PLAYINGへ進まない:
TRACK STOP 通常再起動 / 電源OFF後 / 正常版復帰:
失敗時の最後の表示・音:
```

## User hardware report — 2026-09-10

Firmware: 2.6. Normal-probe music and both channels were reported PASS.

| Candidate | FAIL code | RESET display | Silent one minute / no PLAYING | Warm restart / cold start / normal recovery |
| --- | --- | --- | --- | --- |
| ARGS STOP | 2 | OK | OK / OK | OK / OK / OK |
| LIBRARY STOP | 3 | OK | OK / OK | OK / OK / OK |
| TRACK STOP | 4 | OK | OK / OK | OK / OK / OK |

The initial response supplied only the failure numbers in the combined
FAIL/RESET fields. A follow-up explicitly confirmed `RESET: OK` for all three;
reset success was not inferred from silence. No failure was reported.
This accepts the requested admission-failure checks. Restart/recovery items
were reported collectively as OK; no extra counts or instrumented timing are
inferred. The observations do not cover APF transport timeouts, in-playback
faults, failed reset responses or continuous heartbeat/reset lifecycle traces.

## Preparation and evidence

`python -B tools/pocket_m5_failure_package.py --repo .` creates only fresh
outputs. It verifies the accepted native RBF pin and all same-link ROM/OS
bytes, checks existing successful Quartus reports, reconstructs the normal
probe from the accepted File assets and compares it byte-for-byte against
the hardware-accepted Boot ZIP. All executable, OS, RBF and loader bytes are
unchanged. Each output archive is read back and compared to its intended files.
Adjacent local evidence records the complete member hashes; private archive/
music identities are not committed.

The argument candidate uses zero, which the unchanged `track_id` function
rejects before library reads. The library candidate changes only the first
magic bit, an error covered by the production loader's authored host test.
The track candidate retains the complete valid library, checks its single
track table with bounded reads, and chooses a nonzero ID absent from that
table. The production session test covers `TrackNotFound` without mutating
the existing session. Expected on-screen failure numbers come from the
unchanged `m5_file_probe.cpp` branches 2/3/4. They still require hardware
observation; packaging tests do not execute Pocket MMIO or prove silence.

All three paths call the existing `fail` function, which requests a bounded
sound reset, displays its actual result, and holds through the SDK adapter.
No playback pump is created before these failures. This is admission-failure
coverage, not a fault injected into an already playing queue. Returning from
a fault core to normal playback tests recovery across core selection, not
an in-place player retry feature.

Fixed identities are the accepted Boot RBF
`f3d06c863da82254dc1078088111047d5b7ab3eee3ff29d49b060946d894c7d3`,
OS `7cffaf9ba421c9ca9382f60f71f92d7eeacc9fd81448293458d5c048b0d38f9e`,
and app `237b16d8f8abaf67471518cc3bb8c33e76d70f5e11d729c1fe8ae2f46a01fb85`.
No compiler, RTL, FPGA inputs, runtime logic or public contracts changed.
Consequently RTL/Quartus were not rerun for this packaging-only slice.
Existing CRC-stop acceptance remains separate and its package is preserved.

Validation: `python -B tests/pocket/failure_package_tests.py` passed 4 tests
covering immutable fixed assets, single-bit corruption, absent-ID selection,
invalid directory bounds and ambiguous config rejection. The completion
command `pwsh -File tools/host-verify.ps1` passed 49/49 cases on 2026-09-10
(149.33 seconds), including format, tidy, architecture and the existing
production loader/session negative tests. Log: `out/build/m5-failure-host-verify.log`.
The final audit passed all three ZIPs' intended-byte equality and APF JSON
root/magic checks. Six existing Boot/CRC/placement ZIPs retained their saved
hashes; the File control was independently checked by `read_control`.
No new dependency was added.

APF transport errors/timeouts, in-playback faults, missing reset responses,
continuous heartbeat, reset-enter/exit captures, integrated CDC/external-I/O
constraints and future PCM/UI reserve budgets remain open. M5 and substrate
promotion are not declared complete by these packages.
