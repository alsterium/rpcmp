# Third-party components

Record each component's source, exact revision, license, modifications, purpose, and redistribution implications. Do not add a sound core until license and toolchain suitability are reviewed in an ADR.

## Development tools

| Component | Version | Source | License | Purpose | Platform and redistribution |
| --- | --- | --- | --- | --- | --- |
| LLVM `clang-format` and `clang-tidy` | 22.1.8 | <https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8> | Apache-2.0 WITH LLVM-exception | Deterministic C++ formatting and static analysis | Windows host verification; installed via Scoop; not linked or redistributed with RPCMP |
| Quartus Prime Lite | 25.1std.0 Build 1129 | <https://www.altera.com/downloads/fpga-development-tools/quartus-prime-lite-edition-design-software-version-25-1-windows> | Altera Quartus Prime License Agreement | Pocket template synthesis, fitting, timing analysis, and bitstream generation | Windows target-spike tooling; not linked or redistributed with RPCMP |
| Questa Altera Starter FPGA Edition | 2025.2 | Installed with Quartus Prime Lite 25.1 | Altera/Siemens development-tool license; free `SW-QUESTA` entitlement expires after 12 months | Self-checking simulation of project-owned M0 RTL | Windows target-spike tooling; not linked or redistributed with RPCMP; requires a node-locked or floating license through `SALT_LICENSE_SERVER` |
| Official openFPGA core template | v1.3.0, `da3a021b1eaf742604d86d8dc9b33a6666263e6a` | <https://github.com/open-fpga/core-template> | No explicit `LICENSE` file at the pinned revision | Exact APF shell and Cyclone V project used for the M0 integration spike | Consumed from a local clone into ignored build output only; source and generated artifacts are not redistributed by RPCMP |

## Target RTL candidates

| Component | Version | Source | License | Purpose | Platform and redistribution |
| --- | --- | --- | --- | --- | --- |
| JT51 | `985a573dcfc1ff135553a39f7eae21d18ba57cbe` | <https://github.com/jotego/jt51/tree/985a573dcfc1ff135553a39f7eae21d18ba57cbe> | GPL-3.0-or-later | YM2151-compatible RTL selected by ADR-0006 for the M2 fixed-sequence proof | Revision-checked research checkout for local simulation, synthesis, and hardware verification only; source, generated HDL, bitstreams, and packages are not redistributed until complete corresponding source, build material, notices, and the GPLv3 section 6 conveyance method are approved |
