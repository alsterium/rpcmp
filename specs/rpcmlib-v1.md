# `.rpcmlib` Container Specification v1

Status: active M1 contract. The envelope and required-section payload layouts
below are frozen. A partial implementation must not claim full v1 compatibility
until logical validation, the deterministic writer, and golden fixtures land.

## 1. Requirements

- Random access by stable IDs.
- Deterministic/reproducible output from identical normalized inputs.
- Explicit little-endian fixed-width fields.
- Safe parsing of untrusted files with checksummed sections.
- Extensible section directory; unknown non-required sections are ignorable.
- No absolute host paths in the artifact.

## 2. File layout

```text
Header (fixed size)
Section payloads (aligned)
Section directory
Footer/checksum information (optional v1 extension)
```

All integers are unsigned little-endian. The v1 header is exactly 80 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic: ASCII `RPCMLIB` followed by `00` |
| 8 | 2 | format major (`1`) |
| 10 | 2 | format minor (`0`) |
| 12 | 4 | header size (`80`) |
| 16 | 8 | optional feature flags |
| 24 | 8 | required feature flags |
| 32 | 8 | total file size |
| 40 | 8 | section-directory offset |
| 48 | 4 | section-directory entry count |
| 52 | 4 | directory entry size (`40`) |
| 56 | 16 | deterministic library build ID |
| 72 | 4 | header CRC-32 |
| 76 | 4 | reserved, written as zero and ignored by readers |

The header CRC-32 is IEEE CRC-32 (polynomial `0xEDB88320`, initial value and
final XOR `0xFFFFFFFF`) over all 80 header bytes with bytes 72 through 75 set to
zero. v1 defines no feature bits yet, so both flag fields are written as zero;
readers ignore unknown optional bits and reject any non-zero required bit.

The directory immediately describes `entry_count` fixed 40-byte entries:

| Entry offset | Size | Field |
|---:|---:|---|
| 0 | 4 | section type as four ASCII bytes |
| 4 | 4 | section flags; bit 0 means required |
| 8 | 8 | payload offset |
| 16 | 8 | stored payload length |
| 24 | 4 | payload CRC-32 using the same IEEE parameters |
| 28 | 4 | required alignment, a non-zero power of two |
| 32 | 8 | reserved, written as zero and ignored by readers |

The directory range must fit in the declared file size, each payload offset must
obey its entry alignment, and payloads must not overlap the header, directory,
or one another. Empty payloads retain an in-range aligned offset and do not
create an overlap. Section tags are unique. Required v1 tags are the literal
four-byte values `TRAK`, `BLOB`, `STRS`, `DEPS`, `INDX`, and `CSUM`; their
payload layouts are defined below. Unknown entries with required bit set are
rejected; unknown optional entries are checksum-validated and ignored.

### Required-section encoding

IDs are non-zero unsigned 64-bit values. ID zero is the absent-value sentinel
only for optional string references. Section-relative offsets are measured from
the first byte of that section and are checked before access. They never cross
the public library API.

`TRAK`, `DEPS`, `INDX`, and `CSUM` begin with an 8-byte array header: record
count (`u32`) followed by record size (`u32`). Records immediately follow the
header. `TRAK` records are 96 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | TrackId |
| 8 | 4 | format FourCC (`MDX ` initially) |
| 12 | 4 | capability/analysis flags |
| 16 | 8 | primary BlobId |
| 24 | 8 | title StringId |
| 32 | 8 | artist StringId |
| 40 | 8 | album StringId, zero if absent |
| 48 | 8 | composer StringId, zero if absent |
| 56 | 8 | system StringId, zero if absent |
| 64 | 8 | duration ticks, `UINT64_MAX` if unknown |
| 72 | 8 | loop-start ticks, `UINT64_MAX` if unknown |
| 80 | 4 | first dependency ordinal |
| 84 | 4 | dependency count |
| 88 | 2 | year, zero if unknown |
| 90 | 1 | estimate confidence: 0 unknown, 1 estimated, 2 exact |
| 91 | 5 | reserved, written as zero and ignored |

Track records are strictly sorted by TrackId. Their dependency ranges appear in
the same order, do not overlap or leave unreferenced `DEPS` records, and together
partition the entire `DEPS` array.

`BLOB` begins with count (`u32`), record size 48 (`u32`), and section-relative
payload-area offset (`u64`). Blob records immediately follow this 16-byte
header:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | BlobId |
| 8 | 4 | blob-kind FourCC (`MDX ` or `PDX ` initially) |
| 12 | 2 | codec: 0 means none |
| 14 | 2 | reserved |
| 16 | 8 | uncompressed size |
| 24 | 8 | stored size |
| 32 | 8 | section-relative payload offset |
| 40 | 4 | payload alignment |
| 44 | 4 | payload CRC-32 |

Codec zero requires equal stored and uncompressed sizes. Payloads lie at or
after the declared payload-area offset, obey their non-zero power-of-two
alignment, stay within `BLOB`, and do not overlap. Blob records are strictly
sorted by BlobId, and their payload offsets are monotonically non-overlapping in
that same order. Identical content is represented by one BlobId/record rather
than overlapping payload ranges.

`STRS` begins with count (`u32`), record size 16 (`u32`), and section-relative
UTF-8 data-area offset (`u64`). Each record is StringId (`u64`), data-relative
offset (`u32`), and byte length (`u32`). Strings are unterminated UTF-8,
NFC-normalized by writers, individually bounded, and may share an exact byte
range only when their bytes are identical. Embedded NUL is forbidden. String
records are strictly sorted by StringId.

