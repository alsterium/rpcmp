# Tests

Cross-module deterministic fixtures, architecture checks, golden traces, fuzz/property tests, RTL simulation, and host integration tests. See `docs/design/testing-strategy.md`.

Run the implemented M0 host suite on Windows with:

```powershell
pwsh -File tools/host-verify.ps1
```

The workflow requires LLVM 22.1.8 and runs seven CTest gates: contract, runtime/headless, UI/mock-only, positive architecture, negative architecture, `clang-format`, and `clang-tidy`. Formatting and analysis use warnings-as-errors. After configuring `host-msvc`, their CMake targets can be run separately:

```powershell
cmake --build --preset host-msvc --target format
cmake --build --preset host-msvc --target format-check
cmake --build --preset host-msvc --target tidy-check
```

Run the M0 register spike and M2 device queue simulations with Questa Altera
Starter 2025.2:

```powershell
pwsh -File tools/rtl-verify.ps1
```

The script accepts `RPCMP_QUESTA_ROOT` as the directory containing `vsim.exe`; otherwise it checks the documented Quartus Lite installation path. Questa Starter requires a free, 12-month `SW-QUESTA` license. Generate it in Altera's Self Service Licensing Center and set `SALT_LICENSE_SERVER` to the downloaded license-file path before running the script. See the official [Questa Starter licensing instructions](https://docs.altera.com/r/docs/683472/25.3/altera-fpga-software-installation-and-licensing/questa-altera-fpga-edition-and-questa-altera-fpga-starter-edition-software-license). A process started before the environment-variable change automatically reloads the User or Machine value without printing it.

The HDL compiler can validate the source without that environment variable, but
the self-checking simulation cannot run. A passing invocation requires both
testbench PASS markers plus Questa's zero-error, zero-warning summary. The M2
test covers reset, due-time hold, full/reject, sticky flags, backpressure,
wrap/reuse, ordering, and asynchronous clocks. It does not validate the APF
shell, timing closure, YM2151 synthesis, audio output, JTAG, or Pocket hardware.

Run the reproducible Quartus template-integration build separately with:

```powershell
pwsh -File tools/pocket-spike-build.ps1
```

This synthesis/fitting gate does not require the Questa license and does not replace behavioral simulation or on-device testing.

After that build, generate and validate the ignored M0 Pocket SD tree and ZIP with:

```powershell
pwsh -File tools/pocket-package.ps1
```

The package gate validates APF JSON roots and bounds, Interact addresses, byte-level RBF reversal, exact file/ZIP layout, and artifact lengths/hashes. It does not install anything on an SD card or access Pocket hardware.

Linux/GCC CI is currently deferred. Format parsers and real container tests remain outside M0.
