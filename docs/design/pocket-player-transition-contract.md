# Pocket 再生状態遷移の契約案

Status: Core transport portion adopted for M6 slice 3, 2026-09-13.
[Core playback transport v1](../../specs/playback-transport-v1.md) に準備・音声制御の
非同期処理、取消、故障と期限の扱いを採用しました。これはCore内部の契約です。
[詳細仕様案](pocket-library-player-spec-draft.md) の確定済み要件に向け、公開schema 2、
policy/loop/gain、history/settings、実際の音声ポートへの接続は後続実装です。
この文書の未採用の数値条件とAPIは引き続き設計提案であり、
現行 [PlayerCommand v1](../../specs/player-command-api.md)、
[UI snapshot v1](../../specs/ui-state-api.md)、M0 / M5 実装は変更しません。

## 1. 互換性、所有者、時刻

新 profile は command / snapshot の schema 2 を提案します。v1 の型や enum に新しい
値を黙って足さず、旧 consumer は schema 2 を unsupported として拒否します。
v1 の M0 fixture と60,000 Hzの時刻も維持します。schema 2 の C++ 型・capability の
bit 割当を採用時に定義し、旧 schema と明示的に区別した試験を作ります。

新 profile の位置と音量 ramp は、出力された stereo frame を基準とする48,000 Hzの
整数 media ticks です。Core が持つ実際の演奏位置は音源の read-ahead 位置ではありません。
UI publication の時刻は別の monotonic clock とし、Pause 中も UI / 入力 / APF を動かします。
ループ通知・自然終端を含む engine の先読み結果には media timestamp と play generation を
付け、音声側がその位置へ到達するまで公開状態や自動送りに適用しません。
通知も bounded queue と backpressure の対象で、device write との順序を失いません。

同じ frame 境界では device fault / reset を最優先し、続いて ingress の FIFO command、
その後に有効な演奏通知と自動終了を処理します。Stop / PlayTrack で失効した旧世代の終端は
次曲を起動しません。Pause が先に成立した境界の未適用通知は Resume まで保持します。
新世代の曲を開始した境界では前世代の完了通知を再適用しません。

## 2. Commands と観測値

`PlayTrack(library_generation, TrackId)` を追加し、選択・準備・成功後の再生を１つの
意図にします。schema 2 の `LoadTrack` も library generation を検証しますが、成功後は
Stopped に入り、暗黙には再生しません。不明 ID / 古い generation は現在の再生を
止める前に拒否します。既存の accepted は完了を意味せず、最終状態は snapshot で読みます。

`SetPlaybackPolicy` は次の全体を１回で置き換えます。初回の既定値は AlbumOrder / Default。
Q19 により、次回起動時には保存済みの有効な policy を復元します。

| フィールド | 値 |
| --- | --- |
| order | AlbumOrder / ShuffleLibrary |
| repeat | Default / RepeatOne / Counted |
| count | Counted のときだけ `u32` の1〜4,294,967,295。それ以外は absent |

Default の loop target は２、Counted は count、RepeatOne は無期限です。
policy は曲や library の ID と独立したプレイヤー設定として、曲切替・Stop・library の変更で
維持する案です。Q19 の回答により、旧案の「電源断保存を含めない」は撤回します。
保存対象は上表の３フィールドで、再生位置・選択曲・shuffle 履歴の復元や自動再生は含めません。
[設定保存契約案](pocket-playback-settings-contract.md) に形式・非同期 port・変更番号を定義し、
設定の適用・永続化の成功を別々に確認可能にします。Q24 の初期UIは2周→3周→5周→無限を
循環しますが、ここで定める Counted の有効範囲は変更しません。
コマンド queue は32、結果の replay window は
64件、ID / stale sequence / 投影状態の検証順は M0 の規則を継承します。
Pause/Resume 等を含む accepted command は先行 command の遷移中状態に対しても
検証し、非同期完了に伴う実行時失敗は snapshot error に出します。

schema 2 snapshot は既存の汎用情報に、次の型付き playback 情報を追加する案です。
MDX opcode や Pocket の物理ボタンは含めません。

- library generation、所有 AlbumId、play generation、適用済み policy。
- pending operation（原因 command ID、Prepare / Pause / Resume / Stop、対象 TrackId）。
  複数 command が保留なら、公開値は最後の有効な意図と現在実行中の制御を区別する。
- phase（Steady / Fading / RestoringGain）、gain 分子、ramp 経過と全 frame 数。
  transport=Paused と phase=Fading の組合せで、フェード途中の一時停止を表せる。
