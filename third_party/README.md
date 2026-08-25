# Third-party components

Record each component's source, exact revision, license, modifications, purpose, and redistribution implications. Do not add a sound core until license and toolchain suitability are reviewed in an ADR.

## Development tools

| Component | Version | Source | License | Purpose | Platform and redistribution |
| --- | --- | --- | --- | --- | --- |
| LLVM `clang-format` and `clang-tidy` | 22.1.8 | <https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8> | Apache-2.0 WITH LLVM-exception | Deterministic C++ formatting and static analysis | Windows host verification; installed via Scoop; not linked or redistributed with RPCMP |
| Quartus Prime Lite | 25.1std.0 Build 1129 | <https://www.altera.com/downloads/fpga-development-tools/quartus-prime-lite-edition-design-software-version-25-1-windows> | Altera Quartus Prime License Agreement | Pocket template synthesis, fitting, timing analysis, and bitstream generation | Windows target-spike tooling; not linked or redistributed with RPCMP |
