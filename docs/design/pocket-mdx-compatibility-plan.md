# MDX-compatible Pocket player design proposal

Status: recommended design, 2026-09-21. This closes the architecture comparison
with a concrete next prototype. It does not adopt replacement public contracts,
change the r4 package or claim all-file/hardware acceptance. Evidence and
reproduction details are in the [compatibility baseline](../research/mdxplayer-compatibility.md).

## Recommendation

Use the MDXPlayer-derived MXDRV interpreter and PCM8 behavior on a CPU, a
4 MHz YM2151-compatible FM implementation in FPGA, and a small timed audio
transport/mixer. Start with a headless select/play/stop engine and add a
minimal Japanese-capable track list. Choose retained code for measured value,
not because the current player already contains it.

| Candidate | Evidence and decision |
| --- | --- |
| Unchanged software FM+PCM on current CPU | Reject as the default: the authored eight-FM/eight-ADPCM workload needs about 172 million cycles/audio second against a 90 MHz single-issue CPU, with optimistic memory and no UI/OS. |
| Same renderer on the investigated dual-issue CPU | Still about 131 million cycles/audio second before integration. A different renderer/CPU may improve this, but this candidate does not justify full software adoption. |
| Reference interpreter + software PCM8 + FPGA FM | Recommended. Tested PCM-only paths need roughly 52–58 million cycles/audio second on the current CPU; the 80-track split preserves reference event timing and PCM output. Composed real-time performance remains to be measured. |
| Hardware PCM8 from the start | Defer. Software retains the reference sample formats, filters and channel semantics; accelerate a measured bottleneck only if the composed budget fails. |
| Recreate an X68000 CPU/OS or continue expanding the custom MDX subset | Adds a second compatibility implementation while the selected reference already interprets the required features. Keep a reference-based route first. |
| Pre-render music on the PC | Simplifies playback but changes the raw MDX/PDX workflow and the PRD's non-conversion goal; not the default solution to the current request. |

FPGA FM is not the dominant cost of the existing sound integration. Historical
fit estimates put JT51 itself at about 1,186 ALMs, while the inclusive sound
MMIO hierarchy needs about 7,683. These overlap; they are not guaranteed
removal savings. Simplify scheduling/control around FM rather than assuming
that deleting FM buys enough CPU throughput.

## Proposed boundaries

```text
M3U8/folder catalog -> bounded MDX + resolved PDX -> reference interpreter
                                                   |             |
                                      timed FM register writes   PCM8 software
                                                   |             |
                                                FPGA FM     wide PCM samples
                                                   \             /
                                             common-time mixer
                                                     |
                                             48 kHz stereo output

input -> replaceable mapping -> PlayerCommand -> Core -> snapshots -> small UI
```

The catalog line is a proposed storage adapter, not an adopted M3U contract.
IDs and logical blobs remain the consumer boundary. Core never depends on
UI, and delaying or disabling rendering must leave the audio event sequence
unchanged. Avoid connecting snapshots, input interpretation or drawing to
the synthesis/interrupt path.

Keep the reference driver timer as the sequencing authority and preserve its
tested chunk behavior. A shared integer audio timeline orders FM events and
PCM contributions. The hardware timer must not independently tick the CPU
driver a second time; timer/CSM/LFO/noise behavior needs explicit comparison.
Use 4 MHz FM for this reference profile. The current 3,579,545 Hz adapter
is not silently retuned under its existing contract.