- 完了済み loop count、shuffle cycle ID / 総曲数 / 開始済み曲数、can_next / can_previous。
- [演奏履歴](pocket-performance-history-contract.md) の時刻・世代付き optional 観測窓。
- [設定保存](pocket-playback-settings-contract.md) の revision、適用 command の結果、保存状態。

completed loop count は `u64` の内部値から、既存表示用 `u32` 範囲に収まる場合だけ
公開し、範囲外は absent と明示的な overflow indication にします。wrap や無期限の sentinel に
しません。内部 `u64` 枯渇は bounded resource failure とし、先頭に戻して演奏を続けません。

## 3. 準備・再生・取消

play generation は PlayTrack / LoadTrack / Stop / library の切替・閉鎖で単調に進めます。
同一意図の accepted no-op では進めません。
準備完了、reset 完了、音声通知に元の世代を付け、古い完了は廃棄します。カウンターは
wrap させません。準備は bounded な作業単位で進め、入力処理を無期限に遮断しません。
同時に鳴る音源は１つで、曲切替は旧音の bounded reset / silence 確認後に新曲を開始します。
reset request は共用 device port 上で直列化し、新しい意図が古い ACK を自分の完了と
誤認しないようにします。実際の device fault / reset failure は世代の古さを理由に無視せず、
新しい曲の開始も止めます。取消した曲の準備結果と、共用音声系の故障を区別します。

| 有効な意図・イベント | 結果 |
| --- | --- |
| PlayTrack(T) | T を選択して Loading。旧曲を消音・resetし、準備成功後に位置０で Playing |
| LoadTrack(T) | 同じ準備を行い、成功後は位置０の Stopped |
| Loading 中に別の PlayTrack(U) | T の意図を失効させ、U だけが開始可能。古い成功・失敗で U を上書きしない |
| Loading 中に Stop | 再生意図を取消し、消音完了後 Stopped。対象選択は保持し、不要な準備を中断 |
| 未準備の選択を保持した Stopped で Play | 新世代で準備し、成功後に先頭から再生 |
| Playing で Pause / Paused で Resume または Play | 位置・音源状態を保持して停止／再開。実際の音声側 ACK 後に状態確定 |
| Playing で Play、Paused で Pause、Stopped で Stop | accepted no-op。進行中の同一制御を重複要求しない |
| Playing / Paused / Ended で Stop | 保留演奏を破棄、音源 reset、位置０、選択保持、shuffle cycle 終了 |
| Playing / Paused 中の明示的な選曲 | 選択曲を先頭から Playing。一時停止を次曲へ持ち越さない |
| 自然終端または fade 完了 | 下記 policy の次曲があれば Loading、なければ最終位置・選択保持の Ended |
| Ended で Play | 選択曲の先頭から新しい再生開始。shuffle なら新 cycle |
| 準備・演奏・音声制御の失敗 | bounded reset を要求して Error。自動で次候補を試さない |

Loading 中の Play / Pause / Resume / TogglePause は invalid_state とし、UI は A の
重複要求を送らないようにします。Empty での Play 等、未選択の再生操作も invalid_state。
Error での Play は拒否し、reset 成功済みの recoverable error なら新 PlayTrack / LoadTrack
で再準備できます。reset 自体の失敗は terminal device error で、音声出力を fail-closed に
保ちます。Stop が reset 成功したような表示へ置き換えることはありません。
CloseLibrary は取消・消音・参照失効後 Empty、OpenLibrary は同じ境界を経て Loading に入り、
検証成功後は選択なしの Stopped です。

## 4. 周回・自然終端・フェード

この MDX adapter の曲全体の周回は、各 active FM track の `TrackLoop` 通過回数の
最小値と定義する案です。初期値は０、TrackEnd 済みの track は最小値の対象から外し、
全 track が終われば自然終端です。短い RepeatStart / RepeatEnd / RepeatEscape は
周回を増やしません。音を出していない active track も数え、チャンネルの mute / solo や
描画状態では集合を変えません。これは本 profile の定義であり、外部 driver 全般と同一の
周回表示を保証するものではありません。

ある frame で複数 track の終端・loop が発生したら、同じ driver tick の結果をまとめて
適用した後に最小値を求めます。例えば A が３回、B が２回で、残りが TrackEnd なら曲は２周。
A が３回でも B が初回の長い区間を演奏中なら０周です。待機中 track も対象に残すため、
終わらない同期待ちを秒数 timeout で「正常終了」に変換しません。演奏中の停止操作は可能にし、
曲長・loop 判定が未確定でも架空の duration を公開しません。

