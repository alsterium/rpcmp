# MDX and MXDRV compatibility research

Status: research baseline recorded on 2026-09-01. This document precedes the
MDX parser specification and implementation. It is not itself a public parser
contract.

## Purpose and evidence levels

RPCMP must reproduce files created for the historical X68000 driver, rather
than implement a format inferred from one modern player. This report therefore
separates evidence into three levels:

1. **Historical documentation** — contemporary MXDRV 2.06+16/02EX notes by
   YURAYSAN and documentation inventories for 2.06-era tools.
2. **Driver-derived behavior** — MXDRV 2.06+17 Rel.X5-S logic reconstructed
   from MXDRV.X and preserved in `portable_mdx`.
3. **Independent implementation and corpus observation** — `mdxtools` plus a
   read-only structural survey of the user's local files.

An item is suitable for the future RPCMP contract only when the sources agree,
or when a deliberate compatibility choice records the disagreement. Question
marks and undocumented behavior in a source are not silently promoted to a
requirement.

## Historical driver baseline

MXDRV was an X68000 music driver controlling the eight-channel Yamaha YM2151
(OPM), with MSM6258 ADPCM support and later PCM8-family extensions. The
historical inventory identifies 2.06+16 as the version widely ported during the
active era and 2.06+17 as the last beta-line version. The portable driver names
its exact lineage as `X68k MXDRV music driver version 2.06+17 Rel.X5-S`, by
milk., K.MAEKAWA, Missy.M, and Yatsube, then converted from an MXDRV.X
disassembly by GORRY.

RPCMP's initial compatibility target is therefore:

- file structure and core FM behavior common to 2.06+16 and 2.06+17;
- eight YM2151 tracks A–H;
- a PDX reference retained as metadata even when PCM playback is unsupported;
- explicit rejection of PCM/PCM8 execution and unverified extension commands.

The driver API (`trap #4`, load/play/stop calls, and internal work layout) is
historically relevant but is not an RPCMP runtime API. RPCMP decodes MDX into
the existing platform-neutral `DeviceOp` boundary instead of emulating the
resident Human68k driver interface.

## File layout

All multibyte integers below are big-endian, matching the 68000 representation.

```text
title bytes (normally Shift_JIS)
0d 0a 1a
PDX filename bytes (possibly empty)
00
BASE:
  u16 voice_data_offset
  u16 track_A_offset
  ...
  u16 track_P_offset          # 9-track form: A-H, P
  [u16 track_Q_offset ...
   u16 track_W_offset]        # 16-track PCM8 form
voice data and track bytecode at BASE-relative offsets
```

The first track offset also reveals the offset-table width:
`track_count = (track_A_offset - 2) / 2`. The documented forms are 9 and 16
tracks. Every offset must be checked as a BASE-relative unsigned 16-bit value
before access. A parser must find the complete `0d 0a 1a` title terminator and
the PDX-name NUL within bounded input; it must not reproduce permissive scans
that read beyond malformed data.

The title is metadata, not a structural identifier. It may contain multiple
lines and legacy control sequences. RPCMP should retain its original bytes and
perform bounded Shift_JIS display conversion separately. The PDX field is a
filename reference only; dependency resolution must apply the safe relative
path rules of the utility/library layer.

Some historical payloads use an LZX-compressed body. This is not the ordinary
MDX form and is out of scope for the first parser. It must produce a stable
`unsupported` result, not be interpreted as an offset table.

## Voice records

Each voice is 27 bytes. Byte 0 is the voice number, byte 1 packs feedback and
connection, and byte 2 is the operator/slot mask. The remaining six groups of
four bytes are ordered M1, M2, C1, C2 and map to YM2151 parameters:

| Bytes | Four operator values |
| --- | --- |
| 3–6 | DT1/MUL |
| 7–10 | TL |
| 11–14 | KS/AR |
| 15–18 | AME/D1R |
| 19–22 | DT2/D2R |
| 23–26 | D1L/RR |

Unused bits must be validated or masked according to a frozen parser policy;
bits 6–7 of the feedback/connection byte are documented as zero. Voice IDs are
byte-sized and need not be contiguous. Duplicate IDs require an explicit
policy before implementation because existing readers commonly let a later
record replace an earlier one.

## Track bytecode

The following lengths and broad meanings agree between the historical notes,
the MXDRV-derived implementation, and the independent decoder. Signed branch
values are two's-complement big-endian words.

