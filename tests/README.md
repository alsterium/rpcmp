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

Linux/GCC CI is currently deferred. RTL simulation, format parsers, and real container tests remain outside M0.
