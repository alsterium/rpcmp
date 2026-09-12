# Current work

Active milestone: [M6 — Album Player](milestones/M6-album-player.md)

The current task is M6 slice 2: implement Core-owned generation/catalog pages.
The [ALBM storage profile](../specs/album-catalog-v1.md) has a writer and validated
logical reader. [Folder ingestion](../specs/album-ingestion-v1.md) now implements
deterministic album/track order, bounded capacity, explicit exclusions and
native publication through `rpcmp_album_pack`; see the milestone for evidence.
Slice 1 implements [host metadata v1](../specs/host-metadata-v1.md); Windows host
checks and Linux metadata/writer checks passed. See the milestone for evidence.
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
