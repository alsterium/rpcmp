# CPU と音声セッションの接続

状態：2026-09-13、ユーザーのレビュー完了・承認により採用。基準コミット `6c0fde1`。
MDX audible-progress の上に、新しい CPU 接続契約を追加する。
最初の同期層は [Pocket sound session v1](../../specs/pocket-sound-session-v1.md) に定義する。実装と検証の結果は M6 に記録し、採用自体を検証合格としない。

## 採用範囲

既存 Core の Reset / Start / Pause / Resume / SetPolicy を、今回検証した
音声回路へ接続する専用プロトコルを追加する。通常リセット、世代を含む
完了通知、MDX 供給、履歴用の出力記録を持つ。旧 queue/reset v1 の意味は
維持し、新しい backend だけが新プロトコルの能力を宣言する。
Q1–Q24 の操作、曲順、ループ回数、フェード秒数は変更しない。

この範囲の正式仕様を実装前に確定し、ホスト／RTL／CDC／合成で
検証してから Pocket 接続へ進む。数値配置や内部の実装上の判断は以下の
範囲で進め、製品動作の変更や根拠のない時間上限の緩和は別途相談する。
承認は設計の採用と実装・検証を対象とし、将来の合格や実機結果を意味しない。

## 採用時点の接続上の問題

- `rpcmp_jt51_progress_audio` は音声ドメインの同期入力だけを持ち、CPU が
  非同期に書き込む口や、要求を確実に一度だけ受け取る完了通知を持たない。
- 現行の `stream_reset` は、再生中なら契約上 DeviceFault になる。
  通常の Stop／選曲時の Reset として成功扱いに読み替えてはいけない。
  Core の Reset 成功は、無音・旧データの破棄・静止・位置ゼロを要求する。
- 最新の `output_prefix` を遅い CPU で読むだけでは、途中のノートが最初に
  対応したフレームを失う。Tracker に推測した時刻を表示してはいけない。
- 自然終了の境界は pending の終了情報を適用するが、その PCM は消費しない。
  そのため、通常出力と終了境界の記録を区別する必要がある。

根拠は既存の `specs/playback-transport-v1.md`、
`specs/pocket-enveloped-audio-v1.md`、`specs/mdx-performance-v1.md`。
旧 v1 の実装不具合と断定する内容ではなく、新 backend に不足する接続です。

## 1. 世代を保持する音声セッション

一つの Core owner が操作を直列化する。制御要求は nonzero の
`operation_id:u64`、`play_generation:u64`、操作種別を持つ。
Start/Pause/Resume/SetPolicy は既存 RepeatApplication の revision:u64 と
target enabled / target:u32 を含む。Reset は policy payload を持たない。
受付後はコピーし、完了を取り出すまで同じ要求の内容を変更しない。

通常 Reset を新しい明示操作にする。供給を閉じ、旧データと一時停止位置を
破棄し、音源の 2,048 音声クロックの初期化完了後に成功を返す。
旧低レベル `stream_reset` の DeviceFault 規則は維持する。正常 Reset の
ためだけにその故障を隠す例外を作らない。論理セッションのクリアを明示的に
分け、I2S のシリアル時計は動かし続ける。静止を確認できない場合は失敗。

Start は Reset で準備された世代の供給を前提とし、最初の消費境界を確認して
成功を返す。Pause は保持した次フレーム、Resume は同じ保持位置を ACK する。
Resume と同じ境界で実際の消費が始まった場合、ACK の位置と最新の再生位置を
混同しない。Core は ACK を処理してから、対応する新しい状態を取り込める。
SetPolicy は既存 envelope の音声境界の結果を返す。完了は要求の識別情報と
policy を正確に返し、失敗・古い世代・busy を成功扱いにしない。

緊急無音化は通常の制御待ちから独立した、保持される inhibit にする。
それだけでは Reset 成功やバッファ解放を通知しない。成功した明示 Reset
だけが解除できる。故障入力が残っていれば解除／成功しない。
制御完了を CPU が読まないことが音声の Pause/Reset 成立を遅らせない。

## 2. 新しい CPU 用 MMIO と CDC

CPU-local 32-bit little-endian の独立領域を `0x4000_0400` から 1 KiB に置く。
これは APF BRIDGE ではない。旧 `0x4000_0200` queue と `0x4000_0240` reset、
固定 openfpgaCore の SYSREG `0x4000_0000–0x4000_01FF` に重ならない。
固定 checkout の `axi_periph_slave.v` の現在の decode は、この新領域を
REGION_NONE にする。新 decoder の明示追加と alias 拒否を検証対象とする。

| base からの範囲 | 用途 |
| --- | --- |
| 0x000–0x01F | 新 ID/version/capability、busy/fault、緊急 inhibit |
| 0x020–0x07F | 制御要求の staging、submit、完了の明示解放 |
| 0x080–0x0FF | MDX item の staging、submit、受付結果と解放 |
| 0x100–0x17F | 一貫した状態 snapshot の capture と解放 |
| 0x180–0x1FF | 保持された制御完了の読み取り |
| 0x200–0x2FF | コピー済み状態 snapshot の読み取り |
| 0x300–0x3FF | 出力記録の head 読み取り、明示 pop、loss 情報 |

