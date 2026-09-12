# Current work

Active milestone: [M6 — Album Player](milestones/M6-album-player.md)

The current task is M6 slice 3: adopt and implement public schema 2 command
ingress/snapshots around the [Core transport](../specs/playback-transport-v1.md),
then policy/history/settings and mock UI/input behavior through injected ports.
The internal transport now handles asynchronous preparation/reset/start/pause/
resume, cancellation, fault priority and deadlines with scripted host ports.
Its full host gate, Linux sanitizer tests and RISC-V link probe passed; this
does not establish a working Pocket audio adapter or complete slice 3.
Use the [transition proposal](design/pocket-player-transition-contract.md),
preserve v1 compatibility and adopt each contract before its implementation.
Slices 1–2 implement [host metadata](../specs/host-metadata-v1.md),
[ALBM storage](../specs/album-catalog-v1.md),
[folder ingestion](../specs/album-ingestion-v1.md) and
[Core-owned catalog pages](../specs/catalog-query-v1.md). Windows host checks,
focused Linux sanitizer tests and the catalog RISC-V link probe passed; see the
milestone for exact evidence. These are not M6 playback/UI/hardware acceptance.
The [accepted Q1–Q24 requirements](design/pocket-library-player-spec-draft.md)
define the intended album player; do not reopen settled product questions.
Continue through the milestone's host-verifiable slices until a concrete
hardware check or an unresolved product decision requires the user.

[M5 evidence](milestones/M5-real-mdx-library-playback.md) remains preserved.
The user closed its investigation on 2026-09-12; M5 is deferred, not passed.
M6 explicitly owns the remaining timing/CDC, placement, APF failure/lifecycle
and future PCM/UI reserve gates. ADR-0008 production promotion remains gated.
No unrun APF2 hardware result becomes a pass by moving this pointer.

Use the active milestone's acceptance criteria and actual checks as the
completion authority. Update this pointer and milestone evidence when a slice
advances; do not duplicate historical diagnostic instructions here.
