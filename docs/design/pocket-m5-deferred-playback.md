# M5 deferred-library playback profile

Status: implementation plan, 2026-09-05. Governed by M5 and ADR-0008;
this is an experimental application profile, not substrate promotion.

## Real-file admission investigation

The production ingestion CLI rejected all 13,140 locally supplied MDX files
on 2026-09-05 before any candidate library was produced. This is not evidence
that the corpus is malformed: the current FM subset is intentionally limited,
and implementation defects must be separated from unsupported capabilities.

Code inspection found one conflict with the existing MDX v1 contract: both
`validate_track` and `validate_control_flow` required a later `f1 00` even
after an unconditional `f1 rel16` loop. MDX v1 already accepts infinite track
loops. The pinned mdxtools `9c8539f` driver handles `f1 rel16` by adding the
signed displacement plus the three-byte instruction length; it does not read
an additional stop instruction. The pinned MXDRV-derived `portable_mdx`
translation agrees in `L0013dc` / `L0013e6`: zero selects the stop handler;
otherwise the two displacement bytes update the track pointer and loop state.
Existing RPCMP runtime execution agrees, but
its admission tests appended an unreachable stop and concealed the rejection.

Resolve this as a validator bug, not by expanding opcode/PCM admission: an
unconditional track loop terminates the linear track description just as stop
does. The control-flow pass must still prove that its target is an admitted
instruction boundary within that description. A truncated displacement or a
target outside the description remains an error. Add authored regression
fixtures with a final loop and no appended stop before repeating local input
selection. No source music bytes are changed to manufacture compatibility.

## Fixed hardware and input boundary

Reuse the hardware-accepted `0.9.1-m5-audio` RBF, safe-memset OS, boot ROM,
and pinned SDK loader without rebuilding or modifying them. Build a separate
application and package identity; preserve both earlier control packages.
The new package adds a read-only, deferred slot 4 containing `music.rpcmlib`.
Slots 0 through 3 retain their instance/OS/config/application roles.
Do not introduce raw MDX loading on Pocket.

The package supplies the selected nonzero TrackId as a hexadecimal application
argument. Parse it with checked unsigned 64-bit arithmetic. Resolve it using
`LogicalLibrary::find_track` and the existing library session, never by reading
container offsets in the application. Require exactly one track and one blob.
The ID and locally generated library remain local build artifacts, not source
constants or committed identifying corpus metadata.

## Loading limits and ownership

The application provides zero-backed storage for at most 2 MiB of container
bytes. Check slot size before any transfer; require at least the v1 80-byte
header, and reject larger inputs before allocation or read. Transfer chunks
are at most 4,096 bytes, including a potentially shorter final chunk. Check
offset and remaining length using subtraction before each read. A failed or
short transfer exposes no ByteView to library consumers. No retry is permitted
after an asynchronous transfer timeout: its staging buffer may still be owned
by the OS until completion or shutdown.

Use explicit Pocket validation limits: at most 16 sections, 64 records per
section, 4,096 bytes per string, 4 dependencies per track, 2 MiB stored bytes,
and 1 MiB total decoded blob bytes. These lower the host defaults without
changing container v1. The one-track/one-blob profile is checked separately.
Record actual static, initialized-data, and conservative stack usage from the
cross-build before packaging. The prior 4,096-byte initialized-data gate and
524,288-byte stack limit remain in force.

Reset sound before loading. Complete container checksum/reference validation
and MDX admission before initializing the playback queue epoch. Keep the
loaded bytes immutable and alive until exit. Do not perform storage operations
after playback starts. SDK storage and time access stay in platform adapters;
the engine receives only the logical MDX blob.

## Playback and failure behavior

Sequence the original admitted MDX on Pocket, not a host-rendered register
recording. Preserve the 48,000-tick media timeline and the queue adapter's
checked 64-bit conversion to 90 MHz CPU ticks. Retain ordered backpressure.
Long rests are valid musical time, not a two-second device failure: submission
must be paced against the queue clock so future timestamps cannot make a
healthy queue look stuck. Bound both lookahead and the number of driver ticks
processed per servicing iteration. Rendering and storage must not pace audio.

On natural FM-track completion, wait for scheduled work to drain, then reset
sound before reporting completion. Looping tracks continue playing until the
user exits through the Pocket menu. Report admission, queue, and audio faults
outside the scheduling path, request bounded common reset, and do not resume
or reuse timed-out storage state. A reset failure must be reported as such;
software cannot claim measured silence when hardware reset did not complete.

Preserve typed YM2151, legacy ADPCM, and PCM8 routes. Active PCM remains a
transactional unsupported result, not a discarded track or fake FM operation.
This profile neither implements PCM nor removes its future second-source
audio/mixer boundary.

## Verification before the hardware handoff

- Host-test bounded loading with missing, undersized, oversized, exact-limit,
  short-final-chunk, and failed-transfer cases; expose no partial library.
- Validate real local input using the production ingestion and session code;
  reject unsupported files without modifying source bytes or admission rules.
  Preflight an audible interval and record deterministic trace evidence locally.
- Test pacing, long rests, ordering under backpressure, natural end, time
  overflow, and error reset using fake storage/clock/device adapters.
- Cross-build and inspect ELF/static/data/stack budgets and undefined symbols.
- Run affected host tests, format/tidy, architecture checks, and the existing
  behavioral and real-JT51 suites. No new Quartus fit is needed when the native
  RBF hash is exactly the reviewed hardware-accepted one.
- Verify every ZIP member, APF slot/instance linkage, pinned boot/OS/RBF
  identities, and the local library's checksum and primary-blob equality.
- Supply the distinct ZIP and exact firmware 2.6 initial/warm/cold-start,
  audible-playback, and failure-observation instructions. Hardware results and
  ADR-0008 production-promotion gates remain unclaimed until measured.
