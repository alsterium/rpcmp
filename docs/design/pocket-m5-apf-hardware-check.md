# M5 APF lifecycle and transport — hardware handoff

## Scope

Version `0.10.4-m5-apf`, local firmware 2.6 experiment. Hardware results are
pending. The user explicitly approved correcting APF status handling on
2026-09-10. These candidates retain the accepted fail-closed boot ROM, OS,
application, loader and private library; they change the FPGA command handler.

The old handler returned Idle (3) while running because setup had priority,
and both readiness inputs were tied to BCR completion. Executing the extracted
old status branch reproduced expected/actual values 1/1, 2/3 and 4/3. The full
new handler is exercised through BRIDGE by `tests/rtl/apf_lifecycle_tb.sv`.
It prioritizes Running, waits for load completion and the initial RTC before
Ready-To-Run, and preserves the RTC across warm reloads. Reset Enter reports
Idle; replacement loading re-enters Setup. Explicit FSM initialization and
declarations before use allow deterministic simulation without forced state.

Sources checked on 2026-09-10:
[boot process](https://www.analogue.co/developer/docs/core-boot-process),
[commands](https://www.analogue.co/developer/docs/host-target-commands), and
[instance writes](https://www.analogue.co/developer/docs/core-definition-files/instance-json).
RTC is a boot-time notification; a warm reload must not require another one.

## 実機手順

ZIP を SD ルートへ展開してください。既存の合格版とは別のコアです。
楽曲を含むためローカル確認専用です。

| ZIP (`out/build/`) | 表示名 | 期待結果 |
| --- | --- | --- |
| `rpcmp-m5-apf-normal-v2.zip` | RPCMP M5 APF NORMAL | LIBRARY / MDX / PLAYING、左右の音楽 |
| `rpcmp-m5-apf-error-v2.zip` | RPCMP M5 APF ERROR | `M5 FILE: FAIL 3`、`RESET: OK` |
| `rpcmp-m5-apf-timeout-v2.zip` | RPCMP M5 APF TIMEOUT | 数秒待って `M5 FILE: FAIL 3`、`RESET: OK` |

1. NORMAL で初回表示と左右の音楽を確認し、１分再生します。
2. NORMAL を通常再起動３回、完全電源 OFF 後に１回起動し、同じ確認をします。
3. ERROR と TIMEOUT はそれぞれ停止表示・RESET を確認し、１分間無音で
   PLAYING に進まないことを確認します。表示までのおおよその秒数も記録します。
4. 故障版ごとに通常再起動１回、電源 OFF 後の起動１回で同じ停止を確認します。
5. 各故障版から NORMAL へ戻り、左右の音楽が１分安定することを確認します。

無表示、繰り返しリロード、RESET FAILED、停止後の音があれば、その候補を
中断し最後の表示を報告してください。

```text
Firmware: 2.6
NORMAL 初回 LIBRARY / MDX / PLAYING:
NORMAL 音楽 / 左右 / 1分:
NORMAL 通常再起動 1 / 2 / 3 / 電源OFF後:
ERROR FAIL番号 / RESET / 表示まで約何秒:
ERROR 無音1分 / PLAYINGへ進まない:
ERROR 通常再起動 / 電源OFF後 / NORMAL復帰:
TIMEOUT FAIL番号 / RESET / 表示まで約何秒:
TIMEOUT 無音1分 / PLAYINGへ進まない:
TIMEOUT 通常再起動 / 電源OFF後 / NORMAL復帰:
```

## Fault boundary and evidence

The experiment-only register `0xF7000020` is outside APF's reserved region.
Instance writes select normal/error/timeout using repeated-byte values
`0x00000000`, `0x01010101`, `0x02020202`. Writes are admitted only in reset;
Reset Enter clears the mode and the partial-read history. Core IDs are
`RPCMP.M5ApfProbe`, `RPCMP.M5ApfErrorProbe`, `RPCMP.M5ApfTimeoutProbe`.
Platforms are `rpcmp_m5apf`, `rpcmp_m5apferr`, `rpcmp_m5apftmo` (at most
15 characters). Use the v2 ZIPs above: the earlier unhanded-off packages had
overlong fault-platform IDs. Platform metadata has no APF magic field;
core definitions and instances retain `APF_VER_1`, per the separate
[platform schema](https://www.analogue.co/developer/docs/platform-metadata).

The first slot-4 read completes normally. From the second read, ERROR replaces
the returned result with error 3; TIMEOUT suppresses the CPU-facing DONE.
The real APF request and host/DMA completion still happen. This tests handling
of an error or missing completion after partial library input, not an SD-card
electrical failure or a host that never finishes DMA. OS/config/app slots are
unaffected. The unchanged SDK adapter rejects error results and has a 2.5 s
completion wait; the OS async watchdog normally fails first at 2 s. These are
source-level bounds, not measured Pocket timings. The unchanged application
rejects the load and requests sound reset before holding the result screen.

Prepared tree: `out/build/openfpgaos-m5-apf-lifecycle-v3`.
Native RBF: `src/fpga/targets/pocket/bld/rpcmp-m5-apf-lifecycle/output_files/ap_core.rbf`
inside that tree; SHA-256
`b02972159f9dbd3ba360e27a2b7acef9ac16347aecdf1fa0b39abba80cd2e128`.
The existing paired-ELF/MIF/OS verifier passes. `quartus_sh --flow compile ap_core`
completed with 0 errors, 1104 warnings: 13,888/18,480 ALMs, 13/66 DSPs,
setup minimum +0.770 ns, hold minimum +0.034 ns, all 136 timing-summary rows
with zero TNS. Existing integrated CDC/external-I/O constraint work remains open.

Executed checks: `tools/rtl-verify.ps1` (9 suites), then
`tools/rtl-jt51-verify.ps1` (5 suites), and
`tools/rtl-apf-verify.ps1 -PreparedTree out/build/openfpgaos-m5-apf-lifecycle-v3`
all passed. The APF test covers cold boot, warm reload without RTC replay,
normal reads, both fault modes after a successful read, unaffected OS-slot
access and reset clearing the fault mode. The RAM is a behavioral model;
this is not full CPU/SDRAM/host simulation. Package tests use authored data.
Every ZIP is read back and compared byte-for-byte; adjacent ignored evidence
contains private artifact hashes and must not be copied into committed docs.
`pwsh -File tools/host-verify.ps1` passed 50/50 tests in 158.51 s, including
format, tidy and architecture checks (`out/build/m5-apf-host.log`). After the
metadata correction, `python -B tests/pocket/apf_package_tests.py` passed both
cases again. Final v2 readback also confirmed every OS/app/config/library/loader
payload against the accepted Boot ZIP, all identities and per-schema JSON
checks. No SDK C adapter execution or heartbeat waveform test was run in this
slice; their source inspection is not promoted to runtime evidence.

Hardware acceptance, direct hardware capture of each APF command/status,
continuous-heartbeat/failure observation, faults during active playback,
failed sound-reset responses, general placement stability and production
promotion remain open. A successful normal run is indirect evidence of APF
progress; it does not independently measure the heartbeat waveform or prove
every status response. This handoff advances the bounded M5 experiment only.
