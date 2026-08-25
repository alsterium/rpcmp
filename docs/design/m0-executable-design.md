# M0 Executable Design

## 1. Purpose and status

This document turns the language-neutral v1 contracts into an implementable M0 host profile. It does not define a wire format, freeze a C++ ABI, implement `.rpcmlib`, or claim Pocket compatibility. ADR-0001 through ADR-0004 are normative for M0 decisions; `docs/design/pocket-platform-boundary.md` records the known APF constraints without selecting an execution substrate.

M0 proves this closed loop:

```text
deterministic clock -> mock runtime -> immutable value snapshot -> replaceable UI
                              ^                              |
                              +--------- command -----------+
```

The renderer is outside every arrow that advances media time.

## 2. Source and target layout

Implementation should add only the following M0 surfaces:

```text
CMakeLists.txt
CMakePresets.json
cmake/
core/
  contracts/include/rpcmp/contracts/
  runtime/include/rpcmp/runtime/
  runtime/src/
  ui/include/rpcmp/ui/
  ui/src/
  platform/host/
  rtl/                         # placeholder only
tests/
  architecture/
  contracts/
  runtime/
  ui/
  fixtures/m0/
tools/
```

Target responsibilities:

| Target | May depend on | Must not depend on |
|---|---|---|
| `rpcmp_contracts` | C++ standard library | runtime, UI, platform, engines, RTL |
| `rpcmp_runtime` | contracts | UI, host renderer/input, MDX, real container layout |
| `rpcmp_ui` | contracts | runtime, engines, device buses, container layout |
| `rpcmp_host_ports` | contracts, runtime | UI unless it is a separate UI adapter target |
| `rpcmp_headless_m0` | contracts, runtime, host fake ports | UI |
| `rpcmp_mock_ui_m0` | contracts, UI, test fixture factory | runtime, engines |

Test fixture factories are not production dependencies. Shared contract fixtures live below `tests/fixtures/m0`, not in runtime.

## 3. Concrete M0 contract profile

All enums have explicit fixed-width underlying types. All IDs are strong value types rather than interchangeable integer aliases. Tick and sequence arithmetic uses `uint64_t`; counts and fixed capacities use unsigned fixed-width values. No contract exposes a pointer, callback, path, container offset, engine/parser type, pixel coordinate, physical control, or FPGA register.

C++ objects are source-level values only. Tests compare fields, not padding or raw object bytes.

### 3.1 Bounds

These are M0 in-memory admission limits, not `.rpcmlib` v1 limits:

| Value | M0 maximum |
|---|---:|
| channels per snapshot | 16 (fixture uses exactly 8) |
| devices per snapshot | 4 |
| extensions per snapshot | 8 |
| extension payload | 256 bytes |
| visualization samples | 64 |
| title / artist / album / composer | 96 UTF-8 bytes each |
| channel label | 24 UTF-8 bytes |
| instrument label | 48 UTF-8 bytes |
| error detail | 160 UTF-8 bytes |
| library reference | 96 UTF-8 bytes |
| queued commands | 32 |
| retained command results | 64 |

Admission checks count bytes and separately validate UTF-8. A value exactly at its maximum is valid. Truncation is forbidden at the contract boundary; an over-limit producer receives a typed error. UI formatting may elide a valid value for display without changing the snapshot.

### 3.2 Snapshot details

The concrete types contain every field from `specs/ui-state-api.md` plus explicit M0 representations:

- capability sets are `uint64_t` bitsets; unknown bits are preserved by copies and ignored by consumers;
- normalized activity/level is an integer from 0 through 255;
- pan is optional signed Q1.7-style presentation data in the inclusive range -127 through 127, where 0 is center; it is not a mixer authority;
- MIDI-like note is optional `uint8_t` in 0 through 127;
- fine pitch is optional signed cents in -100 through 100;
- `PositionState.tick_rate` is non-zero; unknown duration/loop values use empty optionals, never zero sentinels;
- channel/device relationships use typed IDs and are validated within a complete snapshot;
- extension `type_id` is a stable 32-bit identifier, `version` is `uint16_t`, and payload is an opaque bounded byte value in M0;
- visualization contains bounded activity summaries only; M0 does not publish audio samples.

