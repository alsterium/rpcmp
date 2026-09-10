# M5 placement matrix — firmware 2.6 hardware handoff

## 実機で確認するもの

`0.10.2-m5-placement` の４候補について、2026-09-10 にユーザーから
**全項目 PASS** の実機報告を受けました。[配置調査](pocket-m5-placement-review.md)の
最初の比較行列の結果を下記に記録し、手順を再確認用に保持します。各候補はコードか BSS に 64 バイトを追加し、
元のオブジェクトから再リンクしています。

次の ZIP を SD カードのルートへ展開してください。４候補はコア名と
platform を分けてあり、合格済み File／Boot／CRC Stop の各コアも保持できます。
各 ZIP は既存の楽曲・設定を含むローカル確認専用です。

| ZIP（リポジトリ内） | openFPGA 表示 / core ID | 比較する基準 |
| --- | --- | --- |
| `out/build/rpcmp-m5-os-code.zip` | RPCMP M5 OS CODE / `RPCMP.M5OsCodeProbe` | 合格済み M5 Boot |
| `out/build/rpcmp-m5-os-bss.zip` | RPCMP M5 OS BSS / `RPCMP.M5OsBssProbe` | 合格済み M5 Boot |
| `out/build/rpcmp-m5-app-code.zip` | RPCMP M5 APP CODE / `RPCMP.M5AppCodeProbe` | 合格済み M5 File Probe |
| `out/build/rpcmp-m5-app-bss.zip` | RPCMP M5 APP BSS / `RPCMP.M5AppBssProbe` | 合格済み M5 File Probe |

platform ID は順に `rpcmp_m5oscode`、`rpcmp_m5osbss`、
`rpcmp_m5appcode`、`rpcmp_m5appbss` です。
OS 候補は fail-closed 修正版の ROM と OS を同じリンクから生成しています。
アプリ候補は比較計画に従い、元の File Probe の OS・ROM・RBF を固定しています。

各候補で次を確認してください。

1. 初回起動で `LIBRARY: PASS`、`MDX: PASS`、`PLAYING`。
2. 同じ曲が左右両方から聞こえ、１分間安定して再生する。
3. メニューへ戻って同じコアを起動する通常再起動を **３回**行い、各回で
   初回と同じ表示・左右の音・１分の再生を確認する。
4. 完全に電源 OFF にしてから起動する確認を **１回**行い、同じ項目を確認する。

失敗した候補はそこで中断し、最後に見えた表示、無音か、再起動を繰り返すかを
記録してください。元の合格済みコアに戻せます。

```text
Firmware: 2.6
OS CODE 初回 LIBRARY / MDX / PLAYING:
OS CODE 音楽 / 左右 / 1分:
OS CODE 通常再起動 1 / 2 / 3 / 電源OFF後:
OS BSS 初回 LIBRARY / MDX / PLAYING:
OS BSS 音楽 / 左右 / 1分:
OS BSS 通常再起動 1 / 2 / 3 / 電源OFF後:
APP CODE 初回 LIBRARY / MDX / PLAYING:
APP CODE 音楽 / 左右 / 1分:
APP CODE 通常再起動 1 / 2 / 3 / 電源OFF後:
APP BSS 初回 LIBRARY / MDX / PLAYING:
APP BSS 音楽 / 左右 / 1分:
APP BSS 通常再起動 1 / 2 / 3 / 電源OFF後:
失敗時の最後の表示・症状:
```

## User hardware report — 2026-09-10

Firmware: 2.6. The user reported PASS for every cell below.

| Candidate | Initial LIBRARY / MDX / PLAYING | Music / both channels / one minute | Warm starts 1 / 2 / 3 | Full power-off cold start |
| --- | --- | --- | --- | --- |
| OS CODE | PASS | PASS | PASS / PASS / PASS | PASS |
| OS BSS | PASS | PASS | PASS / PASS / PASS | PASS |
| APP CODE | PASS | PASS | PASS / PASS / PASS | PASS |
| APP BSS | PASS | PASS | PASS / PASS / PASS | PASS |

