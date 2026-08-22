# RPCMP Architecture

## 1. Context and dependency direction

```text
source files -> Utility -> .rpcmlib
                              |
                              v
                       Library Manager
                              |
                              v
                       Playback Engine
                         |          |
                  Player State   timed DeviceOps
                         |          |
                         v          v
                  Snapshot API   Device Ports -> RTL/audio
                         |
                         v
                        UI

Pocket input -> UI input adapter -> PlayerCommand -> Core
```

Allowed compile-time dependency direction:

```text
contracts <- runtime <- platform adapters
contracts <- ui      <- platform renderer/input adapters
device contracts <- engines
```

Forbidden: `runtime -> ui`, `engine -> renderer`, `ui -> engine internals`, `ui -> RTL`, and `audio clock -> video/render clock`.

## 2. Logical modules

- `contracts`: public value types, commands, snapshots, IDs, result/error codes.
- `library`: validated logical access to `.rpcmlib` tracks and blobs.
- `player`: transport lifecycle, command dispatch, track preparation, errors.
- `engines`: format-specific parsers/sequencers emitting timestamped `DeviceOp` values.
- `devices`: typed ports for register writes, reset, clocks, and capability discovery.
- `state`: immutable snapshots assembled from Core-owned state.
- `ui`: pure view models, navigation, widgets, and rendering against contracts only.
- `platform`: Pocket/host clocks, storage, input, renderer, and audio/device adapters.
- `rtl`: FPGA sound device, mixing, clock-domain crossing, resampling, and audio output.

## 3. Runtime control model

Core owns all mutable playback state. A single command ingress serializes state transitions. Commands are validated against current state and return an acknowledgement/result; asynchronous completion or failure appears in a later snapshot/event.

Snapshots are immutable observations. They carry `schema_version`, a monotonically increasing `sequence`, and playback time. Publication may drop intermediate snapshots; consumers must always be able to render the newest complete snapshot. Snapshot loss must not lose commands or device operations.

## 4. Timing model

- Use integer ticks or fixed-width integer time units; never floating point as the scheduling authority.
- The engine emits ordered operations with absolute or normalized timestamps.
- The scheduler is driven by an injected monotonic time source and feeds a bounded device queue.
- UI snapshot generation samples Core state outside the critical device scheduling path.
- Pause freezes media time; resume preserves position; stop resets track-local engine/device state.
- Clock rates and rational conversions must be explicit and testable for drift and overflow.

## 5. Engine/device boundary

An engine parses a format and emits generic operations such as:

```text
DeviceOp { at_tick, device_id, operation }
operation = WriteRegister(address, value) | Reset | SetClock(...)
```

MDX may understand commands and timing, but it does not know the concrete YM2151 RTL implementation. A YM2151 adapter validates and transports register writes. Device observations used by UI are translated into generic channel state plus an optional typed device extension.

## 6. Core/UI boundary

Public boundary files are `specs/ui-state-api.md` and `specs/player-command-api.md`.

- Core publishes data, capability flags, and errors; it does not prescribe layout.
- UI may derive ephemeral presentation state (selection, scroll, active view), but not playback truth.
- Physical controls are translated by a platform/UI input adapter.
- The initial transport may use in-process immutable values. Preserve semantics so it could later use double buffering or a message queue.

Required proof tests:

1. Core library/runtime builds and runs without linking UI.
2. UI test target builds with contracts and mock snapshots but without runtime/engines.
3. Dependency tooling rejects a synthetic `runtime -> ui` import.
4. A delayed renderer does not change deterministic device-event output.

## 7. Library boundary

The container reader validates header/directory/checksums before exposing logical records. The rest of Core receives typed `TrackId`, `BlobId`, metadata, and bounded byte views/streams. Container offsets, compression details, and source paths do not cross the library boundary.

## 8. FPGA boundary

Host logic and RTL meet at a documented register/FIFO protocol with reset, backpressure, clock, and overflow behavior. Clock-domain crossings must use reviewed CDC structures. RTL simulation covers reset, ordering, FIFO boundaries, and representative register sequences before Pocket integration.

## 9. Error strategy

Errors are typed by domain (`library`, `format`, `device`, `resource`, `unsupported`, `internal`) and include a stable machine code plus optional diagnostic text. Malformed input must produce a bounded failure, never panic, hang, out-of-bounds access, or partial unchecked playback.

## 10. Evolution

- Additive fields require defaults and capability negotiation.
- Breaking contracts increment major versions.
- Device-specific snapshot extensions use `(extension_type, extension_version, payload)` and remain optional.
- Architectural decisions are recorded in `docs/adr/` before choices lock in language, build system, third-party cores, or hardware protocol.