| Opcode | Bytes | Meaning |
| --- | ---: | --- |
| `00`–`7f` | 1 | rest for opcode + 1 ticks |
| `80`–`df` | 2 | note/sample number; second byte + 1 is duration |
| `ff n` | 2 | set global YM2151 Timer B value |
| `fe r v` | 3 | direct OPM register write |
| `fd n` | 2 | select voice |
| `fc n` | 2 | pan/output phase |
| `fb n` | 2 | set coarse/fine volume |
| `fa` / `f9` | 1 | adjust volume by one driver step |
| `f8 n` | 2 | gate/staccato (`q`/`@q`) |
| `f7` | 1 | suppress next note's key-off (legato/tie) |
| `f6 n 00` | 3 | repeat start with mutable counter |
| `f5 rel16` | 3 | repeat end/back edge |
| `f4 rel16` | 3 | final-repeat escape |
| `f3 s16` | 3 | detune |
| `f2 s16` | 3 | per-tick portamento delta |
| `f1 00` | 2 | track end |
| `f1 rel16` | 3 | track loop/back edge |
| `f0 n` | 2 | key-on delay in ticks |
| `ef ch` | 2 | release a waiting channel |
| `ee` | 1 | wait for channel synchronization |
| `ed n` | 2 | FM noise or PCM rate, depending on channel |
| `ec ...` | 2 or 6 | software pitch LFO off/on/configure |
| `eb ...` | 2 or 6 | software amplitude LFO off/on/configure |
| `ea ...` | 2 or 6 | YM2151 hardware LFO off/on/configure |
| `e9 n` | 2 | key-on-to-LFO delay |
| `e8` | 1 | declare PCM8 mode |
| `e7 sub ...` | variable | +16/+17 extension family |
| `e6 sub ...` | variable | unofficial 02EX extension family |
| `e0`–`e5` | — | undefined in the examined baseline |

Important behavioral consequences:

- `ff` changes a global timer even when encountered in one track.
- `fe` can write arbitrary YM2151 registers, so register validation and event
  ordering cannot be inferred solely from high-level notes.
- historical notes and one modern decoder use opposite increase/decrease words
  for `fa` and `f9`, partly because the driver stores attenuation rather than
  audible gain. Their byte-to-audible-direction mapping must be golden-traced
  before the sequence contract names either operation.
- note start, gate expiry, key-on delay, software LFO, portamento, sync wait,
  and loop control all evolve on driver ticks, independently of video/audio
  callbacks.
- the original repeat implementation mutates the repeat counter embedded in a
  writable playback copy. RPCMP must model equivalent per-instance state and
  never modify the immutable source blob.
- `f1` has two encodings: two-byte end when the next byte is zero, otherwise a
  three-byte signed relative loop. Bounds checks must occur before choosing or
  following either form.
- sync wait can stall forever and loops can be infinite by design. Parsing and
  pre-analysis therefore require independent instruction, branch, tick, and
  loop budgets; reaching a budget is a bounded diagnostic, not evidence that
  the file is malformed.

## Timing

MXDRV timing is driven by YM2151 Timer B at the X68000 OPM clock of 4 MHz. For
Timer B byte `T`, one driver tick has the rational duration

```text
tick_seconds = 1024 * (256 - T) / 4,000,000
tick_rate    = 4,000,000 / (1024 * (256 - T))
```

MXC convention uses 48 ticks per quarter note, yielding
`BPM = 78,125 / (16 * (256 - T))` with integer compiler rounding. RPCMP must
retain the Timer B byte and use rational/integer accumulation as its timing
authority; converting to rounded BPM is display-only. Equal-tick operations
must preserve the historical channel service order after that order is proven
by golden traces.

## Frozen FM note-start observations

The M3 implementation rechecked the pinned `portable_mdx` translation before
adding device routing. For FM channels, the driver initializes pan to both
outputs (`c0`), coarse volume to 8, detune to zero, and no selected voice.
Commands update channel state; a selected voice is applied when the following
note is serviced. An absent selected voice is therefore rejected before that
note emits any register write.

For a note, the observed register order is:

1. If the voice changed, write four DT1/MUL values at `40+ch,+8...`, four TL
   values at `60+ch,+8...` (carriers temporarily muted with `7f`), then the 16
   KS/AR through D1L/RR values at `80` through `e0` in the same operator order.
