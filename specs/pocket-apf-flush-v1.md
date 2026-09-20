# Pocket APF flush transport — v1

Status: adopted for M6 slice 4, 2026-09-20. This is the command transport
portion of the approved [settings adapter plan](../docs/design/pocket-player-platform-adapters.md).
It does not advertise durable settings, reserve save slots or choose a watchdog.

## Command and ownership

The pinned openfpgaOS overlay adds CPU `DS_COMMAND = 5` at `0x40000038`.
`DS_SLOT_ID` at `0x40000020` supplies the 16-bit slot. The bridge emits APF
Target command `0x0188` with one zero-extended slot parameter. Offset, length
and buffer addresses are not flush parameters. Target busy/done and the
existing three-bit result return through the same path as other file commands.
Official result 0 means all bytes written; result 1 means undefined slot.
These encodings and the size-table behavior were checked against the
[official command specification](https://www.analogue.co/developer/docs/host-target-commands)
on 2026-09-20. Nonzero results remain failures for flush: an unknown result
above 7 maps to local error 7, never to success by truncating its upper bits.

The additive CPU read-only register `0x40000188` returns `0x00010001` for this
transport (version 1, flush bit 0), or zero when it is not instantiated.
The shared peripheral's `INCLUDE_APF_FLUSH` parameter defaults to zero; only
the Pocket overlay enables it. Legacy builds retain their previous protocol.
No unrelated upstream feature bit is reassigned.

In this profile, accepting any command 1..5 copies its parameters and locks
the command port until its matching completion. Writes while active cannot
replace the command or its copied payload, including before ACK and between
ACK and DONE. Unsupported full-word command values cause no request or status
change. Software still needs one OS owner for all Target commands; a rejected
write is not a queued request. Existing parameter staging may change while
busy, but in-flight copied parameters may not.

Flush follows the existing CPU-to-bridge synchronizer and payload latch.
All five operations, including GETFILE, latch their own parameters on request.
The completion/drain tracker must recognize flush as a new operation so a
previous DONE cannot complete it. A repeated flush requires a new rising edge.
Host status, menu, reset and data-slot commands continue while a flush waits.

Host Reset Enter clears locally pending command state. This neither cancels
an SD write nor proves an external transfer quiescent. The forthcoming OS
arbiter must retain ownership after timeout/reset uncertainty; it must not
reuse a save bank or staging buffer merely because CPU request bits cleared.
The settings adapter must additionally publish a complete RAM bank, update
the matching size-table entry, flush successfully, and independently read back
the SD record before reporting Saved.

## Acceptance

Exercise the actual patched command handler and peripheral with authored APF
transactions: success/error, repeated flush, stale DONE, copied slot identity,
busy rejection, unsupported commands, delayed host completion, independent
CPU/bridge clocks, and Host commands/reset during a pending flush. Preserve
the existing APF lifecycle regression. Synthesis/timing and integrated RAM,
OS arbitration, readback, package and firmware 2.6 evidence remain separate
requirements; a command-handler simulation does not prove persistence.