The M0 fixture publishes one device summary with ID 1, generic type `ym2151`, operational state `mock`, and channel IDs 0 through 7. “YM2151” describes the intended logical device shape; no synthesis or real register behavior is implemented.

### 3.3 Command result details

`CommandResult` contains the command ID, outcome (`accepted`, `rejected`, or `duplicate`), stable reason, and the latest snapshot sequence observed during admission. M0 reason codes are:

```text
none
unsupported_schema
invalid_command_id
duplicate_command_id
stale_command_id
stale_snapshot_sequence
malformed_request
invalid_state
unknown_library
unknown_track
unknown_channel
unsupported_capability
queue_full
resource_busy
```

Diagnostic strings are optional and never used in equality or control flow.

## 4. M0 fixture model

### 4.1 Library and tracks

The fake library port recognizes only logical reference `m0:library`. It contains three tracks with IDs 1, 2, and 3 and synthetic metadata. No source files, paths, container bytes, or offsets exist. `NextTrack` and `PreviousTrack` wrap in this declared order.

Internal state has independent `library_open` and `track_prepared` flags. The public transport is `empty` whenever no track is prepared, whether or not the fake library is open. M0 library browsing is supplied directly to the mock UI fixture and is not smuggled into `PlayerSnapshot`.

### 4.2 Transport transitions

The table describes semantic admission against projected FIFO state. “same” means accepted without changing transport. Any unlisted transition is rejected as `invalid_state` unless a more specific validation error wins.

| Command | empty, library closed | empty, library open | stopped | playing | paused | ended |
|---|---|---|---|---|---|---|
| `OpenLibrary` | empty/open | `resource_busy` | `resource_busy` | `resource_busy` | `resource_busy` | `resource_busy` |
| `CloseLibrary` | invalid | empty/closed | empty/closed | empty/closed | empty/closed | empty/closed |
| `LoadTrack(known)` | invalid | stopped | stopped/reset | stopped/reset | stopped/reset | stopped/reset |
| `Play` | invalid | invalid | playing | same | playing | playing from position 0 |
| `Pause` | invalid | invalid | invalid | paused | same | invalid |
| `Resume` | invalid | invalid | invalid | same | playing | invalid |
| `Stop` | invalid | invalid | same at position 0 | stopped/reset | stopped/reset | stopped/reset |
| `TogglePause` | invalid | invalid | invalid | paused | playing | invalid |
| `NextTrack` / `PreviousTrack` | invalid | stopped with wrapped selection | stopped/reset | stopped/reset | stopped/reset | stopped/reset |

Additional rules:

- `LoadTrack` never starts playback.
- `Play` from paused is exactly `Resume`.
- `Play` from ended restarts the selected track at position zero.
- `NextTrack` with no current selection chooses track ID 1; `PreviousTrack` with no current selection chooses track ID 3. Both then wrap in the declared order.
- Loading or changing a track clears channel mute/solo overrides.
- Closing a library stops playback, clears the selected track and overrides, and returns position zero.
- `Seek` is always rejected as `unsupported_capability` in M0.
- Mute/solo commands require a prepared track and channel ID 0 through 7. Repeating the same Boolean value is an accepted no-op.
- Mute and solo flags are independently retained. A channel is effectively enabled when it is not muted and either no channel is soloed or that channel is soloed.
- `ClearChannelOverrides` requires a prepared track and is idempotent.
- The normal fixture never enters `loading` or `error`; focused contract fixtures may construct those states to verify UI fallback.

### 4.3 Channel pattern

At media tick `position_ticks`, define:

```text
frame = position_ticks / 1000
phase(channel) = (frame + 3 * channel) mod 32
key_on(channel) = phase(channel) < 24
note(channel) = 36 + 5 * channel + ((frame / 8) mod 12), when key_on
activity(channel) = 255 - 8 * phase(channel), when key_on; otherwise 0
```

Channel pan cycles left, center, right using -96, 0, and 96. Labels are `FM 1` through `FM 8`; instrument labels are `Mock FM 1` through `Mock FM 8`. Mute/solo affects `enabled` but does not change the underlying synthetic key/note pattern, allowing policy and source observation to be tested separately.

