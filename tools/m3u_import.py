"""M3UのMDXと依存PDXから、Minimal Player r1用の曲集を生成します。"""
import argparse
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import zipfile

import prepared_playlist

LIST_LIMIT = 1024 * 1024
MUSIC_LIMIT = prepared_playlist.PAIR_LIMIT
ASSET = Path("Assets/rpcmp_minimal/common/playlist.hpl")


def read_bounded(path, limit):
    if not path.is_file():
        raise ValueError("通常のファイルではありません")
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if not data or len(data) > limit:
        raise ValueError(f"ファイルが空、または上限 {limit} バイトを超えています")
    return data


def unpack(data, limit=MUSIC_LIMIT):
    """Bounded LZX token decoder; format provenance is in the design document.

    Control bits are MSB-first, interleaved with literal/offset bytes. Matches
    have a signed 8/13-bit distance; a long match with extension zero ends it.
    """
    if data[4:8] != b"LZX ":
        if len(data) > limit:
            raise ValueError("展開サイズが上限を超えています")
        return data
    if len(data) < 46:
        raise ValueError("LZXヘッダーが途中で切れています")
    size = int.from_bytes(data[18:22], "big")
    if not 0 < size <= limit:
        raise ValueError("LZX展開サイズが範囲外です")
    marker = next((i for i in range(38, len(data) - 3, 2)
                   if data[i:i + 4] == b"\x7f\xff\xffL"), None)
    if marker is None:
        raise ValueError("LZX開始マーカーがありません")
    position, control, bits = marker + 4, 0, 0
    output = bytearray()

    def byte():
        nonlocal position
        if position >= len(data):
            raise ValueError("LZXデータが途中で切れています")
        value = data[position]
        position += 1
        return value

    def bit():
        nonlocal control, bits
        if bits == 0:
            control, bits = byte(), 8
        bits -= 1
        return (control >> bits) & 1

    while True:
        if bit():
            if len(output) >= size:
                raise ValueError("LZXが宣言サイズを超えました")
            output.append(byte())
            continue
        if bit() == 0:
            count = 2 + 2 * bit() + bit()
            distance = 256 - byte()
        else:
            high, low = byte(), byte()
            distance = 8192 - ((high << 5) | (low >> 3))
            count = (low & 7) + 2
            if count == 2:
                count = byte() + 1
                if count == 1:
                    if len(output) != size:
                        raise ValueError("LZX展開サイズが宣言と一致しません")
                    return bytes(output)
        if distance > len(output) or len(output) + count > size:
            raise ValueError("LZX参照位置または展開サイズが範囲外です")
        for _ in range(count):
            output.append(output[-distance])


def local_path(name, base, root):
    """Resolve each component without reading outside the chosen music root."""
    name = name.replace("\\", "/")
    if (not name or name.startswith("//") or "://" in name or
            any(ord(c) < 32 or ord(c) == 127 for c in name) or
            ":" in (name[2:] if re.match(r"^[A-Za-z]:/", name) else name)):
        raise ValueError("ローカルファイルのパスを指定してください")
    if os.name != "nt" and re.match(r"^[A-Za-z]:", name):
        raise ValueError("この環境ではWindowsドライブのパスを使えません")
    candidate = Path(os.path.abspath(base / name))
    if not candidate.is_relative_to(root):
        raise ValueError("音楽ルートの外を参照しています（--rootを確認）")
    current = root
    for part in candidate.relative_to(root).parts:
        matches = [p for p in current.iterdir() if p.name.casefold() == part.casefold()]
        if not matches:
            raise FileNotFoundError(f"ファイルまたはフォルダーがありません: {part}")
        if len(matches) != 1:
            raise ValueError(f"大文字・小文字だけが異なる候補が複数あります: {part}")
        current = matches[0].resolve()
        if not current.is_relative_to(root):
            raise ValueError("リンク先が音楽ルートの外です")
    return current


