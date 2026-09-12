# M5 APF2 — exclude filesystem primer reads

## 実機確認

`0.10.5-m5-apf-primer`。旧 APF TIMEOUT で報告された
`Filesystem init...` 中の約40秒待ちを修正する候補です。実機結果は未確認です。
既存の合格版と旧 APF 候補は保存し、別コアとして追加します。

2026-09-12 のユーザー判断で、この不具合調査はここで区切りました。
以下は保存用の検証手順であり、現在の次タスクではありません。
APF2 の実機結果は未確認のまま保持し、次の要件決めの前提にはしません。
新しい不具合が発生した場合は、その症状に対して原因解析を行います。

| ZIP (`out/build/`) | 表示名 | 期待結果 |
| --- | --- | --- |
| `rpcmp-m5-apf2-normal.zip` | RPCMP M5 APF2 NORMAL | LIBRARY / MDX / PLAYING、左右の音楽 |
| `rpcmp-m5-apf2-error.zip` | RPCMP M5 APF2 ERROR | FAIL 3 / RESET: OK、無音 |
| `rpcmp-m5-apf2-timeout.zip` | RPCMP M5 APF2 TIMEOUT | READING の約２秒後に FAIL 3 / RESET: OK、無音 |

SD ルートへ展開してください。楽曲を含むローカル確認専用 ZIP です。

1. 各候補で `Filesystem init...` の長い停止と `bridge timeout DONE` が
   **出なくなったこと**を確認します。ロード開始から READING までと、
   READING から結果表示までの時間を分けて記録します。
2. NORMAL は初回表示、左右の音楽、１分再生、通常再起動１回、
   電源 OFF 後の起動１回を確認します。
3. ERROR / TIMEOUT は FAIL 3 / RESET: OK、無音１分、PLAYING に進まない
   ことを確認します。通常再起動１回と電源 OFF 後も同じ結果を確認します。
4. 各故障版から NORMAL へ戻り、左右の再生が１分安定することを確認します。

約40秒待ちが残る、無表示、RESET FAILED、停止後に音がする場合は、
その候補の最後の表示を報告してください。旧版の正常再生・無音実績は
修正版の実機合格に流用しません。

```text
Firmware: 2.6
NORMAL 初回 / 音楽左右1分 / 通常再起動 / 電源OFF後:
NORMAL Filesystem init の長い停止・エラー有無:
ERROR Filesystem init の長い停止・エラー有無:
ERROR ロード開始→READING / READING→結果（秒）:
ERROR FAIL番号 / RESET / 無音1分 / PLAYINGへ進まない:
ERROR 通常再起動 / 電源OFF後 / NORMAL復帰:
TIMEOUT Filesystem init の長い停止・エラー有無:
TIMEOUT ロード開始→READING / READING→結果（秒）:
TIMEOUT FAIL番号 / RESET / 無音1分 / PLAYINGへ進まない:
TIMEOUT 通常再起動 / 電源OFF後 / NORMAL復帰:
```

## Diagnosis and change

The user reported old NORMAL playback/stereo/one minute and three warm starts
plus cold start OK. Both faults explicitly displayed FAIL 3 / RESET OK, stayed
silent without PLAYING for one minute, and passed warm/cold/recovery checks.
TIMEOUT took roughly 40 seconds after core loading, but about two seconds from
READING to FAIL. The delay was later localized to the OS `Filesystem init...`
screen with `bridge timeout DONE #19 st=21`.

`filesystem_init()` calls `dir_probe_slots()`, which retries `of_file_get_name()`.
Each filename attempt primes the slot with a four-byte read at offset zero.
The previous injection counted *all* slot-4 reads and faulted the second and
later ones. It could therefore fault an OS primer before app entry. The new
regression reproduces this on the old actual command handler (fatal at 3130 ns)
and passes on the repair. The exact firmware operation represented by #19 was
not instrumented on hardware; the primer path is a supported explanation,
not a captured transaction trace.

`st=21` is hexadecimal: ACK and READY set, DONE absent. The printed mask is
`0x3f`, so this line says nothing about WR_IDLE (bit 6). #19 is the synchronous
wait-call count, not a slot ID. The synchronous wait uses 200,000,000 loop
iterations; its source comment "~2 seconds" is not a wall-clock bound.
This repair isolates the experiment; it does not change the OS wait policy.

The injection now requires slot 4 and excludes exactly **offset 0, length 4**.
Every filename primer passes unchanged; the app's first real read faults.
Nonzero-offset reads still fault even when only four bytes long. This is a
bounded M5 one-file profile, not a general caller-identification mechanism.
M5's admitted library is at least 80 bytes, and its first read is up to 4096
bytes, so it cannot be mistaken for a primer. Packaging enforces the existing
80-byte minimum and 2 MiB maximum. Public contracts are unchanged.

An intermediate offset >=4096 approach was rejected before handoff: the
accepted local library fits in one chunk, so that approach would not inject
a fault. Its build is retained as a local experiment only. The final approach
works for both single-chunk and multi-chunk libraries without changing the
music or application. It does not claim a partially admitted app library.

The regression covers eight repeated primers per fault mode, unaffected OS
slots, 80-byte and 4096-byte first-read failures and four-byte tail failures,
primers after failure, and warm reset/recovery. ERROR still substitutes result
3; TIMEOUT still hides CPU DONE after real host/DMA completion. No electrical
SD fault, permanently outstanding DMA, active-playback fault, or heartbeat
failure is simulated. The accepted ROM, OS, app, loader and library are fixed.

Prepared tree: `out/build/openfpgaos-m5-apf-primer-final`.
Build job: `src/fpga/targets/pocket/bld/rpcmp-m5-apf-primer` within that tree.
Use `tools/rtl-apf-verify.ps1 -PreparedTree out/build/openfpgaos-m5-apf-primer-final`
to repeat the command-handler test. Build/gate results are recorded with the
completed handoff; hardware acceptance and M5 production promotion remain open.

## Verification

`pwsh -File tools/host-verify.ps1` passed 50/50 tests in 181.31 seconds
(`out/build/m5-apf-primer-final-host.log`), including formatting, static analysis
and architecture checks. The final authored-fixture package tests were also
rerun directly: three cases PASS. `tools/rtl-apf-verify.ps1` passed with zero
errors/warnings; the existing `tools/rtl-verify.ps1` (9 suites) followed by
`tools/rtl-jt51-verify.ps1` (5 suites) passed. The testbench uses a modeled RAM
and APF host responses; no SDK/CPU execution or real-time deadline proof is
implied. Pocket confirmation of the removed startup delay remains required.

Full Quartus compilation completed on 2026-09-11 with 0 errors and 1104
warnings. Fit: 13,819/18,480 ALMs, 171/308 RAM blocks and 13/66 DSPs. Minimum
reported setup +0.407 ns and hold +0.098 ns; all 136 timing-summary rows have
zero TNS. Existing integrated CDC/external-I/O constraint work remains open.
Native RBF SHA-256:
`b271980aa424491532365b59013c3851a29bf1bee077376c40b08904bdc1d663`.
The packaging gate checks the linked ELF/MIF/OS pair, reviewed RBF and reports.
All three final ZIPs passed byte/hash readback and JSON/configuration checks,
rechecked on 2026-09-12. Their OS, app, config, library and loader match the
accepted Boot ZIP exactly. The final overlay reverse-check matches the
compiled source tree. Private library identities and hashes remain only in
ignored local package evidence. Final package tests passed again (three cases).
