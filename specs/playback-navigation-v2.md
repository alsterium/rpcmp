# Playback navigation — schema 2

Status: adopted for M6 slice 3 from the
[transition proposal](../docs/design/pocket-player-transition-contract.md).
This extends [commands](player-command-v2.md), [state](player-state-v2.md) and
[repeat policy](playback-policy-v2.md). Existing v1 and the constructors without
a navigation/random port retain their current transport-only behavior.

## Scope and commands

The navigation profile connects album auto-advance and whole-library shuffle.
Capability bit 5 declares this profile and its optional navigation observation;
it requires repeat control (bit 4). The Core owner injects a nonblocking
RandomSource with `next(u32&) -> bool`. Submission and public reads never call it.
Its absence does not install a hidden random generator or pretend shuffle works.

`NextTrack = 8` and `PreviousTrack = 9` carry neither selection nor policy.
They begin the destination from its start, including from Paused; repeat mode
does not override an explicit move. Album order uses adjacent track ordinals
within the current album, never wraps and never turns Previous into restart.
Missing neighbours return `NoNextTrack = 15` / `NoPreviousTrack = 16`.
Unavailable capability, library, selection or failure follows the existing
admission rules. Known destinations may supersede a Loading selection.

Each command is projected against earlier accepted commands. A shuffle cycle
whose random ordering has not yet been produced is explicitly unresolved in
the projection: subsequent Next/Previous returns ResourceBusy until Core steps.
Explicit PlayTrack/LoadTrack, Stop and policy changes can still supersede it.
No random input is consumed for a rejected request, duplicate or snapshot read.
An accepted command can fail asynchronously if random input is unavailable.

## Order and history

The base index contains at most 300 IDs in album display/track ordinal order,
copied from generation-checked catalog pages. It has no container offsets.
Album auto-end selects the next track or retains final position and enters
Ended at the album's last track. RepeatOne's same-track restart takes priority.

A shuffle cycle pins the selected track first, shuffles the remaining IDs,
and counts a track only on successful current-generation audio Start. Entering
shuffle during Playing/Paused registers the already started current track;
entering during preparation registers it only after Start succeeds. Selection,
preparation, cancellation and failure do not count as playback.

Fisher–Yates runs from the tail of the remaining range. For bound b, accept
u32 r only if `r < floor(2^32/b)*b`, then use `r % b`. At most 32 draws per
stage; failed draws or exhaustion report ResourceExhausted, with no biased
fallback. A complete candidate permutation is committed transactionally.
Nonzero cycle IDs increase monotonically; counter exhaustion is terminal.

The cycle keeps first-start history and a separate set of started IDs.
Previous selects the preceding first-start history entry. Next within history
selects its successor; from its tail it selects an unstarted candidate. While
a new candidate is Loading, Next selects another unstarted candidate, if any;
the cancelled candidate remains eligible for later automatic playback.
Previous from such a candidate selects the last started track, if present.
An audible end always chooses the first remaining unstarted candidate, even
after a manual history replay. When none remains, it enters Ended. Preparation
or playback failure stops with an error rather than silently trying another ID.

Pause/Resume, repeat changes, identical policies and RepeatOne restarts preserve
the cycle. Explicit PlayTrack, and Play after Stop/Ended, start a new cycle.
Leaving shuffle discards its ordering. Stop ends the active cycle; its in-memory
ordering/history can still resolve an explicit neighbour, which starts a new
cycle. With no ordering yet, Next from a stopped selected track builds a cycle
and chooses a different candidate; Previous has no history. Catalog changes
and failures invalidate all navigation data. No history persists across boot.

## State, ownership and verification

The copied navigation observation reports can_next/can_previous and an optional
active or ended shuffle cycle: nonzero ID, matching library generation, total
tracks and started count (0..total). Stop removes the public cycle observation.
Availability accounts for projected selection, library, errors and pending
cycle construction. Navigation pending intents identify their resolved target;
automatic transitions have no fabricated public command ID.

The transport owns navigation and uses the same projection code for admission
and execution. Existing generation invalidation, fault-before-command ordering,
reset/preparation ownership and timeout rules apply. The audio port owns time;
navigation responds to committed end/Start observations, never rendering.

Verify independently authored permutations and rejection-sampling boundaries,
one/300-track catalogs, 32-draw limits, cycle exhaustion, album boundaries,
partial/failed/cancelled preparation, history replay, Stop/restart, live order
changes and stale end/Start. Exercise the public ingress and published state
with the real transport and envelope, including sparse publication. Host and
link checks do not establish Pocket sound, UI or hardware acceptance.
