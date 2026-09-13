# RTL

Reserved for sound-device, clocking, CDC, mixing, resampling, and audio-output RTL plus simulation assets.

`pocket/rpcmp_pocket_audio.sv` is the project-owned M2 rational-rate and Pocket
AUDIO serializer defined by `specs/pocket-audio-adapter-v1.md`. It accepts a
wide signed stereo stream in the 12.288 MHz domain, saturates to 16 bits, and
emits exact 48 kHz frames with sticky boundary diagnostics. It is not yet
integrated with JT51 or an APF shell.

`pocket/rpcmp_jt51_audio.sv` implements the adapter in
`specs/ym2151-rtl-adapter-v1.md`: bounded reset and busy-aware two-stage JT51
writes, rational clock enables, and native-sample connection to the Pocket
AUDIO adapter. The ordinary RTL suite uses a behavioral JT51 boundary model;
`tools/rtl-jt51-verify.ps1` separately requires and compiles the clean pinned
GPL checkout from ignored research output.

M6's [JT51 hold fixture](../../specs/jt51-hold-experiment-v1.md) generates a
separate hold-capable copy under `out/`, preserving the original checkout and
existing path. `tools/rtl-jt51-hold-verify.ps1` compares it with unmodified JT51
under retained clock edges; `-CenOnly` must reproduce the in-flight-register
hold failure. This proves native-engine behavior for an authored trace, not
frame-boundary Pocket pause, output-buffer ownership or a new MMIO capability.

`pocket/rpcmp_jt51_media_audio.sv` and `pocket/rpcmp_pocket_media_audio.sv`
implement the new [M6 media boundary](../../specs/pocket-media-audio-v1.md).
They retain native/write/converter state during pause while serial clocks
continue, then consume the retained edge at a stereo boundary. Stream reset
clears sound without stopping serial timing. The new serializer has the APF
one-bit delay; legacy v1's phase is unchanged. `tools/rtl-media-audio-verify.ps1`
checks converter equivalence against v1 and decoded real-JT51 output against
uninterrupted playback. CPU controls, media queue/CDC and producer-progress
mapping are pending.

`pocket/rpcmp_media_envelope.sv` implements the separate
[RTL envelope](../../specs/media-envelope-rtl-v1.md): 256 mapped progress
intervals, generation/policy checks, exact five-second fade and 20 ms restore.
Controls apply before due progress or completion; no immutable old-policy fade
command survives a cancellation. `tools/rtl-media-envelope-verify.ps1` checks
the full 70/75-second frame numbers, signed output, pauses, cancellation and
FIFO/error boundaries against analytical expectations.

`pocket/rpcmp_jt51_enveloped_audio.sv` connects that envelope to the shared
`rpcmp_jt51_media_source` and `rpcmp_media_output`. The latter loads scaled old
pending samples at consuming boundaries; the source uses the same enable for
native state and bus writes. Terminal states initiate a 2,048-edge native reset
while preserving serial clocks and the envelope's reason/position. The old
pause wrappers use these shared internals with their existing public behavior.
The native source numbers retained audio edges; the converter keeps the
captured position with the selected sample through pending and serialized
output. Pause/startup silence has no source position, reset discards old
positions, and source-clock exhaustion closes the enveloped stream as a fault.
The source also assigns ordered write/marker tokens and holds a receipt of the
actual data-transfer or marker edge until consumed. Receipt delivery may drain
during hold, without advancing synthesis; token exhaustion resets the enveloped
stream. These bus receipts do not yet certify native-pipeline or audible commit.
`rpcmp_native_completion` consumes each enveloped receipt in a 32-entry RAM
queue and retires its ordered prefix after the specified native direct-control
processing bound. The prefix travels with selected PCM to actual frame output.
This is [native completion v1](../../specs/jt51-native-completion-v1.md);
retained musical history and the remaining source-progress producer are outside
that certificate. Queue overflow arithmetic faults through the shared reset.
`tools/rtl-enveloped-audio-verify.ps1` checks decoded stereo against an unscaled
native reference and analytical gain, including reset/restart cases. This is
the [local integration contract](../../specs/pocket-enveloped-audio-v1.md), not
CPU control, CDC or the still-required audible-progress mapper.

`pocket/rpcmp_media_source_queue.sv` implements the separate
[retained source queue](../../specs/media-source-queue-v1.md). Its 64-entry RAM
accepts copied timestamped writes and batch-ending markers, streams larger
batches under backpressure, and dispatches against retained native time.
Markers seal source supply; missing input at an eligible empty dispatch latches
a fault for the shared audio owner. This does not supply mapped envelope
intervals. The enveloped suite connects a maximum-size authored batch to native
JT51, checks actual bus bytes/receipt edges and output prefixes, and compares
PCM with independently authored bytes bypassing the queue at the same edges.

`pocket/rpcmp_m2_fixed_core.sv` is the ADR-0007 hardware-validation substrate.
It reproduces the frozen 17-operation fixture as MMIO transactions into the v1
queue and exposes completion/fault diagnostics. It is deliberately not a
general Pocket runtime or future MDX sequencer.

`pocket/rpcmp_m4_mdx_core.sv` reuses that validated hardware path for the M3
self-authored MDX probe trace. Its 33 RTL literals are mechanically compared
with `specs/fixtures/mdx-fm-probe-trace-v1.csv`, which is independently compared
with fresh C++ parser-to-scheduler output.

`pocket/rpcmp_stereo_probe_core.sv` is the bounded M4S channel-mapping
diagnostic. It emits separated logical-left and logical-right tones through the
same queue, JT51, and Pocket AUDIO path before real-file playback work begins.

`pocket/rpcmp_device_queue.sv` is the project-owned M2 v1 CPU-to-device queue.
Its CPU-local MMIO map, eight-entry FIFO, due-time behavior, backpressure,
reset, and bundled-data toggle CDC are defined in
`specs/core-rtl-device-queue-v1.md`. It is not yet integrated into an APF shell
or a selected YM2151 core.

`pocket/rpcmp_spike_regs.sv` is a project-owned M0 register-boundary experiment. Its provisional map and limitations are defined in `docs/design/pocket-spike-registers.md`. It is not yet connected to the openFPGA template or accepted as a production protocol.

The ignored integration build described in `docs/design/pocket-template-integration.md` overlays this source onto a pinned export of the official template without vendoring third-party HDL.
