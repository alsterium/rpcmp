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
MDX or audio code. Q1-Q16 are answered. The concrete
[catalog proposal](design/pocket-album-catalog-contract.md) now defines ALBM bytes,
IDs, deterministic ordering, validation and generation-scoped pages. The
[transport proposal](design/pocket-player-transition-contract.md) defines schema-2
compatibility, cancellation, multi-channel loop counting, pause/fade policy
changes and shuffle history. These are reviewable proposals, not active public
contracts or implemented capabilities. Queue v1 and sound-reset v1 do not
provide state-preserving pause or gain ramps; a separately reviewed sound-control
port is a prerequisite. The implementation milestone still needs explicit
ownership of the remaining M5 gates. Q17 is answered: B first stops
playback while staying on the playback screen; a new B press after confirmed
stop returns to the track list. The second action stays in UI and sends no
additional Core command; repeated presses before stop completes are not queued
as navigation. A retains play/pause/resume.

Q18-Q20 are now answered. Settings use on-screen icons selected with the d-pad,
with each selection toggling the value, rather than a dedicated settings-screen
button. Q21 confirms d-pad focus plus A confirmation: moving focus alone changes
neither policy nor playback. A on a settings icon changes that setting; A on the
playback control retains play/pause/resume. Initial focus on that playback control
is proposed so a second A after starting a track can pause it. Playback policy is
to persist across power cycles (Q19), superseding the proposal that excluded
persistence; playback-position/history restoration and automatic playback are
not part of that decision. L/R select the previous/next track on the playback
screen (Q20). The screen/input table and settings-storage proposal below
concretize these decisions.

Q22 confirms browsing the track list while playback continues; Q23 selects
vertically scrolling per-channel playback history for the Tracker view. The
user adopted [Issue #1](https://github.com/alsterium/rpcmp/issues/1) as the initial
layout reference. The [screen proposal](design/pocket-tracker-screen-design.md)
uses one large switchable Tracker/keyboard/library pane, song information below
left and minimal controls below right. B remains the stop-then-list path; view
switching is a separate browsing path that does not stop playback. The initial
profile remains eight FM channels, with a combined play/pause control; the
reference image does not introduce PCM/rhythm or speed-control implementation.
Q24 confirms the repeat-icon cycle: two loops, three, five, RepeatOne, then two.
The [history proposal](design/pocket-performance-history-contract.md) defines
256 timestamped observations, audible commit, retriggers, generation changes and
explicit loss. The [settings proposal](design/pocket-playback-settings-contract.md)
defines a bounded asynchronous port and two 64-byte records, separating policy
application from persistence. The screen proposal now includes focus adjacency,
contextual B, input edges/repeat and ordered policy input. These remain design
proposals; neither v1 contracts nor M5 implementation have changed.

Next, settle title decoding/NFC and fonts, the physical settings adapter's
durability/deadlines, and the sound-control port's MMIO/CDC/ACK and audible-commit
contract. Then adopt the implementation milestone with the remaining M5 gates
explicitly assigned. This is still specification work, not a new implementation
milestone; the closed diagnosis is not reopened.

General placement stability, integrated timing/CDC and external-I/O constraints,
broader APF lifecycle/failure coverage, and explicit future PCM/UI reserves remain open.
M5 and production-substrate promotion remain open.

Use the milestone's acceptance criteria and progress evidence as the authority.
Do not infer completion from the existence of a ZIP or passing host tests.
Repository maintenance can proceed without advancing the product milestone.
When work advances, update this pointer and the milestone evidence together.
