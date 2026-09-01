# RTL

Reserved for sound-device, clocking, CDC, mixing, resampling, and audio-output RTL plus simulation assets.

`pocket/rpcmp_pocket_audio.sv` is the project-owned M2 rational-rate and Pocket
AUDIO serializer defined by `specs/pocket-audio-adapter-v1.md`. It accepts a
wide signed stereo stream in the 12.288 MHz domain, saturates to 16 bits, and
emits exact 48 kHz frames with sticky boundary diagnostics. It is not yet
integrated with JT51 or an APF shell.

`pocket/rpcmp_device_queue.sv` is the project-owned M2 v1 CPU-to-device queue.
Its CPU-local MMIO map, eight-entry FIFO, due-time behavior, backpressure,
reset, and bundled-data toggle CDC are defined in
`specs/core-rtl-device-queue-v1.md`. It is not yet integrated into an APF shell
or a selected YM2151 core.

`pocket/rpcmp_spike_regs.sv` is a project-owned M0 register-boundary experiment. Its provisional map and limitations are defined in `docs/design/pocket-spike-registers.md`. It is not yet connected to the openFPGA template or accepted as a production protocol.

The ignored integration build described in `docs/design/pocket-template-integration.md` overlays this source onto a pinned export of the official template without vendoring third-party HDL.
