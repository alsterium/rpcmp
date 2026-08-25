# ADR-0002: M0 state and command transport

- Status: accepted
- Date: 2026-08-25
- Deciders: RPCMP maintainers

## Context

The v1 specifications require immutable snapshots, serialized command handling, duplicate protection, bounded queues, stale-view detection, and independence from rendering. They intentionally do not choose an in-process transport or threading model.

## Decision

M0 uses an in-process, single-owner model with two independent boundary objects:

- `CommandIngress::submit(PlayerCommand) -> CommandResult`
- `SnapshotSource::latest() -> PlayerSnapshot` (a value copy)

The Core owns mutable state. A returned snapshot contains no Core references and can outlive or be modified by the caller without affecting Core. Runtime code never calls UI code. M0 is single-threaded, but callers still interact only through these boundaries so a later bounded message queue or double buffer can preserve the same semantics.

### Command ordering and capacity

- One ingress lock/owner establishes a total FIFO order.
- Queue capacity is exactly 32 commands in M0.
- Submission validates against a projected command state that includes all earlier accepted queued commands. Therefore `accepted` means the command is both structurally and state-valid at its FIFO position.
- Draining applies the same transitions to actual Core state. A mismatch between projected and actual state is an `internal/projection_mismatch` fault, not a second user-visible command rejection.
- Runtime/device failures after acceptance appear in a later snapshot error.

Validation order is fixed:

1. supported `schema_version`
2. command ID replay/order rule
3. `expected_snapshot_sequence`, when present
4. payload shape, bounds, IDs, capabilities, and projected state
5. queue capacity

Command IDs are non-zero and strictly increase within a Core session. The ingress retains the 64 most recent results. A repeated retained ID returns `duplicate` with the original result and never enqueues again. An ID at or below the high-water mark but outside that history is rejected as `stale_command_id`; it can never be applied. Every structurally valid submission consumes its ID even when rejected, so retrying changed intent requires a new ID.

An expected snapshot sequence must equal the latest published sequence at submission. Multiple FIFO commands may intentionally cite the same latest snapshot before another snapshot is published.

### Deterministic clock and publication

- The injected M0 clock uses unsigned 64-bit ticks at exactly 60,000 ticks/second. This is a synthetic media clock, not an audio sample-rate commitment.
- Snapshot cadence is 1,000 clock ticks (60 snapshots/second).
- Core starts with complete snapshot sequence 0 at clock tick 0.
- `advance_to(T)` rejects time reversal, drains queued commands at the previous observed clock tick, advances media state using only the injected delta, and publishes every cadence boundary crossed through `T`.
- `published_at_tick` follows the injected clock. Media position advances only while playing.
- Paused snapshots continue to publish, but position and channel observations remain frozen.
- UI reads do not advance Core, acknowledge commands, or emit device events. Intermediate snapshots may be overwritten by `latest()`.
- All tick addition and cadence iteration are overflow-checked and bounded per call. Tests advance in bounded chunks; a future production adapter must define a catch-up limit.

## Alternatives considered

- **UI calls mutable Core methods directly:** violates the command boundary and makes render timing observable by playback.
- **Callback/push snapshots into UI:** reverses the dependency and complicates headless operation.
- **Threaded queues in M0:** adds scheduler nondeterminism before a target threading model exists.
- **Unbounded duplicate set or command queue:** violates target resource constraints.
- **Validate state only while draining:** would allow `accepted` commands to later fail for ordinary state reasons, contradicting the command API.

## Consequences

- Queue-full, duplicate, stale-view, and compound queued-command behavior are precisely testable without threads or sleeps.
- The projected state adds implementation work, but eliminates ambiguity between admission and execution.
- Strictly increasing command IDs are an M0 profile rule, not a statement about future distributed transports. Changing it later requires an explicit v1 profile revision and compatibility tests.
- Value copies are intentionally simple. Their measured cost and Pocket memory budget must be evaluated before choosing double buffering or another target transport.
