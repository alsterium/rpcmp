# Pocket platform adapter

M0 contains only a target-feasibility package and register-boundary experiment. It is not a production runtime adapter.

The `apf` directory contains project-owned APF definition JSON for the standalone `alsterium.RPCMP` spike. It declares no platform, assets, data slots, controller mappings, variants, sleep support, cartridge hardware, or link-port use. Its Interact menu maps only to the provisional registers defined in `docs/design/pocket-spike-registers.md`.

Generate and validate ignored SD/ZIP output with `pwsh -File tools/pocket-package.ps1` after the Quartus integration build. Do not commit the generated bitstream or package.
