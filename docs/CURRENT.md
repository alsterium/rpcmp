# Current work

Active milestone: [M5 — Real MDX Library Playback](milestones/M5-real-mdx-library-playback.md)

The slice-5 `0.10.0-m5-file` candidate passed the user-reported firmware 2.6
real-file hardware check on 2026-09-06: initial playback, both audio channels,
one minute of playback, warm relaunch, and cold start. See the milestone record
for the exact observations.

The [placement review](design/pocket-m5-placement-review.md) records the app
memory map, startup ownership, and a concrete ROM/OS mismatch: fixed ROM IRQ
and syscall targets do not follow the historical OS's `+0x40` code shift.
The next task is recovering a byte-matching control ELF/map and adding a
ROM/OS pairing gate before the review's separate OS and app perturbations.
The review also identifies a boot CRC fail-open conflict with ADR-0008;
the affected boot-failure change requires explicit resolution under AGENTS.md.

General placement stability, integrated timing/CDC and external-I/O constraints,
APF lifecycle/failure silence, and explicit future PCM/UI reserves remain open.
M5 and production-substrate promotion remain open.

Use the milestone's acceptance criteria and progress evidence as the authority.
Do not infer completion from the existence of a ZIP or passing host tests.
Repository maintenance can proceed without advancing the product milestone.
When work advances, update this pointer and the milestone evidence together.
