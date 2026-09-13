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
snapshots/catalog/command replies. `render_player` consumes the checked view
through the synchronous PlayerCanvas port; it allocates no heap storage and
cannot submit input or advance Core. Tracker, full-range eight-channel keyboard
and paged library share one information/control panel.

The host `rpcmp_player_ui_mock` executable links the UI, contracts and SVG port
without runtime/player/library. After the host build, generate a review image:

```powershell
New-Item -ItemType Directory -Force out/ui-preview | Out-Null
out/build/host-msvc/rpcmp_player_ui_mock.exe tracker out/ui-preview/tracker.svg
out/build/host-msvc/rpcmp_player_ui_mock.exe keyboard out/ui-preview/paused.svg paused
out/build/host-msvc/rpcmp_player_ui_mock.exe library out/ui-preview/library.svg long
```

The first argument is `tracker`, `keyboard` or `library`; the optional final
argument is `playing` (default), `paused`, `empty`, `error`, `gap`, `long` or
`unsupported`. Inputs are authored metadata/events, with no music asset or real
playback. Open the resulting SVG in a browser. The host uses installed fonts and
a provisional 8/16-pixel grid, with bounded UTF-8 text, clipping and explicit
ellipsis. This confirms mock layout, not Pocket font metrics or readability.
`player_render` checks drawing semantics and `player_ui_svg` parses all 21
view/scenario outputs. Pocket input/framebuffer/font and audio/storage adapters
remain M6 integration work.
