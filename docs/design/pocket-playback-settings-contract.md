# Pocket 再生設定の保存契約案

Status: proposal, 2026-09-13. Q19 の電源断後の設定復元と Q24 のアイコン操作を具体化します。
保存方式・数値・port は設計提案であり、現行 v1 契約、APF 定義、M5 実装を変更しません。
再生の意味は [状態遷移案](pocket-player-transition-contract.md)、操作は
[画面案](pocket-tracker-screen-design.md) に従います。

## 保存対象とアイコンの値

保存する値は PlaybackPolicy の order / repeat / optional count だけです。
ライブラリや選択曲の ID、再生位置、音量、shuffle の順列・履歴、画面・フォーカスは含めません。
保存済み設定の復元は自動再生を起こしません。初回は AlbumOrder / Default です。

Q24 の確定した循環を、１回の A edge に対して次の１回の SetPlaybackPolicy に対応させます。
繰り返しアイコンは order を、シャッフルアイコンは repeat / count を維持します。

| 表示 | Core の値 | 次の A |
| --- | --- | --- |
| 2周 | Default、count absent | Counted(3) |
| 3周 | Counted、count 3 | Counted(5) |
| 5周 | Counted、count 5 | RepeatOne |
| 無限 | RepeatOne、count absent | Default |

シャッフルは AlbumOrder / ShuffleLibrary を交互に切り替えます。RepeatOne 中は shuffle
でも現在曲を繰り返し、無限を解除した後の終了時から自動送りが有効です。
有限曲は Default / Counted では１回で終了し、RepeatOne でだけ先頭から繰り返します。
初期 UI に自由入力の回数編集を追加しません。

Core の Counted は引き続き `u32` の1以上を受理する提案です。別の対応 consumer が保存した
4なども壊れた設定とは扱わず、`4周` と表示する案です。その場合の次の A は循環の先頭の
Default とします。Counted(2) も有効で、表示は2周、次の A は3周です。
操作の固定候補と Core の有効範囲を混同せず、保存時に勝手に近い値へ丸めません。

## 反映、保存、結果の所有者

Core が有効な policy 変更を適用したとき、session 内の `policy_revision: u64` を進めます。
初期値は1、同値 command は増やしません。SetPlaybackPolicy の適用は Q15 の通り現在位置を
維持し、保存完了を待ちません。UI は immutable snapshot の適用済み policy を描画します。

schema 2 の settings 状態には、policy_revision、optional persisted_revision、
`save_state = NotSaved | Pending | Writing | Saved | Failed | Unavailable` と、
optional の stable storage error code を提案します。Saved は現在の revision が永続化済みの
場合だけです。古い revision の成功は persisted_revision を更新できても、現在値を Saved に
しません。保存失敗は再生失敗と区別し、現在値を以前の設定へ巻き戻しません。
復元できた policy は session 内の revision 1 が保存済みです。保存番号は起動を越える番号で、
policy_revision は session 内の変更番号なので、同じ値である必要はありません。
復元結果は `restore_state = Reading | Restored | Missing | Recovered | Invalid | Conflict |
Unsupported | IoError | TimedOut` に分け、save_state と別に保持します。
初回は NotSaved、有効な復元は Saved、復元失敗は Failed、未知形式等で書けない場合は
Unavailableです。利用者の変更を保存できても、起動時に起きた復元失敗を成功へ書き換えません。
別に optional `last_policy_result { command_id, Applied | Failed, policy_revision, reason? }` を公開する案です。
同値の適用も Applied として通知し、UI が反映を待つ間に停止し続けないようにします。
これは保存完了の通知ではありません。

UI は保存を待ってボタンを固めず、下端に保存中・未保存・保存失敗を表示できます。
設定 icon は Core の反映結果を表示し、保存状態の通知がフォーカスを移すことはありません。
settings 用の失敗表示を transport Error や SOUND RESET 成功に置き換えません。

Core の外部I/Oは PlaybackSettingsPort に隔離します。port の completion は request ID と
revision を持つ値で、Core owner が照合します。UI、engine、音声 callback が filesystem に
アクセスしません。host は同じ port に memory / fault-injection adapter を差し込めます。

| port 操作 | 契約 |
| --- | --- |
| BeginRead(request_id) | 二つの bounded slot を読む。Missing / bytes / Unsupported / I/O error を区別 |
| BeginCommit(request_id, slot, record[64], revision) | 一つの記録を保存。開始後はこの bytes を変更しない |
| Poll | 即時復帰する bounded な進捗取得。Pending または元 request ID の completion |

初期 read と commit は直列化し、同時進行は１操作までです。保存待ちは最新 revision の
１件だけを保持し、変更の度に無制限の write queue を伸ばしません。最後の変更から250 msの
monotonic wall time を経過したら保存を始める案です。Pause中も時間が進みます。
書込み中に新しい変更が来たら、開始済みの保存を完了した後で最新値を保存します。
失敗は明示して自動で無期限に再試行せず、次の有効な設定変更で再試行可能とします。
書込みの失敗・timeout 後は最新枠が変わっている可能性があるため、再試行前に静止確認と
両枠の再読込・検証を行います。再読込は保存先と次番号の決定用で、現在の policy を復元値で
置き換えません。結果不明の枠を、古いキャッシュの番号だけで上書きしません。