Transport PCM contributions with headroom, proposed as signed int32 stereo
at the reference's 62.5 kHz internal rate. Match the FM/PCM level relationship,
combine before final saturation, then resample to the Pocket's
[fixed 48 kHz stereo output](https://www.analogue.co/developer/docs/bus-communication).
Source reference volume settings are FM -12 dB, PCM 0 dB and
global volume 256; a hardware FM gain must be calibrated rather than assuming
equal numeric amplitudes. The exact resampler and MMIO layout belong to the
prototype contract, not this high-level choice.

FM writes enter an ordered, bounded timed queue; PCM uses bounded buffering.
Specify copy/ownership, start/preload, generation changes, stop/reset, overflow,
underrun and CDC before implementing a production adapter. While playing,
audio time must continue independently of rendering and CPU submission.
An underrun is an explicit failed playback state with safe output, not an
unreported stretched note. Exact pause and the previous rich fade/progress
machinery are outside this MVP's required implementation.

The largest observed FM burst needs 242 writes. A 20 us/write model drains it
within 4.84 ms, and the separate bus experiment measures shorter back-to-back
service for its authored cases. This motivates a prototype queue with explicit
headroom, not an all-file queue bound. Desired event time, actual bus receipt
and output position must remain distinguishable. Prebuffering removes CPU
jitter; it cannot make serial chip writes simultaneous. Do not claim waveform
identity with FMGEN or choose a timing tolerance without listening/comparison
evidence. If bit-identical MDXPlayer audio becomes a requirement, reopen the
FM implementation choice.

## Platform and library

Use the existing openfpgaOS integration first as the measured comparison host.
Retain its APF boot, storage, memory and input facilities where they reduce
work. The complete OS, current framebuffer renderer and RPCMP sound-control
stack are not mandatory. Test audio with rendering disabled, then with a
bounded dirty-region list UI. SDL by itself does not change the synthesis
cycle budget. A platform rewrite is justified by a measured residual cost,
not needed before the first complete audio comparison.

Recommend UTF-8 M3U8 plus unchanged MDX/PDX files as the user-facing library
direction: albums can be generated from folders, entries preserve order, and
only a selected song and its dependency need loading. Keep titles/selection
metadata separate from audio blobs. The current 179.7 MiB loose collection
does not fit the old all-in-memory 32 MiB library profile.

The user's [HarpMudd example](https://github.com/harpmudd/HarpMudd.mp3player#playlists)
supports a plain line-per-track list with paths relative to the playlist.
That is useful workflow evidence, not a reason to inherit its 256-track /
12 KB limits or assume its MP3/FLAC decoder budget applies to FM synthesis.

Before adoption, define relative-path rules, Japanese encoding handling,
MDX-title fallback, missing/ambiguous PDX reporting, LZX bounds and optional
explicit dependency mapping. M3U lists tracks; it does not resolve PDX by
itself. Preserve `.rpcmlib` v1 and generic IDs/blob interfaces during an
explicitly versioned adapter transition. Do not build an index format or
general database merely to begin the audio prototype.

## Next prototype and decision gates

Implement one headless hybrid vertical slice before rebuilding the full
player. Start with authored eight-FM/eight-PCM loads and the existing private
comparison set; keep a native full-software oracle. The first slice must:

1. Preserve ordered FM events, PCM formats/pan/rates and the driver's timer;
   verify dense bursts, key transitions, loops/end and stop/restart. Compare
   the actual mixed output for clipping, levels, FM/PCM alignment and clicks.
2. Measure worst block completion, queue occupancy and underruns with the
   real CPU/memory/audio transport. Reject a candidate that misses a refill
   deadline; propose numerical margin from measured storage/UI interference
   before promotion, rather than treating a passing average as sufficient.
3. Show identical audio scheduling with UI absent and delayed; then demonstrate
   responsive select/play/stop and Japanese titles. Startup remains silent.
4. Pass the relevant host/architecture tests, RTL ordering/CDC/reset checks,
   fit/timing and final Firmware 2.6 hardware checks. No host result substitutes
   for these hardware integration observations.

Extend the private manifest to every user-reference-playable MDX, including
PCM and compressed data, with whole-song or defined loop-boundary comparisons.
Classify missing dependencies and source-reference failures separately from
RPCMP failures; none becomes an exclusion merely because it is inconvenient.
The installed iOS build and settings are still unverified. Review the mixed
driver/sound-library licensing before distributing a port, and harden input
copy/decompression boundaries before exposing the legacy decoder to files.

If the composed current CPU lacks margin, first measure where cycles go;
compare a fitting faster CPU or narrowly scoped PCM acceleration. Reopen
full software synthesis only with measured improvements that cover the
whole budget. The present investigation is sufficient to choose this prototype;
further abstract comparison should not delay testing its complete audio path.
