# Pocket の曲名変換と文字表示の設計案

Status: host portion adopted in [M6 slice 1](../milestones/M6-album-player.md), 2026-09-13.
ホスト部分の規範は [host metadata v1](../../specs/host-metadata-v1.md) です。
文字表示・font部分は 2026-09-20 に [Pocket bitmap canvas v1](../../specs/pocket-bitmap-canvas-v1.md)
として M6 slice 5 の内部 adapter に採用しました。Core/UIの公開 v1 契約は変更しません。
実際の framebuffer 接続・実機の判読性・取り込み時の欠字通知は引き続き統合作業です。
[ADR-0005](../adr/0005-m1-utility-language-and-normalization.md) の C++17 utility / 正規化済み
writer 境界を維持します。

## PC で変換を終える

MDX 内の曲名は、固定した Microsoft CP932 表で厳密にデコードする初期 profile を提案します。
UTF-8らしく見える場合の自動判別、ホストの locale / Windows ANSI code page は使いません。
この選択は X68000 全 driver の文字解釈と同一という主張ではありません。
たとえば `82 A0` は「あ」、`81 60` は U+FF5E、`B6` は U+FF76 です。
表にない値・不完全な２byte列は変換失敗とし、Q14のファイル名 fallback を使います。
根拠は [Microsoft CP932 table 2.01](https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP932.TXT)
です。取得した表の SHA-256 は
`c9bc0b0cd42e0fbcb82a09635bb5abed86afbdd4abc9e76fa5716638217cb59f`。

parser が返す original_title_bytes だけを対象とし、MDX の終端区切りや音楽本体を文字として
読み足しません。原本 blob / PDX依存名 / 音色番号は変換しません。最大4096 bytesの入力を
検証し、先行byteは後続byteの存在と表の組合せを確認してから消費します。
表にない外字も音楽構造の不正とは区別します。

CR / LF / TAB は表示用に空白へ置換し、前後の U+0020 / U+3000 を除きます。
その他の C0 / C1 制御文字・NULを含む曲名、空になった曲名は filename stem に fallback。
変換失敗・空・制御文字の理由は取り込み結果へ記録しますが、その理由だけで曲を除外しません。
ファイル名は Windows ならUTF-16から、UTF-8 filesystemなら厳密なUTF-8から取得し、
曲名同様にNFCにします。OSのファイルを開く際は元のnative pathを保持し、表示名をpathに戻しません。

