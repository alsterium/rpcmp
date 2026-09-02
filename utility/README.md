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
library is produced. Shift_JIS title conversion is deliberately not guessed by
this first slice, so the caller supplies the normalized UTF-8 display title.
