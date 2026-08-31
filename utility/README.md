# Utility

Host-side scanner, metadata/dependency extractor, validator, and deterministic `.rpcmlib` builder. It must not share UI code or require Pocket hardware.

M1 implements the utility in C++17. Its deterministic byte writer accepts a
normalized logical model; the ingestion layer is responsible for UTF-8
validation and NFC normalization before stable IDs are generated. See
`docs/adr/0005-m1-utility-language-and-normalization.md`.
