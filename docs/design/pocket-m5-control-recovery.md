# M5 accepted-control recovery and ROM reference audit

Investigation result, 2026-09-09. This closes the missing accepted-control
ELF/map artifact reconstruction identified in the placement review. It does
not close ADR-0008's placement stability or production-promotion gates.

## Recovery result

The retained historical objects relink to identical `.boot`, `.boot_data`,
`.fasttext`, `.fastdata` and `.osdata` sections of the saved historical ELF.
The same check succeeds for the hardware-accepted `0.10.1-m5-boot` firmware.
Adding `--emit-relocs` retains reference evidence without changing those loaded
bytes. Both ELFs identify GCC 13.2.0 (Ubuntu 13.2.0-11ubuntu1+12).

Starting with the historical object list, replace only `kernel/main.o` and
`hal/memtest.o` with their diagnostics-disabled counterparts retained in
`out/build/m5-firmware-control-v3`. This reconstructs the accepted safe-memset
control's complete ROM and OS payload. Replacing main alone leaves a 64-byte
OS-size difference; replacing terminal as well does not remove it. The
main/memtest combination was validated against the already accepted hashes,
not against expected values generated from the recovery implementation.

`tools/pocket_m5_control_relink.py` repeats the successful link into a fresh
directory. It verifies the original MIF and OS pins, then checks every MIF word
and the complete OS payload/CRC/ABI/entry/BSS against the recovered ELF using
the existing pairing gate. The accepted ZIPs, research sources and retained
objects are not modified. The output records hashes of every object/archive,
the linker script, the compiler version, command, map and relocation list.

| Artifact | Result / identity |
| --- | --- |
| Recovered ELF with relocation metadata | `32577cd4d108513b5fff3cfd25d75bada682a7a10ae5b8df993cfa5e10ab4778` |
| ROM image, 15,652 bytes | Exact accepted bytes; `bc904414d8188d4ff8cb38b8b08202f507b3d30d04a5a48bcb09c7d42d4f43f4` |
| Original complete OS, 135,448 bytes | Pair check PASS; `3bb812a1b320c7350046097d361dbf8567662218c9d8ba2f0457e0325f2826a9` |

This is **retained-object recovery**, not a clean-source reproducibility claim.
The earlier clean-source rebuild still differs; the cause of those compiler
input/code-generation differences is not established. In particular, the
mutable control-v3 source tree is not evidence that its retained objects were
compiled from its current source bytes. Preserve the recorded objects.

## Memory and reference findings

Recovered sections use half-open ranges:

| Section | Range | Bytes |
| --- | --- | ---: |
| `.boot` | `0x00000000..0x00002f4c` | 12108 |
| `.boot_bss` | `0x00002f50..0x00003210` | 704 |
| `.fasttext` | `0x00003240..0x00003d24` | 2788 |
| `.osdata` | `0x10320000..0x103410f8` | 135416 |
| `.os_bss` | `0x10341500..0x1038d9f0` | 312560 |

`.boot_data` and `.fastdata` are empty. The 32-byte OS footer ends at
`0x10341118`; the previously documented copy/clear, heap and stack ownership
remains unchanged. Recovery provides exact symbols for this accepted image,
not a runtime stack-depth or memory-high-water measurement.

The retained executable/data relocation sections contain **141 BRAM-to-OS
relocation records**, all in `.rela.boot`, covering 61 distinct symbol-value /
expression pairs. High/low address halves count separately. Debug relocations
are excluded: their offsets can look like BRAM addresses without being CPU
instructions. No such records occur in `.rela.fasttext` or `.rela.fastdata`.

| Target category | Recovered value / consequence |
| --- | --- |
| IRQ / syscall | `0x10336980` / `0x10335e40`; fixed trap-entry calls |
| OS entry fallback | `os_main = 0x1032c740`; OSE2 entry can override the fallback |
| BSS fallback | `0x10341500..0x1038d9f0`; OSE2 can override the fallback |
| Load base | Two linker names at `0x10320000` |
| UART state | Section anchor `0x10342500`; actual ring symbol `0x10342b90` (1024 bytes), head `0x1038d864`, tail `0x1038d868` |
| Diagnostic literals | 51 distinct OS-resident literal addresses referenced from BRAM code |

Thus the trap helpers are not the only fixed dependencies. Boot/trap diagnostic
code and UART handling also depend on OS data placement. Updating two function
pointers alone would leave those dependencies. The local evidence JSON retains
every offset/type/target expression; no third-party firmware bytes are committed.
This inventory covers emitted symbolic relocations in retained BRAM sections;
it is not an exhaustive proof about computed pointers or numeric constants.
The complete byte-pair gate remains necessary in addition to this inventory.

## Reproduction and next experiment

The tool runs in the existing firmware image; the output directory must not
exist. These exact retained inputs are intentionally required for this recovery.

```powershell
docker run --rm --network none -v F:/source/rpcmp:/workspace/rpcmp -w /workspace/rpcmp openfpgaos-firmware python3 -B tools/pocket_m5_control_relink.py --repo /workspace/rpcmp --output /workspace/rpcmp/out/build/m5-control-recovered
```

Image identity: `sha256:98771990c464c0b3231d685de02579a25c6b5b0f952b68d5e573679664070e58`.
The executed command returned `result=PASS bram_to_os_relocations=141`.
`python -B tests/pocket/control_relink_tests.py` passed both tests, covering
debug-relocation exclusion and preservation of existing/out-of-root outputs.
The completion command `pwsh -File tools/host-verify.ps1` passed all 47 CTest
cases, including format, tidy and architecture checks.

Next, prepare the review's separate `+0x40` OS code and BSS perturbations from
the now hardware-accepted fail-closed baseline, preserving identical input
objects. Re-link ROM and OS together; verify actual symbol deltas and pairing,
then generate distinct RBFs/packages and repeat fit/timing and hardware checks.
Each variant still needs three warm relaunches and one cold start. The recovered
old safe control remains a comparison artifact. Do not deploy a shifted OS
with its unmodified ROM. App-only perturbations remain a separate experiment.

No new RTL, FPGA build, hardware observation, or placement variant was created
in this investigation. Historical failing-package ROM provenance and full
placement stability still require evidence.
