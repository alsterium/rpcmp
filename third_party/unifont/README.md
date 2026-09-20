# RPCMP Bitmap JP source

Unmodified `unifont_jp-16.0.04.hex.gz` from
[GNU Unifont 16.0.04](https://unifoundry.com/pub/unifont/unifont-16.0.04/font-builds/).
SHA-256: `5ba84e901b9f7fad3bce0571c7e4b4b0aef4ce0acc4ee6622ef0cfa2eef38a6f`.

Copyright (C) 1998-2025 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller,
Johnnie Weaver, David Corbett, Nils Moskopp, Rebecca Bettencourt, Minseo Lee,
Ho-Seok Ee, et al. This is the copyright statement in the release's
`font/Makefile`. The Japanese build includes the public-domain jiskan glyphs
described in COPYING. No font utility source is imported or linked.

RPCMP chooses **SIL Open Font License 1.1** from the font's dual license. Both
the upstream licensing explanation (COPYING) and OFL-1.1.txt are retained;
only trailing whitespace and surrounding blank lines in these notices were
normalized. They came from the corresponding official release archive,
SHA-256 `2bd4e4679757126f48e1bf2c1be40b09aa92162bfedda4683ce5fbc70a2a5972`.

The build generates a derivative named **RPCMP Bitmap JP**, licensed under the
same OFL 1.1. It selects printable CP932 scalars, their NFC results from pinned
utf8proc 2.11.3, and the UI symbols listed in `tools/bitmap_font_generate.py`.
Glyph pixels are unchanged. The generated index/bitmap live read-only on the
Pocket; neither utf8proc nor a decompressor runs there. The 8192-glyph and
512 KiB font limits are checked before output. Ship this directory's notices
with binaries that embed the font. The enclosing application's license is
independent of the font license.
