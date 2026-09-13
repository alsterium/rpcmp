# Tests

Cross-module deterministic fixtures, architecture checks, golden traces, fuzz/property tests, RTL simulation, and host integration tests. See `docs/design/testing-strategy.md`.

For current prerequisites and task routing, see the
[development harness](../docs/development/harness.md). Run the complete host
suite on Windows with:

```powershell
pwsh -File tools/host-verify.ps1
```

The workflow requires LLVM 22.1.8 and runs the registered CTest gates, including contract, runtime/headless, UI/mock-only, library/MDX/M5, package, positive/negative architecture, harness, `clang-format`, and `clang-tidy` checks. Discover the current list with `ctest --preset host-msvc -N`; a listing is not an executed test result. Formatting and analysis use warnings-as-errors. After configuring `host-msvc`, their CMake targets can be run separately:

```powershell
cmake --build --preset host-msvc --target format
cmake --build --preset host-msvc --target format-check
cmake --build --preset host-msvc --target tidy-check
```

Run the M0 register spike plus M2 device queue and Pocket AUDIO simulations
with Questa Altera Starter 2025.2:

```powershell
pwsh -File tools/rtl-verify.ps1
```

The script accepts `RPCMP_QUESTA_ROOT` as the directory containing `vsim.exe`; otherwise it checks the documented Quartus Lite installation path. Questa Starter requires a free, 12-month `SW-QUESTA` license. Generate it in Altera's Self Service Licensing Center and set `SALT_LICENSE_SERVER` to the downloaded license-file path before running the script. See the official [Questa Starter licensing instructions](https://docs.altera.com/r/docs/683472/25.3/altera-fpga-software-installation-and-licensing/questa-altera-fpga-edition-and-questa-altera-fpga-starter-edition-software-license). A process started before the environment-variable change automatically reloads the User or Machine value without printing it.

The HDL compiler can validate the source without that environment variable, but
the self-checking simulation cannot run. A passing invocation requires all
configured testbench PASS markers plus Questa's zero-error, zero-warning summary.
The M2 tests cover the queue's reset, due-time hold, full/reject, sticky flags,
backpressure, wrap/reuse, ordering, and asynchronous clocks; the AUDIO test
covers rational sample selection, exact serial framing, silence, saturation,
underflow, overflow, flag clear, and reset. They do not validate APF/JT51
integration, timing closure, physical audio output, JTAG, or Pocket hardware.

Run the same integration test against the actual clean, pinned JT51 checkout
instead of the boundary model with:

```powershell
pwsh -File tools/rtl-jt51-verify.ps1
```

This second command refuses a different revision or local JT51 changes and does
not copy GPL source or generated output into the repository. It also runs the
complete fixed-sequence state machine through the production MMIO queue and
requires 30 ordered JT51 address/data writes plus native audio samples.

M6's native-engine hold experiment has a separate generated-copy comparison:

```powershell
pwsh -File tools/rtl-jt51-hold-verify.ps1 -CenOnly
pwsh -File tools/rtl-jt51-hold-verify.ps1
```

The first command requires the specific expected register-hold failure; the
second requires sample/status equivalence and zero simulator errors/warnings.
Run these and other RTL suites sequentially. The [fixture contract](../specs/jt51-hold-experiment-v1.md)
defines the source recipe, reset/hold semantics, coverage and remaining I2S/
queue/gain/CDC work. Host `jt51_hold_prepare` checks generation guards without
requiring vendor HDL; it does not replace that real-JT51 simulation.

The M6 synchronous pause/output boundary is checked separately:

```powershell
pwsh -File tools/rtl-media-audio-verify.ps1
```

`-OutputOnly` runs the converter/I2S bench without vendor source. The full
command also generates pinned JT51 and compares actual decoded stereo frames
from paused and uninterrupted playback, with independently timed device writes.
The converter oracle is the unchanged v1 implementation at retained test clock
edges. It covers all 256 request phases, held source pulses, old/new pending
ordering, empty holds, clipping/overflow/underflow and urgent reset. The native
bench covers both write halves, reset of a retained write and silence after
reset. A third bench submits back-to-back writes using only `dev_ready`, then
reads the native operator scan to check TL, DT1/MUL and KS/AR values across all
32 slots. Different starting phases and inserted holds must preserve all
accepted values; counting accepted bus operations alone is not sufficient.
These local synchronous tests do not establish CPU/CDC/ACK or Pocket
hardware acceptance; see [the contract](../specs/pocket-media-audio-v1.md).

The mapped-progress and gain controller has a vendor-independent RTL suite:

```powershell
pwsh -File tools/rtl-media-envelope-verify.ps1
```

It evaluates the real 3,360,000/3,600,000 frame start/end values, cancellation
at zero gain, 960-frame restoration, partial-gain restart and signed rounding.
Its gain oracle uses the analytical formula, independently of the RTL's
quotient/remainder recurrence. FIFO capacity/retry, simultaneous replacement,
epoch/policy failures, pause, natural end, fault priority and counter ceilings
are checked. Frame ticks are accelerated; this is not elapsed Pocket playback
or proof that the offered progress has been mapped to audible output correctly.

The composed native/gain/output path has a real-JT51 suite:

```powershell
pwsh -File tools/rtl-enveloped-audio-verify.ps1
```

It decodes external I2S and compares with uninterrupted unscaled native output
multiplied by an analytical policy timeline, removing pause frames. Both signs
and channels, completed restoration, interrupted restoration, stale controls,
Stop, natural end, protocol/coverage faults and post-reset silence are checked.
Converter fault routing is tested by explicit sticky-register injection; the
converter's actual overflow/underflow cases remain in the existing output
suite. This does not prove the source-progress mapper or CPU/CDC transport.

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

Linux/GCC CI is currently deferred. Host checks do not establish Pocket hardware acceptance.
