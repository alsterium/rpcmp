# Tests

Cross-module deterministic fixtures, architecture checks, golden traces, fuzz/property tests, RTL simulation, and host integration tests. See `docs/design/testing-strategy.md`.

For current prerequisites and task routing, see the
[development harness](../docs/development/harness.md). Run the complete host
suite on Windows with:

```powershell
pwsh -File tools/host-verify.ps1
```

The workflow requires LLVM 22.1.8 and runs the registered CTest gates, including contract, runtime/headless, UI/mock-only, library/MDX/M5, package, positive/negative architecture, harness, `clang-format`, and `clang-tidy` checks. Discover the current list with `ctest --preset host-msvc -N`; a listing is not an executed test result. Formatting and analysis use warnings-as-errors. After configuring `host-msvc`, their CMake targets can be run separately:

```powershell
cmake --build --preset host-msvc --target format
cmake --build --preset host-msvc --target format-check
cmake --build --preset host-msvc --target tidy-check
```

The host `mdx_source_producer` test checks copied maximum-size MDX batches,
repeated Full responses on each write/marker, partial feed, cancelled/old
epochs, malformed batches and token exhaustion. Its real-engine fixture uses
authored commands and an independently calculated integer tick interval;
pending audio must not advance the engine or its display checkpoint. It also
prints host ownership sizes. RISC-V link and Pocket execution are separate
checks; see [M6 evidence](../docs/milestones/M6-album-player.md).

Run the M0 register spike plus M2 device queue and Pocket AUDIO simulations
with Questa Altera Starter 2025.2:

```powershell
pwsh -File tools/rtl-verify.ps1
```

The script accepts `RPCMP_QUESTA_ROOT` as the directory containing `vsim.exe`; otherwise it checks the documented Quartus Lite installation path. Questa Starter requires a free, 12-month `SW-QUESTA` license. Generate it in Altera's Self Service Licensing Center and set `SALT_LICENSE_SERVER` to the downloaded license-file path before running the script. See the official [Questa Starter licensing instructions](https://docs.altera.com/r/docs/683472/25.3/altera-fpga-software-installation-and-licensing/questa-altera-fpga-edition-and-questa-altera-fpga-starter-edition-software-license). A process started before the environment-variable change automatically reloads the User or Machine value without printing it.

The HDL compiler can validate the source without that environment variable, but
the self-checking simulation cannot run. A passing invocation requires all
configured testbench PASS markers plus Questa's zero-error, zero-warning summary.
The M2 tests cover the queue's reset, due-time hold, full/reject, sticky flags,
backpressure, wrap/reuse, ordering, and asynchronous clocks; the AUDIO test
covers rational sample selection, exact serial framing, silence, saturation,
underflow, overflow, flag clear, and reset. They do not validate APF/JT51
integration, timing closure, physical audio output, JTAG, or Pocket hardware.

Run the same integration test against the actual clean, pinned JT51 checkout
instead of the boundary model with:

```powershell
pwsh -File tools/rtl-jt51-verify.ps1
```

This second command refuses a different revision or local JT51 changes and does
not copy GPL source or generated output into the repository. It also runs the
complete fixed-sequence state machine through the production MMIO queue and
requires 30 ordered JT51 address/data writes plus native audio samples.

M6's native-engine hold experiment has a separate generated-copy comparison:

```powershell
pwsh -File tools/rtl-jt51-hold-verify.ps1 -CenOnly
pwsh -File tools/rtl-jt51-hold-verify.ps1
```

The first command requires the specific expected register-hold failure; the
second requires sample/status equivalence and zero simulator errors/warnings.
Run these and other RTL suites sequentially. The [fixture contract](../specs/jt51-hold-experiment-v1.md)
defines the source recipe, reset/hold semantics, coverage and remaining I2S/
queue/gain/CDC work. Host `jt51_hold_prepare` checks generation guards without
requiring vendor HDL; it does not replace that real-JT51 simulation.

The M6 synchronous pause/output boundary is checked separately:

```powershell
pwsh -File tools/rtl-media-audio-verify.ps1
```

