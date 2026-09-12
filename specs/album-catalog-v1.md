# Album catalog v1

Status: adopted for M6 slice 2 on 2026-09-13.

## Compatibility and identity

The album profile uses the rpcmlib 1.0 envelope and its six required sections
unchanged, followed by optional `ALBM` (flags 0, alignment 8). Its absence is an
error only for the new album reader. Old readers validate its CRC and ignore it.
The header build ID still hashes only the six required payloads; changing only
album/member order may leave it unchanged. Compare whole-file SHA-256 for file
identity and use a Core-owned generation for live reference invalidation.

The original `write_rpcmlib` API retains its behavior and golden output. The new
`write_album_rpcmlib` accepts a normalized library and album list in display
order; each album names input track indices in track order. Its input has
1–300 tracks, 1–300 blobs and 1–300 nonempty albums. Every input track belongs
to exactly one album and its album metadata equals that album's name.

Host ingestion NFC-normalizes folder keys, filenames and metadata before the
writer. Folder keys use relative `/` components; the root is `.`. Reject empty
keys/components, other `.` or `..` components, leading/trailing `/`, backslash,
NUL and drive prefixes. Non-root album names equal the last component. The root
name is supplied by the host, never inferred by Pocket. Keys and names are
nonempty UTF-8 of at most 4096 bytes. The reader enforces UTF-8, this path syntax,
unique key bytes and references; NFC is the host producer's precondition, as
for STRS v1, not a second Unicode normalizer in Pocket.

`AlbumId` is a nonzero u64: first eight SHA-256 bytes, interpreted little-endian,
of `"album\0" || key_byte_length:u32le || NFC_key_bytes`. Assign IDs in key byte
order. Zero or collision with a different key retries with a u32le counter
appended to that original material, starting at 1; exhaustion is an error.
Track/Blob/String identity rules remain v1. Identical TrackIdentity input is
a global error in the album writer, not a second placement or a hash collision.
This explicitly corrects the proposal's claim that the old writer already
rejects identical tracks: its occupied-ID retry currently assigns another ID.
That legacy behavior is not changed by adopting this separate profile.

## Payload bytes

All values are unsigned little-endian. ALBM header is exactly 32 bytes:

| Offset | Bytes | Value |
| ---: | ---: | --- |
| 0 / 2 | 2 each | major 1 / minor 0 |
| 4 / 6 / 8 | 2 each | header 32 / album record 40 / member record 16 |
| 10 | 2 | reserved 0 |
| 12 / 16 | 4 each | album count A / member count T |
| 20 | 4 | reserved 0 |
| 24 | 8 | member array offset, exactly 32 + 40*A |

Albums start at 32 in strictly increasing nonzero AlbumId order. Each 40-byte
record contains AlbumId u64 at 0, name StringId u64 at 8, folder-key StringId
u64 at 16, display ordinal u32 at 24, first member index u32 at 28, member count
u32 at 32 and reserved u32 at 36. Display ordinals cover `[0,A)` exactly once.

Each 16-byte member contains TrackId u64 at 0, within-album ordinal u32 at 8
and reserved u32 at 12. Album ranges partition members without gaps or overlap
in album-record order. Within each range ordinals are exactly `0..count-1` in
physical order. Every TRAK ID appears exactly once and its album StringId
equals its owner's name StringId. Name/key references exist and are nonempty;
key bytes are unique. Readers ignore reserved fields, matching v1 conventions.
Payload size is exactly `32 + 40*A + 16*T`; only payload 1.0 is accepted.

## Validation and logical access

Validate the envelope, CRCs and all six required sections before exposing a
catalog. Before array access or allocation enforce A/T 1–300, T=TRAK count,
at most 300 MDX blobs, at most 1 MiB each, 32 MiB decoded total, 32 MiB whole
container, 16 sections, 4096 records per section, 4096 strings, 4096 bytes per
string and 1 MiB total string bytes. Tracks have MDX format and no dependency
records in this initial profile. Blob admission as playable FM remains the
ingestion/engine's job; catalog validation is not a substitute for MDX parsing.

An invalid open leaves the caller's previous immutable view untouched. The new
`AlbumCatalog` exposes AlbumId, display ordinal, name, track count and TrackView
lookup by AlbumId/track ordinal, with no container offsets or folder keys in
those values. Views borrow immutable file storage. Default/unopened catalog
lookups fail safely. Core owns open/close/reload generation and bounded copied
pages; UI never owns or mutates this storage view.

Folder scanning uses the natural byte-run ordering already specified in
[the design](../docs/design/pocket-album-catalog-contract.md#2-決定的な表示順).
The source adapter produces that order; the normalized writer never sorts by
display titles or invents filenames. Reordering input storage with album/member
references adjusted must yield identical output.

Independent acceptance includes a hand-laid-out two-album/four-track fixture
with ID order different from display order, CRC-correct corruptions of every
reference/range/order condition, absent/versioned ALBM, 300/301 boundaries,
old writer goldens and unchanged build ID for ALBM-only order changes.
