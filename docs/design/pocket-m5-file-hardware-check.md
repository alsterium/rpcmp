# M5実ファイル再生：実機確認手順

対象は `RPCMP M5 File Probe`、バージョン `0.10.0-m5-file`。
実曲を含む個人用パッケージで、配布・リポジトリへの追加は禁止。

## 導入と確認

1. Pocketの電源を切り、SDカードをPCにつなぐ。
2. `out/build/rpcmp-m5-file.zip` を展開し、中の `Cores`、`Assets`、
   `Platforms` をSDカードのルートへコピーする。ZIP自体や、その外側の
   フォルダーをコピーしない。既存のAudio Probe/BSS Probeは削除しない。
3. SDカードを安全に取り外してPocketへ戻し、Firmware 2.6で起動する。
4. openFPGAまたはDeveloper → Buildsから `RPCMP M5 File Probe` を選ぶ。
   ファイル選択が出た場合は `m5-file.json` を選ぶ。
5. 最後に以下の表示が出て、実曲が聞こえることを確認する。

```text
RPCMP M5 real file
READING LIBRARY...
LIBRARY: PASS
MDX: PASS
PLAYING (menu to exit)
```

画面は再生中に更新しない。固定表示のまま音楽が続くのが正常。
画面の色や、以前のテスト音の周期は合否条件にしない。
まず音量を小さくしてから左右の音声を確認し、1分程度聞く。
曲が自然終了した場合は無音になり `M5 FILE: COMPLETE` が表示される。
ループする曲はPocketメニューから終了するまで再生が続く。

6. Pocketメニューから終了して、同じコアを再度起動する（通常再起動）。
7. 電源を完全にOFFにしてから、再度起動する（コールドスタート）。

## 結果の報告

```text
Firmware: 2.6
初回: LIBRARY PASS / MDX PASS / PLAYING（または最後の表示）
音楽: 聞こえる / 聞こえない
左右: 両方 / 左のみ / 右のみ
1分: OK / 途切れ・停止・異常音あり / 自然終了COMPLETE
通常再起動: PASS / 最後の表示
電源OFF後: PASS / 最後の表示
```

失敗時は最後に見えた文字列と数値を省略せず報告する。
`LOAD`、`LIB`、`SESSION`、`PARSE`、`MDX`、`STATE`、`ERROR`、
`RESET: FAILED` が出た場合は写真でもよい。ブラックアウトの場合は
`Loading...` / `RPCMP M5 real file` / `READING LIBRARY...` のどこまで
見えたかを記録する。大きな異常音が出たら音量を下げて終了する。

これは実曲再生の実験版であり、PCM、曲選択UI、シーク、完全なMDX互換性、
汎用openfpgaOS配置の安定性を保証するものではない。将来のPCM拡張経路は
保持しているが、現時点ではFM-onlyファイルだけを受け付ける。

## 開発側の再現

既存パッケージを上書きしない。次は専用出力がまだ存在しない作業環境で実行する。
ローカルのMDXと生成ライブラリはリポジトリに含めない。

```powershell
docker run --rm -v F:/source/rpcmp:/workspace/rpcmp -w /workspace/rpcmp rpcmp-openfpgaos-toolchain:14.2.0-3 make -f spikes/pocket/openfpgaos/Makefile m5-file-budget
python -B tools/pocket_m5_file_package.py --repo F:/source/rpcmp --sdk out/research/openfpgaSDK-a408ddc --elf out/build/pocket-openfpgaos-m5-file/rpcmp-m5-file.elf --rbf out/build/openfpgaos-m5-audio-bootfix/src/fpga/targets/pocket/bld/rpcmp-m5-bootfix/output_files/ap_core.rbf --library out/build/m5-local-music.rpcmlib --source out/build/m5-admitted-local.mdx --preflight out/build/host-msvc/rpcmp_m5_file_preflight.exe --title 'Local FM track'
```
