# Pocket settings RAM owner — v1

Status: adopted for M6 slice 4, 2026-09-20. This is the synchronous, BRIDGE-clock
storage boundary of the approved [adapter plan](../docs/design/pocket-player-platform-adapters.md).
It does not change the [64-byte settings record](playback-settings-v2.md),
advertise persistence, or select the physical APF/MMIO address map.

## Storage and lifetime

There are two logical slots, each with two 64-byte banks, and one independent
64-byte media-readback area. Each slot publishes a bank and a length together.
The inactive bank receives CPU candidates or Host loads, never both. A CPU
candidate publishes only after every byte has been written. A Host load
publishes its declared length (0..64) only after all its required bytes arrive.
Bytes beyond the published length read as zero. Record CRC/format validation
belongs to Core; in particular, the RAM owner preserves unknown-format bytes.

Only FPGA configuration/power reset clears metadata and leases. RAM contents
need not be initialized because no byte is readable before its validity is
established. Power reset may assert asynchronously; its deassertion must be
synchronized to this clock by the platform. Audio reset is not connected here.
`abort_candidate` discards unpublished CPU candidates on application/Host reset;
it preserves published
bytes/lengths, Host loads, read locks, published CPU leases and readback leases.
It blocks BeginCpu, WriteCpu and Publish on that edge. The retained response
also survives; the upstream command mailbox must share this lifetime.
A new CPU session must recover/drain
existing ownership before issuing replacement operations.

Publishing acquires a CPU lease until explicit ReleaseCpu. This freezes the
bank/length for flush, errors, cancellation and late completion. Elapsed time,
APF Reset Enter and a software timeout never release it. Host reads may acquire
a separate read lock on that same published bank. CPU snapshot reads have their
own lock. Either read lock blocks publication and new loads/candidates.
Unpublished candidates may continue receiving bytes while a read lock exists.
BeginRead/LockSnapshot are idempotent for their one serialized external owner;
they are not recursive locks. One owner must never release another's lock.

Readback has Empty, Receiving and Sealed states. BeginReadback clears validity;
only external BRIDGE writes can supply its bytes. FinishReadback seals only a
complete declared prefix. CPU reads require Sealed. ReleaseReadback is the only
way to reuse it, including after failure. The caller must first establish that
no more writes from the previous Target operation can arrive. It is never filled
from a published slot or CPU candidate.

## Local command boundary

All ports use one clock. An upstream arbiter supplies one command on
`command_valid && command_ready`; its result is retained until
`response_valid && response_ready`. Acceptance and execution are one edge, with
no combinational response. No command is accepted while a response is retained.
The adapter must copy/CDC complete requests and preserve ownership across CPU
reset. This contract does not implement that adapter or its APF deadlines.

Slot and word fields are checked before indexing: slots 0/1 are save slots,
2 is readback; word indexes are 0..15. Length is a full unsigned 32-bit value,
checked against 64 before narrowing. Data is a little-endian numeric word;
mask bit 0 enables its lowest byte. Unused command fields are ignored.
Results are 1 Success, 2 Busy, 3 Invalid, 4 Incomplete. Rejected commands have no
effect, except an incomplete FinishLoad discards that load and latches its error.

| Kind | Name | Effect |
| --- | --- | --- |
| 0 | BeginCpu | Reserve inactive slot bank; clear candidate coverage |
| 1 | WriteCpu | Write enabled bytes of candidate word; mask must be nonzero |
| 2 | Publish | Require all 64 bytes, atomically publish length 64 and acquire CPU lease |
| 3 | AbortCpu | Discard an unpublished CPU candidate |
| 4 | ReleaseCpu | Release published CPU lease, only after external quiescence |
| 5 | BeginLoad | Reserve inactive slot bank for Host, length 0..64 |
| 6 | FinishLoad | Publish complete Host load; incomplete load preserves old bank and length |
| 7 | BeginRead | Lock the published slot for Host reads; blocked during a Host load |
| 8 | EndRead | Release Host read lock after external access completes |
| 9 | BeginReadback | Reserve independent readback area, slot 2, length 0..64 |
| 10 | FinishReadback | Seal complete readback prefix; incomplete remains Receiving |
| 11 | ReleaseReadback | Discard/release readback, only after external quiescence |
| 12 | ReadWord | Return published slot word under snapshot lock, or sealed readback word |
| 13 | Status | Return metadata without acquiring ownership |
| 14 | LockSnapshot | Lock published slot bank/length for CPU snapshot reads |
| 15 | UnlockSnapshot | Release CPU snapshot lock |

