# Player Command API v1

Commands are user/system intent submitted to Core. They do not expose physical buttons, UI views, parser internals, FPGA registers, or library offsets.

## 1. Envelope

```text
PlayerCommand {
  schema_version: 1
  command_id: u64
  expected_snapshot_sequence: u64?
  payload: Command
}
```

`command_id` supports acknowledgement and duplicate detection. `expected_snapshot_sequence` is optional optimistic concurrency for commands whose meaning depends on observed state.

## 2. Commands

- `OpenLibrary(library_ref)`
- `CloseLibrary`
- `LoadTrack(track_id)`
- `Play`
- `Pause`
- `Resume`
- `Stop`
- `TogglePause`
- `NextTrack`
- `PreviousTrack`
- `Seek(position_ticks)` (only when capability indicates support)
- `SetChannelMute(channel_id, bool)`
- `SetChannelSolo(channel_id, bool)`
- `ClearChannelOverrides`

Library browsing queries may be a separate read-only library API; do not overload transport commands with UI pagination.

## 3. Results

Command ingress returns `accepted`, `rejected`, or `duplicate`, with a stable reason code. Acceptance means queued/validated, not necessarily completed. Resulting state is authoritative in subsequent snapshots.

Rejection examples: invalid state, unknown track/channel, unsupported capability, stale sequence, resource busy, malformed request.

## 4. State semantics

- `Play` from stopped starts the prepared track; from paused it may be defined as resume, but M0 must choose and test one rule.
- `Pause` is idempotent while paused; `Stop` is idempotent while stopped.
- `LoadTrack` prepares and resets position but does not implicitly play unless a future distinct command says so.
- `Stop` resets track-local engine/device state and position while retaining the selected track.
- Channel overrides are player policy, not UI-local state, and therefore appear in snapshots.

## 5. Ordering and threading

- Core processes commands in a documented total order.
- UI submission must not directly invoke engine mutation from a render callback.
- Queue capacity and overflow behavior are bounded and tested.
- Repeated delivery of the same `command_id` cannot apply a non-idempotent effect twice.

## 6. M0 acceptance criteria

- State-transition table tests cover all M0 commands in valid and invalid transport states.
- Duplicate IDs, unknown IDs, stale sequence, and queue-full behavior are tested.
- No command type references Pocket buttons, views, MDX opcodes, YM2151 registers, or `.rpcmlib` offsets.
- The same command trace produces byte/value-equivalent snapshots in repeated deterministic runs.

## 7. M0 host profile

The concrete M0 admission, ordering, queue, state-transition, and reason-code rules are defined in `docs/adr/0002-m0-state-and-command-transport.md` and `docs/design/m0-executable-design.md`.

This profile makes the previously open `Play` choice explicit: `Play` from `paused` resumes, while `Play` from `playing` is an accepted no-op. Command IDs are non-zero and strictly increasing within an M0 Core session; retained repeats return `duplicate`, and older IDs are rejected without application. These are M0 profile rules, not a serialized representation or a promise that a future inter-process transport will use the same replay window.