Each 24-byte `DEPS` record contains owner TrackId (`u64`), role FourCC (`u32`),
reserved zero (`u32`), and target BlobId (`u64`). A track's dependency range is
within the array, contiguous, and every covered record names that track.

Each 24-byte `INDX` record contains entity kind (`u32`: 1 track, 2 blob),
reserved zero (`u32`), entity ID (`u64`), zero-based record ordinal (`u32`), and
reserved zero (`u32`). Entries are strictly sorted by `(entity kind, entity ID)`
and contain exactly one entry for every track and blob. Readers use this array
for bounded binary search rather than scanning record sections per lookup.

Each 24-byte `CSUM` record contains entity kind (`u32`, 2 for blob), CRC-32
(`u32`), entity ID (`u64`), and decoded byte length (`u64`). Entries are
strictly sorted by `(entity kind, entity ID)` and contain exactly one record for
every blob. Its checksum and length must agree with the Blob record and decoded
payload. CRC-32 detects corruption and is not an authenticity mechanism.

### Header semantic fields

- magic: eight-byte RPCMP signature
- format major/minor
- header size
- feature/required-feature flags
- file size
- section-directory offset and entry count
- library UUID/build ID
- header checksum

### Required logical sections

- `TRACKS`: fixed/indirect track records keyed by `TrackId`
- `BLOBS`: source MDX and later sample payloads keyed by `BlobId`
- `STRINGS`: UTF-8 string table
- `DEPS`: ordered dependency relationships
- `INDEX`: lookup structures
- `CHECKSUMS`: integrity values for addressable payloads

Optional sections include `ANALYSIS`, artwork, search indices, and future format/device metadata.

## 3. Logical validation and public records

`TrackRecord` includes stable ID, format tag, primary blob ID, title/artist strings, optional album/composer/system/year, dependency range, duration/loop estimates with confidence, and capability/analysis flags.

`BlobRecord` includes stable ID, kind, codec (`none` initially), uncompressed/stored sizes, payload location, alignment, and checksum.

Dependencies identify roles (for example `pdx_samples`) by IDs. They do not expose source paths to runtime.

Before exposing a library, readers validate section headers and record-array
arithmetic, configured admission limits, non-zero unique IDs, strict INDEX and
CSUM ordering/completeness, UTF-8 strings, codecs, dependency ranges, and every
cross-reference. Public values contain typed IDs, copied metadata or bounded
immutable byte/string views, and logical dependency roles. They contain no
section tags, codecs, record ordinals, or offsets.

## 4. Identity and reproducibility

- BlobId is the first 64 digest bits, interpreted little-endian, of SHA-256 over
  `"blob\0"`, the blob-kind
  FourCC, and canonical uncompressed bytes. StringId uses SHA-256 over
  `"string\0"` and NFC UTF-8 bytes. TrackId uses SHA-256 over `"track\0"`, the
  format FourCC, primary BlobId, ordered dependency role/BlobId pairs, and
  canonical metadata StringIds. Multi-byte inputs to these hashes are encoded
  little-endian. A zero result or truncated-ID collision is resolved
  deterministically by appending a little-endian `u32` counter starting at 1
  and rehashing; the counter is not stored because readers consume the resolved
  ID.
- Directory traversal order is normalized.
- Timestamps and machine-specific paths are excluded by default.
- Utility ingestion validates UTF-8 and NFC-normalizes strings before creating
  the normalized writer model. Stable IDs and the byte writer consume the exact
  NFC UTF-8 bytes from that model; the byte writer does not apply a second,
  platform-dependent normalization pass. The normalization implementation and
  conformance tests must be recorded before arbitrary source metadata is
  admitted.
- Blob deduplication is content-based.
- The v1 writer emits required sections in `TRAK`, `BLOB`, `STRS`, `DEPS`,
  `INDX`, `CSUM` order with 8-byte file alignment. Its deterministic 16-byte
  build ID is the first 16 SHA-256 digest bytes over the six complete section
  payloads concatenated in that order, before file-alignment padding.

## 5. Validation order

1. Validate minimum size, magic, supported major version, and header size.
2. Check arithmetic overflow for every `offset + length` and `count * size`.
3. Ensure directory and section ranges lie within the file and obey alignment.
4. Reject illegal overlaps except explicitly shared/deduplicated payloads.
5. Enforce configurable limits before allocations.
6. Verify required sections, unique IDs, references, codecs, and checksums.
7. Expose typed logical views only after validation succeeds.

Host defaults are at most 64 sections, 1,000,000 records per section, 4 KiB per
string, 64 dependencies per track, 4 GiB stored file size, and 8 GiB total
decoded blob size. Callers may lower them. Pocket-specific limits must be
measured and frozen before the Pocket integration slice; they must not silently
inherit the host allocation budget.

## 6. Compatibility

- Readers reject unsupported major versions.
- Minor versions are additive; required-feature bits force rejection when unsupported.
- Unknown optional sections are skipped using directory lengths.
- Reserved fields are written as zero and ignored when read.

## 7. M1 acceptance criteria

- Golden fixtures define every byte of a minimal and multi-track file.
- Corruption tests cover truncation, overflow, overlap, duplicate IDs, missing sections, bad UTF-8, invalid references, and checksum failure.
- A 1,000-track synthetic library opens and arbitrary tracks/blobs are retrieved by ID.
- Rebuilding unchanged input produces byte-identical output.
- UI and playback engines never access raw container offsets.
