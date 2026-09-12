# Album ingestion v1

Status: adopted for M6 slice 2 on 2026-09-13.

This host profile implements the accepted folder-based Q8/Q12/Q14 behavior and
produces [album catalog v1](album-catalog-v1.md). The single-file packer is
unchanged. Filesystem access is an adapter; the batch builder consumes a bounded
manifest and an injected file reader, with no rendering or Pocket dependency.

## Input and ordering

Scan one native root recursively. Its basename is the root album name. Each
directory directly containing accepted MDX files is one album; empty albums
are omitted. Root files use key `.`; other keys are root-relative `/` paths.
Native Windows UTF-16 and POSIX UTF-8 names follow host-metadata-v1. Never reopen
an NFC-normalized name. Skip symbolic links, Windows reparse points (including
junctions) and other non-regular entries, and report them separately from MDX
exclusions. Do not traverse linked directories, including a linked root.

Accept the ASCII case-insensitive `.mdx` extension (a basename consisting only
of `.mdx` has no extension). Other regular files are reported as not-MDX and
are not read. NFC-normalize each relative path before identity/order comparison.
Different entries collapsing to the same normalized path are a global naming
collision, including empty directories. Reject absolute paths, empty or dot
components, backslashes, drive prefixes and malformed Unicode.

Use the natural byte-run comparison in the album catalog design: album keys,
then filename stems, then complete filenames as the final tie-breaker. ASCII
numeric runs compare by significant length and digits, never fixed-width
conversion; all-zero runs equal zero. Equivalent runs continue before the final
whole-string byte tie-breaker. Display titles do not affect order. Diagnostics
are sorted by normalized relative-path bytes. Source enumeration order, root
parent location, locale and timestamps never enter container bytes.

Bound a scan to 4096 entries (including directories/skipped entries), depth 64,
4096 bytes per relative path/root name and 1 MiB total manifest text. Native
adapters check limits before retaining entries; the builder validates the same
public input boundary before derived allocation/access. These host scan limits
are separate from the accepted 300 tracks and 32 MiB container limits.
Failure diagnostics obey the same 4096-byte path bound: omit an over-limit
path rather than allocating another copy merely to report its rejection.

## Admission and failure

Read one candidate at a time, at most 1 MiB, with a stable-size read. Run the
existing complete structural and FM-only admission checks. Preserve every
accepted MDX byte. Select embedded CP932 title/fallback using host-metadata-v1;
album comes from its folder, artist is empty, other optional metadata absent.
Fallback is a diagnostic, not an exclusion.

Distinguish complete creation, creation with MDX exclusions, and failure.
Structural/unsupported playback errors and non-size title metadata failures
exclude that MDX with stage/code/offset. Input/metadata/decoder capacity limits,
scan/read errors, naming collisions, identical TrackIdentity, writer errors,
output errors and zero accepted tracks are global failures. Never discard an
extra valid track to make 301 fit 300, truncate metadata, or publish an empty
library. Duplicate identity reports both source paths. Failed results contain
no output bytes; diagnostics/counts on failure describe only work completed so
far, not a successful library. Bound retained unique source blobs to 32 MiB.

The caller must keep the source tree unchanged during scanning/reading.
The native adapter rechecks entry kinds/parent links and exact file length;
detected changes fail globally. This is not a filesystem security boundary
against a concurrently hostile process replacing paths between checks.

## Output and CLI

`rpcmp_album_pack <input-folder> <output.rpcmlib>` uses native arguments. Return
0 for complete creation, 2 for creation with exclusions, 1 for any global/usage
failure. Print each excluded/skipped path and reason plus accepted/excluded/
skipped counts; escape control characters in path diagnostics. Reports stay on
the local console and are not embedded in the artifact. No source paths or music
identities are committed as test evidence.

Only after successful ingestion create an exclusive temporary regular file in
the output directory. Write all bytes, flush and close successfully, then
replace the destination by a same-directory rename. Do not use cross-volume
copy/delete, truncate an existing destination first, or claim success after
any failed step. On failure discard the owned temporary file and report a
cleanup failure separately. Existing destinations are preserved before the
publish step; a successful publish replaces them with the complete artifact.
Reject a non-regular or linked destination and a destination matching an input
MDX. OS crashes/power loss and filesystem durability beyond the OS primitive
are not claimed by this host publication contract.

Windows uses exclusive `CreateFileW`, `FlushFileBuffers`, then
[`MoveFileExW`](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw)
with replace-existing and write-through, without copy-allowed. Reparse points
are detected from native attributes; Windows
[junctions](https://learn.microsoft.com/en-us/windows/win32/fileio/hard-links-and-junctions)
are reparse points. POSIX uses exclusive file creation, `fsync`, close and
same-directory [rename](https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html).
Playback settings durability is a separate contract.

## Independent acceptance

Use self-authored MDX fixtures with manually chosen filenames/titles and a
Python section inspector independent of the writer. Verify numeric runs longer
than 64 bits, leading zeros, NFC/case distinctions, reverse enumeration, root
relocation, nested/same-name albums, mixed/all exclusions, duplicate identities,
300/301 accepted tracks and every global failure preserving output. Exercise
injected read/write/flush/publish failures and real native Unicode paths and
link/junction skips. No RTL or hardware acceptance follows from these tests.