def pdx_path(reference, mdx, root, search):
    if Path(reference.replace("\\", "/")).is_absolute() or ":" in reference:
        raise ValueError("PDX参照は相対パスで指定してください")
    names = [reference] if reference.lower().endswith(".pdx") else [reference, reference + ".pdx"]
    for directories in ([mdx.parent], search):
        matches = set()
        for directory in directories:
            for name in names:
                try:
                    matches.add(local_path(name, directory, root))
                except FileNotFoundError:
                    continue
        if len(matches) > 1:
            raise ValueError("PDX候補が複数あります。配置または--pdx-dirを確認してください")
        if matches:
            return matches.pop()
    raise ValueError(f"必要なPDXが見つかりません: {reference}")


def validate_mdx(body):
    if len(body) < 20:
        raise ValueError("MDXトラック表が途中で切れています")
    voices, first = struct.unpack_from(">HH", body)
    if first not in (20, 34) or len(body) <= first:
        raise ValueError("MDXの9/16トラック表が不正です")
    offsets = struct.unpack_from(f">{(first - 2) // 2}H", body, 2)
    if not first <= voices <= len(body) or any(not first <= p < len(body) for p in offsets):
        raise ValueError("MDXの音色・トラック位置が範囲外です")
    voice_end = min((p for p in offsets if p > voices), default=len(body))
    if (voice_end - voices) % 27:
        raise ValueError("MDX音色データが途中で切れています")


def validate_pdx(data):
    if len(data) < 768:
        raise ValueError("PDXサンプル表が途中で切れています")
    end, cursor = len(data), 0
    while cursor < end:
        if cursor + 8 > end:
            raise ValueError("PDXサンプル表の境界が不正です")
        offset, length = struct.unpack_from(">II", data, cursor)
        cursor += 8
        if length:
            if offset < max(768, cursor) or offset > len(data) or length > len(data) - offset:
                raise ValueError("PDXサンプル範囲が不正です")
            end = min(end, offset)
    if cursor != end:
        raise ValueError("PDXサンプル表と音声が重なっています")


def prepare(mdx, root, search):
    raw = read_bounded(mdx, MUSIC_LIMIT)
    title_end = raw.find(b"\r\n")
    separator = raw.find(b"\x1a", max(title_end, 0))
    pdx_end = raw.find(b"\0", max(separator, 0))
    if title_end < 0 or separator < 0 or pdx_end < 0:
        raise ValueError("MDXの曲名・PDX参照ヘッダーが不正です")
    try:
        title = " ".join(raw[:title_end].decode("cp932").split())
        if any(ord(c) < 32 or ord(c) == 127 for c in title):
            title = ""
    except UnicodeDecodeError:
        title = ""
    title = title or mdx.name
    # The final HPL1 writer truncates at a UTF-8 character boundary.
    title = title[:252] + "…" if len(title) > 255 else title
    reference = raw[separator + 1:pdx_end].decode("cp932")
    body = unpack(raw[pdx_end + 1:], MUSIC_LIMIT - 10)
    validate_mdx(body)
    pdx = b""
    if reference:
        dependency = pdx_path(reference, mdx, root, search)
        pdx = unpack(read_bounded(dependency, MUSIC_LIMIT), MUSIC_LIMIT - len(body) - 20)
        validate_pdx(pdx)
        pdx = bytes.fromhex("00000000000a00020000") + pdx
    header = "00000000000a00080000" if pdx else "0000ffff000a00080000"
    return title, bytes.fromhex(header) + body, pdx


