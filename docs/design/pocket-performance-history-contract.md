# Pocket 演奏履歴 snapshot の契約案

Status: proposal, 2026-09-13. [画面案](pocket-tracker-screen-design.md) の Q23 を実現する
[schema 2](pocket-player-transition-contract.md) の設計提案です。
[UI State API v1](../../specs/ui-state-api.md) と M0 / M5 の実装・型・上限は変更しません。
履歴は演奏の観測値であり、再生を駆動する event queue や編集可能な pattern ではありません。

保存・公開部分は [performance profile](../../specs/performance-history-v2.md) で採用しました。
現在の8チャンネル値は履歴と同じ境界を保証するため optional 値の内部にまとめ、
未取得と不正データ用に Waiting / Invalid を追加しています。
[MDX の演奏観測](../../specs/mdx-performance-v1.md) は実装し、出力確定の境界を
注入して保存・公開する経路を host で検証します。実機 commit 対応・保持待ち queue・
Tracker 行への投影は、引き続き M6 の未完了項目です。

## 所有者と時刻

engine は音高・key-on/off・音色変更の観測値を、device write と対応する media timestamp、
play generation、同時刻内の source order とともに Core へ渡します。Core は音声出力の
commit 境界に到達した観測値だけを履歴へ入れます。位置は48,000 Hzの stereo frame 数です。
先読み完了・MMIO enqueue・UI の描画完了を、聴こえた時刻の代用にしません。
具体的な commit 通知は今後の音声 port に必要な機能で、現行 queue v1 には追加しません。

演奏に必要な loop/end 通知と、表示専用の履歴を区別します。前者の順序と backpressure は
[状態遷移案](pocket-player-transition-contract.md) に従います。後者は bounded な観測経路とし、
容量不足で履歴を失っても device write を落としたり音声を待たせたりしません。
snapshot 作成・文字列整形・描画・UI への通知待ちは音声の期限を持つ処理内で行いません。

## schema 2 の optional な公開値

`PlayerSnapshot.performance_history` を型付き optional 値として提案します。
未対応なら absent とし、UI は現在の channels と「履歴未対応」を表示できます。
v1 の最大256 bytesの extension payload に大きな履歴を詰め込みません。

| フィールド | 型・意味 |
| --- | --- |
| version | `u16 = 1`。この履歴型の version |
| play_generation | `u64`。同じ snapshot の再生世代と一致 |
| observed_through_frame | `u64`。観測が確定した media 境界。未来のイベントは含めない |
| next_sequence | `u64`。次の観測に割り当てる番号。世代内で1から単調増加 |
| events | 最大256件。sequence 昇順の、現在保持している履歴全体 |
| retention_lost | 保持窓から古いイベントを押し出したことを示す bool |
| capture_lost | 表示用観測の取得経路でイベントを失ったことを示す bool |
| availability | Available / Degraded / Exhausted。欠落や番号枯渇を正常と偽らない |

各 event は次の値を持ち、可変長文字列、ポインター、MDX opcode を含みません。

| フィールド | 型・意味 |
| --- | --- |
| sequence / at_frame | `u64 / u64`。同時刻でも sequence は異なる |
| channel_id | `u16`。snapshot 内の８ FM channel の ID に解決できる |
| kind | KeyOn / KeyOff / PitchChanged / InstrumentChanged |
| key_on | event 適用後の論理 gate。音声 sample が非ゼロである保証ではない |
| note / fine_pitch_cents | optional `u8` 0〜127 / optional `i16`。後者は cents、note 不明なら absent |
| detail | optional typed `MdxFmV1 { voice_number: u8? }`。不明なら absent |

note は表示用の MIDI-like 音高で、発音 scheduling の情報源にはしません。
fine_pitch_cents は基準 note からの偏差で、UI が音名へ丸めても Core の音高は変わりません。
voice_number は MDX adapter が実際に選択した音色番号だけを入れます。UI は optional detail
を理解しなくても音高と gate を表示でき、汎用 ChannelState に MDX 固有の番号を混ぜません。
波形・音量・オペレーターの実測値をこの番号や gate から推測しません。

同じ音の鳴らし直しは独立した KeyOn として残します。前後の key_on / note が等しいことを
理由に消しません。実際の論理発音を二重に数えないよう、音源への構成レジスター書込みを
すべて別の KeyOn に変換することもしません。pitch / instrument の同値設定は省略可能です。
同時刻の順序は sequencer が確定した source order とし、UI の列順で並べ直しません。

## 保持上限、欠落、世代