`-OutputOnly` runs the converter/I2S, sample-position/prefix, completion-FIFO
and source-queue benches without vendor source. The full command also generates
pinned JT51 and compares actual decoded stereo frames
from paused and uninterrupted playback, with independently timed device writes.
The converter oracle is the unchanged v1 implementation at retained test clock
edges. It covers all 256 request phases, held source pulses, old/new pending
ordering, empty holds, clipping/overflow/underflow and urgent reset. The native
bench covers both write halves, reset of a retained write and silence after
reset. The burst bench submits back-to-back writes using only `dev_ready`, then
reads the native operator scan to check TL, DT1/MUL and KS/AR values across all
32 slots. Different starting phases and inserted holds must preserve all
accepted values; counting accepted bus operations alone is not sufficient.
The source-receipt bench observes actual data pulses and counts retained edges
independently. It checks all 32 release phases, write/marker ordering, blocked
delivery and reserved slots, simultaneous replacement, notification delivery
during hold, resets of each bus phase, and 64-bit token/position boundaries.
The native phase bench checks a closed-form sample-edge equation over 32 reset
phases with Pause/Resume and concurrent writes. The host `media_clock_math`
test covers the complete rational selection period and proves at least 29
retained edges from selected-sample capture to consumption after an aligned
stream start. This is sample availability, not a write-to-audio commit bound.
The sample-position bench uses a closed-form rational selection oracle and
authored 64-bit positions. It checks the position together with every external
I2S bit through drop, overflow, same-edge old/new selection, pause and reset;
it now checks opaque 64-bit completion prefixes with the same ownership rules.
Valid zero positions/prefixes are distinguished from frames without a sample.
The completion FIFO test checks capacity, simultaneous retirement/admission,
held delivery, reset through head prefetch, pointer wrap and exact 64-bit due
addition boundaries. The LFO test covers both initial values of the native
unreset serial-reset latch, startup and every steady serial phase. Conservative
data-dependency tags are checked against two native copies with different
multiplier state; retained musical history stays identical. The accumulator
test injects exact impulses at all 32 phases. These support the separately
specified [native completion bound](../specs/jt51-native-completion-v1.md).
These local synchronous tests do not establish CPU/CDC/ACK or Pocket
hardware acceptance; see [the contract](../specs/pocket-media-audio-v1.md).
The source queue bench checks 64-entry capacity/retry, copied order, pointer
wrap, simultaneous admission/dispatch, held admission and 64-bit due times.
It distinguishes native backpressure from missing input, including refill
on the missed opportunity and the exact exclusive coverage boundary.
These three queue/sample benches also check opaque full-width payload ownership:
ordinary completion receipts preserve the last flagged checkpoint, and payloads
follow FIFO copies, hold, reset and old/new sample selection.

The mapped-progress and gain controller has a vendor-independent RTL suite:

```powershell
pwsh -File tools/rtl-media-envelope-verify.ps1
```

It evaluates the real 3,360,000/3,600,000 frame start/end values, cancellation
at zero gain, 960-frame restoration, partial-gain restart and signed rounding.
Its gain oracle uses the analytical formula, independently of the RTL's
quotient/remainder recurrence. FIFO capacity/retry, simultaneous replacement,
epoch/policy failures, pause, natural end, fault priority and counter ceilings
are checked. Frame ticks are accelerated; this is not elapsed Pocket playback
or proof that the offered progress has been mapped to audible output correctly.

The composed native/gain/output path has a real-JT51 suite:

```powershell
pwsh -File tools/rtl-enveloped-audio-verify.ps1
```

It decodes external I2S and compares with uninterrupted unscaled native output
multiplied by an analytical policy timeline, removing pause frames. Both signs
and channels, completed restoration, interrupted restoration, stale controls,
Stop, natural end, protocol/coverage faults and post-reset silence are checked.
The integration also counts actual native enables independently of the
completion queue and rejects a sample prefix before 768 subsequent enables.
Prefixes and positions are checked with decoded PCM, including a zero-write
marker held over Pause/Resume and a real completion-position overflow.
Converter fault routing is tested by explicit sticky-register injection; the
converter's actual overflow/underflow cases remain in the existing output
suite. A separately counted native source clock and closed-form sample
selection check the source position of each output frame through gain/pause.
The source-edge counter's real final increment is exercised near UINT64_MAX,
including a preceding hold and subsequent fault/reset. These tests cover sample
positions, not the register-to-native-sample part of the source-progress mapper
or CPU/CDC transport.

