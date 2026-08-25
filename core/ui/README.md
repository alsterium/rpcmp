# Replaceable UI

Presentation, navigation, input mapping, widgets, and visualizers. It depends only on public contracts and platform-facing UI adapters, and must support mock snapshots.

M0 provides a deterministic text view-model renderer with Overview and Channels views. The `rpcmp_mock_ui_m0` target links contracts and UI only; view switching and channel selection remain UI-local state.
