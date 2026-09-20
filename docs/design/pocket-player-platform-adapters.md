# Pocket の設定保存・音声制御アダプター案

2026-09-20 更新：ユーザー指示により設定の永続保存は将来対応に延期しました。
今回は毎起動で既定値に戻し、保存用 APF/SDK/RAM 接続と媒体検証を完成条件から外します。
以下の保存部分は将来用の提案です。音声制御と Pocket 統合は引き続き進めます。

Status: proposal, 2026-09-13. [設定保存](pocket-playback-settings-contract.md) と
[再生状態遷移](pocket-player-transition-contract.md) をPocketへ接続する方式案です。
公式APF仕様は同日オンライン確認しました。新しいRTL/SDKや公開v1契約は実装・変更していません。
調査を区切った過去の起動障害の再試験を、この設計の開始条件に戻しません。

2026-09-20: 設定保存のうちコマンド転送層を
[APF flush transport v1](../../specs/pocket-apf-flush-v1.md) として採用しました。
CPU/CDC/Target handler の flush 発行を実装し、独立クロックで検証しています。
同日の [settings RAM owner v1](../../specs/pocket-settings-ram-v1.md) は、
二面の bank、所有権の保持、独立した読戻し領域を単一クロック境界として実装します。
物理アドレス、CPU/BRIDGE CDC、APF size table、OS arbiter、媒体の読戻しと期限の
接続は引き続き未実装部分です。RAM 単体の検証を SD 保存の成功とは扱いません。

## 設定は小さいnonvolatile slotで保持する案

