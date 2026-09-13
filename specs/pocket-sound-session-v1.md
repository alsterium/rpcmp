# Pocket sound session v1

Status: adopted for M6 slice 4 following the user's review and approval of the
CPU sound connection proposal on 2026-09-13. This first layer implements the
existing [Core audio controls](playback-transport-v1.md) over
[audible progress](mdx-audible-progress-v1.md). The broader approved
[CPU connection plan](../docs/design/pocket-cpu-sound-connection.md) owns the
subsequent MMIO/CDC, retained producer and display journal. No CPU capability
or hardware deadline is advertised by this synchronous layer alone.

## Synchronous request and response

`rpcmp_sound_session` has one owner on the 12,288,000 Hz audio clock. All inputs
except common asynchronous reset are synchronous. Common reset release must
be synchronized. Serial clocks continue through normal session reset/inhibit.
One copied request remains owned until its response handshake; a response pop
does not simultaneously admit the next request. Held valid means a handshake
on ready, not repeated execution while busy. Reads/copying never execute work.

Request fields are nonzero `request_id:u64`, `request_generation:u64`,
`request_kind:u3` (Reset=0, Start=1, Pause=2, Resume=3, SetPolicy=4),
`request_revision:u64`, `request_target_enabled` and `request_target:u32`.
Reset requires zero revision/target/enable. Other controls require nonzero
revision, positive enabled target, and zero target when disabled. Values are
validated before being driven into the envelope. ID must exceed the last
well-formed admitted request's ID; stale/replayed IDs never execute again.

Response fields echo the copied request exactly, plus `response_frame:u64` and
`response_result:u3`: Success=1, Invalid=2, Stale=3, Failed=4. The response is
immutable until `response_valid && response_ready`; its validity is separate
from shared fault. A later device fault does not rewrite an already completed
response, but is still reported independently. Invalid/Stale requests do not
disturb playback. Busy means request_ready is false, not successful completion.
No counter wraps. Invalid form precedes stale ID/generation classification.

## Reset, generation and supply

Common reset starts inhibited, with no prepared generation or feed epoch.
An explicit valid Reset accepts a generation at least as new as the last Reset
request. It advances a nonzero, nonwrapping `feed_epoch:u64`, closes supply,
and pulses a distinct logical session reset. Old completion/producer messages
must carry their old epoch and cannot become new work. Feed epoch overflow
fails the request, latches shared fault/inhibit and requires common reset.
The last started generation and last request ID survive ordinary session resets.

`rpcmp_jt51_progress_audio` adds synchronous `session_reset`, tied low by
existing callers. It clears logical envelope state and triggers the existing
native/output reset hold, without assigning DeviceFault to this intentional
operation. The old `stream_reset` fault semantics remain unchanged. This is
explicit state destruction, not conversion of a failure into success. The
containing session alone validates generations and owns normal reset requests.

Reset Success requires physical quiescence after the 2,048-clock native reset
hold, frame zero, cleared old work and no active external fault/inhibit request.
It prepares the requested generation, opens its new feed epoch and clears
latched fault/inhibit. Reset requested with a continuing external fault fails;
a new fault or emergency request while reset is pending fails that request.
Failure does not report quiescence or release borrowed CPU data. A later explicit
Reset may retry after the external condition clears; no implicit retry occurs.

Items add `item_generation:u64` and `item_epoch:u64` to the existing source
fields. Common reset returns Closed=4. Otherwise while valid, a mismatching
generation/epoch returns Stale=5. Matching
items while supply is closed, reset acceptance, fault or inhibit
return Closed=4. Otherwise the source queue owns Accepted/Full/Invalid/Closed.
Only Accepted moves producer ownership. Reset wins over a same-edge item.
Supply remains open during Pause and closes on terminal state/failure. Queued
items never depend on display delivery. Native receipts are consumed locally
without waiting for a CPU display reader.

## Playback controls and observation

Start requires the successfully reset generation, an epoch open for supply,
and a generation strictly newer than the last accepted Begin. The existing
nonempty source queue, quiescence and bootstrap checks remain mandatory.
Begin acceptance reserves that generation; Start Success follows the first
actual consuming boundary and reports its resulting frame count. A rejected
Begin does not reserve the generation. It may be retried with a new request ID
after its missing input is supplied.

Pause/Resume/SetPolicy require the active started generation. Policy revision
cannot go backwards or reuse the current revision with a different target.
Pause requires running, Resume requires paused; SetPolicy permits either.
Controls are retained until an audio boundary, with fault taking priority.
Pause reports the held next-frame position. Resume reports that same pre-consume
position, even when the latest observation has advanced by the time the ACK is
read. SetPolicy reports its application boundary. The echoed policy is the
copied request; latest state may advance but cannot tear into that response.

Control already overtaken by terminal state is Invalid, not a fabricated success
that cancels an old end. A hardware failure is Failed and remains a shared fault.
All current state outputs retain the envelope's generation/frame/policy/count/
gain/end meanings. The future CPU capture mailbox must copy them coherently.
Core processes the completion before advancing its latest media observation.

`emergency_silence` is independent of request/response ownership. Its sampled
assertion latches inhibit, closes supply and enters native fault/reset. It can
truncate a partial frame, while MCLK/LRCK continue. Inhibit alone never proves
Reset completion. Only successful explicit Reset clears it. A device/envelope
failure also latches shared fault/inhibit; normal controls then fail. Completed
responses remain stable and readable during emergency handling.

## Local latency and acceptance

From a request's audio-clock acceptance edge, the designed bounds are Reset
2,050 subsequent edges, Start 259, and Pause/Resume/SetPolicy 257, when valid and
not faulted. Reset uses one registered reset pulse, the 2,048-edge native hold
and observation of quiescence. Start includes request dispatch, Begin/bootstrap,
the next eligible serial boundary and observation of the first consumed frame.
Boundary controls include one observation edge after application to distinguish
a concurrent envelope failure from success; their copied frame is still the
pre-consume position. Fault interrupts pending controls. Invalid/Stale is returned without execution.
These are acceptance targets for this local layer, not measured CDC/CPU ACK
deadlines. MMIO transfer/synchronizer/service costs require their own proof.

Test normal reset from running/paused/faulted states, continuous serial clocks,
old item epoch rejection, response backpressure, duplicate/stale/malformed
requests, all serial request phases, exact Resume position, live policy,
terminal races and emergency during each control phase. Observe real JT51 and
actual serialized output; include source starvation and generation recovery.
Verify local latency bounds and negative controls, preserve old RTL/host gates,
then synthesize and time the registered composition. Whole-Pocket CDC, CPU
producer/history, packaging and firmware 2.6 acceptance remain separate work.
