"""Exercise real native-path ingestion and inspect v1 bytes independently."""

import pathlib
import struct
import subprocess
import sys
import tempfile
import zlib


def inspect(data: bytes) -> tuple[str, bytes]:
    assert data[:8] == b"RPCMLIB\0"
    directory, count, stride = struct.unpack_from("<QII", data, 40)
    assert stride == 40
    sections = {}
    for i in range(count):
        tag, flags, offset, length, crc = struct.unpack_from("<4sIQQI", data, directory + i * stride)
        payload = data[offset:offset + length]
        assert len(payload) == length and zlib.crc32(payload) == crc
        sections[tag] = payload
    strings = sections[b"STRS"]
    nstrings, record_size, area = struct.unpack_from("<IIQ", strings)
    assert record_size == 16
    decoded = {}
    for i in range(nstrings):
        identity, offset, length = struct.unpack_from("<QII", strings, 16 + i * 16)
        decoded[identity] = strings[area + offset:area + offset + length].decode("utf-8")
    title_id = struct.unpack_from("<Q", sections[b"TRAK"], 8 + 24)[0]
    blob = sections[b"BLOB"]
    nblobs, record_size, area = struct.unpack_from("<IIQ", blob)
    assert nblobs == 1 and record_size == 48
    unpacked, stored, offset = struct.unpack_from("<QQQ", blob, 16 + 16)
    assert stored == unpacked and offset >= area
    return decoded[title_id], blob[offset:offset + stored]


def main() -> None:
    executable = pathlib.Path(sys.argv[1]).resolve()
    fixture = bytes.fromhex(pathlib.Path(sys.argv[2]).read_text(encoding="ascii"))
    # Only replace the title before its format-defined delimiter. Offsets in
    # the MDX body are relative to that body, not the title or file beginning.
    delimiter = fixture.index(b"\r\n\x1a")
    body = fixture[delimiter:]
    with tempfile.TemporaryDirectory(prefix="rpcmp-metadata-") as temporary:
        root = pathlib.Path(temporary)
        source = root / "01-か\u3099.mdx"
        output = root / "日本語.rpcmlib"

        def run(raw_title: bytes, override: str | None = None):
            mdx = raw_title + body
            source.write_bytes(mdx)
            args = [str(executable), str(source), str(output)]
            if override is not None:
                args.append(override)
            result = subprocess.run(args, capture_output=True, check=False)
            assert result.returncode == 0, result.stderr
            title, blob = inspect(output.read_bytes())
            assert blob == mdx
            assert source.read_bytes() == mdx
            return title, result.stderr

        assert run(b"\x82\xa0\x81\x60\xb6")[0] == "あ～ｶ"
        first = output.read_bytes()
        run(b"\x82\xa0\x81\x60\xb6")
        assert output.read_bytes() == first
        for raw, reason in ((b"", b"empty"), (b"\x82", b"invalid-encoding"),
                            (b"bad\x1btitle", b"control-character")):
            title, diagnostic = run(raw)
            assert title == "01-が" and b"title fallback=" + reason in diagnostic
        assert run(b"\x82", "か\u3099")[0] == "が"
        normalized = output.read_bytes()
        assert run(b"\x82", "が")[0] == "が"
        assert output.read_bytes() == normalized
        assert run(b"title", "ｶﾞＡ①")[0] == "ｶﾞＡ①"

        # Neither rejection may truncate a previously valid destination.
        previous = output.read_bytes()
        for bad in (b"invalid", b"x" * (1048576 + 1), b"\x82\xa0" * 1366 + body):
            source.write_bytes(bad)
            rejected = subprocess.run([str(executable), str(source), str(output)],
                                      capture_output=True, check=False)
            assert rejected.returncode != 0
            assert output.read_bytes() == previous
        source.write_bytes(fixture)
        rejected = subprocess.run([str(executable), str(source), str(output), "x" * 4097],
                                  capture_output=True, check=False)
        assert rejected.returncode != 0 and output.read_bytes() == previous
    print("native-path metadata ingestion: PASS")


if __name__ == "__main__":
    main()
