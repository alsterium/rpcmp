# Utility

Host-side scanner, metadata/dependency extractor, validator, and deterministic `.rpcmlib` builder. It must not share UI code or require Pocket hardware.

M1 implements the utility in C++17. Its deterministic byte writer accepts a
normalized logical model; the ingestion layer is responsible for UTF-8
validation and NFC normalization before stable IDs are generated. See
`docs/adr/0005-m1-utility-language-and-normalization.md`.

M5 adds `rpcmp_mdx_pack`, a deterministic single-file ingestion tool. It runs
the complete MDX v1 structural and FM-only playback-admission gates before
writing anything. Usage is:

```text
rpcmp_mdx_pack input.mdx output.rpcmlib "UTF-8 title"
```

The source MDX is stored unchanged as the track's primary logical blob. Active
PDX/PCM and unsupported commands are rejected transactionally; no partial
library is produced. M6 adds automatic title selection when the last argument is
omitted: strict CP932 embedded title, then native filename stem on invalid,
empty or control-containing title. A fallback diagnostic explains that choice.
Explicit titles and automatically selected metadata are normalized to NFC with
the pinned host-only dependency. See [host metadata v1](../specs/host-metadata-v1.md).

```text
rpcmp_mdx_pack input.mdx output.rpcmlib
```

Windows uses native UTF-16 arguments/paths; POSIX metadata uses strict UTF-8.
Input is bounded to 1 MiB before allocation; metadata is bounded to 4096 UTF-8
bytes without truncation. Input/admission/metadata rejection leaves an existing
output untouched. Output I/O errors remain errors and may leave a partial new
file; atomic multi-file batch publication belongs to the album-ingestion slice.

The low-level `ingest_single_mdx` API still accepts already-normalized metadata
and retains its errors. `ingest_mdx` is the raw-metadata entry point. The
`rpcmp_utility_writer` target contains only IDs/serialization and has no Unicode
dependency; `rpcmp_utility` adds host ingestion, CP932 and NFC. Include the
[dependency notices](../third_party/README.md) when distributing host binaries.

M6 adds folder ingestion with [album ingestion v1](../specs/album-ingestion-v1.md):

```text
rpcmp_album_pack Music collection.rpcmlib
```

Each folder directly containing accepted MDX becomes an album. Albums use
relative-folder natural order; tracks use filename-number order while their
display titles come from embedded CP932 metadata. The source bytes remain
unchanged. Unsupported or malformed MDX files are listed with their reasons;
non-MDX files, links/junctions and other non-regular entries are listed separately.
Titles that fall back to filenames also receive a diagnostic.

Exit codes are **0** for complete creation, **2** for creation with exclusions,
and **1** for global/usage failure. Capacity overflow, unreadable/changed input,
NFC naming collisions, identical track identities and zero accepted tracks
are global errors. Limits are 300 accepted tracks, 1 MiB per MDX and 32 MiB
per container; scanning has separate 4096-entry/64-level/1 MiB name-data limits.
Native OS path/access limits also apply. Keep the source tree unchanged while
the command runs; no path-race security guarantee is made for a hostile process.

The album tool writes an exclusive temporary file beside its destination,
flushes and closes it, then publishes the complete file. It replaces an existing
regular destination after successful ingestion; earlier failures preserve it.
It refuses output onto an input MDX (including hard-link aliases), links or
directories. Failed publication removes the owned temporary file; cleanup
failure is reported. Power-loss durability is not claimed for this host tool.

The batch API consumes a bounded `AlbumSourceManifest` through `AlbumReadPort`;
`AlbumOutputPort` separates publication stages for deterministic fault tests.
`rpcmp_utility_native` contains the Windows/POSIX filesystem adapters, while
`rpcmp_utility` can ingest injected sources without linking those adapters.
