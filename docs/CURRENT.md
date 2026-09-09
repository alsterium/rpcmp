# Current work

Active milestone: [M5 — Real MDX Library Playback](milestones/M5-real-mdx-library-playback.md)

The slice-5 `0.10.0-m5-file` candidate passed the user-reported firmware 2.6
real-file hardware check on 2026-09-06: initial playback, both audio channels,
one minute of playback, warm relaunch, and cold start. See the milestone record
for the exact observations.

The [placement review](design/pocket-m5-placement-review.md) records the app
memory map, startup ownership, and a concrete ROM/OS mismatch: fixed ROM IRQ
and syscall targets do not follow the historical OS's `+0x40` code shift.
The user approved bounded CRC retries followed by terminal failure without OS
entry. The `0.10.1-m5-boot` candidate implements that repair, with a same-link
ROM/OS packaging gate and separate normal/CRC-fault probes. Both passed the
user-reported firmware 2.6 [boot check](design/pocket-m5-boot-hardware-check.md)
on 2026-09-09: normal playback, CRC-failure stop/silence, warm/cold starts,
and return from the stopped probe to normal playback.
The [control recovery](design/pocket-m5-control-recovery.md) reconstructed an
ELF/map matching every accepted old ROM/OS byte from retained objects, and
identified 141 BRAM-to-OS relocation records, including UART state and diagnostic
literals. Clean-source reproducibility remains unresolved.
The four [placement candidates](design/pocket-m5-placement-hardware-check.md)
are built and packaged: separate OS code/BSS `+0x40` variants with coherent
fail-closed ROM/OS pairs, and app code/BSS variants with the accepted File
Probe ROM/OS/RBF fixed. The next task is their firmware 2.6 hardware matrix:
initial playback, both channels, one minute, three warm starts and one cold
start per variant. Hardware results remain pending.

General placement stability, integrated timing/CDC and external-I/O constraints,
broader APF lifecycle/failure coverage, and explicit future PCM/UI reserves remain open.
M5 and production-substrate promotion remain open.

Use the milestone's acceptance criteria and progress evidence as the authority.
Do not infer completion from the existence of a ZIP or passing host tests.
Repository maintenance can proceed without advancing the product milestone.
When work advances, update this pointer and the milestone evidence together.
