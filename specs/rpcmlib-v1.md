# `.rpcmlib` Container Specification v1

Status: active M1 contract. The envelope below is frozen; required-section
payload layouts remain unfrozen until the corresponding M1 slice lands. A
partial implementation must not claim full v1 compatibility.

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
payload layouts are frozen by later M1 slices. Unknown entries with required bit
set are rejected; unknown optional entries are checksum-validated and ignored.

Until every required payload layout is frozen, writers must not claim complete
v1 compatibility.

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

## 3. Logical records

`TrackRecord` includes stable ID, format tag, primary blob ID, title/artist strings, optional album/composer/system/year, dependency range, duration/loop estimates with confidence, and capability/analysis flags.

`BlobRecord` includes stable ID, kind, codec (`none` initially), uncompressed/stored sizes, payload location, alignment, and checksum.

Dependencies identify roles (for example `pdx_samples`) by IDs. They do not expose source paths to runtime.

## 4. Identity and reproducibility

- IDs are derived deterministically from canonical content and role, or assigned by a documented collision-safe deterministic process.
- Directory traversal order is normalized.
- Timestamps and machine-specific paths are excluded by default.
- Strings are valid UTF-8; normalization policy is recorded and tested.
- Blob deduplication is content-based.

## 5. Validation order

1. Validate minimum size, magic, supported major version, and header size.
2. Check arithmetic overflow for every `offset + length` and `count * size`.
3. Ensure directory and section ranges lie within the file and obey alignment.
4. Reject illegal overlaps except explicitly shared/deduplicated payloads.
5. Enforce configurable limits before allocations.
6. Verify required sections, unique IDs, references, codecs, and checksums.
7. Expose typed logical views only after validation succeeds.

Suggested defensive defaults: bounded section count, string length, dependency count per track, total decoded size, and compression ratio. Final values must be justified against Pocket memory limits in M1.

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