adapter の read / commit deadline とキャンセル後の静止確認は採用時の必須項目です。
timeout 後に古い I/O がまだ書き得る間は次の write を始めません。静止を確認できなければ
その session の保存を Unavailable にして音声・操作は継続します。単なる UI の待ち時間を
I/O キャンセル成功と扱いません。revision / request ID / 保存番号は wrap させません。
保存番号枯渇は保存を Unavailable にします。policy revision 枯渇時は設定変更を
resource_exhausted で拒否し、適用済み policy と演奏を維持します。

## 二枠のレコード形式

adapter に論理 slot A / B を用意し、それぞれ64 bytesの独立した記録とします。
物理ファイル名、APF data slot、予約領域や filesystem flush API は本書では割り当てません。
全整数は little-endian、レコード長は正確に64。構造体のメモリーを直接書き出しません。

| offset | bytes | 値 |
| --- | --- | --- |
| 0 | 4 | ASCII `RPS1` |
| 4 | 2 | format version = 1 |
| 6 | 2 | record bytes = 64 |
| 8 | 8 | 保存番号。1以上の `u64` |
| 16 | 1 | order: 0 AlbumOrder / 1 ShuffleLibrary |
| 17 | 1 | repeat: 0 Default / 1 RepeatOne / 2 Counted |
| 18 | 2 | flags: bit0 count present、それ以外0 |
| 20 | 4 | Countedなら1以上。非Countedではflag0かつbytesは0 |
| 24 | 36 | reserved、すべて0 |
| 60 | 4 | bytes 0〜59 の CRC-32/ISO-HDLC |

CRC は reflected polynomial `0xEDB88320`、initial / xor-out `0xFFFFFFFF`、refin/refout true。
方式確認の独立した既知値は ASCII `123456789` に対する `0xCBF43926` です。
非Countedの0は absence flag と併用する canonical encoding で、無限を表す count sentinel
ではありません。不正 version / length / enum / flag / reserved / CRC を検証してから値を使い、
読込長を信頼した可変長割当はしません。未知 version は Unsupported と区別します。

両枠が有効なら保存番号の大きい方を採用します。同番号・同 bytes は同一記録としてよく、
同番号・異なる bytes は競合として復元せず通知します。片方だけ有効ならそれを復元し、
もう片方が不正である場合は回復したことを表示します。
I/O error は検証できた不正記録とは異なります。一方でも読めない枠があれば最新値を判定
できないため、復元失敗として扱い、両枠を再読込できるまで新しい保存を開始しません。
双方 Missing の初回は既定値 / NotSaved。双方不正、競合、I/O failure のときは明示した
復元失敗と既定値を表示し、自動で両枠を上書きしません。
Missing や破損の次に利用者が設定を変更したときは新しい保存を要求できます。
未知 version がどちらかにあれば内容を保護してこの session の保存を Unavailable とします。
既知で有効な枠がある場合はその policy を利用できても、復元が完全だったとは表示しません。
未知形式の移行・消去は別の明示的な処理が必要で、通常起動で破棄しません。

初期 read が完了するまで、UI は設定読込中を示し、選曲開始と policy 変更を有効にしません。
deadline に達したら復元失敗を明示した既定値で使用を開始し、遅い read 完了は捨てます。
利用者が変更した値を後から復元値で上書きしません。read timeout 後の保存は、元 I/O の
静止を確認するまでは開始しません。

保存先は、有効な最新枠とは別の枠です。有効枠がない場合は A、同一の記録が両枠なら B に
書きます。番号は検証済みの最大値+1、検証済み記録がなければ1とします。不正記録に含まれる
未検証の番号を採用しません。以前の最新枠を消去してから新しい値を書く手順は禁止します。
保存完了は adapter の永続化保証と読戻し一致の両方を必要とし、readback だけでは不十分です。
二枠は途中で切れた記録への論理的な対策であり、媒体全体・filesystem metadata の破損耐性を
保証しません。Pocket での永続化手段・deadline・電源断試験が未確定なら Saved を実装済みと
主張できず、この port の契約案だけで Q19 のハードウェア受入を完了しません。

## 採用時の受入例

| 条件 | 期待値 |
| --- | --- |
| 2周から A を４回、それぞれ反映完了後に押す | 3周→5周→無限→2周。order は不変 |
| rev2の保存中にrev3へ変更、rev2だけ成功 | 現在値はrev3、Savedではない。次にrev3を保存 |
| 両枠 Missing / 有効なShuffleLibrary+Counted(5)で起動 | 前者は既定値、後者は同じpolicy。どちらも自動再生しない |
| Aに番号7、Bの番号8を書込中に各byte境界で電源断を模擬 | 完全で有効な8か、残った7を選ぶ。途中のpolicyを混合しない |
| 不正CRC/enum/count、同番号競合、未知version | 分類した復元失敗を表示。未知versionを通常起動で上書きしない |
| 永続化失敗、readbackだけ成功、timeout後の遅い完了 | Savedにしない。現在の演奏を保持し、重なるwriteを始めない |
| Pause中の変更、UIなし、連続変更、番号枯渇 | 保存時計は動作。待ち領域は１件。音声は保存処理を待たない |

これらは採用時の試験仕様です。host の fault injection と、実媒体の電源断・再起動試験は
別の証拠であり、今回実行した試験ではありません。
