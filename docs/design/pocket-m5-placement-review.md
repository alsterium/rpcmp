# M5 memory placement and boot/OS pairing review

Status: investigation and experiment plan, 2026-09-06. Governed by
[M5](../milestones/M5-real-mdx-library-playback.md) and
[ADR-0008](../adr/0008-m5-conditional-openfpgaos-substrate.md).
No new hardware candidate or substrate promotion is established here.

## Finding: the ROM contains OS addresses beyond entry and BSS

The historical `0.5.32` versus `0.5.36` comparison moved later OS code and BSS
by `0x40`; `0.5.37` retained the failing layout while skipping the added call.
That history concerns OS placement, not the independently linked M5 app.

Inspection of the accepted boot MIF's decoded instructions found fixed OS
call targets in the BRAM trap entry. The saved diagnostic ELF/map pairs expose
the corresponding symbols:

| Reference | Accepted ROM target | `0.5.32` ELF symbol | `0.5.36` ELF symbol |
| --- | --- | --- | --- |
| IRQ call at ROM `0x126..0x12e` | `0x10336980` | `irq_handler = 0x10336980` | `irq_handler = 0x103369c0` |
| Ecall dispatch at ROM `0x152..0x15a` | `0x10335e40` | `syscall_dispatch = 0x10335e40` | `syscall_dispatch = 0x10335e80` |

These are PC-relative `auipc`/`addi` address constructions followed by `jalr`;
their destinations are fixed when the ROM is linked. `targets/pocket/boot/start.S`
uses `la t0, irq_handler` and `la t0, syscall_dispatch`. Replacing only `os.bin`
does not relocate these instructions in the FPGA's initialized BRAM.

This is a demonstrated incompatibility between the accepted ROM and the shifted
diagnostic ELF, and a concrete explanation to test for the historical blackout.
It is not yet a hardware-confirmed root cause: the historical failing package's
embedded ROM must also be tied to its exact build evidence, and a coherently
linked replacement must pass hardware. Do not classify the history as an
unexplained cache fault or declare it repaired from this inspection alone.

The OS footer already carries `OABI`, `OSE2`, and `OFC1`. All four inspected
OS binaries pass CRC32 over all bytes before the final eight-byte CRC trailer.
They share boot ABI ID `0x4f2ee14d`, despite different function locations:

| OS image | Entry | BSS start | BSS end (exclusive) |
| --- | --- | --- | --- |
| Safe-memset control | `0x1032c740` | `0x10341500` | `0x1038d9f0` |
| `0.5.32` label-only | `0x1032c780` | `0x10341510` | `0x1038da00` |
| `0.5.36` empty-write | `0x1032c780` | `0x10341550` | `0x1038da40` |
| `0.5.37` skipped-empty-write | `0x1032c780` | `0x10341550` | `0x1038da40` |

Accepted ROM `0x8c8..0x938` implements the OSE2 reader; it loads entry/BSS
into BRAM words `0x2f58`, `0x2f54`, and `0x2f50`. Its fallback constants also
match the safe control. OSE2 does not contain IRQ/syscall targets. Therefore
valid CRC, matching OABI, and updated OSE2 alone do not prove ROM/OS pairing.

## Inspected identity and memory ownership

The local `rpcmp-m5-file.zip` ELF and OS members were byte-compared with the
inspected files, with equality for both. The ELF is 89,412 bytes, SHA-256
`237b16d8f8abaf67471518cc3bb8c33e76d70f5e11d729c1fe8ae2f46a01fb85`.
The 135,448-byte OS retains the recorded safe-control SHA-256
`3bb812a1b320c7350046097d361dbf8567662218c9d8ba2f0457e0325f2826a9`.
The reviewed MIF decodes to 15,652 bytes through `0x3d24`, SHA-256
`bc904414d8188d4ff8cb38b8b08202f507b3d30d04a5a48bcb09c7d42d4f43f4`.
No music identity or music hash is needed for this review.

All ranges below are half-open. App values come from its ELF/map; OS values
come from the checked image footer and pinned source. Reserved areas are
source-defined boundaries, not measured runtime high-water marks.