2. Write pan plus feedback/connection to `20+ch`.
3. Compute `pitch = ((note & 7f) << 6) + 5 + detune`, clamp it to
   `0000..17ff`, write `(pitch * 4) & ff` to `30+ch`, then translate
   `pitch >> 6` through the YM2151 key-code table and write `28+ch`.
4. Add the channel attenuation to the original TL bytes only for the carrier
   mask selected by the algorithm, saturating at `7f`, and write those carrier
   TL registers in operator order.
5. Write `(slot_mask << 3) | ch` to register `08` for key-on.

The carrier masks by connection number 0 through 7 are `08,08,08,08,0c,0e,0e,0f`.
The coarse volume-to-attenuation table is
`2a,28,25,22,20,1d,1a,18,15,12,10,0d,0a,08,05,02`; values with bit 7 set are
direct attenuation values with that bit removed. `ff` writes register `12`,
and `fe` retains its exact address/value and position. Gate expiry, delayed
key-on, tie/key-off suppression, and per-tick portamento occur in separate
tick lifecycle stages and must not be approximated inside this note-start
mapping.

The tick lifecycle order is also frozen from `L001050`, `L0011b4`, and
`L000c66`: portamento accumulation occurs first when key-on delay is zero;
gate expiry can then emit `08 = channel` key-off; newly decoded commands follow;
finally a pending note either decrements its delay or emits the note-start
sequence. A nonnegative gate byte computes `((gate * raw_duration) >> 3) + 1`.
A negative gate byte adds to the raw duration modulo 256 and uses one tick when
the addition does not carry. `f7` suppresses gate expiry for the note following
it, and a tied note updates parameters and pitch without issuing another
key-on while the channel remains on.

## Extension boundary for the first FM milestone

The historical notes describe `e7` extensions added in +16/+17 (forced error,
fade, direct PCM8 operation, key-off policy, cross-channel control, duration
addition, and a flag) and unofficial `e6` commands in 02EX. One `e7`
cross-channel form has data-dependent length. Treating all `e7` commands as a
fixed three bytes, as one modern utility does, is not safe.

The first RPCMP FM-only implementation should:

- accept the 9-track layout and parse P as a known but unsupported PCM track;
- recognize the 16-track/`e8` form and return `unsupported_pcm8` before
  playback unless all extra tracks are proven inert by a later approved rule;
- reject `e6`, unknown `e7` subcommands, and `e0`–`e5` with opcode and bounded
  byte-offset diagnostics;
- implement an `e7` subcommand only after its exact length, control effect, and
  corpus need are documented and tested;
- never skip an unknown opcode by guessing its length.

## Local corpus survey

The user authorized read-only analysis of `C:\Users\new03\Documents\mdx`.
No file, title, track name, music data, PDX sample, or proprietary ROM was
copied into the repository. Results on 2026-09-01:

| Observation | Result |
| --- | ---: |
| `.MDX` files | 13,140 |
| Total MDX bytes | 44,045,401 |
| Size range | 98–86,202 bytes |
| Unique SHA-256 payloads | 12,058 |
| Duplicate copies | 1,082 |
| Exact `0d 0a 1a` terminator found | 13,139 |
| NUL-terminated PDX field after that marker | 13,139 |
| Non-empty PDX reference | 4,238 |
| Empty PDX reference | 8,901 |
| Heuristic 9-track layout | 8,764 |
| Heuristic 16-track layout | 4,351 |
| Other inferred widths | 21 |
| LZX body signature | 3 |

The track-width result is intentionally called a heuristic: it uses the
documented first-track-offset formula before complete semantic validation.
The 21 outliers and the one missing title marker must become negative or
compatibility-investigation cases, not reasons to weaken bounds checks. The
corpus contains 1,637 `.PDX` files, but filename matching was not used as proof
of a safe dependency because case, extension omission, aliases, duplicates,
and directory traversal rules still need a dedicated resolver audit.

The corpus is useful for coverage measurement and private differential tests,
but it cannot be committed or used as the sole fidelity oracle. Repository
tests need self-authored byte fixtures for every accepted command and malformed
boundary. Locally supplied files may add private event-trace hashes and command
coverage without recording copyrighted payload bytes.

### Bounded corpus-auditor result