Slot Status bits: 6:0 length, 8 CPU candidate, 9 CPU published lease,
10 Host load, 11 Host read lock, 12 load error, 13 CPU snapshot lock,
14 changed since Host load; others 0.
Readback Status: 6:0 declared length, 8 Receiving, 9 Sealed; others 0.
Load error clears on the next successful FinishLoad, not on ordinary reset.
Changed-since-load is set by Publish and cleared only by a successful FinishLoad
or power reset. It survives ReleaseCpu, failed Host loads and application reset.
A new software session must rescan SD for such a slot before treating it as
restored/durable: publication and even I/O quiescence do not establish that
the mirror matches media. The current operation's flush/readback verification
remains an external responsibility and does not clear this conservative marker.
Absent leases/owners make their release command Invalid, except the two
idempotent read-lock acquisitions. BeginCpu/BeginLoad require no owner/lease/
read lock. A CPU lease does not prevent either read lock.

## BRIDGE data boundary

This module takes a checked, local byte offset: 0x00..0x3f slot A,
0x40..0x7f slot B, 0x80..0xbf readback. Larger or unaligned offsets read zero
and cannot write; there is no address truncation or aliasing. The outer decoder
must select the physical window before supplying these offsets. These local
offsets do not reserve a global BRIDGE window.

Reads return a registered word one clock later and require a Host read lock
or CPU published lease. Readback is CPU-only. Writes require an active Host
load or Receiving readback. Under that lease the read result is valid until the
next BRIDGE access; a write clears it. Complete a read before releasing its
lease. Each accepted word updates only enabled bytes below
the declared length; tail padding is ignored. Zero masks and words outside the
declared length are rejected. `bridge_write_accepted` is an edge qualification,
not an APF bus acknowledgment. Missing bytes prevent Finish; the caller must
not claim success from bus activity alone. Finish must follow the final write
edge and external drain, not coincide with it. Finish on an accepted write to
its region returns Busy and can be retried after drain.
If read and write are asserted together, read takes priority and the write is
rejected. The two physical memory ports can otherwise serve CPU and BRIDGE on
the same edge; leases ensure that their writes cannot target the same bank.

## Acceptance / next connection

The implementation binds the existing Quartus 25.1std Cyclone V `altsyncram`
primitive directly; Questa 2025.2 tests use `altera_mf_ver`, not a replacement
RAM model. It uses 128 physical 32-bit words, of which 80 are addressable here.
This is FPGA-specific, under the installed Altera Program/IP license terms
(for Altera devices); no vendor implementation is copied into the repository.
Metadata remains ordinary synthesizable logic. The two ports must never have
the same address with either writing; an executable collision assertion checks
that guarantee because mixed-port read-during-write is unspecified.

Exercise every interrupted byte prefix, lengths 0..64, invalid full-width
length/index/offset inputs, partial masks, retries, both banks, independent
slot ownership, read locks, retained responses, reset during candidate/lease/
Host load/readback, late writes and separate readback data. A local registered
fit checks resource/timing feasibility, not CPU/BRIDGE CDC or whole-Pocket fit.

The APF integration must map Host request/all-complete to these locks, preserve
the actual nonvolatile ID/size entries on reset, and make published length and
the APF size table coherent before acknowledging Host/Target accesses. The
stock positional reset clear is not compatible with arbitrarily placed new
save entries. The OS arbiter, CDC, map, package, media verification and hardware
acceptance remain separate work; this local owner alone cannot declare Saved.
