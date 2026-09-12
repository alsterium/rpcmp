# Host metadata profile v1

Status: adopted for M6 slice 1 on 2026-09-13.

This host-only ingestion profile implements Q14 and ADR-0005. It does not
change `.rpcmlib` v1 bytes or the normalized writer's input precondition.

- Decode parser-owned MDX title bytes with Microsoft CP932 table 2.01, pinned
  in `third_party/cp932`. Never infer UTF-8 or use the host locale. Invalid or
  unmapped sequences cause whole-title filename-stem fallback. `82 A0` is
  U+3042, `81 60` U+FF5E and `B6` U+FF76.
- Replace title CR/LF/TAB with U+0020; trim outer U+0020/U+3000. Empty results
  and other C0/C1/NUL cause fallback with distinct diagnostics. A valid title
  never falls back merely because its serialized size exceeds the limit.
- Filename fallback uses the native filename stem converted strictly from
  Windows UTF-16 or POSIX UTF-8, then the same display sanitation. Invalid or
  empty fallback is a metadata failure, not a fabricated title. Display
  sanitation is separate from path/ordering identity; never reopen a normalized
  path. MDX blob bytes, PDX references and music data remain unchanged.
- Normalize UTF-8 with pinned utf8proc 2.11.3 (Unicode 17.0.0), using only
  `STABLE | COMPOSE`. No compatibility, width or case folding. Reject malformed
  UTF-8/UTF-16 and embedded NUL; preserve unassigned scalar values. General NFC
  conversion does not trim or sanitize path identity.
- CP932 input and serialized UTF-8 are limited to 4096 bytes. UTF-8 metadata
  input is also limited to 4096 bytes; native UTF-16 conversion accepts at most
  4096 code units and rejects output beyond 4096 UTF-8 bytes. Check lengths
  before access/allocation. Normalization uses a counted decomposition pass
  before allocating at most 64 KiB of per-string working memory. Limit or
  conversion failures never return truncated output or claim success.
- The existing normalized `ingest_single_mdx` API preserves its precondition
  and error behavior. The raw-metadata ingestion entry point performs admission,
  title selection and NFC before calling the same byte writer. Explicit title
  overrides are normalized strictly (no fallback or display sanitation).
- `rpcmp_mdx_pack input.mdx output.rpcmlib [utf8-title]` retains explicit-title
  use and adds automatic embedded-title/fallback selection when omitted. Native
  Windows command-line arguments use UTF-16, not the ANSI code page. Read at
  most the existing MDX 1 MiB limit before admission; reject before opening the
  output when input/admission/metadata fails. Report fallback reason separately
  from exclusion. Existing output is untouched on those rejection paths.

The dependency source hashes and license obligations are in
[third-party components](../third_party/README.md). Font coverage/rendering and
album traversal are later M6 slices, not part of this host profile.
