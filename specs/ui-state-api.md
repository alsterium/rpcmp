# UI State API v1

This is a language-neutral semantic contract. M0 maps it to concrete immutable types.

## 1. Snapshot envelope

```text
PlayerSnapshot {
  schema_version: 1
  sequence: u64
  published_at_tick: u64
  capabilities: CapabilitySet
  transport: TransportState
  track: TrackSummary?
  position: PositionState
  channels: [ChannelState]
  devices: [DeviceSummary]
  visualization: VisualizationState
  error: PlayerError?
  extensions: [StateExtension]
}
```

Snapshots are complete, immutable, and internally consistent. Consumers may skip sequence numbers and render the latest snapshot. Absence is explicit; sentinel strings/numbers are forbidden.

## 2. Core types

`TransportState`: `empty | loading | stopped | playing | paused | ended | error`.

`PositionState`: integer media ticks, declared tick rate, optional duration, optional loop start/count, and seekability. Unknown duration is not zero.

`TrackSummary`: stable track ID and display metadata only; no path, offset, parser pointer, or engine object.

`ChannelState`:

- stable `channel_id` and display label
- generic kind (`fm`, `pcm`, `psg`, `other`)
- enabled/muted/solo capability and current flags
- key-on/activity state
- optional MIDI-like note number plus fine pitch (presentation aid, not scheduling authority)
- normalized level/activity values with declared valid range
- pan where meaningful
- optional instrument/program display label
- device/channel references

`VisualizationState`: bounded recent activity summaries suitable for meters/waveforms, never an unbounded audio history. Exact samples are optional and capability-gated.

`DeviceSummary`: stable ID, generic type (initially YM2151), channel range, operational state, and capability flags.

## 3. Extensions

```text
StateExtension {
  type_id: stable identifier
  version: u16
  payload: bounded typed/serialized value
}
```

The generic UI must function when it does not understand an extension. A future YM2151 detail extension may expose algorithm, feedback, operators, envelopes, and register-derived values without placing those fields in every generic channel.

## 4. Ownership and publication

- Core owns source state; snapshot values contain no mutable Core references.
- M0 may copy values. Later optimization may use double-buffered immutable storage while preserving semantics.
- Publishing is non-blocking with respect to time-critical playback.
- Strings and arrays have documented maxima appropriate to the target.

## 5. Error visibility

`PlayerError` contains domain, stable code, severity/recoverability, and optional bounded display-safe detail. UI chooses presentation. Core never sends layout instructions.

## 6. M0 acceptance criteria

- Deterministic mock snapshots model eight YM2151 FM channels across at least 120 frames.
- Snapshot sequence and ticks are monotonic.
- UI renders/navigates using only this contract and UI-local state.
- UI test target does not link runtime or MDX code.
- Unknown extensions and unavailable optional fields do not break rendering.
- A contract test verifies snapshots contain no platform control names or container offsets.

## 7. M0 host profile

Concrete M0 field representations, admission bounds, the 60,000-tick synthetic clock, the 60 Hz publication cadence, and the eight-channel fixture pattern are defined in `docs/design/m0-executable-design.md`. Those limits are in-memory M0 limits; they do not freeze a C++ object ABI, a wire encoding, Pocket memory budgets, or `.rpcmlib` bytes.