有限曲は Default / Counted のいずれも自然終端で１回終了し、無音の５秒待ちを加えません。
RepeatOne だけが有限曲を先頭から再開します。loop 曲の RepeatOne はイントロをやり直さず
元の TrackLoop を続けます。Default / Counted は loop target 到達時から５秒フェードします。

フェードは YM2151 のレジスター書換ではなく、音声出力段で左右同じ利得を掛けます。
５秒は `D=240,000` stereo frames。利得は分母240,000の整数 G（0〜240,000）で表し、
開始値 S から目標 E へ、経過 n に対して
`G(n) = S + trunc_toward_zero((E-S) × n / D)`、`0 <= n <= D` とします。
乗算は signed 64-bit。出力 sample は `trunc_toward_zero(sample × G / 240000)`。
左右共通で適用し、０到達後は旧世代の演奏を破棄・resetして次へ進みます。
通常は S=240,000 / E=0。n=0 の frame は元の利得、n=240,000 の境界で０になるため、
ちょうど240,000 framesを経過して終了します。イントロ10秒＋区間30秒の２周なら、
frame 3,360,000でフェード開始、3,600,000で終了です。

## 5. 再生中・停止中の設定変更

曲順だけの変更は位置・周回数・gain ramp を変更しません。同じ policy の再送は
fade の５秒や shuffle をやり直さない no-op とします。

| 状態と新 repeat | 動作 |
| --- | --- |
| Steady、新しい回数に到達済み | 現在位置から５秒フェード開始 |
| Fading、新しい回数にも到達済み | 元の fade deadline を維持。５秒を再加算しない |
| Fading、RepeatOne または未到達の大きい回数へ変更 | fade を取り消し、同じ位置から演奏を続ける |
| Paused 中に変更 | policy と再開時の意図だけ更新。位置、周回、gain の経過は進めない |
| Stopped / Loading / Ended 中に変更 | 次に演奏する際の policy を更新。設定変更だけで再生を開始しない |

fade 取消時の段差を避けるため、現在利得から全利得へ20 ms（960 frames）で戻す案です。
補間式は前節と同じで D=960 / E=240,000。これは Q10 の終了 fade ５秒とは別の、
取消時だけの音量復帰条件です。復帰中に新たな終了条件を満たしたら、その時の利得から
改めて５秒 fade を始めます。Pause 中は両方の ramp を凍結し、条件変更の効果を Resume の
最初の frame から進めます。未開始 fade の取消には、架空の音量減少を適用しません。

## 6. 曲順と shuffle

AlbumOrder は現在曲の所有 album の次 ordinal を選び、最後の曲なら Ended になります。
RepeatOne 中はこの自動送りを行いません。明示的な NextTrack / PreviousTrack は repeat
設定より優先して隣の曲を先頭から再生し、端では no_next_track / no_previous_track を返します。
暗黙の先頭への wrap や「再生から数秒なら曲頭へ戻る」分岐は含めません。

ShuffleLibrary は全 TrackId の１つの permutation を作り、同じ cycle の自動選曲は各曲１回、
全曲を開始し終えた後の曲終了で Ended にします。現在曲を再生／一時停止した状態から
shuffle に入った場合、その曲を cycle の先頭として開始済みにし、残りだけを並べ替えます。
Loading 中に入る場合は対象の準備成功・再生開始時に先頭とします。曲の登録は最初の
再生開始時で、cursor を動かしただけ、準備失敗しただけでは開始済みにしません。

順序の元列は album display ordinal / track ordinal 順です。Fisher-Yates の末尾からの
交換で並べ替え、各段の一様な index は注入可能な RandomSource から取得します。
範囲 b では `r:u32 < floor(2^32/b)×b` のときだけ `r mod b` を使います。再抽選は各段32回
までとし、枯渇は resource error、偏った modulo fallback は行いません。host は入力乱数列を
固定して再現性を検証します。暗号用乱数を要件にせず、seed の生成は platform の責務です。

Pause / Resume、repeat の変更、同一 shuffle policy の再送では cycle を作り直しません。
RepeatOne の再開も同じ曲の滞在として扱います。ShuffleLibrary から AlbumOrder へ変えたら
cycle を終了し、現在曲の album の次へ進みます。再度 shuffle に入ると新 cycle です。
明示的な一覧からの PlayTrack、Stop 後の Play、Ended 後の Play は新しい cycle を始めます。

