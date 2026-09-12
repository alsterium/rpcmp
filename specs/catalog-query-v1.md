# Read-only album catalog query v1

Status: adopted for M6 slice 2 on 2026-09-13 from
[the catalog proposal, section 4](../docs/design/pocket-album-catalog-contract.md#4-読み取り専用カタログ-api).
This is a separate read-only API. PlayerCommand and PlayerSnapshot v1, including
prepare-only LoadTrack, remain unchanged. Playback schema 2 is a separate profile.
`valid_catalog_text` exposes the existing bounded UTF-8/prefix check for that
publisher without changing catalog text admission or truncation rules.

## Ownership and lifecycle

Core owns a CatalogSession; UI receives only the const CatalogReader interface.
Queries return complete copied values with catalog schema_version 1. They expose
logical IDs and display ordinals, never file offsets, folder keys or borrowed
storage. Calls and publication are serialized in Core's control context, outside
the audio callback. This interface does not itself provide thread synchronization.

LibraryGeneration is a u64 in one Core session's namespace. Zero means no issued
generation. Every begin_open (including reload) and close advances it, immediately
removes the previous catalog and invalidates all old references, even when the
next load fails. It is unrelated to a file's build ID. A replacement CatalogSession
within the same Core session must receive the last issued generation in its
constructor; a new Core session starts at zero and discards all prior references.
The counter never wraps. Exhaustion clears availability and remains a resource
error until the Core session and its references are recreated.

begin_open returns a generation ticket and enters Loading. complete_open accepts
only that current ticket while Loading, validates the bounded album storage
profile and then publishes Ready. fail_open records a read failure. An obsolete
completion is rejected before examining its file; duplicate completion is rejected
as NotLoading. Failed validation publishes Error, without restoring the old view.
close enters Empty. Same-library track preparation does not call these methods
and does not affect catalog availability or generation.

Ready borrows immutable, validated library bytes from the Core/platform owner.
The owner must retain them until begin_open, close or session destruction, and
must separately respect any playback engine's borrowed-buffer lifetime. Returned
pages contain no borrowed data and remain readable after storage is released;
they do not remain authorized to select a track after their generation expires.

CatalogStatus contains schema version, generation, phase (Empty, Loading, Ready,
Error), failure (None, InvalidLibrary, ReadFailed, GenerationExhausted), and total
album/track counts. Only Ready has nonzero counts: 1 <= albums <= tracks <= 300.
Only Error has a failure; Loading, Ready and Error require a nonzero generation.
No independent publication sequence is introduced: content is immutable within
one Ready generation; integration publishes status using the player snapshot's
sequence. Consumers must discard superseded asynchronous publications there.

## Queries and copied pages

CatalogPageQuery contains schema_version 1, generation, start_ordinal:u32 and
limit:u16 (default 16). albums(query) walks album display order;
tracks(album_id, query) walks that album's member order. The read-only interface
also returns CatalogStatus. No query changes playback or library state.

Both page headers contain response schema 1, the original query, total:u32,
count:u16, optional next_start:u32 and CatalogQueryError. Track pages also echo
the requested AlbumId. Errors retain request identity, but have total/count zero
and no next_start. Unused fixed-array entries are value-initialized and ignored.
An error's echoed generation is not a statement of current availability.

Validation order is: unsupported query schema; unavailable (not Ready); stale
generation (including zero); limit outside 1..16; unknown album for track pages;
start beyond total. No fallback, clamping or implicit reload occurs. The response
error values are None, UnsupportedSchema, Unavailable, StaleLibrary, InvalidLimit,
UnknownAlbum and InvalidStart. An in-memory storage invariant failure is reported
as Unavailable with no partial items; it is never turned into a shorter success.

start == total is a successful empty final page. Otherwise count is exactly
min(limit, total - start), and next_start is start + count if that is less than
total; it is absent at the end. Album items contain AlbumId, display_ordinal,
name and track_count. Track items contain TrackId, owner AlbumId, the ordinal
within that album and title. Each page has capacity 16 and no heap-owned fields.

CatalogText stores a length:u16, 96 bytes and a truncated flag. The bytes up to
length are valid UTF-8; no terminator is implied and trailing storage is ignored.
Metadata is copied up to the largest complete code-point prefix fitting 96 bytes.
truncated is true iff source bytes were omitted. Thus a truncated nonempty valid
source uses 93..96 bytes; a final four-byte character can leave three unused bytes.
This is not grapheme truncation or Unicode normalization. Saved metadata, IDs
and ordering are unchanged; stored empty titles remain empty (ingestion normally
provides a filename fallback). Album names are nonempty. Existing storage UTF-8
semantics are preserved, including the embedded-NUL prohibition in
[rpcmlib v1](rpcmlib-v1.md#required-section-encoding). Renderers use the explicit length.

Contract-only validators accept generated/recorded status and page values without
Core or filesystem access. They check schema, enum domains, counts, limits,
generation, page arithmetic, UTF-8 boundaries, nonzero/unique item IDs, owner and
ordinal consistency. They cannot prove membership outside the page or that a
truncated string is the correct prefix of unavailable source bytes. Error pages
check empty results and applicable request conditions; unused items are ignored.

## Core selection boundary

Core's resolve_track(generation, TrackId, output) checks Ready, then generation,
then logical ID before returning a borrowed TrackView. It leaves output unchanged
on rejection. It is not exposed through CatalogReader, performs no transport
change, and is the guard for later PlayerCommand schema 2 integration. Callers
must not retain the result across invalidation. Full selection/playback behavior
remains part of M6 slice 3, not acceptance of this read-only API.

## Acceptance

Use the independently encoded two-album storage fixture to check display/member
order and exact IDs. Check 16-item pages, limit 0/17, end/over-end/UINT32_MAX,
unknown IDs and generations, invalid/duplicate/obsolete load completions, failed
reload/close invalidation, generation exhaustion and copied-page buffer lifetime.
Use separately authored long metadata and mock pages to check 96-byte boundaries,
multi-byte truncation and malformed response rejection. ALBM-only reorder with
unchanged build ID must still invalidate references. Existing v1 tests continue.
