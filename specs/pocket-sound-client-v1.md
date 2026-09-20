# Pocket CPU sound client v1

Status: internal implementation contract within the CPU connection plan
approved on 2026-09-13, M6 slice 4. Implements the existing
[sound MMIO v1.1](pocket-sound-mmio-v1.md), its approved 1,000 us watchdog and
the retained producer/history ownership rules. No public player schema,
product policy, hardware word or legacy queue/reset meaning changes.

## Ownership and service

One CPU owner uses injected 32-bit MMIO and a monotonic microsecond clock.
Initialization checks the exact implemented ID, version, capabilities and
clock rates, and requires all four mailbox slots empty. Startup inhibit is
expected before a successful sound Reset. Initialization neither resets sound
nor discards unknown borrowed requests. A common hardware reset requires a
fresh client after the platform confirms that reset; ordinary Reset does not.

Control, feed, capture and journal retain separate copied requests and results.
Busy means either hardware still owns a request or the caller has not taken
its copied result. Submitting to a busy slot performs no staging writes.
Service is bounded: one status read and at most one copied response per slot,
followed by deadline checks. It never waits for a hardware edge or retries a
Full feed. The retained MDX producer alone decides when to retry the same item.
UI/public snapshot reads do not call this client.

All u64 fields use explicit low/high u32 words. Read only documented response
words after their valid bit; validate request echoes, result enums, reserved
bits and bounded fields before use. Release hardware exactly once after the
copy, while keeping the CPU result until the caller takes it. Feed's local
token comes from its single retained offer, never from a fabricated wire field.
Valid Invalid/Stale/Failed wire results retain their meanings and are not
successful controls. The enclosing backend processes control completion before
the newer captured media observation and rejects obsolete generations/epochs.

## Deadlines and recovery

Record time immediately after submit using the injected real clock. A service
first consumes matching responses, then checks elapsed time, including equality
at 1,000 us. Use checked monotonic ordering and subtraction, never wrapping
deadline addition. A backwards clock is a failure, not a fresh timer epoch.
This is a shared clock failure, not display-only journal loss; it inhibits sound
and rejects further timed submissions until platform clock/reset recovery.

Timeout reports one failed result but retains hardware ownership. The slot
cannot be resubmitted after the caller takes that failure until its late
response is drained. A late response cannot replace the recorded failure or
fabricate a successful control/feed/capture. Absence of valid/busy bits is not
proof of cancellation or permission to reuse borrowed state. Only a drained
response or confirmed common reset ends hardware ownership.

Critical client failures in control/feed/capture request independent INHIBIT;
they never claim quiescence. A subsequent explicit successful Reset may recover
the sound communication state; outstanding slots still retain their own drain
requirements. Reset submission waits for the
independent emergency delivery to finish; it does not block on a full feed.
Each new critical failure during recovery requests inhibit again, so an older
inhibit cannot be cleared by a late Reset whose deadline already failed.
A Reset response may clear the client failure only if no newer INHIBIT was
issued after that Reset's submission. Its immutable wire success remains a
past fact even when a newer emergency request supersedes recovery.

Journal uses a 1,000 us display-service deadline as an internal implementation
choice. Its timeout or malformed response marks display acquisition failed,
retains/drains that slot and leaves audio/control/feed/capture running. It
never requests inhibit. The backend uses Waiting/Degraded history and the
existing latest-record recovery, without guessing event frames. This display
deadline is not a new audio timing guarantee.

Normal sound Reset can run while old feed/capture/journal transfers remain
borrowed. Their old identities remain attached until they drain; Reset success
does not cancel or release them. A stale transfer cannot acknowledge new work.
No implicit reset, replay or response-defaulting is performed by this client.

## Acceptance

Use scripted MMIO/clock tests with independently authored word transcripts:
every request/response word and high half, missing/illegal reads, copied results,
independent busy slots, Full, late old epochs, malformed echoes/fields, deadline
999/1000/late completion, clock reversal and u64 limits. Verify journal failure
never writes INHIBIT or blocks another slot, and expired Reset cannot later
become success. Run host architecture/format/static checks and RISC-V compile/
link memory checks. The full AudioTransportPort backend, producer/history
service, storage adapter, whole-Pocket decoder/timing and firmware acceptance
remain separate integration work.
