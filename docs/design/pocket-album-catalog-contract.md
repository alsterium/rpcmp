# Pocket アルバムカタログ契約案

Status: proposal, 2026-09-13. [詳細仕様案](pocket-library-player-spec-draft.md) の
保存形式を具体化する設計文書です。以下のバイト配置は提案であり、現行の
[Container v1](../../specs/rpcmlib-v1.md)、writer、M5 一曲 profile は変更しません。
契約採用時に `specs/` へ移し、実装と独立した fixture を先に定義します。

## 1. 互換性と識別

新しい Pocket album profile は v1 の必須６ section に加えて `ALBM` を要求します。
envelope の major/minor は1/0、feature flags は０を維持し、`ALBM` の directory
flags は０、alignment は８とします。旧 reader は未知 optional section として
CRC を検証して無視できます。新 profile は欠落・未対応 version・論理不正を拒否し、
曲順を TrackId 順へ黙って置き換えません。

追加対応 writer は必須６ payload の後に `ALBM` を出力します。既存 writer API の
出力は変えません。header build ID は v1 通り必須６ payload だけから生成するため、
**曲順だけを変えた場合に同じ build ID になることがあります**。成果物全体の同一性は
ファイル全体の SHA-256 で比較し、実行中の参照失効には後述の library generation を
使います。ALBM を含めるよう既存 build ID のハッシュ規則を変更しません。