| Area | Address range / boundary | Initialization owner |
| --- | --- | --- |
| CPU BRAM | `0x00000000..0x00008000` | FPGA MIF initializes boot/trap/hot code; `_start` clears boot BSS |
| Optional app BRAM | `0x00004000..0x00007800` | OS ELF loader; unused by this M5 ELF |
| Boot/trap stack | Top `0x00008000` | ROM startup/trap entry; exact worst-case depth remains unmeasured |
| App framebuffers | Bases `0x10000000`, `0x10100000`, `0x10200000` | OS video service |
| Terminal framebuffer | Uncached base `0x50300000` | OS terminal service |
| Safe OS loaded file | `0x10320000..0x10341118`, including footer | ROM copies slot 1 through CRAM0 scratch to uncached SDRAM alias |
| Safe OS BSS | `0x10341500..0x1038d9f0` | ROM `os_finalize_memory` clears through uncached alias before OS entry |
| OS linker region | `0x10320000..0x103e0000` | OS linker limits; unused tail is not an app allocation |
| Interact area | Base `0x103fe000` | OS/APF adapter |
| App file-backed segment | `0x10400000..0x10410820` | OS `elf_load` reads slot 3 through uncached alias |
| App BSS | `0x10410820..0x106ec338` | OS loader clears `p_memsz - p_filesz` then invalidates cached aliases |
| App heap start | `0x106ec340` (16-byte aligned segment end) | OS `syscall_init`, then musl allocation |
| App static ceiling | `0x13a00000` | SDK linker and OS segment validation |
| App stack, no sample bank | `0x13a00000..0x13a80000` | Source-derived 512 KiB below audio reserve; not a measured SP trace |
| OS audio ring reserve | `0x13a80000..0x13b00000` | OS mixer reserves 512 KiB even though M5 AUDIO uses JT51 directly |
| OS file cache | `0x13b00000..0x13f00000` | OS file service |
| OS runtime stack | `0x13f00000..0x13f80000` | ROM clears; trap/syscall C code uses this stack |
| High guard / GPU tables | `0x13f80000..0x14000000` | Source reservations; not application headroom |

The app has one `PT_LOAD`: file offset `0x1000`, VMA/PAddr `0x10400000`,
FileSiz `0x10820`, MemSiz `0x2ec338`, alignment `0x1000`; entry is `0x10400000`.
Its `.text` ends at `0x104106d4`; `.eh_frame` ends at `0x1041073c`;
the four-byte constructor array ends at `0x10410740`; `.data` ends at
`0x10410820`. SDK small BSS is deliberately included in file-backed `.data`.
The global pointer is `0x10410fd0`.

The large BSS objects are pump storage (`0x10410820`, `0x20048` bytes), session
workspace (`0x10430868`, `0xb83f8`), session (`0x104e8c60`, `0x2fd0`), and
container buffer (`0x104ebc30`, `0x200000`). `m5_file_probe.cpp::in_bss`
uses `alignas(T)` storage and placement construction after loader clearing;
normal C++ default initialization still supplies values such as Timer B.
musl CRT handles startup and constructor arrays. No public defaults change.

The existing budget records 3,064,632 static bytes and a conservative
18,288-byte app stack bound; this review does not reinterpret that bound as
measured stack usage or a complete OS/interrupt call-graph bound. The selected
CPU configuration has 16 KiB I-cache and 32 KiB D-cache, with 64-byte lines;
upstream HAL comments describing a 128 KiB D-cache are not the selected fit.
Cached `0x10xxxxxx` and uncached `0x50xxxxxx` SDRAM are aliases, not additional RAM.

The preserved `firmware-0.5.32.elf` and `firmware-0.5.36.elf` BRAM images differ
from the accepted ROM by 115 and 124 bytes respectively when allocated,
file-backed BRAM sections are copied into a zero-filled 15,652-byte image.
They are useful comparison symbols, not the final control ELF. Recover a
byte-matching control ELF/map before claiming ADR-0008 gate 2 fully closed.

## Separate conflict: boot CRC failure currently permits execution

Pinned upstream `targets/pocket/boot/boot.c::boot_load_os_sd` returns success
after eight failed CRC loads, after displaying `BOOTING UNVERIFIED IMAGE`.
The accepted ROM contains that message at `0x2ea8`; disassembly at
`0x118a..0x11a2` compares the retry counter against seven, prints the message
when exceeded, and continues into metadata handling instead of halting. This behavior
conflicts with ADR-0008's requirement that storage failures stop playback work
and remain silent, and with the PRD's bounded failure requirement for untrusted
input. Ordinary successful playback does not exercise this path.

The user explicitly approved bounded CRC retries followed by terminal failure,
without entry into the failed OS image and with sound reset/silence. This
resolves the implementation decision required by `AGENTS.md`; it does not
relax ADR-0008. The 2026-09-08 repair is a separate coherent baseline, documented
in the [boot hardware handoff](pocket-m5-boot-hardware-check.md), with hardware
failure silence still pending. The accepted control remains preserved.

The clean pinned-source build plus safe-memset patch did not reproduce the old
OS: 135,768 bytes versus the accepted 135,448. The new fail-closed build also
has a 135,768-byte OS, linked with its own boot ROM. Do not call this recovery
of the old control or an isolated placement perturbation. Exact reproduction
and the broader reference/placement investigation remain open.

## Bounded experiment plan

1. Recover the exact control build recipe and ELF/map in a new ignored output
   tree from core `618a3eb985759a4154115109c2c8036271252888` plus recorded patches.
   Require both OS image and decoded ROM equality with the control; do not use
   the mutable research tree's latest `firmware.elf` as provenance. Record all
   ROM-to-OS code and data references, including trap helpers and global state,
   not just the two demonstrated call targets.