The same command also runs `jt51_source_queue_tb`: 8,192 authored writes plus
three ordered markers through the 64-entry scheduled queue into real JT51.
Actual bus bytes and receipt edges are checked against the authored stream;
native PCM is compared with an unqueued source supplied independent bytes at
the same accepted edges. Pause, external receipt backpressure, overdue writes,
the final output prefix, queued/in-flight reset and real supply-fault/reset
routing are covered.
Its envelope interval is explicit test input; this fixture does not claim to
produce mapped MDX loop/end intervals or establish CPU refill deadlines.

`jt51_progress_audio_tb` in the same command checks the automatic local mapper.
Actual authored JT51 writes/marker receipts and the certified completion bound
feed an independent rational-selection and analytical-gain oracle, including
serialized stereo bits. It checks loop-boundary Pause/Resume, live target changes,
natural end during restoration, paused RepeatOne end, late-phase startup,
zero-write checkpoints, marker receipt replacement, multiple checkpoints per
sample, empty Begin rejection, metadata validation and real supply-fault recovery.
This establishes local marker-to-output mapping, not a CPU/CDC adapter or a
per-event performance-history producer.

`sound_session_tb` adds copied Reset/Start/Pause/Resume/SetPolicy requests over
the same native path. It checks all 256 serial acceptance phases, immutable
responses under backpressure, exact paused/Resume ACK positions, generation and
epoch rejection, normal reset, real tone and starvation, live policy, natural
end, emergency during each pending phase, and u64 exhaustion fixtures. Local
bounds are 2,050 audio edges for Reset, 259 for Start and 257 for boundary controls
(including post-boundary failure observation). These are not CPU/CDC deadlines.
Use `pwsh -File tools/rtl-enveloped-audio-verify.ps1 -SessionOnly` for this focused
bench; omit the switch for all four native integration benches.

`pwsh -File tools/rtl-sound-mmio-verify.ps1` tests the new CPU-local protocol
using five initial phases of independent 90 MHz/12.288 MHz clocks. It checks
every register's access direction, illegal/reserved/partial accesses, copied
request and response identity, all feed payload bits at the session boundary,
actual authored native bus bytes, Full/retry, coherent captures across playback
and policy changes, emergency follow-up delivery and both common-reset origins
at six transfer stages for each of three mailboxes. The exact CPU-visible
completion edges are bounded independently of test software polling. Use
`-PhasePs 0` for one focused phase; the default runs the complete phase set.
Every invocation also runs the generic mailbox bench with 64 full-width
requests/responses, destination acceptance/response delays and unread source
completions, checking that borrowed bundles stay fixed without loss or repeats.

The same command checks the 32-record output journal: empty/full/simultaneous
Pop, RAM wrap, loss saturation and sequence exhaustion, then its independent
CPU mailbox at all five phases. A 64-marker native stream has identical output
record frames/prefixes with no reader (32 lost) and frequent Pops (zero lost).
Copied responses survive playback changes and normal Reset; old-epoch Pops
cannot remove a new epoch's equal-sequence head. Every register direction and
all four mailboxes' six reset stages are checked. Full-width record fields use
an explicitly injected observation fixture, not a claimed long hardware run.
The native progress bench independently derives 34 output records, including
six unconsumed natural-end boundaries, from actual receipts and sample selection.
These establish local mapping/transfer, not CPU display publication or Pocket
service speed.

Run the reproducible Quartus template-integration build separately with:

```powershell
pwsh -File tools/pocket-spike-build.ps1
```

This synthesis/fitting gate does not require the Questa license and does not replace behavioral simulation or on-device testing.

After that build, generate and validate the ignored M0 Pocket SD tree and ZIP with:

```powershell
pwsh -File tools/pocket-package.ps1
```

The package gate validates APF JSON roots and bounds, Interact addresses, byte-level RBF reversal, exact file/ZIP layout, and artifact lengths/hashes. It does not install anything on an SD card or access Pocket hardware.

Linux/GCC CI is currently deferred. Host checks do not establish Pocket hardware acceptance.