フォルダーキーはルートからの相対フォルダー名を NFC UTF-8 にし、区切りを `/` に
統一した値です。ルート自身だけを `.` と表します。その他のキーは空 component、
`.` / `..` component、先頭・末尾 `/`、NUL、`\`、絶対パスを含めません。
通常ファイル・ディレクトリーだけを走査し、symbolic link / junction を追いません。
キーは識別用文字列であり、Pocket でファイルを開くパスとして使いません。
正規化後に同じキーへ潰れる別フォルダーは、取り込み全体の命名衝突エラーです。

`AlbumId` は非ゼロ `u64` です。SHA-256 の先頭８ bytes を little-endian として読む
方式で、入力を `"album\0" || key_byte_length:u32le || key_utf8` とします。
キーを UTF-8 バイト順に整列してから割り当てます。０または別キーの割当済み ID に
衝突したら、元のハッシュ入力へ `counter:u32le` を１から付け直して再ハッシュします。
counter の枯渇は全体エラーです。reader は解決済み ID を使い、ID の一意性・非ゼロと
キーの一意性を検証します。既存 TrackId / BlobId / StringId の算出は変更しません。
キーが同じなら、親となる PC ルートの場所を移動しても AlbumId は同じです。

## 2. 決定的な表示順

アルバムはフォルダーキー、曲は拡張子を除いたファイル名を比較します。
双方を NFC に正規化し、ASCII 数字 run とその他の UTF-8 byte run に分けます。
先頭から対応する run を比較し、数字同士は先行ゼロを除いた桁数、続いて数字の
バイト順で比較します。全ゼロの値は１桁の `0` と同じ数値です。数字以外同士は
バイト辞書順、種類が違う場合は先頭 byte の値で比較します。同値 run は次へ進み、
片方だけ終わったら短い方が先です。全 run が同値なら正規化済みの全文字列の
バイト順で決めます。曲でなお同じなら拡張子を含むファイル名のバイト順を使います。
それも一致する別ファイルは命名衝突として拒否します。locale と大小文字の同一視は
使わず、数字を固定幅整数へ変換しません。

手計算で決まる例は `1.mdx, 02.mdx, 2.mdx, 10.mdx` です。`02` と `2` の数値は
等しく、最後の全文バイト比較で `02` が先です。表示タイトルを変えてもこの順序は
変わりません。除外後の採用曲だけで ordinal を０から振り直し、空アルバムは出力しません。

同名の別フォルダーは別 AlbumId です。ただし既存 TrackId は音楽とメタデータから
決まるので、別入力でも同じ TrackId になる場合があります。この初期 profile では
同一 TrackId の複数配置を表現しません。重複 TrackId は既存 writer 通り全体エラーとし、
ローカル報告で該当する入力を示します。曲を黙って落としたり、既存 ID 規則に
ファイル名を足したりしません。これは未対応・破損曲の個別除外とは別のエラーです。

## 3. ALBM payload

すべて unsigned little-endian、section-relative offset です。header は32 bytes。

| Offset | Bytes | 値 |
| ---: | ---: | --- |
| 0 | 2 | payload major = 1 |
| 2 | 2 | payload minor = 0 |
| 4 | 2 | header size = 32 |
| 6 | 2 | album record size = 40 |
| 8 | 2 | membership record size = 16 |
| 10 | 2 | reserved = 0 |
| 12 | 4 | album count A |
| 16 | 4 | membership count T |
| 20 | 4 | reserved = 0 |
| 24 | 8 | membership array offset = 32 + 40 × A |

album array は offset 32 から始まり、AlbumId の昇順です。

| Record offset | Bytes | 値 |
| ---: | ---: | --- |
| 0 | 8 | AlbumId |
| 8 | 8 | album name StringId |
| 16 | 8 | folder key StringId |
| 24 | 4 | album display ordinal |
| 28 | 4 | first membership index |
| 32 | 4 | track count |
| 36 | 4 | reserved = 0 |

membership array は album record 順に各アルバムの連続範囲を並べます。

| Record offset | Bytes | 値 |
| ---: | ---: | --- |
| 0 | 8 | TrackId |
| 8 | 4 | album 内の track ordinal |
| 12 | 4 | reserved = 0 |

所有 AlbumId は album record の範囲から決まり、membership ごとには重複保存しません。
payload 長は正確に `32 + 40 × A + 16 × T` とし、末尾の余剰 bytes を認めません。
将来 version での変更を区別するため、この profile は payload 1.0 のみ受理します。
reserved は writer が０にし、reader は v1 の拡張規則に合わせて無視します。

公開前に以下をすべて検証します。失敗時に途中のカタログを公開しません。

- envelope / section CRC と必須６ section の既存検証を通す。
- 計算の加算・乗算・整数変換を検査し、A/T は1〜300、T は TRAK count と一致。
  範囲検査と上限検査を allocation / access より先に行う。
- AlbumId は非ゼロかつ strict ascending。名前・キーは存在する非空 STRS 参照で、
  キーは正規形かつ重複なし。ルート以外の名前はキーの最終 component と一致。
  ルートの名前は取り込み元ルート名として格納し、Pocket でホスト名を推測しない。
- album display ordinal は `[0,A)` の重複なし全要素。保存位置と表示位置を混同しない。
- 各 album の track count は正、範囲は array 全体を隙間・重複なく分割。
  各範囲の membership ordinal は物理順に `0..count-1` と一致。
- 全 TrackId がちょうど１回現れ、参照先 TRAK が存在。TRAK の album StringId と
  所有 album の name StringId が一致。TRAK 自体は従来通り TrackId 順。

最大 A=T=300 の ALBM は `32 + 12,000 + 4,800 = 16,832 bytes` です。
STRS のキー・名前と header/directory は別途32 MiB全体上限へ算入します。

## 4. 読み取り専用カタログ API

これは新 profile の論理 API 案であり、既存 `LogicalLibrary` の ABI を固定しません。

```text
AlbumPage(generation, start_display_ordinal:u32, limit:u16)
TrackPage(generation, album_id:AlbumId, start_track_ordinal:u32, limit:u16)
```

limit は1〜16。start は `0..total` を許可し、total と同じなら空の最終ページを返し、
超過は拒否します。結果は generation、total、各項目の論理 ID と display ordinal、
コピーされた表示メタデータ、次ページ開始位置（存在するときだけ）です。AlbumSummary は
AlbumId / 名前 / 曲数、TrackSummary は TrackId / タイトル / 所有 AlbumId を含みます。
公開値は container の array index、offset、folder key、借用 mutable pointer を含みません。
各表示文字列は最大96 UTF-8 bytesとし、切り詰め時は code point 境界を保ち、truncated を
明示します。保存側の4096 bytes上限とは別で、ID と曲順は表示短縮から影響を受けません。

generation は Core session 内で非ゼロ単調増加する `u64` です。open/close/reload の
開始時に既存参照を失効させ、検証成功後だけ新 generation のカタログを公開します。
カウンターを wrap せず、枯渇は session 再作成を要する resource error にします。
library の検証中 / Empty / library Error では unavailable、別 generation は stale_library、
存在しない AlbumId は unknown_album として拒否します。読める snapshot が後から
古くなっても Core を変更せず、選曲時も同じ generation を検証します。
同じ library 内で曲を準備する Loading はカタログを失効させず、引き続き閲覧できます。

## 5. 実装前に用意する独立した合格例

| ケース | 期待する保証 |
| --- | --- |
| ２ album × ２ track、ID 順と表示順が逆 | オフセット・範囲を手計算した fixture 通りに表示・再生順を復元 |
| 走査順を逆転、ルート親を移動 | 同一の正規化入力から同一 bytes / ID。時刻・絶対パスなし |
| 曲名だけ変更、数字100桁、先行ゼロ、同名フォルダー | 曲名は順序に影響せず、整数 overflow / album 混同なし |
| ALBM のみ変更、CRC 再計算済み | 旧 build ID は不変でも全ファイル hash が変化し、reload で旧 generation は失効 |
| ０/300/301件、32 MiB境界、切れた header、巨大 count | 書き込み前・allocation 前に規定通り受理または拒否 |
| 重複 ID、欠落参照、順序の穴、範囲重複、album 名不一致 | CRC が正しくても論理不正を拒否 |
| ALBM なし / 未対応 version | 旧 M5 は旧契約通り、新 profile は明示的に拒否 |
| page 16/17件、末尾、stale generation | bounded なページと拒否理由。UI に保存位置を漏らさない |

これらは試験設計であり、fixture 実装や実行済みの合格結果ではありません。
文字コード変換/NFC の依存、重複入力の将来の複数配置、同名アルバムの見分け方は
この保存形式で対応済みとは主張しません。