2. Add a host build/pairing gate before creating candidates: ELF bounds,
   alignment, footer CRC/entry/BSS, and every retained ROM/OS reference must
   agree. Use the archived `0.5.32`/`0.5.36` target difference as independent
   evidence for a negative case. Repository tests use project-authored minimal
   instruction/ELF fixtures, not imported ROMs. A metadata-only check must fail
   this mismatched-pair case. No public RPCMP or APF contract needs changing.
3. Resolve the CRC failure conflict separately and verify the terminal path
   with injected CRC/read failure before incorporating it into any new ROM.
   Do not combine a behavior repair and a placement perturbation as one
   unexplained comparison: establish a coherent baseline first.
4. Perturb OS code and BSS separately using linker padding in isolated build
   scripts: first `+0x40` before the later OS text, then `+0x40` before OS BSS.
   Record resulting symbol deltas, not merely requested padding. Keep code
   semantics and public defaults unchanged. **Generate boot ROM and OS from
   the same link** for every OS variant; OSE2 does not make the old ROM safe.
   Produce the corresponding RBF and re-run fit/timing/CDC/report/MIF gates.
   If the chosen flow patches memory initialization after fitting, verify that
   flow explicitly and record final assembler reports and RBF identities.
5. Separately perturb only the app: `+0x40` before ordinary text while keeping
   `_start` explicit, then `+0x40` before large BSS. Relink from identical object
   inputs using isolated linker scripts. Keep the accepted OS/ROM/RBF fixed.
   Record section, PT_LOAD, gp, workspace, heap, and stack boundaries and reject
   changes to instructions other than relocation or to non-padding data.
   Repeat budget, host, sequencing preflight, APF/path, and package checks.
6. Give each baseline/variant distinct core/platform/package identity, with
   hashes for ELF, map, OS, MIF, RBF, loader, and build inputs. Existing control
   ZIPs and the local library remain unchanged. Hardware runs use firmware 2.6:
   initial boot, real-file admission, both audio outputs, one minute of stable
   playback, **three warm relaunches and one full power-off cold start per
   variant**. Record last visible stage on failure and stop that variant.

This first matrix has four perturbations, one variable per row, plus its
baseline(s). Only broaden offsets after a failure or unresolved explanation
warrants it. A passing app matrix does not close the OS placement gate. A
passing coherent OS matrix still needs review of the reference explanation;
if it cannot bound placement stability, ADR-0008 requires the minimal VexRiscv
SoC comparison. APF lifecycle/heartbeat, integrated CDC/external-I/O policy,
hardware failure silence, and explicit future PCM/UI reserves remain separate.

## Evidence and reproduction

Inspected local sources: SDK `a408ddc12aed0dfaa4aa22c06af82f829db77126`
`src/sdk/app.ld`; clean core revision above, `src/firmware/os/os.ld`,
`targets/pocket/target_platform.h`, `targets/pocket/boot/{start.S,boot.c}`,
`kernel/{loader.c,main.c,syscall.c}`, and `hal/{cache.c,mixer.c}`. ROM bytes
were decoded from the MIF pinned by `tools/pocket_m5_audio_prepare.py`.
The app map/budget are under `out/build/pocket-openfpgaos-m5-file`.
Historical maps/ELFs are in the `-lf` research tree's `src/firmware/os/bld/pocket`.
These ignored files must remain local; their presence alone does not prove a
reproducible control build.

The following read-only cross-tool invocation was run with the existing pinned
image. Substitute `riscv-none-elf-nm -n <saved-ELF>` for symbol listings and
`riscv-none-elf-objdump -d --start-address=0x124 --stop-address=0x176 <saved-ELF>`
for the trap comparison. Decoded ROM inspection uses objdump's `-D -b binary
-m riscv:rv32` options at the same addresses.

```powershell
docker run --rm -v F:/source/rpcmp:/workspace/rpcmp:ro -w /workspace/rpcmp rpcmp-openfpgaos-toolchain:14.2.0-3 riscv-none-elf-readelf -lW out/build/pocket-openfpgaos-m5-file/rpcmp-m5-file.elf
```

Python `struct.unpack('<6I', image[-32:-8])` reads the OABI/OSE2 words;
`zlib.crc32(image[:-8])` was compared with the final little-endian word after
checking the `OFC1` marker. ZIP members were compared byte-for-byte, not by
filename alone. These are artifact inspection results, not CPU execution tests.

[Analogue's boot process](https://www.analogue.co/developer/docs/core-boot-process)
was checked on 2026-09-06: heartbeat must continue after bitstream startup;
reset-enter stops execution and reset-exit starts it. A successful UI launch
alone is not a captured APF lifecycle trace. No new RTL, Quartus, or hardware
execution was performed for this documentation investigation.
