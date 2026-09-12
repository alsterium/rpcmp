# Player state schema 2 — transport profile

Status: adopted for the observation portion of M6 slice 3, 2026-09-13.
This implements the transport observations in the
[transition proposal](../docs/design/pocket-player-transition-contract.md).
The public v1 types, validation and M0 synthetic clock remain unchanged.
Schema 2 command ingress, policy, history, settings and visualizations remain
subsequent parts of the same slice. This profile does not claim those features.

## Boundary and evolution

`contracts::v2::SnapshotSource::latest()` returns a complete `PlayerSnapshot`
value. It has no references to Core, borrowed text, container offsets, device
registers, physical controls or callbacks. A caller may retain or modify its
copy without affecting playback. Core alone owns the concrete publisher.
Reading snapshots never polls ports, consumes completions or advances time.

These are source-level values, not a serialized C++ ABI. Schema is exactly 2.
Future additive, optional observations need a recorded contract, explicit
absence/defaults and capability negotiation. Existing fields and enum meanings
must remain compatible. Unknown capability bits are preserved and ignored by
consumers. No channel, loop, duration or persistence observation is fabricated
to stand in for a feature that has not been connected.

## Values and bounds

The concrete declarations are in `rpcmp/contracts/player_state_v2.hpp`.
All values have fixed capacity; publication and reads allocate no memory.

- `sequence:u64` increases once per successful publication. Initial sequence
  is 0 (or the Core owner's last issued sequence when continuing its namespace).
- `published_at_us:u64` is the injected monotonic observation time in
  microseconds. Equal timestamps are allowed. It is independent of media time.
- Capability bit 0 is transport observations; bit 1 is state-preserving pause.
  The latter reports the backend declaration, not a hardware test result.
- `library` is a copied catalog schema 1 status. It is the status synchronized
  by the same Core transport step that produced this observation.
- `transport` is the confirmed state; `projected` is the current command intent.
  Playing with projected Paused is valid until the audio acknowledgement.
- `track` is optional, containing TrackId, owning AlbumId, track ordinal, title
  and album name. Metadata uses the catalog's 96-byte UTF-8 prefix and explicit
  truncation flag. No selection means absent, not an empty synthetic title.
- `play_generation:u64` and `position_frames:u64` come from transport.
  `frame_rate` is exactly 48,000 stereo frames/second. It never uses engine
  read-ahead or elapsed UI time. Stopped has position 0; Ended retains
  the final committed position; Paused retains its committed position.
- `prepared` means the selected generation owns a prepared session;
  `silence_confirmed` reports reset acknowledgement, not merely a mute request.
- Optional `pending_intent` contains its nonzero command ID, one of PlayTrack,
  LoadTrack, Play, Pause, Resume, TogglePause, Stop, and a selection only for
  PlayTrack/LoadTrack. This names the last effective intent, not every queued
  command or proof that an operation completed.
- Optional preparation and audio-control observations each contain a nonzero
  operation ID and play generation. Preparation also identifies its library
  generation/track and cancellation request. Audio kind is Reset/Start/Pause/
  Resume. Their generations may be older than the selected generation while
  cancellation or a shared control acknowledgement is outstanding.
- Optional `error` has a stable typed code and `terminal` flag. Codes are
  Library=1, Preparation=2, PreparationTimeout=3, AudioControl=4, DeviceFault=5,
  AudioTimeout=6, ResetFailed=7, Protocol=8, Clock=9,
  InvalidConfiguration=10, ResourceExhausted=11. Error transport requires an
  error value; other transports have none. Nonterminal does not mean a new
  track can already start: quiescence/reset requirements still apply.

Transport observation types intentionally do not expose the internal ports or
their mutable state. Empty/Loading/Stopped can have no selected track;
Playing/Paused/Ended require a selection from the ready catalog. Operations
belonging to invalidated generations remain visible until their ownership ends.

## Publication

One Core owner serializes catalog changes, transport steps and publication.
`SnapshotPublisher::publish(now_us, transport_snapshot)` copies the observation
and resolves display metadata through the read-only catalog pages. The Core
transport snapshot includes its synchronized catalog status and pause capability.
Publishing checks that status against the current catalog before resolving IDs.
Metadata is cached only while both library generation and TrackId are unchanged.
Catalog bounds limit an uncached lookup to at most 300 albums and 300 tracks.

The owner chooses publication cadence independently of rendering. Each call
publishes at most one complete value; skipped calls do not fabricate intermediate
positions or sequence numbers. The UI may skip any number of publications.
Publication has no callbacks into rendering and no audio/preparation port access.

Failures return ClockReversed, SequenceExhausted, CatalogChanged or
InvalidObservation. They leave the previous complete snapshot and sequence
unchanged. CatalogChanged means Core must synchronize transport before retrying;
it must not publish metadata from a replacement library with an old transport
selection. Counters never wrap. The integrating Core owner must handle a
publication failure, including terminating/recreating an exhausted observation
namespace; a successful audio operation is not inferred from a publication failure.

`valid_player_snapshot` is a contract-only structural check for mock/recorded
producers: schema, enums, catalog, bounded valid UTF-8, identities, position rate,
selection/error consistency and operation shape. It cannot prove audible timing,
album ownership against bytes, hardware silence or relationships between separate
publications. Those need the Core and adapter tests.

## Acceptance

Use independently authored snapshots in a contracts-only target. Reject malformed
lengths/tags/rates/identities while accepting unknown capability bits and old
in-flight operation generations. Verify public v1 rejects schema 2 unchanged.
Use the independently encoded album fixture for real publisher/controller tests:
selection metadata, asynchronous pause/stop observations, reload between step and
publish, caller mutation, sparse reads, clock/counter boundaries and failure
visibility. No renderer, private music or actual hardware is needed for this unit.
