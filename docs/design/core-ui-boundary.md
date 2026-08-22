# Core/UI Boundary Checklist

## Adding or replacing a UI

A new UI may depend on public contracts, a read-only library browsing interface, and platform rendering/input adapters. It must not depend on runtime private headers/modules, engines, parser data, device buses, or container layout.

The UI owns view navigation, focus, selection, scrolling, animation, formatting, and layout. Core owns transport, selected/loaded track truth, playback position, channel overrides, capabilities, and errors.

## Review questions

- Can Core run with this UI deleted?
- Can this UI run from a JSON/value fixture or generated snapshots?
- Would a dropped frame alter playback, event ordering, or media position?
- Does any command encode a physical button or screen name?
- Does any snapshot encode pixels, coordinates, colors, or widget instructions?
- Are format/device details optional extensions with generic fallback?
- Are mutable references, pointers, callbacks into engine internals, or raw offsets crossing the boundary?

Any “yes” to the last three questions blocks merge unless the public specification is intentionally revised.