This accepts the requested first four-row placement matrix on the reported
firmware. No failure was reported. These are user observations, not instrumented
timing, stack, electrical audio or APF lifecycle measurements. The coherent OS
code shift now has positive hardware evidence alongside the demonstrated
fixed-ROM target mismatch, but this does not identify the exact embedded ROM
of the historical failing package or prove stability at arbitrary placements.
Next is reviewing that bounded explanation against ADR-0008, including the
remaining historical-package provenance and clean-source reproduction gaps.

## Measured placement and limits

OS control relinking reproduces the accepted fail-closed boot bytes
(`d666a02a…df643`) and complete OS (`7cffaf9b…8f9e`). App control relinking
reproduces every allocated section and the LOAD/GNU_STACK layout of the
accepted app. Retained relocation records enlarge the ELF files; they do not
change the control's loaded image. This is retained-object reproduction,
not a new claim of clean-source recovery of the old safe firmware.

| OS profile | Entry | IRQ | Syscall | BSS start..end, exclusive |
| --- | --- | --- | --- | --- |
| Fail-closed control | `1032c880` | `10336ac0` | `10335f80` | `10341640..1038db30` |
| OS CODE | `1032c880` | `10336b00` | `10335fc0` | `10341680..1038db70` |
| OS BSS | `1032c880` | `10336ac0` | `10335f80` | `10341680..1038db70` |

Addresses are hexadecimal. OS CODE pads at `0x1032df40`, after the first LTO
text partition. Later code/data and BSS move by `0x40`; the entry does not.
OS BSS pads at `0x10341640`, before `__os_bss_start`; code/data stay fixed.
The BSS section includes the unused 64-byte gap while the clear-range symbols
exclude it. ROM addresses and section sizes stay fixed in both variants;
the required OS reference immediates change with their targets.

| App profile | `.text` bytes | PT_LOAD filesz / memsz | gp | heap start |
| --- | ---: | --- | --- | --- |
| File control | 67284 | `10820 / 2ec338` | `10410fd0` | `106ec340` |
| APP CODE | 67348 | `10860 / 2ec378` | `10411010` | `106ec380` |
| APP BSS | 67284 | `10820 / 2ec378` | `10410fd0` | `106ec380` |

App entry/LOAD VMA remain `0x10400000`, file offset `0x1000`, alignment
`0x1000`; GNU_STACK remains RW, alignment 16. Code padding is at
`0x10400038`, preserving `_start` and `_start_c`. Both app variants move pump
storage to `0x10410860`, workspace to `0x104308a8`, session to `0x104e8ca0`,
and container storage to `0x104ebc70`. BSS ends at `0x106ec378`.
The CODE variant moves subsequent `.eh_frame`, constructor array, and data
by 64; BSS-only keeps these fixed and increases the BSS section by 64.
Each app uses 3,064,696 static bytes and 228 initialized-data bytes. The
retained stack records give the unchanged conservative 18,288-byte bound;
this is not a runtime stack measurement. Stack/cache/SDRAM reservations and
startup ownership remain those in the placement review.

The comparator checks the full allocated section set, exact section-size and
address shifts, retained defined symbols, and non-padding bytes. Changes must
be covered by relocation-immediate masks in both images; opcodes and register
fields cannot change. Unknown relocations receive no permission. This checks
the linker's output against identical inputs; it is not an independent
formal proof of every relocation calculation or indirect runtime pointer.
The same-ELF ROM/OS gate checks all boot bytes, payload/footer, CRC, entry,
and BSS, including the embedded IRQ/syscall references.

## Build and verification evidence

OS CODE Quartus completed on 2026-09-09 and OS BSS on 2026-09-10. Both full
25.1std builds report 0 errors, 1104 warnings, 13,822 ALMs, 171 RAM blocks,
and 13 DSP blocks. Both report minimum setup `+0.770 ns`, hold `+0.047 ns`,
and zero TNS in all 136 timing-summary rows. Both MIF inputs are present;
reports contain no missing-initialization warning 127003 or zero-ROM fallback.
The first pilot MIF used one-line declarations that Quartus rejected; separate
declaration lines fixed that syntax error without changing decoded ROM bytes.
Only the subsequent successful full builds supplied the packaged RBFs.

