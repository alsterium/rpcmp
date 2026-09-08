# M5 boot CRC repair — firmware 2.6 hardware check

## 実機で確認するもの

ユーザー承認済みの方針に従い、OS の CRC 検証に 8 回失敗したら OS を
起動せず、音源共通リセットを要求し、診断画面で停止する候補です。
バージョンは `0.10.1-m5-boot`。実機結果は **未確認** です。

次の ZIP を SD カードのルートへ展開してください。コア ID と platform ID
を分けてあるため、合格済み `RPCMP.M5FileProbe` は保持できます。

| ZIP（リポジトリ内） | コア / platform | 用途 |
| --- | --- | --- |
| `out/build/rpcmp-m5-boot.zip` | `RPCMP.M5BootProbe` / `rpcmp_m5boot` | 正常起動・同じ実ファイルの再生 |
| `out/build/rpcmp-m5-crc-stop.zip` | `RPCMP.M5CrcStopProbe` / `rpcmp_m5crcstop` | CRC 不一致時の停止 |

正常版は既存の合格済み ZIP からアプリ・設定・ライブラリ・loader をそのまま
引き継ぎます。ユーザーの楽曲を含むためローカル確認専用です。停止版には
アプリ・設定・楽曲を入れず、OS の保存済み CRC の 1 bit だけを反転します。
両者の RBF は同一です。

1. **RPCMP M5 Boot**: 初回に `LIBRARY: PASS`、`MDX: PASS`、`PLAYING` を
   確認し、左右から音が出て 1 分安定するか確認します。通常再起動と完全な
   電源 OFF 後の起動も各 1 回確認してください。
2. **RPCMP M5 CRC Stop**: `CRC FAILED E`、エラー番号 `5`、診断値
   `C0DE0008` と `SOUND RESET: PASS` を確認します。表示が維持され、
   1 分間音が出ず、OS やプレイヤーへ進まないことを確認してください。
   通常再起動と完全な電源 OFF 後も同じ停止になるか各 1 回確認します。
3. 停止版からメニューへ戻り、正常版を再起動して再生が戻るか確認します。
   失敗時は最後の表示を記録し、そのケースを中断してください。

`SOUND RESET: FAILED` は合格ではありません。ホストでのリセット応答検査は
実機の無音を証明しないため、表示と音の観察を分けて報告してください。

```text
Firmware: 2.6
正常版 初回: LIBRARY / MDX / PLAYING
正常版 音楽 / 左右 / 1分:
正常版 通常再起動 / 電源OFF後:
停止版 表示 / SOUND RESET:
停止版 無音1分 / OSへ進まない:
停止版 通常再起動 / 電源OFF後:
停止版から正常版への復帰:
```

## Software and build evidence — 2026-09-08

The old upstream retry function failed the authored persistent-CRC test:
eight failed reads returned success. The replacement passes success on every
attempt 1–8, terminal CRC exhaustion, and I/O failure on every attempt. Sound
tests cover generation wrap, wrong identity, busy timeout, invalid, missing
generation advance, queued work, and audio diagnostics with bounded polling.
The boot path never submits playback work or establishes a playback epoch.

The firmware pair checker reconstructs all retained BRAM sections from the
same ELF and compares every MIF word, including ROM-to-OS references. It also
checks OS payload equality, CRC, ABI, entry and BSS against that ELF. Authored
negative fixtures include the independently observed IRQ shift from
`0x10336980` to `0x103369c0` despite valid metadata. This is a build-time pairing
gate, not a runtime relocation mechanism or a full untrusted ELF loader.

Actual linked disassembly has the eight-attempt counter at `0xed6`, terminal
`-5` at `0x11b6`, reset command write at `0x124a`, and terminal self-loop at
`0xfdc`. The error branch cannot fall through into OS metadata/entry handling.
Reset status is bounded to 1,000,000 MMIO polls; that is not a measured
wall-clock deadline. A failed reset reports failure and still holds in ROM.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Linked firmware ELF | 314740 | `65ed4c2247c03b4b7d32208469f4c6cef9e1272f3d8a280046943eec18087092` |
| Boot instruction image | 15908 | `d666a02a0561f02e3f320e976e71f447845ef01135e017feeea026e5b18df643` |
| MIF | — | `a7ebbfbd7014e3f519bb8baea51e3f962e3f0c6d4448d6636e55ffe01aae5621` |
| Valid OS image | 135768 | `7cffaf9ba421c9ca9382f60f71f92d7eeacc9fd81448293458d5c048b0d38f9e` |
| Native RBF | 1775480 | `f3d06c863da82254dc1078088111047d5b7ab3eee3ff29d49b060946d894c7d3` |

Quartus 25.1std full compile completed with 0 errors and 1104 warnings.
Fit: 13,822 ALMs, 171 M10K blocks, 13 DSP blocks. Reported worst setup slack
is +0.770 ns, hold +0.047 ns; reported clock-summary TNS values are zero.
Both MIFs appear in the report inputs and there is no missing-initialization
warning 127003. These measurements do not close the existing integrated CDC
or external-I/O constraint policy gaps. Archive members are read back and
compared with the intended bytes; adjacent ignored evidence JSON files retain
input and artifact hashes without publishing private music identities.

The accepted ROM/OS control is unchanged. Its exact ELF/map reconstruction is
still unresolved; this new same-link pair is a behavior-repair baseline, not
the placement matrix. No code/BSS padding variants or new hardware tests were
performed. M5 and ADR-0008 production promotion remain open.

Validation on 2026-09-08: `pwsh -File tools/host-verify.ps1` passed 46 CTest
cases including format/tidy/architecture. The sequential commands
`pwsh -File tools/rtl-verify.ps1` and `pwsh -File tools/rtl-jt51-verify.ps1`
passed 9 and 5 simulation markers respectively.

## Reproduction

Prepare a fresh source tree with `tools/pocket_m5_firmware_prepare.py` using
`--fail-closed`, the clean pinned core checkout and the existing musl tree.
The script normalizes archived CRLF text to LF for Linux scripts/ABI hashing,
applies safe-memset and the boot overlay, and records input identities.
Firmware GCC is 13.2.0 in local image
`sha256:98771990c464c0b3231d685de02579a25c6b5b0f952b68d5e573679664070e58`.
Set `CPATH=/usr/lib/picolibc/riscv64-unknown-elf/include` and run
`make -j4 TARGET=pocket all` from the prepared `src/firmware/os` directory.
No new dependency was added.

Use `tools/pocket_m5_audio_prepare.py` with its existing pinned FPGA/JT51/netlist
inputs, adding `--firmware-elf`, `--os-image`, and `--boot-mif` from that same
build's `src/firmware/os/bld/pocket` directory. Generate the new QSF following
`overlays/openfpgaos/README.md`, with `JOB=rpcmp-m5-fail-closed`. Convert only
that QSF's `/workspace/rpcmp` prefix to `F:/source/rpcmp`, preserving SEARCH_PATH,
and run Quartus full compilation from the new job directory. Inspect all
timing summaries and MIF inputs before packaging.

```powershell
python -B tools/pocket_m5_boot_package.py --repo F:/source/rpcmp --firmware out/build/m5-firmware-fail-closed --rbf out/build/openfpgaos-m5-fail-closed/src/fpga/targets/pocket/bld/rpcmp-m5-fail-closed/output_files/ap_core.rbf
```

The builder pins the reviewed RBF and its input MIF and refuses existing ZIPs.
Use fresh output names for a new reproduction; do not overwrite controls.
A different fit requires review before changing pins. Byte-identical Quartus
output is not promised. Keep adjacent evidence local because it contains
private artifact identities.
