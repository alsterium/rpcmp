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

The target ELF itself has not run until the same semantic record is observed
on Pocket. The package is a local experiment only: its prebuilt bitstream is
not approved for RPCMP redistribution.
