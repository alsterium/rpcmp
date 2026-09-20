# Third-party components

Record each component's source, exact revision, license, modifications, purpose, and redistribution implications. Do not add a sound core until license and toolchain suitability are reviewed in an ADR.

## Host metadata dependencies

M6 slice 1 adopts these dependencies before integration. Sources are vendored
unchanged for offline host builds; `metadata-sources.sha256` pins each file.
They do not link into the normalized byte writer, Core, UI or Pocket.

| Component | Source/version | License | Purpose and distribution |
| --- | --- | --- | --- |
| utf8proc | [v2.11.3](https://github.com/JuliaStrings/utf8proc/tree/v2.11.3), Unicode 17.0.0 | MIT plus bundled Unicode data notice in `utf8proc/LICENSE.md` | Strict UTF-8 and bounded NFC at host ingestion; static C library, C++17 callers; ship the full notice with host binaries |
| Microsoft CP932 mapping | [table 2.01](https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP932.TXT), SHA-256 `c9bc0b0cd42e0fbcb82a09635bb5abed86afbdd4abc9e76fa5716638217cb59f` | Unicode data license in `cp932/LICENSE.txt`, original table header retained | Build-time generation of a fixed decoder table; no host code-page dependence; retain data/source notices in host distributions |

Only `utf8proc.c`, `utf8proc.h`, `utf8proc_data.c` and its license are imported
from utf8proc. Project CMake compiles the C99 static source without modifying
it or applying project C++ warning policy to upstream C. CP932 generation is
project-owned tooling. The Pocket font added in slice 5 is described separately below.
The upstream generated `utf8proc_data.c` contains trailing spaces and a final
blank line. Only those two whitespace checks are excepted for that exact file
in `.gitattributes`, to preserve the source hash instead of rewriting upstream
data. The build and `metadata_source_integrity` test verify its complete bytes;
project source formatting and all other diff checks remain enabled.

## Pocket bitmap font

M6 slice 5 adopts the fixed GNU Unifont Japanese 16.0.04 glyph source under
SIL OFL 1.1. See [its source, copyright and notices](unifont/README.md).
The generated derivative is named RPCMP Bitmap JP. Only its read-only index
and glyph bits link into the CPU renderer; the host-only pinned utf8proc
helper and Python generator do not link into Pocket. This does not introduce
a font dependency into Core, the player, or the generic UI.

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
