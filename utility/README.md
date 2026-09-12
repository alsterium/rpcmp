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