def import_playlist(playlist, output, root=None, pdx_dirs=(), encoding="utf-8-sig"):
    playlist, output = Path(playlist).resolve(), Path(output).resolve()
    archive = output.with_name(output.name + ".zip")
    if output.exists() or archive.exists():
        raise ValueError("出力先またはZIPが既にあります。未使用の名前を指定してください")
    if playlist.suffix.lower() not in (".m3u", ".m3u8"):
        raise ValueError(".m3uまたは.m3u8を指定してください")
    root = Path(root).resolve() if root else playlist.parent
    if not root.is_dir():
        raise ValueError("音楽ルートがありません")
    search = [local_path(str(p), playlist.parent, root) for p in pdx_dirs]
    if any(not p.is_dir() for p in search):
        raise ValueError("--pdx-dirにはフォルダーを指定してください")
    text = read_bounded(playlist, LIST_LIMIT).decode(encoding)
    entries = [(n, line.strip()) for n, line in enumerate(text.splitlines(), 1)
               if line.strip() and not line.lstrip().startswith("#")]
    if not 1 <= len(entries) <= 300:
        raise ValueError("M3Uの曲数は1〜300曲にしてください")
    tracks, results = [], []
    total = 32
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="rpcmp-import-", dir=output.parent) as temporary:
        staging = Path(temporary)
        for line, name in entries:
            result = dict(line=line, source=name)
            try:
                path = local_path(name, playlist.parent, root)
                if path.suffix.lower() != ".mdx":
                    raise ValueError("M3UにはMDXを指定してください（PDXは自動解決）")
                title, mdx, pdx = prepare(path, root, search)
                if total + 128 + len(mdx) + len(pdx) > prepared_playlist.FILE_LIMIT:
                    raise ValueError("曲集全体が512MiBを超えます")
            except (ValueError, OSError) as error:
                result.update(status="excluded", reason=str(error))
            else:
                # Input errors may exclude a song; output I/O failures abort.
                number = len(tracks) + 1
                a, b = f"{number:03d}.mdx.bin", f"{number:03d}.pdx.bin"
                (staging / a).write_bytes(mdx)
                if pdx:
                    (staging / b).write_bytes(pdx)
                tracks.append(dict(title=title, mdx=a, pdx=b if pdx else None))
                total += 128 + len(mdx) + len(pdx)
                result.update(status="imported", track=number, title=title, pcm=bool(pdx))
            results.append(result)
        packed = None
        if tracks:
            manifest = staging / "tracks.json"
            manifest.write_text(json.dumps(tracks, ensure_ascii=False), encoding="utf-8")
            packed, _ = prepared_playlist.pack(manifest)
    output.mkdir()
    report = dict(imported=len(tracks), excluded=len(entries) - len(tracks), entries=results)
    (output / "取り込み結果.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                            encoding="utf-8")
    lines = [f"取り込み: {len(tracks)} 曲 / 除外: {report['excluded']} 曲", ""]
    for item in results:
        detail = f"{item['track']:03d} {item['title']}" if item['status'] == "imported" else "除外: " + item['reason']
        lines.append(f"{item['line']}行目 {item['source']} -> {detail}")
    (output / "取り込み結果.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    if packed is not None:
        (output / ASSET).parent.mkdir(parents=True)
        (output / ASSET).write_bytes(packed)
        guide = Path(__file__).resolve().parents[1] / "docs/development/m3u-library.md"
        (output / "導入手順.md").write_bytes(guide.read_bytes())
        with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as zipped:
            for path in sorted(output.rglob("*")):
                if path.is_file():
                    zipped.write(path, path.relative_to(output).as_posix())
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("playlist", type=Path, help="MDXを並べたM3U/M3U8")
    parser.add_argument("--output", required=True, type=Path, help="新しい出力フォルダー")
    parser.add_argument("--root", type=Path, help="音楽ファイルを読み取れるルート（既定: M3Uの場所）")
    parser.add_argument("--pdx-dir", action="append", default=[], help="共有PDXフォルダー。複数指定可")
    parser.add_argument("--encoding", choices=("utf-8-sig", "cp932"), default="utf-8-sig")
    args = parser.parse_args()
    try:
        report = import_playlist(args.playlist, args.output, args.root, args.pdx_dir, args.encoding)
    except (ValueError, OSError) as error:
        print(f"取り込み失敗: {error}", file=sys.stderr)
        return 1
    print(f"取り込み {report['imported']} 曲 / 除外 {report['excluded']} 曲")
    print(f"詳細: {args.output / '取り込み結果.txt'}")
    if report['imported']:
        print(f"SDコピー用: {args.output.with_name(args.output.name + '.zip')}")
    return 0 if report['imported'] else 1


if __name__ == "__main__":
    raise SystemExit(main())
