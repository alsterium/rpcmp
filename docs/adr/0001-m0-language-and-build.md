# ADR-0001: M0 host language and build system

- Status: accepted
- Scope: M0 host proof; Pocket deployment remains gated by ADR-0004
- Date: 2026-08-25
- Deciders: RPCMP maintainers

## Context

M0 needs one implementation language and one repeatable host verification path. At the initial decision, the inspected Windows host had CMake 4.4.2, CTest, Visual Studio Build Tools/MSBuild 18.9.1 with the MSVC C++ workload, MSVC 19.51.36256 for x64, and Python 3.11. `cl.exe` was not exported in the ordinary PowerShell environment, but CMake detected the installed x86/x64 C++ component. Rust, Verilator, and standalone GCC were unavailable.

LLVM 22.1.8 was subsequently installed for the required host format/static-analysis gates. Quartus Prime Lite 25.1 and Questa Starter 2025.2 were subsequently installed and the official openFPGA template compile was verified as recorded by ADR-0004. Tool presence and a template build remain distinct from selecting a Pocket execution substrate.

The official openFPGA core template supplies FPGA source and APF metadata, but it does not define a general-purpose C/C++ application runtime for an arbitrary core. Therefore M0 must distinguish a portable host reference implementation from the still-unknown Pocket execution substrate.

## Decision

- Implement M0 contracts, runtime, mock device, and replaceable UI in portable C++17.
- Use CMake 3.25 or newer, CMake Presets/Workflow Presets, and CTest. Ninja is the preferred CI generator. The local Windows verification wrapper initializes the MSVC developer environment and uses Visual Studio's bundled Ninja. Unix Makefiles remain supported where available.
- Compile production runtime code with exceptions and RTTI disabled. Do not throw across module boundaries. Use typed result/error values.
- Limit target-facing code to fixed-width integers, value types, bounded containers, `std::variant`, and `std::optional`. Do not make the C++ object layout a serialized, C, FPGA, or plugin ABI.
- Keep host/platform access behind ports. No host filesystem, wall clock, terminal, renderer, threads, or audio API may be referenced by `contracts` or the platform-neutral runtime.
- Use Python 3.11 or newer only for repository/architecture checks. Python is not part of the player runtime.
- Do not implement the utility in M0 and do not use this ADR to choose its eventual language.
- Do not select a Pocket cross-compiler, soft CPU, firmware runtime, Quartus version, or RTL language revision until ADR-0004's evidence gate is closed.

The intended target graph is:

```text
rpcmp_contracts
  ^           ^
  |           |
rpcmp_runtime rpcmp_ui
  ^           ^
  |           |
headless_m0   mock_ui_m0
```

`rpcmp_runtime` must never link or include `rpcmp_ui`. `mock_ui_m0` must never link runtime or engines.

## Alternatives considered

- **C#/.NET:** available on the inspected host, but provides no credible path to the bare FPGA target and would make the M0 proof misleading.
- **Rust:** strong safety properties, but no Rust toolchain is present and no Pocket execution environment has been established.
- **C++20 or newer:** useful library features, but C++17 is the smaller compatibility baseline for a future embedded cross-toolchain.
- **Pure RTL:** appropriate for sound devices and timing-critical adapters, but inefficient for proving the replaceable UI and host-testable contracts.
- **Adopt a third-party soft-CPU/OS now:** premature and would add licensing, FPGA resource, toolchain, and ABI decisions outside M0.

## Consequences

- M0 can establish deterministic behavior and dependency boundaries on ordinary hosts without claiming that the same binary runs on Pocket.
- This workstation can configure and build C++17 with MSVC and Visual Studio's bundled Ninja, and the host workflow enforces the pinned LLVM formatter and analyzer.
- Quartus can compile the official Pocket template, but no Pocket runtime, cross-compiler, or production RTL flow is selected by this ADR.
- Target-facing ownership and allocation assumptions remain conservative, but a later Pocket adapter may require a C facade or a separately compiled firmware implementation.
- M0 completion requires CI evidence from at least GCC and one of Clang/MSVC, plus the Pocket execution decision or an explicit milestone waiver. Host success alone is not Pocket compatibility evidence.

## Evidence reviewed

- Official openFPGA core template: <https://github.com/open-fpga/core-template> (accessed 2026-08-25)
- Initialized `analogue-openfpga-skill` snapshot at commit `81da4c1fac028da594cc2a602ce0ef47a45b64e5` (reviewed 2026-08-25)
