# Retro PC Music Player — Product Requirements Document

- **Version:** 0.8
- **Target:** Analogue Pocket / openFPGA
- **Architecture:** Utility + Library Container + Player Core + Replaceable UI Layer
- **MVP target:** MDXPlayer-compatible MDX / YM2151 + PDX/ADPCM/PCM8
- **Later targets:** rich visualization and other PC-98/PC-88/MSX/FM-7 formats and devices

On 2026-09-21 the user set [asaday/MDXPlayer](https://github.com/asaday/MDXPlayer)
as the playback compatibility reference: all locally supplied files that played
there must be supported. The MVP interaction is track selection, play and stop;
PCM is required. This replaces the earlier FM-only release scope and prioritizes
compatibility over M6 visualization/policy expansion. See the
[reference baseline and acceptance definition](research/mdxplayer-compatibility.md).
On 2026-09-21 the user approved the hybrid verification direction and the
[project charter](../AGENTS.md#project-charter-2026-09-21). Previous RPCMP APIs,
formats and implementations need not remain compatible. Implement the smallest
working slice, document changed boundaries, and retain only useful separation;
music compatibility, input safety and audio independence remain required.
The existing container/UI descriptions below describe the implemented baseline;
they do not require preserving it in the replacement player.
Later on 2026-09-21, the user accepted the six-song hardware playback results,
continued playback, song endings and loops, and closed the current MDX
compatibility investigation. Broad corpus comparison is no longer a prerequisite
for the next MVP work. Keep MDXPlayer compatibility as the support target and
diagnose/fix engine defects when encountered. This is user acceptance of the
current verification scope, not a claim that every file was compared; see the
[acceptance record](research/mdxplayer-compatibility.md#compatibility-verification-accepted).

After accepting Minimal Player r1 and the 27-track M3U import/playback report,
the user selected [continuous M3U-order playback](design/pocket-mdx-compatibility-plan.md#continuous-playback-boundary)
as the next M6 slice. Play from the chosen entry through the end of the list;
looping songs use one intro plus two loop bodies and a five-second fade.
Naturally ending songs play once. Skip recoverable per-track failures, retain
the browsing cursor and mark the playing entry separately. Minimal Player r2
implements these requirements and passes the user's
[Firmware 2.6 hardware report](milestones/M6-album-player.md#minimal-player-r2-hardware-result--2026-09-22).
No errors occurred, so failed-track skipping has host-test coverage but was not
exercised in that hardware report. See the
[implementation evidence](milestones/M6-album-player.md#continuous-playback-implementation--2026-09-21)
and [Japanese hardware procedure](development/pocket-minimal-player.md).

On 2026-09-22 the user selected [pause/resume](design/pocket-mdx-compatibility-plan.md#pause-and-resume-boundary)
as the next small slice. X pauses/resumes the current track; A starts the browsing
selection from its beginning and B stops. X is a provisional input binding,
expected to change when the UI is developed. Keep it separate from playback
commands. Pausing holds song/fade progress and automatic advancement while
list browsing remains available. Minimal Player r3 implements this boundary and
passes the user's [Firmware 2.6 hardware report](milestones/M6-album-player.md#minimal-player-r3-hardware-result--2026-09-22).
It established the hardware baseline for the following loop/repeat slice. See the
[implementation evidence](milestones/M6-album-player.md#pause-and-resume-implementation--2026-09-22).

The subsequent approved slice is [loop/repeat switching](design/pocket-mdx-compatibility-plan.md#loop-and-repeat-switching-boundary):
Y cycles two, three, five loops and repeat one. Changes affect the current song;
the setting is shown and retained during the session, resetting to two at launch.
Counted modes retain five-second fades and M3U advancement; repeat one repeats
the same song. This supersedes the configurable-loop deferral below for this slice.
Minimal Player r4 passes the user's [Firmware 2.6 hardware report](milestones/M6-album-player.md#minimal-player-r4-hardware-result--2026-09-22)
and established the preceding hardware baseline. Failed-track skipping was not reported
in this check; its host evidence remains separate. The user subsequently selected
[multiple-playlist support](design/pocket-mdx-compatibility-plan.md#multiple-playlist-requirements)
and settled its Q1–Q6 requirements. Import multiple M3Us on the PC, with optional
folder import, and browse named lists on Pocket while playback continues from
its original list until a track is selected. Retain each list's cursor/page during
the session. Minimal Player r5 implements 100 lists of up to 300 entries using
HPL2; existing HPL1 collections require reimport. The target build uses
4,486,412 bytes of static memory, including the resident index. Host checks cover
30,000 registrations. The user's [Firmware 2.6 / r5 hardware report](milestones/M6-album-player.md#minimal-player-r5-hardware-result--2026-09-22)
passes the normal three-list/42-entry flow and reports the 100 × 300 entry check
as OK. Index load/validation takes 20 ms normally and 6,598 ms for the scale
collection; total boot time and Q were not separately reported. See the
[implementation evidence](milestones/M6-album-player.md#multiple-playlist-implementation--2026-09-22).
r5 is now the accepted hardware baseline for
[UI interaction requirements](design/pocket-mdx-compatibility-plan.md#ui-interaction-requirements).
The user selected this scope and settled Q1: B returns from tracks to playlists
without stopping playback when the list has focus, and stops when the controls
have focus. B does not change panels. Q2 uses L/R for panel switching; the D-pad
operates within a panel, with up/down entry selection and left/right pages in
lists. Q3 leaves X/Y unassigned and uses A on control icons. Q4 makes the
play/pause icon act on the playback track: pause/resume, or restart the
last-played track after stop. Initial playback starts from the track list.
Q5 adds previous/next within the playback playlist to play/pause, stop and
repeat-count icons. Q6 starts the target from its beginning even from pause/stop.
Q7 disables the unavailable direction at the first/last entry, without wrapping
or changing playback state. Q8 remembers control-icon focus during the session
and retains list cursor/page positions. The remaining operation table/layout
details are under discussion; these requirements have not yet been implemented
in r5.

## 1. Product vision

RPCMP is a portable music player that preserves the character of retro-computer music by sequencing original music data against reconstructed sound hardware. It presents a browsable music library and rich visualization without allowing presentation concerns to alter playback correctness.

## 2. Goals

- Play the user's MDXPlayer-compatible collection with FM and PCM intact,
  using a YM2151-compatible FPGA implementation for FM unless a later decision
  changes that device choice.
- Package tracks, dependencies, metadata, indices, and pre-analysis into a portable `.rpcmlib` container.
- Keep playback/audio logic independently replaceable from UI layout, navigation, and rendering.
- Make parsers, scheduling, library access, and UI logic testable on a host without Analogue Pocket hardware.
- Implement PDX/ADPCM/PCM8 for the MVP and preserve extension points for new
  formats, devices and UIs.
- Provide deterministic state suitable for library screens, keyboard views, channel monitors, FM detail, and waveform/activity visualization.

## 3. Non-goals for the initial release

- Cycle-perfect emulation of an entire X68000 computer.
- Running original X68000 driver binaries or an OS image.
- Editing, authoring, or converting music.
- Network services, streaming, accounts, or DRM.
- Tracker/keyboard visualization, shuffle, configurable loop counts and
  persistent settings as current completion requirements. The fixed two-loop,
  five-second fade is part of the approved continuous-playback slice.
- A final visual design during playback bring-up.

## 4. Product components

### 4.1 RPCMP Utility

A host-side command-line tool that scans source folders, parses supported formats, resolves safe relative dependencies, extracts metadata, performs optional playback pre-analysis, validates limits, and writes reproducible `.rpcmlib` files.

### 4.2 RPCMP Library Container

A versioned, little-endian, random-access container containing track records, deduplicated blobs, metadata, dependency links, optional analysis, and integrity information. Runtime users address content by stable IDs, never by raw file paths or offsets.

### 4.3 Player Core

The platform-neutral owner of library access, transport state, format engines, scheduling, sound-device commands, audio control, and published playback/visualization state. It operates headlessly and receives time from an injected monotonic clock or deterministic test driver.

### 4.4 Replaceable UI Layer

The owner of input mapping, navigation, layout, rendering, widgets, and visualizers. It consumes public snapshots and submits commands. It has no knowledge of MDX parser internals, library offsets, scheduler queues, or FPGA register buses.

## 5. Primary user experience

1. On a host, the user builds a library from legally obtained source files.
2. On Pocket, the user opens the library and browses tracks without scanning source directories.
3. The user starts, pauses, resumes, stops, seeks when supported, and changes tracks.
4. During playback, the UI can show track metadata, transport state, eight FM channels, active notes, levels/activity, and device-specific detail.
5. Playback remains stable while switching views or when rendering is delayed/disabled.

## 6. Functional requirements

### Library

- Reject malformed containers safely and predictably.
- Support stable track lookup and retrieval of associated blobs.
- Support at least 1,000 synthetic tracks without linear full-file scanning on every lookup.
- Preserve unknown optional sections when practical in tooling; runtime may ignore them safely.

### Playback

- Define a generic engine/device boundary before MDX implementation.
- Schedule device writes from playback time, independent of video frames.
- Support deterministic start, pause, resume, stop, and track selection semantics.
- Publish state at a UI-friendly cadence without making snapshot publication part of audio timing.
- Surface errors as state/events rather than drawing dialogs from Core.

### MDX/YM2151 first target

- Parse untrusted MDX data with bounds checking and bounded loops.
- Convert MDX sequencing into timestamped generic device operations.
- Route YM2151 register writes through a device port, not directly from UI or parser.
- Begin with fixed YM2151 test sequences, then validate FM-only MDX fixtures.
- Keep PDX references representable even before sample playback is implemented.

### UI

- Browse libraries and display transport/player state.
- Support Overview, Keyboard, Channel Monitor, and later FM Detail views.
- Run entirely from mock/recorded snapshots for development and visual tests.
- Map platform input to commands in an adapter; public Core commands contain no Pocket button names.

## 7. Quality attributes

- **Correctness:** deterministic host tests and golden event traces precede hardware testing.
- **Timing:** audio/sequencing has priority over snapshots and rendering; UI stalls cannot alter event timing.
- **Safety:** all input is untrusted; arithmetic, ranges, offsets, recursion/loops, and allocations are bounded.
- **Portability:** platform dependencies live behind narrow interfaces.
- **Performance:** no dynamic allocation or blocking I/O on time-critical playback paths after track preparation, unless explicitly measured and approved.
- **Maintainability:** contracts are versioned; dependency direction is mechanically checked.
- **Licensing:** selected FPGA cores and libraries must be compatible with intended distribution and documented before integration.

## 8. Success metrics

- M0 proves headless Core and mock-driven UI in automated tests.
- M1 opens a 1,000-track synthetic library and retrieves arbitrary tracks by ID.
- YM2151 bring-up produces a deterministic fixed test sequence through simulation and hardware audio.
- The MVP plays every locally supplied file known to play in the selected
  MDXPlayer reference, with its required PCM dependencies and musical behavior.
  A successful parse or FM-only result is insufficient. Record unresolved files
  explicitly rather than excluding them to improve the pass rate.
  The current compatibility investigation is accepted by the user on the basis
  recorded above; exhaustive collection testing is not a gate before continuing
  selection/play/stop work. Future reported incompatibilities remain defects
  against this support target.
- Replacing or adding a UI view requires no playback-engine changes.

## 9. Open decisions

- Production Pocket execution substrate: ADR-0008 conditionally selects stripped openfpgaOS for the bounded M5 experiment; production promotion remains gated by placement stability, APF lifecycle, timing/CDC, failure silence, and future PCM headroom, with a project-owned VexRiscv SoC as the required fallback if placement cannot be stabilized.
- Runtime language/build system supported by the Pocket toolchain.
- Licensed YM2151 implementation (for example, whether JT51 is suitable after license/toolchain review).
- Pocket AUDIO output is fixed at 48 kHz by APF; internal device/scheduler clock rates and the resampling topology remain open.
- Snapshot transport and memory ownership on the final platform.
- Fidelity oracle and redistribution-safe MDX fixtures.