APFにはnonvolatile slot、coreごとの固定ファイル名、Targetのread/write/flushがあります。
nonvolatile slotは終了時にも保存され、flushでData Slot Size Tableの値が使われます。
[data.json](https://www.analogue.co/developer/docs/core-definition-files/data-json) と
[Host/Target Commands](https://www.analogue.co/developer/docs/host-target-commands) が根拠です。
これを用い、二つの64-byte記録は通常ロードされる小さいRAM slotに置く案を選びます。
大きいライブラリ用のdeferloadを、設定の128 bytesのために流用する必要はありません。

| 項目 | このprofileの候補 |
| --- | --- |
| 論理slot A / B | APF ID 6 / 7。採用packageの全slot表と衝突を検査 |
| filename | `player-a.sav` / `player-b.sav`。曲や選択library名から派生させない |
| required / nonvolatile / deferload | false / true / false |
| parameters | bit1だけ有効（2、core-specific）。slot0からの命名、read-only、reloadは無効 |
| size_maximum | 64。ロードされた実長が64かはCoreで確認。未作成はsize0 |
| address | Core専用のBRIDGE RAM窓２つ。CPUからの別名、デコーダ、reset所有者を採用時に固定 |

現行M5 packageのID0〜4には触れません。ID6/7やaddressをここで製品の予約済み領域にはしません。
APF上の配置は `/Saves/<platform>/<core-id>/player-a.sav` とBです。配布物にユーザーの
save fileや毎回上書きする初期saveを含めません。core/platform ID変更時は保存先が変わるため、
製品IDを採用時に固定し、実験用core名をそのまま引き継ぎません。

起動前にRAMを既知の値で初期化し、APFのload完了と実長を確認してから二枠を検証します。
未作成、CRC不正、未知formatを区別し、既存の復元規則に従います。64 bytesを越えるファイルは
APFロード境界で拒否されるため、これも実機受入に含めます。RAMの初期値を有効な保存記録と
誤認しません。未知formatのRAM内容を、既定値のrecordで上書きして終了時保存させません。
初期復元は起動時にロードされたRAMを使いますが、保存失敗後の再試行用の読込みはSDから
別staging領域へ行います。更新済みのRAM mirrorを読み直すだけでは、媒体の最新番号を判定できません。

commitの流れは **候補record作成 → 対象RAMへ原子的に公開 → size更新 → Target flush →
SDからreadback → 完了通知** です。最新の有効なもう一枠は維持します。
Target `0x0188` の成功後、`0x0180` で64 bytesを別のstaging領域へ読み、一致した場合だけ
Savedにします。readbackの行先を保存用RAMにして、証拠を自己コピーで作りません。
これらのコマンドは [公式の定義](https://www.analogue.co/developer/docs/host-target-commands) に従います。
通常nonvolatile slotでの明示flush、未作成ファイルの生成、SDからのreadbackの組合せは、
このprofileの実機確認項目です。今回の資料確認だけで、その組合せの実機成功を主張しません。

CPUの16回のword storeの途中をAPFに見せないよう、RAM slotごとに64 bytesの二面を用意し、
検証済み候補の公開はbank切替で行う案です。合計256 bytes、読み戻し用は別に64 bytes。
flush/Host読出し中のbankは固定します。RAMとbank選択は音声reset・app resetで消さず、
Host Reset Enter後もAPFの終了時読出しに応答できる所有域に置きます。
boot load時のHost書込みとruntimeのCPU公開を、同じbankへ同時に許可しません。
Host読出し、size更新、Target完了、CRCを使った途中切断の確認がRTL/adapter受入条件です。

新adapterは全Target commandをOS側の一つのarbiterで直列化します。Core/UIが既存OSとは
別にAPFのcommand mailboxを書き換える構造にしません。Begin/Pollは即時復帰し、音声制御、
PAD heartbeat、Host command対応は保存待ち中も動かします。250 msの保存まとめは既存案通りです。
APFの完了時間の上限は公式ページで保証されていません。各Target操作を実時間1秒で打切り判定
する初期watchdogを提案しますが、これは性能保証・キャンセル成功の意味ではありません。
timeoutならそのsessionの新しいTarget保存を止め、元要求の静止を確認するまでbufferを再利用しません。
正常保存時間・終了時保存・電源断耐性はfirmware 2.6の実測で別々に確認します。

### 現行基盤との差分として分かったこと

固定SDK `a408ddc` の `of_file.h` は非同期readを公開しています。runtime-producing core
`618a3eb985759a4154115109c2c8036271252888` の
[file.c](https://github.com/openfpgaOS/openfpgaCore/blob/618a3eb985759a4154115109c2c8036271252888/src/firmware/os/targets/pocket/file.c)
ではslot writeが待機ループを通り、
[core_bridge_cmd.v](https://github.com/openfpgaOS/openfpgaCore/blob/618a3eb985759a4154115109c2c8036271252888/src/fpga/targets/pocket/core_bridge_cmd.v)
のTarget発行分岐にはread/write/get/openがあり、flush発行分岐はありません。
今回、同revisionのlocal sourceを読んで確認した範囲です。
したがって `fwrite` / `fclose` を単に呼べばこの非同期保存契約を満たすとは扱いません。
専用RAM窓、flush要求/完了、SDKサービスのcapability、Hostとの調停を一つの採用sliceで追加し、
ROM/OS/SDK/RTLの対応する組合せを検証する必要があります。現在の実装との契約不一致を
黙って修正する作業ではなく、未実装の新機能を定義する提案です。

## 一時停止とフェードの接続方式

現行 [Queue v1](../../specs/core-rtl-device-queue-v1.md) はCPUのfree-running時刻を使い、
[Reset v1](../../specs/pocket-sound-reset-v1.md) は音源とqueueを破棄します。
実際の [JT51 wrapper](../../core/rtl/pocket/rpcmp_jt51_audio.sv) と
[audio adapter](../../core/rtl/pocket/rpcmp_pocket_audio.sv) にも、全状態を保持するfreezeやgainはありません。
後者はrate phase、未出力のpending sample、I2S serial phaseを一つのclockで進めています。
cenだけを止めて一時停止対応とする前に、vendor内を含む全状態のholdを確認する必要があります。

新profileは、**演奏状態を進めるenable** と **常時動作する出力・制御** を分ける案です。
I2Sは [APF AUDIO](https://www.analogue.co/developer/docs/bus-communication) の48 kHz / 16-bit stereo /
12.288 MHzを維持します。fabric clockの組合せANDで停止clockを作りません。

| 演奏enableで保持するもの | 停止中も動かすもの |
| --- | --- |
| JT51の全演算・register処理状態、wrapperの書込み途中 | APF I2S serializer。左右0の完全なframeを出す |
| fractional clock-enable phase、resampling phase、pending sample | CPU、入力、UI、APF Host/Target、heartbeat |
| media時刻、device queueの実行位置、gain rampの経過 | pause/resume/reset要求とACKのCDC、実時間watchdog |

Pauseは次のstereo frame境界で成立し、その境界で消費するはずのpending sampleを保持します。
次の未再生sampleから再開するので、鳴っている途中のframeを半分だけ0にしません。
Resumeもframe境界で成立し、保持していたsample・write・rate phaseを一度だけ進めます。
vendor内部のRAM write enable、cen非依存のregisterまでholdの対象を列挙し、
保持の抜けをsimulationで検出します。resetはholdより常に優先し、Pause中でもStop/faultの
resetが成立します。通常のPauseではresetを要求しません。

CPU時刻のQueue v1を停止しているように偽装せず、音声domainが所有するmedia sub-tick
（12,288,000 Hz、256 ticks/frame）でdueを比較する新queue profileを提案します。
CPU→audioはepoch / due / payloadを一体で渡し、enqueue成功後はbundleを変更しません。
Pause中も受付側のCDCは動作でき、boundedな満杯はbackpressureです。演奏のdispatchだけが止まります。
現在のMDX driverの4 MHz基準とJT51の3,579,545 Hzの設定はこの機能追加では変更せず、
新しいmedia tickへの変換だけを剰余付き整数演算にします。

gainはresampling後、I2Sへ渡すstereo sampleに同じ倍率を掛けます。rampの240,000 frames /
20 ms復帰 / 整数丸めは状態遷移案を継承します。Pause中は経過を進めません。
自動fadeの開始をCPUが後から通知する方式では開始frameが遅れ得るため、先読みで判明した
終了条件はepoch / policy revision付きの予約として送り、音声境界で成立させる設計が必要です。
今の曲への設定変更は古い予約を無効にします。単に将来gain値をqueueへ入れて変更不能にしません。

履歴のaudible commitも、CPU投入時刻やqueueから取り出した時刻ではありません。
writeの反映、JT51のpipeline、resampling、出力frameまでの対応が必要です。
新しいcontrol portの意味上の要求は SetEpoch / Pause / Resume / ScheduleRamp /
CancelPolicyReservations / CapturePosition とし、完了値は request ID / epoch / status /
media frame / gainを持つimmutableな束として返す案です。
予約取消と同じ境界のfade開始、古いepochの完了を、新profileの順序規則で一意に解決します。

## 採用前に証拠が必要な部分

音声の方式は上記へ絞りますが、MMIO offsetやACK上限を今のRTLから確定できる段階ではありません。
特に全JT51状態のfreeze、policy予約の取消、観測tagと実際の出力frameの対応を先に証明します。
これらを曖昧にしたまま、予約bitの転用や固定sleepで新しい能力を宣言しません。

| 確認対象 | 実装採用を判断する証拠 |
| --- | --- |
| Pause/Resume | address/data書込み途中、pending sample、各resampler phaseで停止し、挿入した無音frameを除いた全sample列が基準と一致 |
| fade予約 | 70秒開始/75秒終了、直前のpolicy変更・Stop・Pauseの独立trace。deadlineを後ろへずらして合格させない |
| audible commit | 対象writeと出力sampleの対応を既知のRTL刺激で導出。queue ACKだけを根拠にしない |
| MMIO/CDC/ACK | 上記状態機械の最大latencyからdeadlineを計算。要求bundle保持、同期、全reset世代の扱いをreview |
| 設定保存 | flush実装、実RAM bank/sizeの整合、連打・Host終了・timeout・各byte境界の途中切断。実SDでの再起動/電源断は別に確認 |

次の実装は、先にhostで検証できる取り込み・文字・mock UIを独立sliceにできます。
音声能力を使う統合sliceは、この表の証拠と新しいpublic port契約の採用を前提にします。
M5のtiming/CDC/配置・production promotionの残gateは、次milestone採用時に所有先を明記します。