NFC は [utf8proc v2.11.3](https://github.com/JuliaStrings/utf8proc/tree/v2.11.3) を host の
ingestion 層だけで使う案です。同版は Unicode 17.0.0を使い、NFCの指定は STABLE / COMPOSE。
明示したbyte長で変換し、NFKC・casefold・width foldを追加しません。
[同版の仕様](https://raw.githubusercontent.com/JuliaStrings/utf8proc/v2.11.3/README.md) に基づき、
`か + U+3099` は「が」へ合成しても、半角カナを勝手に全角へ変えない profile にします。
正規化失敗を「既にNFC」と扱いません。作業バッファは一文字列64 KiBまで、保存文字列は既存案の
4096 UTF-8 bytesまでとし、上限超過は明示したmetadata/容量エラーです。切詰めた名前をIDに使いません。
変換器が必要とする要素数とbyte数をchecked arithmeticで確認してから作業域を確保し、
無制限に確保するAPIを呼んだ後で上限を確認する実装にはしません。

フォルダーキー・ファイル名の自然順とIDは [カタログ案](pocket-album-catalog-contract.md) の
NFC bytesに従います。表示名への制御文字の置換や後述の省略は、IDや曲順の計算へ逆流させません。
NFCによるフォルダー/ファイル名の衝突は既存案通り明示的な取り込み失敗です。

## Pocket の renderer

Pocket は検証済みの UTF-8 を読み、等幅 bitmap を描画します。CP932 decoder、NFC library、
outline rasterizer を音声側へ載せません。現行 terminal の文字出力だけで日本語対応済みとはしません。
音名・16進列は8×16、主な日本語文字は16×16を初期候補にします。byte数と画面上の幅を混同せず、
桁数は glyph advance で計算します。制御文字をterminal escapeとして解釈しません。

フォント候補は [GNU Unifont Japanese 16.0.04 の hex](https://unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/)
です。CP932表の印字文字、そのNFC結果、`… ← ↑ → ↓ ∞ □ U+FFFD` の和集合を抽出し、
code point順の固定glyph表を生成します。runtimeに元のhex/gzipを読ませません。
この固定集合の外のUnicode文字もmetadataとして保持できますが、初期rendererでは欠字記号を
表示し、未対応glyphの存在を取り込み結果に記録します。フォントの欠字だけで曲を除外しません。
結合文字・異体字selector・複雑な文字組みは初期rendererの対応済み範囲に含めず、未対応を
黙って消しません。UTF-8の不正列はUI境界で拒否します。

描画はsnapshotの表示上限96 UTF-8 bytesを尊重し、glyph境界で領域へ収めて末尾を省略します。
API側で既に切詰められている場合も省略表示にし、全文を受け取ったようには見せません。
元のSTRSのmetadataをUIが書き換えません。長い曲名の自動横スクロールは初期案に追加せず、
読みやすさと表示密度はmock UIで試用してから調整します。

## 容量を確認した範囲

2026-09-13に上記CP932表とfont gzipを取得し、研究用
`python -B out/research/player-profile-20260913/inspect_font.py` で集合とbitmap幅を確認しました。
font gzipのSHA-256は `5ba84e901b9f7fad3bce0571c7e4b4b0aef4ce0acc4ee6622ef0cfa2eef38a6f`。
この計算はPythonのUCD 14.0.0によるNFCを使う参考調査で、提案したutf8procの採用試験ではありません。

| 実際に確認した項目 | 結果 |
| --- | --- |
| CP932 mapping entries / distinct printable code points | 7915 / 7484 |
| NFCによる追加 / 表示記号を含む集合 | 3 / 7488 glyph |
| 集合内の欠字 | 0 |
| 元bitmap幅 | 8 / 16 pixels、高さ16 |
| bitmap / 仮の8-byte index | 233,040 / 59,904 bytes |
| 合計（生成物のheader・alignmentを除く） | 292,944 bytes |

初期上限は8192 glyph、font blob全体512 KiBとします。生成器はglyph ID重複、bitmap長、
範囲・offset・上限を検証します。固定indexを二分検索でき、動的glyph cacheは不要です。
既存のUI/font作業域2 MiBの内数で、予算を追加したものではありません。
例として640×480 RGB565を２面持つと1,228,800 bytes、font予算と合わせ1,753,088 bytesで、
残り344,064 bytesです。これは配置計算であり、実際のframebuffer所有域との重複計上を
避けてELF/mapで再確認します。論理解像度・文字密度・日本語の判読性はまだ実機で確認していません。

## 依存物の扱いと採用条件

| 候補 | 用途・配置 | ライセンスと採用時の扱い |
| --- | --- | --- |
| CP932 table 2.01 | hostで固定decoder表を生成 | [Unicode dataの利用条件](https://www.unicode.org/license.txt) と元の出典を記録し、配布noticeへ含める |
| utf8proc 2.11.3 | host ingestionのNFCのみ | [MITと同梱Unicode data notice](https://raw.githubusercontent.com/JuliaStrings/utf8proc/v2.11.3/LICENSE.md) を維持。Pocket/runtime/writerへリンクしない |
| Unifont JP 16.0.04のglyph subset | build時生成、Pocketのread-only font | [配布元](https://unifoundry.com/unifont/index.html) が示すdual licenseから [SIL OFL 1.1](https://unifoundry.com/OFL-1.1.txt) を選ぶ。copyright/licenseを同梱し、派生fontは別名 RPCMP Bitmap JP としてOFLを維持 |

生成スクリプトや工具のライセンスをglyphのライセンスと混同しません。
M6 slice 1ではCP932とutf8procのsource hash/noticeを固定してhostに組み込みます。
font asset は slice 5 で採用し、source hash と notice を
[third_party/unifont](../../third_party/unifont/README.md) に固定しました。
同じ utf8proc で再計算した集合は 7,488 glyph、bitmap 233,040 bytes、
実装の明示的な3フィールド index は 89,856 bytesです。元glyphの省略記号と置換文字は8pixel幅で、
host SVG の仮16pixel幅とは異なります。実機の表示結果はまだ確認していません。

独立した試験例は CP932の上記３例、末尾の先行byte、空曲名、改行、半角カナ、NFC合成と
衝突、4096-byte境界、欠字、glyph index不正、96-byte/pixel幅での省略です。
host OS/localeを変えて同じmetadata/ID/出力bytesになることを確認します。
utf8procの固定Unicode版でglyph集合を再計算し、今回の参考計算を期待値として無検証で採用しません。
