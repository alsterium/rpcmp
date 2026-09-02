# Retro PC Music Player — Product Requirements Document

**Version:** 0.4  
**Target:** Analogue Pocket / openFPGA  
**Architecture:** Utility + Library Container + Player Core + Replaceable UI Layer  
**Initial format/device target:** X68000 MDX / YM2151  
**Later targets:** PDX/MSM6258, PCM8, then other PC-98/PC-88/MSX/FM-7 formats and devices

## 1. Product vision

RPCMP is a portable music player that preserves the character of retro-computer music by sequencing original music data against reconstructed sound hardware. It presents a browsable music library and rich visualization without allowing presentation concerns to alter playback correctness.

## 2. Goals

- Play FM-only MDX accurately through a YM2151-compatible FPGA implementation.
- Package tracks, dependencies, metadata, indices, and pre-analysis into a portable `.rpcmlib` container.
- Keep playback/audio logic independently replaceable from UI layout, navigation, and rendering.
- Make parsers, scheduling, library access, and UI logic testable on a host without Analogue Pocket hardware.
- Establish extension points for PDX/MSM6258, PCM8, new formats, new devices, and new UIs.
- Provide deterministic state suitable for library screens, keyboard views, channel monitors, FM detail, and waveform/activity visualization.

## 3. Non-goals for the initial release

- Cycle-perfect emulation of an entire X68000 computer.
- Running original X68000 driver binaries or an OS image.
- Editing, authoring, or converting music.
- Network services, streaming, accounts, or DRM.
- PDX/PCM8 playback in the first FM-only milestone.
- A final visual design during architecture bring-up.

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
- MDX FM milestone plays a curated, redistributable or locally supplied FM-only conformance set with event traces matching approved references.
- Replacing or adding a UI view requires no playback-engine changes.

## 9. Open decisions

- Production Pocket execution substrate: ADR-0008 conditionally selects stripped openfpgaOS for the bounded M5 experiment; production promotion remains gated by placement stability, APF lifecycle, timing/CDC, failure silence, and future PCM headroom, with a project-owned VexRiscv SoC as the required fallback if placement cannot be stabilized.
- Runtime language/build system supported by the Pocket toolchain.
- Licensed YM2151 implementation (for example, whether JT51 is suitable after license/toolchain review).
- Pocket AUDIO output is fixed at 48 kHz by APF; internal device/scheduler clock rates and the resampling topology remain open.
- Snapshot transport and memory ownership on the final platform.
- Fidelity oracle and redistribution-safe MDX fixtures.