M3 slice 2 added `tools/mdx_corpus_audit.py`. It recursively reads `.mdx` files
with a 1 MiB hard ceiling, scans each derived physical track region linearly,
and never follows branches or loops. It stops at `e6`/`e7` because their length
is not safely inferable from the opcode alone. Its JSON schema contains only
aggregate counts and extrema; root paths, relative paths, titles, PDX names,
payloads, and per-file hashes are not fields.

The formal auditor was run read-only against the same local corpus on
2026-09-01:

| Observation | Result |
| --- | ---: |
| Files and bytes observed | 13,140 / 44,045,401 |
| Bounded ordinary layouts | 13,038 |
| Missing title terminator | 1 |
| LZX body signature | 3 |
| Invalid inferred track table | 24 |
| Offset outside input | 74 |
| Linearly terminated track regions | 146,919 |
| Regions stopped at `e6`/`e7` | 551 |
| Regions stopped at undefined `e0`–`e5` | 19 |
| Truncated instruction at region end | 15 |
| Region exhausted without `f1` | 22 |
| Maximum title / PDX-reference bytes | 159 / 21 |

Opcode counts are occurrences in bounded physical regions, not file counts and
not proof that an opcode is semantically valid on a particular target. Relevant
compatibility pressure observed by the linear scan includes:

| Command family | Occurrences |
| --- | ---: |
| `fa` / `f9` attenuation steps | 707,634 / 687,265 |
| `e9` key-on LFO delay | 45,837 |
| `ea` hardware LFO | 45,244 |
| `eb` amplitude LFO | 11,770 |
| `ec` pitch LFO | 159,566 |
| `ed` noise/PCM rate | 59,548 |
| `e8` PCM8 declaration | 2,557 |
| `e7` +16/+17 family | 407 |
| `e6` 02EX family | 144 |

These results change implementation priority but not the v1 acceptance rule.
In particular, `fa`/`f9` and LFO behavior need golden traces early because they
are common, while `e6`/`e7` remain reject-on-sight until every used subcommand
has a proven length and effect. The 102 files outside the bounded ordinary set
remain compatibility-investigation inputs; they are not justification for
weakening offset or terminator checks.

## Required work before parser implementation

1. Write an M3 milestone and `mdx-v1` parser/sequence contract that freezes
   accepted layouts, opcodes, integer timing, service order, error codes, and
   resource budgets.
2. ~~Build a read-only corpus auditor that reports structure and opcode coverage
   without executing unbounded control flow or emitting titles/filenames.~~
3. Select at least two independent fidelity oracles. One must be the
   MXDRV-derived 2.06+17 behavior; the other should be a separately implemented
   decoder or an X68000/emulator register trace.
4. Generate self-authored 9-track FM-only fixtures from documented MML and
   hand-verify their exact bytes. Add malformed variants for every length,
   offset, branch, loop, sync, voice, and arithmetic boundary.
5. Only then implement structural parsing. Sequencing and YM2151 event
   generation follow in a separate verified slice.

## Sources and pinned observations

- [Historical MXDRV 2.06+16/02EX data notes](https://w.atwiki.jp/mxdrv/pages/23.html)
  — contemporary-era format, commands, extensions, and driver calls; contains
  explicitly unknown fields and is therefore not sufficient alone.
- [MXDRV version and document inventory](https://z80.msx.click/index.php?title=MMLCOMPILER_MXDRV)
  — identifies 2.06+16-era adoption, 2.06+17 status, and surviving manuals.
- [portable_mdx](https://github.com/yosshin4004/portable_mdx) commit
  `60c43e79c8880e5090c077395f3378947e2d8651` — MXDRV 2.06+17 Rel.X5-S
  disassembly-derived control behavior and bounded file helpers. This is a
  behavioral research oracle, not an approved RPCMP dependency.
- [mdxtools](https://github.com/vampirefrog/mdxtools) commit
  `9c8539fec2757fcf7c85d1986171b50ebe2ef1e5` — independent parser/compiler/player
  used to identify agreements and disagreements. Its format document labels
  itself work in progress.
- [mdxtools MDX format notes](https://github.com/vampirefrog/mdxtools/blob/master/docs/MDX.md)
  and [MXDRV MML notes](https://vgmrips.net/wiki/MXDRV_MML) — secondary command
  descriptions and the 48-tick MML timing convention.

No third-party source or binary from this research is linked into RPCMP, and no
new dependency is selected by this document.