個々の word offset と reserved bit は、この領域分けと以下のフィールドを
正式仕様に列挙してからコード化する。未使用領域、misalignment、部分 word
write、未定義 bit は拒否し、旧レジスターに alias させない。

制御、MDX 供給、snapshot は独立した bounded mailbox とし、通常制御が
満杯の供給待ちに巻き込まれないようにする。要求 bundle を ACK まで固定し、
request/ack toggle を各方向に 2 段同期する。受信側は同期後に安定した bundle
をコピーする。応答も明示的な取り出しまで保持する。再読は再実行ではない。
Reset は共有の mailbox 世代を更新し、片側だけのリセットや旧 ACK が新しい
要求を完了させないようにする。物理 common reset の解除は各時計で同期する。

状態の multiword 読み取りは、capture で一度コピーした値だけを返す。
generation、media frame、policy revision/target、loop count、phase/gain/ramp、
pause、terminal/fault、reset/quiescence を同じ音声時点で保持する。
CPU が読み終える途中に一部だけ新しい値へ更新しない。

CPU 90 MHz / audio 12.288 MHz の候補について、制御の音声境界待ち、
bootstrap、2,048 クロック reset、同期と応答保持の段数から最大 ACK 時間を
導出する。既存音声回路の数値を、そのまま未実装 CDC の保証と呼ばない。
位相をずらした RTL と configured clock の timing/CDC がこの上限を満たす
ことを確認した後、正の実時間 watchdog 値を固定する。timeout は未完了であり、
成功・cancel 完了・静止の代用にしない。

## 3. 保持した MDX tick の供給

engine の scheduler tick rate を明示的に 12,288,000 として既存の剰余付き
整数変換を使う。Timer B の 4 MHz と音源の 3,579,545 Hz は変更しない。
tick の全書き込み、progress、performance、次 tick 時刻を成功時にコピーし、
最大 8,192 件、同一時刻、単調なループ数、終端と token の上限を検証してから
最初の item を送る。1 tick の一部を送った後で再実行して二重投入しない。

item は generation、write/marker/end、at/until、address/value、loops を持つ。
64 件の音声 queue に順番に送り、Full は同じ item の後刻再試行とする。
Accepted だけで送信カーソルと論理 token を進める。Pause 中の供給は可能だが、
CPU の先読みを公開位置にしない。Reset 後は旧世代の offers/results を破棄する。
UI 描画待ちとは独立に service し、actual source starvation は既存通り故障。
全合法 MDX の nominal timestamp を必ず守れるという性能保証は置かない。

## 4. 履歴用の出力記録

音声側で、出力 prefix が進んだ境界を 32 件の bounded journal に保存する。
各記録は generation、frame index、prefix、通常消費／自然終了の区別を持つ。
自然終了は適用した pending checkpoint を記録し、未消費 PCM を再生済みにしない。
Stop/fault は未適用の future checkpoint を公開しない。

CPU の読み出し遅延で journal が満杯になっても音声へ backpressure をかけない。
欠落を明示して履歴だけを Degraded にし、正確な対応が失われたイベントの
時刻は推測しない。最新の独立した完了 checkpoint から表示状態を再取得する。
CPU が読まないことを理由に音源の native receipt を止めない。

CPU は出力記録と batch 内の `after_write` から各イベントの対応フレームを
求める。全 checkpoint がまだ未完了の batch は丸ごと保持し、同じ after_write
は同じ frame にする。同一 frame の複数 batch を順に処理する。保留イベントより
先へ PerformanceHistory の observed-through を進めて、後から過去時刻を
挿入する方式にはしない。既存仕様の Waiting/Degraded を用い、推測で埋めない。

表示用 batch は 16 件を上限にコピー保持する。表示用保持が満杯になった場合も
音声入力を止めず、省略を loss として数える。音声供給中の immutable write batch
の所有権と、表示用イベントの retention は別に管理する。メモリ量はホスト sizeof
と target link で実測し、M6 全体の framebuffer/font/stack と合わせて受け入れる。

## 採用後の確認

- ホストで最大 batch、Full/retry、部分供給、cancel/reset、世代と u64 上限を検証。
- 32 件 journal の境界、表示欠落、同一 frame 複数 batch、不完全 checkpoint の
  保持を検証。音声 trace が表示頻度と欠落によって変わらないことを照合。
- 非同期 CPU/audio clocks の位相、各 mailbox 段階での reset、古い ACK、
  連続操作、mid-write Pause、snapshot の破断、停止と故障の優先順位を RTL で検証。
- 通常 Reset と緊急 inhibit の実際の無音・静止・旧データ破棄を確認する。
- host/architecture、既存 RTL、局所 timing/CDC、全体 fit/cross-build を実施。
  package と firmware 2.6 の実機確認は、実際に接続した候補で最後に実施する。

採用レビューの根拠：`docs/design/pocket-player-transition-contract.md` は
「実際の MMIO 配置と CDC は別の採用レビュー対象です」としている。
`specs/pocket-enveloped-audio-v1.md` も将来の制御 adapter を separately adopted
protocol としている。2026-09-13 のユーザー承認により、本書の範囲の採用レビューは
完了した。個々の実装・検証結果は引き続き M6 の受入条件で判断する。
