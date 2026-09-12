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
Probe ROM/OS/RBF fixed. On 2026-09-10 the user reported all four candidates
PASS on firmware 2.6: initial playback, both channels, one minute, three warm
starts and one cold start per variant. The [bounded explanation review](design/pocket-m5-placement-explanation.md)
confirms that historical packaging commits paired diagnostic OS images with
the fixed safe RBF. Retained MIF/OS/ELF identities support the mismatch;
exact historical ZIP attribution and old clean-source reproduction remain open.
The firmware 2.6 [admission-failure check](design/pocket-m5-failure-hardware-check.md)
passed by user report on 2026-09-10 for all three input-only probes: FAIL codes
2/3/4, explicitly confirmed RESET OK, one minute of silence/no PLAYING,
warm/cold starts and return to normal playback. The old APF candidates passed
reported playback/silence/recovery, but TIMEOUT also stalled OS filesystem
initialization for about 40 seconds. The [APF2 repair](design/pocket-m5-apf-primer-hardware-check.md)
excludes offset-zero OS primers; its hardware results remain unverified.
Admission-failure silence does not establish transport-timeout or in-playback
fault handling; these candidates retain that distinction.

On 2026-09-12 the user closed the current bug-investigation workstream and
directed that new failures be diagnosed when observed. Do not require APF2
hardware confirmation or more placement experiments before defining the next
requirements. Preserve the existing results and unverified items as recorded;
this decision does not turn an unrun check into a pass.

The current task is the [album-player specification draft](design/pocket-library-player-spec-draft.md),
based on the accepted [track selection and playback requirements](design/pocket-next-requirements.md).
The user's Q1-Q8 answers, completed on 2026-09-13, select 100-300 FM-only tracks,
album-first browsing, play on track confirmation, folder-derived albums and
numeric filename order. Normal playback advances within the album and stops at
its end; looping tracks default to two passes followed by fade-out. Explicit
single-track repeat, count-limited repeat and whole-library shuffle are included.
Define album/order metadata, Pocket loading budgets, playback-policy contracts,
pause/fade behavior and implementation acceptance before coding. Do not reopen
the settled feature-priority questions. M5 remains active during this planning.
The draft proposes folder-derived catalog/order metadata and an in-memory
300-track / 32 MiB profile with an explicit budget estimate. These are design
proposals, not changed public contracts or measured new-build results.
Q9-Q11 are answered: play the intro once and loop section twice, fade over five
seconds, and stop after one shuffle cycle with no repeated track within it.
Q12-Q14 are answered: build from admitted tracks and report every exclusion;
apply counted loops only to looping tracks, with finite tracks playing once;
prefer the embedded MDX title and fall back to the filename when unreadable or
empty. Filename-based ordering remains unchanged. The batch-ingestion decision
does not change the existing one-track M5 rejection contract.
Q15 is answered: apply playback-setting changes to the current track without
resetting its position; start fading when the new loop count has already been
reached. Q16 selects A for play/pause/resume and B for stop, with an easily
replaceable input/UI mapping so later usability changes do not alter Player,
MDX or audio code. Q1-Q16 are answered; next is concrete contract and state-machine
design, not another pass over the same preference questions.

General placement stability, integrated timing/CDC and external-I/O constraints,
broader APF lifecycle/failure coverage, and explicit future PCM/UI reserves remain open.
M5 and production-substrate promotion remain open.

Use the milestone's acceptance criteria and progress evidence as the authority.
Do not infer completion from the existence of a ZIP or passing host tests.
Repository maintenance can proceed without advancing the product milestone.
When work advances, update this pointer and the milestone evidence together.