256件の ring は新しい event を保持して古いものを押し出します。snapshot は差分配信ではなく、
その時点の保持窓をコピーした immutable 値です。UI の ACK や読取位置は Core に送りません。
consumer は世代と sequence で重複を除き、UI 側も最大256件に制限します。
参照中の公開面は書き換えません。空いた公開面がなければ publication を見送り、音声側の
演奏と bounded ring の更新を続けます。後の snapshot が欠落の有無を伝えます。

番号は履歴への格納より前に割り当て、表示用経路で取り逃した分も次の観測までに欠番として
確定させます。取り逃し件数を確定できない場合は capture_lost を立て、完全な区間と表示しません。
欠落フラグは世代内で保持します。イベントがないことと、観測できなかったことを区別します。
連続取得中の last_sequence の次が保持窓から消えていたら、UI は欠落マーカーを１つ表示し、
最も新しい窓へ進みます。新しく表示を開いたときは retention_lost を「以前の履歴は範囲外」、
capture_lost を「履歴欠落」と区別できるようにします。偽の音や行で穴を埋めません。

たとえば256件を保持し next_sequence=301 なら、欠落のない窓は45〜300です。
UI が最後に40を見ていた場合は41〜44を取り逃しています。通常の window 更新による
snapshot.sequence の飛びだけでは、演奏イベントを取り逃したと判断しません。
保持窓内の sequence の穴と capture_lost も確認します。

play generation が変われば ring / 欠落フラグ / UI の履歴を破棄し、番号を1から始めます。
Stop による世代変更も同じです。古い世代の遅延観測を新曲へ混ぜません。
Pause は世代を変えず、media 位置と履歴を凍結します。UI publication は続き、鍵盤には
保持中の note / gate と Paused を併記して、現在出力中の音と区別します。
UI 非表示・表示切替は履歴の生成条件にしません。

`u64` の番号を wrap させません。次番号を表せなくなる前に履歴を Exhausted にし、
新世代まで記録を止めます。これは表示機能の上限であり、音声の停止理由にしません。
snapshot 内の時刻・ID・順序が不正なら UI はその履歴を使用せず、無効データと表示します。
長さ・version・ID 検証はアクセス前に行い、固定上限を超えた割当はしません。

## Tracker 行と鍵盤への投影

Tracker の行は「演奏変化があった時点」で進みます。60 Hzの画面更新や推定 BPM に合わせた
架空の pattern 行は生成しません。同一 at_frame の隣接 event を、１行内で同じ channel が
二度変わらない範囲でまとめます。同じ channel の次 event が来たら、同時刻でも新しい行に
します。行の ID は最初の sequence とし、２行とも保持して鳴らし直しや短い音を残します。
同時刻内の別チャンネルの変化は８列で比較できます。

音高は初期案では `C-4` 形式（note 60 = C-4）、音色は既知の voice_number を２桁16進数で
表示します。key-off は OFF、変化のないセルは空欄、不明な値は `--` の表示上の表記にします。
これらは UI の文字列で、Core の unknown 値を sentinel 数値で置き換えません。
鍵盤の現在値は同じ snapshot の channels を使い、途中が欠けた履歴から復元しません。

event の値型は実装時に１件64 bytes以内を検証し、ring１つ、公開用２面、UI保持１つを
最大64 KiB、付随管理域込み128 KiB以内に収める初期予算です。これは
[全体案](pocket-library-player-spec-draft.md) の既存1 MiBの catalog/snapshot予算内に含めます。
C++ ABI や wire encoding を固定する値ではなく、実装時の sizeof / map 計測が必要です。

## 採用時の独立した受入例

| 入力・条件 | 期待値 |
| --- | --- |
| frame 100: ch0 On60、ch1 On64、ch0 Off、ch0 On60の順 | ４ event。Tracker は３行。最初の行はch0/ch1、次はOff、最後はOn |
| 上記観測を先読みしたが commit は99まで | 履歴に含まれない。100へ到達後、元の順で公開 |
| 256件の窓を越えてUI停止、41〜44を取り逃す | 欠落を示し45〜300へ追従。音声 trace は UI 常時表示時と同じ |
| 一度の publication 間に短い On/Off | 両 event を保持。現在の鍵盤だけなら Off でも、履歴は発音を残す |
| Pause / Resume、世代7の遅延値が世代8へ到着 | Pause中は位置・履歴不変。世代7を世代8へ追加しない |
| unknown detail、破損ID、上限超過、sequence枯渇 | optional detail は無視可能。不正履歴は拒否し、表示上限で音声を停止しない |

上の期待値は手で与えた入力から導きます。mock UI は runtime / MDX をリンクせず検証し、
音声 trace の同一性は headless と低頻度描画で別に比較します。まだ実行済みの試験ではありません。
