# Replaceable UI

Presentation, navigation, input mapping, widgets, and visualizers. It depends only on public contracts and platform-facing UI adapters, and must support mock snapshots.

M0 provides a deterministic text view-model renderer with Overview and Channels views. The `rpcmp_mock_ui_m0` target links contracts and UI only; view switching and channel selection remain UI-local state.

M6 adds `rpcmp::ui::v2::PlayerUi`, following [Album UI v1](../../specs/player-ui-v1.md).
It takes only CommandIngress and const CatalogReader, copies validated schema 2
snapshots, and owns focus, paged browsing, transport waits, ordered setting
actions and sixteen projected Tracker rows. Keyboard consumers use the copied
current channels. `view()` is const and valid until the owner's next update/input;
rendering must neither dispatch actions nor advance Core.

InputBindings replaces physical button bits and repeat timings; UiConfiguration
replaces focus adjacency and seeds the command-ID namespace. Rebinding requires
held buttons to be released, but preserves command IDs and playback state.
The `rpcmp_player_ui_tests` target links only UI/contracts and supplies authored
snapshots/catalog/command replies. The host canvas, Pocket input/framebuffer/font
adapters and actual readability checks remain the next M6 work.
