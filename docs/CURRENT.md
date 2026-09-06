# Current work

Active milestone: [M5 — Real MDX Library Playback](milestones/M5-real-mdx-library-playback.md)

The slice-5 `0.10.0-m5-file` candidate passed the user-reported firmware 2.6
real-file hardware check on 2026-09-06: initial playback, both audio channels,
one minute of playback, warm relaunch, and cold start. See the milestone record
for the exact observations.

The next product task is the remaining
[ADR-0008 production-promotion review](adr/0008-m5-conditional-openfpgaos-substrate.md),
starting with the memory-map/startup ownership explanation and a bounded
code/BSS placement-perturbation verification plan. This candidate's successful
starts do not establish general placement stability. Integrated timing/CDC and
external-I/O constraints, APF lifecycle/failure silence, and explicit future PCM
and UI reserves also remain open. M5 and production-substrate promotion remain
open.

Use the milestone's acceptance criteria and progress evidence as the authority.
Do not infer completion from the existence of a ZIP or passing host tests.
Repository maintenance can proceed without advancing the product milestone.
When work advances, update this pointer and the milestone evidence together.
