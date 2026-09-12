# ADR-0005: M1 utility language and normalization boundary

Status: accepted

## Context

M1 requires a deterministic host-side `.rpcmlib` writer. The repository already
verifies portable C++17 with CMake, while Unicode NFC normalization is not part
of the C++17 standard library. Adding a Unicode dependency only to serialize an
already-normalized model would enlarge the dependency and licensing surface.

## Decision

- Implement the M1 utility and byte writer as host-side C++17 CMake targets.
- Keep the byte writer free of filesystem, clock, UI, and Pocket dependencies.
- Define the writer input as a normalized logical model. The future ingestion
  layer validates UTF-8 and converts user metadata to NFC before constructing
  that model.
- Hash and serialize the exact NFC UTF-8 bytes in the normalized model. The byte
  writer does not perform platform-dependent Unicode conversion.
- Record and review the normalization implementation and its license before the
  ingestion layer accepts arbitrary source metadata.

## Consequences

The writer and golden fixtures are deterministic and host-testable without a
new dependency. ASCII fixtures are valid NFC test inputs. End-to-end ingestion
of non-ASCII metadata remains incomplete until a normalization implementation
is selected and tested; callers must not label unverified text as normalized.

M6 slice 1 adopts [host metadata v1](../../specs/host-metadata-v1.md) and pinned
utf8proc 2.11.3 for this ingestion responsibility. The normalized writer remains
independent in `rpcmp_utility_writer`; the low-level normalized API retains its
precondition. The new raw-metadata entry point performs NFC before serialization.