The fake device sink records a private test value `{at_media_tick, channel_id, key_on, note}` whenever key state or note changes. It is not a public `DeviceOp` and must not be interpreted as a YM2151 register protocol. Repeated traces must produce value-identical event sequences.

## 5. Core stepping algorithm

The only time-driving operation is `advance_to(clock_tick)`:

1. reject `clock_tick < previous_clock_tick` without changing state;
2. drain accepted commands in FIFO order and assert projected/actual transition agreement;
3. for each crossed 1,000-tick clock boundary, advance media position by the playing portion of the interval, update the mock channels and fake sink, and publish the next complete snapshot;
4. advance any remaining sub-cadence interval without publishing;
5. set the observed clock to `clock_tick`.

For M0, commands take effect at the previous observed clock tick. Tests submit commands only between `advance_to` calls, which removes hidden host scheduling from the trace. A production scheduler with asynchronous ingress must later timestamp or define command cut-off behavior in a new ADR.

If a single call would cross more than 1,024 publication boundaries, M0 returns `resource/catch_up_limit` before mutation. This bounds work and prevents hostile or accidental huge deltas from creating an unbounded loop.

End-of-track is not generated by the default unknown-duration fixture. A focused test fixture with a declared duration verifies `playing -> ended`; reaching duration clamps position, deactivates channels, and retains the selected track.

## 6. Minimal replaceable UI

The host UI is a deterministic text view-model renderer, not a final visual design. Its production target depends only on contracts.

UI-local types include `UiView { overview, channels }`, selection, and scroll offset. A `UiAction::switch_view` changes `UiView` locally and never becomes a `PlayerCommand`. Platform input mapping lives in a host adapter outside the UI library.

- Overview shows transport, position/tick rate, track metadata or `—`, device state, and recoverable error summary.
- Channels shows all eight rows with label, enabled/mute/solo, key state, optional note, activity, and pan.
- Missing values render as `—`.
- Unknown capability bits and state extensions are ignored.
- Unknown enum values render a stable `unknown` fallback and never index an unchecked table.

Tests call pure rendering functions and assert semantic rows/tokens. They do not capture terminal pixels, colors, fonts, timing, or locale-dependent output.

## 7. Required traces and acceptance mapping

The canonical determinism trace is:

1. submit `OpenLibrary(m0:library)` and `LoadTrack(1)`;
2. advance to tick 1,000;
3. submit `Play` and advance through tick 60,000;
4. submit `Pause`, advance the clock by 30,000 ticks, and verify media/channel freeze;
5. submit `Resume`, advance through tick 150,000;
6. apply mute and solo overrides, clear them, then stop.

It produces more than 120 cadence snapshots. Run it twice, once without calling `latest()` during playback and once with sparse reads; compare the full generated snapshot log retained by the test observer and all fake-device events.

M0 is not complete until the implementation also supplies:

- a 32-command success and 33rd-command queue-full trace;
- recent duplicate and old stale-ID traces;
- two commands admitted against one expected snapshot sequence;
- invalid-state coverage for every table cell;
- an unknown extension with a 256-byte payload and an over-limit rejection;
- a mock UI executable whose link closure excludes runtime;
- a headless executable whose link closure excludes UI;
- a synthetic forbidden dependency that makes the architecture checker fail.

## 8. Implementation sequence

Keep review slices small and in this order:

1. CMake targets, presets, contract value types, and contract tests.
2. Architecture checker plus its positive and negative fixtures.
3. Fake clock/library/device ports and command ingress/projection tests.
4. Mock runtime stepping, 120-snapshot trace, and determinism tests.
5. UI-local navigation, pure text views, and mock-only UI target.
6. Full host workflow and CI matrix.

Do not create an MDX source file beyond the existing placeholder, a real container reader, RTL synthesis source, or final assets in any M0 slice.

## 9. Open gates

- ADR-0004 Pocket execution substrate and toolchain spike, following `docs/design/pocket-platform-boundary.md`.
- Pocket memory budgets for snapshot copies, strings, queues, and library indices.
- Target command-ingress concurrency and command cut-off timestamps.
- Host/RTL fake-device boundary versus the future hardware FIFO protocol.
- Exact compiler, formatter, static analyzer, and CI runner versions.