PreviousTrack は開始済み履歴の直前を再生します。この明示操作による再聴は自動選曲の
重複とは区別し、開始済み数を増やしません。履歴内では NextTrack は直後の履歴へ戻り、
履歴末尾からだけ未再生候補へ進みます。履歴を再聴した曲の自動終了では、履歴の後続を
再び自動再生せず、最初の未再生候補へ進みます。未再生がなければ停止します。
したがって手動の再聴があっても、自動選曲が cycle 内で同じ曲を再抽選することはありません。
準備失敗は Error で止め、黙って次候補へ飛ばしません。

## 7. 音声側の実装前提

現行 [queue v1](../../specs/core-rtl-device-queue-v1.md) の NOW は free-running、
[sound reset v1](../../specs/pocket-sound-reset-v1.md) は queue と JT51 の状態を破棄します。
どちらにも音源状態を保持する Pause や出力 gain ramp の契約はありません。
新機能には別 version / capability の制御 port が必要であり、CLEAR や予約 PUSH bit の
意味を変更して実装しません。Pause 対応がない backend は機能を unsupported とします。

新 port が満たすべき条件は以下です。実際の MMIO 配置と CDC は別の採用レビュー対象です。

- sample 境界で Pause を受け付けた位置を ACK し、media clock、pending writes、
  in-flight write、音源の oscillator / envelope、resampler・音声 FIFO の未出力状態を保持。
  部分的に渡した command を二重適用しない。保存しきれない backend は能力を宣言しない。
- 停止中も I2S framing / 実時間 clock は継続し、両 channel に０を出力する。停止中に
  挿入した出力 frame だけを除く全 sample 列が、中断しなかった基準列と同一となる。
- CPU の先読みは保持位置から再計算して二重投入せず、保留 queue と engine の整合性を保つ。
  最終音声 frame と scheduler timestamp の対応・固定 latency を契約化して測定する。
- gain ramp は UI / CPU 描画停止中も音声側で進む。Pause ACK後から Resume ACKまで凍結。
- 制御 ACK の deadline は媒体時刻ではなく実時間で bounded とし、失敗は silence/reset に移る。
  具体値は RTL の最大 in-flight latency から導き、根拠のない待ち時間で固定しない。

この条件は実装済み・実機確認済みではありません。音源 reset 後に表示位置だけ戻す方式や、
CPU だけ止めて queue を鳴らし切る方式では、Pause の合格条件を満たしません。
[Pocket adapter案](pocket-player-platform-adapters.md) は、全演奏状態のenable、常時I2S、
音声domainのmedia queue、取消可能なfade予約に方式を絞ります。audible commitと最大latencyを
RTLで証明した後に、具体的なMMIOとACK deadlineを採用します。

## 8. 合格例と実装順

| 独立した入力・操作 | 確認する結果 |
| --- | --- |
| intro10秒 / loop30秒、Default | 70秒で fade、75秒で終了。描画停止でも同じ frame |
| 有限曲に Counted=3 | 自然終端１回で次へ。RepeatOne の再開始とは区別 |
| A=3周 / B=2周 / 残り ended、短い nested repeat | ２周。nested repeat / mute で増減しない |
| 到達済み count への変更、fade 中の増減、Pause 中の変更 | 上表通りに現在位置と元の deadline を維持または取消 |
| PlayTrack(T), PlayTrack(U), Stop、完了通知を逆順に配送 | T/U とも勝手に再生しない。停止後に旧 end 通知で次曲へ進まない |
| 300曲 shuffle、固定乱数、Previous / Next、mode 再送 | 自動選曲の重複なし、１巡停止、意図しない再 seed なし |
| Pause 境界に pending / in-flight write と fade | 停止区間を除いた音声列が基準と一致。queue を失わず、位置も ramp も凍結 |
| 小さい負/正 sample、gain0/半分/全量、最後の frame | 丸め、左右同一利得、５秒境界を手計算と照合 |
| 準備失敗、ACK timeout、reset 失敗 | 明示 Error・bounded silence。自動スキップや偽の reset 成功なし |

次の実装計画は、(1) カタログ・一括取り込み、(2) host の状態機械と loop 通知、
(3) sound transport / gain の契約と RTL、(4) mock UI と差し替え可能な入力表、
(5) Pocket 統合・100/300曲での容量と操作確認、の依存順に分割します。
各段を別の小さな slice とし、全機能を一度に変更しません。

実装の現在地は [CURRENT](../CURRENT.md)、受入と実行結果は
[M6](../milestones/M6-album-player.md) を参照してください。M5の未完了gateはM6の
sound/storage、統合、実機sliceへ引き継いでおり、合格扱いにはしません。
過去のAPF2再試験を仕様決めの条件に戻しません。公開schema 2と残りの契約を段階的に採用し、
音声ポートの具体的なMMIO・ACK期限はRTLの根拠を得てから固定します。
