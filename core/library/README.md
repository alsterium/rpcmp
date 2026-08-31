# RPCMP Library Reader

`rpcmp_library` validates an entire `.rpcmlib` envelope and all required v1
logical sections before constructing `LogicalLibrary`. Failed opens leave the
caller-provided output unchanged.

The reader owns no file memory. The immutable source bytes must outlive the
`LogicalLibrary` and every returned `TrackView`, `BlobView`, or `Utf8View`.
Consumers look up tracks and blobs by typed IDs; container offsets, record
ordinals, codecs, and section layout stay private to this module.

This is not yet a complete v1 release: the deterministic writer, stable-ID
generation, golden files, and the 1,000-track proof remain M1 work. Pocket code
must use separately measured admission limits and the pinned safe OS/RBF pair.
