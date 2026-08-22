# Testing Strategy

## Test pyramid

- Host unit tests: contracts, transport, parsers, library validation, time conversion.
- Property/fuzz tests: malformed MDX and `.rpcmlib`, overflow, truncation, loops, allocation bounds.
- Golden tests: MDX-to-device event traces and container bytes.
- Architecture tests: forbidden dependencies and independent link targets.
- RTL simulation: reset, clocks, register ordering, FIFO/backpressure, audio test sequences.
- Host integration: deterministic Core with fake library/clock/device sink.
- Hardware integration: bitstream, controls, audio path, long-run stability.

## Determinism

Tests inject clock, storage, and device sinks. They do not depend on sleep duration, host audio, filesystem ordering, locale, or current time. Failures print reproducible seeds/traces.

## Fixtures

Commit only synthetic, public-domain, self-authored, or explicitly redistributable fixtures. Maintain provenance in a fixture manifest. User-supplied music may be used locally but must remain ignored by version control.