These builds retain the existing reset/queue/CDC implementation and constraints.
RTL reset/CDC tests pass, but the integrated CDC/external-I/O policy remains
open: reports still list 6 unconstrained input ports and 28 output ports
(32 input and 72 output paths). Nonnegative constrained timing does not close
those gaps. No new RTL or public contract changes were made.

Artifact SHA-256 values are recorded in
[placement artifacts](pocket-m5-placement-artifacts.json). The adjacent local
ZIP evidence contains every member hash and the private archive hash; those
private identities are deliberately excluded from committed evidence.
The final ZIP audit read all members back, checked byte equality, APF root/
magic, platform name bounds, instance-slot references, and reviewed RBF pins.

Validation commands and local logs:

- `python -B tests/pocket/placement_tests.py`: 11 tests PASS, including wrong
  padding, changed registers/section addresses, missing insertion points,
  preserved package assets, and rejected negative/missing timing coverage.
- `pwsh -File tools/host-verify.ps1`: final run on 2026-09-10 passed all
  48 CTest cases, including format/tidy/architecture, exit 0 (165.43 seconds).
  Log: `out/build/m5-placement-host-final.log`.
- Sequential `pwsh -File tools/rtl-verify.ps1` and
  `pwsh -File tools/rtl-jt51-verify.ps1`: 9 and 5 PASS markers, exit 0.
- The current `rpcmp_m5_file_preflight.exe` passed the retained library's
  one-minute pump/direct-engine comparison using its contained MDX bytes.
  The private identity/trace output stays local. This is sequencing evidence,
  not execution of the relocated RISC-V app on a CPU.
- `python -B out/build/m5-placement-handoff-audit.py`: all four archives PASS.

## Reproduction

Run `tools/pocket_m5_os_placement.py` in the existing `openfpgaos-firmware`
image (GCC 13.2.0), and `tools/pocket_m5_app_placement.py` in
`rpcmp-openfpgaos-toolchain:14.2.0-3`. Each accepts `--repo` and `--output`;
output must be a fresh direct child of `out/build`. The exact retained
objects, archives, link commands, scripts, compiler strings, and maps are
hashed in each output's `evidence.json`. The delivered outputs are
`out/build/m5-os-placement-verified` and `out/build/m5-app-placement-final`.
Final container checks revalidated all 41 OS and 26 app input hashes.
`out/build/m5-placement-fpga-inputs.json` records 213 source/constraint/MIF/QSF
hashes per FPGA candidate, including the prepared tree, JT51 and Pocket RTL;
this is a source inventory superset, not a claim that every listed file was used.

For each OS profile, use `tools/pocket_m5_audio_prepare.py` with its linked
`--firmware-elf`, `--os-image`, and `--boot-mif`, the pinned core/JT51/netlist
from the boot handoff, and a fresh FPGA tree. Generate its QSF with
`make bld/<job>/ap_core.qsf VARIANT=rpcmp JOB=<job> QPROCS=4`; translate the
generated `/workspace/rpcmp` source paths to `F:/source/rpcmp`, preserving
SEARCH_PATH. Run `quartus_sh.exe --flow compile ap_core` inside that job.
The delivered jobs are `rpcmp-m5-os-code` and `rpcmp-m5-os-bss` in their
corresponding `out/build/openfpgaos-m5-os-*` trees.

`tools/pocket_m5_placement_package.py --repo . --firmware <profile-folder>
--rbf <native-rbf> --profile code|bss --target os|app` verifies placement,
pairing, reviewed RBF identity, reports/timing, and creates a fresh ZIP.
App probes use the accepted `openfpgaos-m5-audio-bootfix` RBF and safe OS.
No dependency was added. Existing controls were not overwritten.

The four-row hardware report above is accepted. General placement stability,
historical failing-package ROM provenance, clean-source reproducibility,
broader lifecycle/failure coverage and explicit future PCM/UI reserves remain
open. The passing matrix still requires review of the bounded explanation
under ADR-0008; it does not by itself complete M5 or substrate promotion.
