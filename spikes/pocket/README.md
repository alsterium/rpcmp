# Pocket Execution-Substrate Comparison Probe

This directory contains non-production feasibility code for ADR-0004. It links the real v1 contracts and deterministic Mock Core so candidate Pocket substrates must compile and run representative RPCMP values rather than a simplified hello-world type.

It is intentionally outside `core/`. Production contracts, runtime, and UI may not depend on this directory, and the architecture test enforces that direction.

## Host gate

Run the complete M0 workflow:

```powershell
pwsh -File tools/host-verify.ps1
```

Run the probe observation record directly after the build:

```powershell
out/build/host-msvc/rpcmp_pocket_comparison_probe_host.exe
```

The host executable runs the same trace with and without a renderer and fails if their semantic records differ. The golden host record is:

| Observation | Value |
|---|---:|
| snapshots | 151 |
| snapshot digest | `6832192089657554689` |
| fake device events | 154 |
| event digest | `16311210033269188847` |
| accepted trace commands | 9 |
| command digest | `2446879228733133299` |
| final snapshot sequence | 151 |

The semantic digests are spike-local FNV-1a records over explicit contract fields in little-endian integer order. They are regression and cross-target comparison values, not a public serialization format.

## Candidate adapters

A candidate supplies only:

- `IProbeBlobReader`, backed by a generated 4,096-byte `synthetic.bin` or an equivalent target data slot;
- optionally `IProbeRenderer`, which observes immutable snapshots and cannot advance Core time.

The synthetic byte at offset `n` is `(n * 37 + 11) mod 256`. The probe checks the first 16 bytes, 16 bytes at offset 2,048, the final 16 readable bytes, and rejection of a 16-byte read beginning at offset 4,088.

The host executable uses `iostream` only for reporting. A target wrapper may report the returned `ProbeRunResult` through its platform console or framebuffer without linking the host executable. The common library itself remains the C++ compatibility input.

See `docs/design/pocket-openfpgaos-spike.md` for the openfpgaOS and minimal-SoC gates, package rules, resource evidence, and selection criteria.

## Docker toolchain gate

Docker Desktop with its Linux engine is the supported Windows path for the
openfpgaOS C++ compatibility gate. The script checks the pinned SDK revision,
builds the checksum-verified xPack 14.2.0-3 image, and links the unchanged
common probe against the SDK's musl runtime:

```powershell
pwsh -File tools/pocket-toolchain-verify.ps1
```

The outputs are:

- `out/build/pocket-openfpgaos/rpcmp-probe.elf`, which reads synthetic slot 4,
  compares both traces with the host golden record, and prints the result on
  the Pocket terminal;
- `out/build/pocket-openfpgaos-desktop/app_pc`, which reads synthetic slot 4
  through the SDK PC backend and must reproduce every host golden digest.
- `out/build/pocket-openfpgaos-package`, the exact allowlisted Pocket tree;
- `out/build/rpcmp-openfpgaos-probe.zip` and its adjacent evidence JSON.

The corrected `0.1.0-spike` target ELF produced `RESULT: PASS` on Pocket on
2026-08-28. This confirms the target slot reader and both deterministic semantic
traces. On firmware 2.6, three consecutive relaunches and a full power-off/start
cycle also passed. The `0.2.0-spike` package retains that automatic gate and
adds a physical input-to-command sequence plus 32 timed logical slot reads. It
subsequently reached `INPUT: PASS` and `OVERALL: PASS` on Pocket; its 16-byte
logical reads measured minimum 1,215 us, integer average 1,254 us, and maximum
1,310 us. The package is a local experiment only: its prebuilt bitstream is not
approved for RPCMP redistribution.

The `0.3.0-spike` package adds a fixed-capacity fake device-write queue and a
zero-copy asynchronous Target dataslot-read measurement. It reached `QUEUE:
PASS`, `INPUT: PASS`, and `OVERALL: PASS` on Pocket. The logical read row was
`1214/1253/1300` us and the Target row was `354/695/4665` us for
minimum/integer-average/maximum. The Target maximum is a tail observation, not
an approved scheduling bound.

## Pocket installation and retest

Use Pocket firmware 2.2 or later. Replace the earlier experiment by removing
these exact paths from the SD card, then extract
`out/build/rpcmp-openfpgaos-probe.zip` at the SD-card root:

```text
/Cores/RPCMP.openfpgaOSProbe
/Assets/rpcmp_probe
/Platforms/rpcmp_probe.json
```

The Target/queue `0.3.0-spike` ZIP SHA-256 is
`4a99a5f43f5f156879d033c6bb855f3b21b88da61ec7275fbc34cfc6c0e1d15c`.
Developer Builds displays this package under its metadata shortname
`openfpgaOSProbe`. Run it and follow the prompts in this order:

1. Press A for Play; confirm `INPUT: 1/4` and `state=playing`.
2. Press B for Pause; confirm `INPUT: 2/4` and `state=paused`.
3. Press B for Resume; confirm `INPUT: 3/4` and `state=playing`.
4. Press START for Stop; confirm `INPUT: 4/4`, `INPUT: PASS`, and
   `OVERALL: PASS`.

Before the input prompts, confirm `QUEUE: PASS`. Also record both displayed
minimum/average/maximum rows:

```text
L .../.../...
T .../.../...
```

`L` is the end-to-end logical slot-read path including SDK file open, seek,
read, and close. `T` is the accepted zero-copy asynchronous Target dataslot
read from issue through completion callback; it includes the 16-byte transfer
but excludes stdio and inter-command ready waiting. If a run fails, record the
Pocket firmware version and exact on-screen state before changing the package.
The previously validated `0.2.0-spike` ZIP remains identified by SHA-256
`ccd6d4ef253861e82d49df2c3bfdd84e04405b073a6d5c4c6c1cf8a57132aa82`;
the validated `0.1.0-spike` ZIP is
`6c261468641a4afd00fc5865c9bbd9e80d0f6593a2b66d849f135d4ca7a45fdb`.
