# Pocket sound MMIO v1

Status: implementation contract within the CPU connection plan approved on
2026-09-13. This layer transports the [sound session](pocket-sound-session-v1.md)
without changing Q1–Q24 or the legacy queue/reset v1. Acceptance requires the
M6 CDC, independent-clock RTL and timing checks; adoption alone is not evidence.

## CPU register access

The fixed CPU-local base is `0x40000400`, size 1 KiB, 32-bit little-endian.
It is not APF BRIDGE. All offsets below are relative to this base. Each rd/wr
strobe denotes one CPU-clock transaction; both together are invalid. Only
aligned full-word writes (`byte_enable=0xF`) are supported. Unknown addresses,
wrong access direction and undefined written bits are rejected without changing
staging or submitting work. Rejected accesses return zero and assert error;
invalid/busy diagnostic flags are sticky. There is no bus wait for audio work.
Integrators gate strobes to this block; accesses outside its 1 KiB never alias.

Staging is readable/writable while an earlier copied request is outstanding.
SUBMIT requires value 1 and an idle mailbox with the audio domain online.
Otherwise it sets busy and does not replace or repeat work. RELEASE requires
value 1 and a completed response, otherwise invalid. Reading an absent response
is invalid. Reading a present response never pops it. Response release and a
new submit are separate CPU transactions. Common reset clears all diagnostics.

| Offset | Access | Meaning |
| --- | --- | --- |
| 000 | R | ID `0x52534D31` (RSM1) |
| 004 | R | version `0x00010000` (major 1, minor 0) |
| 008 | R | capability bits 0 control, 1 feed, 2 capture: `0x7` |
| 00C | R | status described below |
| 010 | W | INHIBIT, value 1 sets the independent emergency request |
| 014 | W | CLEAR, bit 0 invalid, bit 1 busy; other bits zero |
| 018 | R | audio tick rate 12,288,000 |
| 01C | R | output frame rate 48,000 |
| 020/024 | RW | staged control ID low/high |
| 028/02C | RW | staged control generation low/high |
| 030 | RW | staged kind in bits 2:0; higher bits zero |
| 034/038 | RW | staged policy revision low/high |
| 03C | RW | staged target enabled in bit 0; higher bits zero |
| 040 | RW | staged target |
| 044 | W | control SUBMIT |
| 048 | W | control RELEASE |
| 080/084 | RW | staged item generation low/high |
| 088/08C | RW | staged item epoch low/high |
| 090/094 | RW | staged item at low/high |
| 098/09C | RW | staged item until low/high |
| 0A0/0A4 | RW | staged item loops low/high |
| 0A8 | RW | staged marker bit 0, end bit 1; higher bits zero |
| 0AC | RW | staged address bits 15:8, value 7:0; higher bits zero |
| 0B0 | W | feed SUBMIT |
| 0B4 | W | feed RELEASE |
| 0B8 | R | copied feed result: session item status 1..5 |
| 0BC/0C0 | R | copied offered generation low/high |
| 0C4/0C8 | R | copied offered epoch low/high |
| 100/104 | RW | staged capture ticket low/high (nonzero u64) |
| 108/10C | RW | staged expected feed epoch low/high |
| 110 | W | capture SUBMIT |
| 114 | W | capture RELEASE |

STATUS bits: 0 control busy, 1 control response valid, 2 feed busy, 3 feed
response valid, 4 capture busy, 5 capture response valid, 6 synchronized audio
fault, 7 synchronized audio inhibit, 8 invalid access, 9 busy rejection,
10 emergency delivery pending, 11 synchronized audio online. Other bits zero.
Busy includes unread completion. Single-bit audio status is diagnostic; use
capture for a coherent multiword observation. Online means reset release,
not native quiescence or Reset completion. Capability bit 3 and `0x300..0x3FF`
are reserved for the approved subsequent output-journal layer and are not
advertised/implemented by this transport-only revision. All other unlisted
offsets are reserved and invalid, not readable zero-filled registers.

Malformed control payloads (zero IDs, kinds 5..7, conflicting policy, etc.)
reach the session and return its explicit Invalid/Stale/Failed response.
MMIO checks only access encoding/reserved bits; it does not duplicate the
session's semantic validation. Each feed submit performs exactly one queue
offer. Full is a completed feed result; retry requires release and a new submit
of the same item. It never holds a control request waiting for source capacity.

## Copied control completion

| Offset | Meaning (all read-only, reserved bits zero) |
| --- | --- |
| 180/184 | request ID low/high |
| 188/18C | request generation low/high |
| 190/194 | request policy revision low/high |
| 198 | request kind |
| 19C | request target enabled, bit 0 |
| 1A0 | request target |
| 1A4/1A8 | response frame low/high |
| 1AC | result: Success=1, Invalid=2, Stale=3, Failed=4 |
| 1B0/1B4 | actual feed epoch at completion copy, low/high |
| 1B8/1BC | actual prepared generation at completion copy, low/high |

The echoed request and frame retain the session's exact meanings, including
Resume's pre-consume position. Epoch/prepared generation are copied on the same
audio response handshake. Later faults and playback do not modify this copy.

## Coherent capture

The audio request handshake captures every field on one edge, before that
edge's state updates. A simultaneous boundary cannot mix old frame with new
policy or vice versa. Capture result is Invalid=2 for zero ticket, otherwise
Stale=3 if expected epoch differs, otherwise Success=1 (including faulted or
inhibited state). Error captures remain coherent diagnostic copies, not state
to publish as a successful current-generation observation.

| Offset | Meaning (all read-only) |
| --- | --- |
| 200/204 | echoed capture ticket low/high |
| 208/20C | echoed expected epoch low/high |
| 210/214 | actual feed epoch low/high |
| 218/21C | prepared generation low/high |
| 220/224 | generation low/high |
| 228/22C | media frame low/high |
| 230/234 | policy revision low/high |
| 238/23C | completed loops low/high |
| 240 | target |
| 244 | bits 0 target enabled, 1 paused, 2 quiescent, 3 resetting, 4 device idle, 5 media enable, 6 fault, 7 inhibited |
| 248 | phase bits 1:0, end reason 4:2, failure 7:5 |
| 24C | gain, low 18 bits |
| 250 | ramp elapsed, low 18 bits |
| 254 | source queued, low 7 bits |
| 258/25C | output prefix low/high |
| 260/264 | output loops low/high |
| 268 | output flags: valid bit 0, checkpoint valid bit 1, ended bit 2 |
| 26C/270 | pending prefix low/high |
| 274/278 | pending loops low/high |
| 27C | pending flags: same layout as output flags |
| 280 | capture result |

All unused bits are zero. Pending fields are not audible history. Latest
output prefix alone is not an event journal; the subsequent journal must
retain first output frames and natural-end checkpoints independently.

## CDC, reset and ownership

Three independent single-request mailboxes carry control, feed and capture.
Each sender registers a complete request and toggles request. Two destination
synchronizers precede copying it into a destination register. The destination
holds valid until acceptance; response is copied into a held bundle and ACK
toggle. Two sender synchronizers precede copying the response into the CPU
register. Bundles remain fixed until the other side has copied them; requests
are not re-used until response release. Mark synchronizer registers and bound
the bundled data routes separately from the asynchronous toggle first stages.

Both platform reset inputs assert one common reset (`cpu_reset_n &&
audio_reset_n`), asynchronously clearing both mailbox sides and the session.
Release is synchronized independently through two registers in each domain.
An assertion originating from either side therefore cancels both sides;
integration must not reset an individual mailbox in isolation. No old toggle,
ACK, request or response survives common reset. Require audio online before
submission. Common reset is distinct from the normal session Reset command.

The shared logical mailbox epoch is the session's checked feed epoch. It
advances only on a valid admitted normal Reset; an Invalid/Stale control does
not cancel playback. Feed responses echo the offered epoch; capture echoes
the expected epoch and records the actual epoch. Normal Reset never clears a
toggle while a bundle is borrowed. Old transfers drain with their original
identity; software discards/releases old feed/capture responses before reusing
those independent slots for the new epoch. A stale response cannot complete a
new request. Reset completion itself never releases a still-borrowed bundle.

INHIBIT uses an independent acknowledged toggle, with two synchronizers in
each direction. Detecting a new toggle produces one audio-clock emergency
pulse; the session latches inhibit until successful explicit Reset. While an
emergency delivery is outstanding, further writes retain one coalesced
follow-up delivery, which is sent after ACK. Thus a short CPU write cannot be
lost, including one following an already-delivered event whose ACK is still
crossing back. Repeated emergency writes do not wait for a control mailbox.
Reset SUBMIT while emergency delivery is pending is Busy: software waits for
that bounded delivery before requesting recovery. Later INHIBIT writes fault
a pending Reset. Invalid/failed Reset and CLEAR cannot cancel an emergency or
clear audio fault/inhibit. Common reset alone cancels the delivery protocol.

## Latency proof and acceptance

With online, running 90 MHz CPU and 12.288 MHz audio clocks, no destination
backpressure beyond the session, and local request bound L audio edges, the
designed submit-to-CPU-visible-response bound is `(L+5)*Taudio + 3*Tcpu`.
It comprises up to three destination edges for toggle synchronization/copy,
one for request acceptance, L local edges, one for response copy, then up to
three sender edges. L is 2,050 Reset, 259 Start, 257 boundary controls, zero
for a single feed offer/capture. Independent-clock tests and local CDC/timing
checks now pass; M6 records their scope. At the nominal rates the resulting
bounds, rounded upward, are:

| Operation | Microseconds | Whole 90 MHz CPU ticks |
| --- | ---: | ---: |
| Reset | 167.270 | 15,055 |
| Start | 21.518 | 1,937 |
| Pause / Resume / SetPolicy | 21.355 | 1,922 |
| One feed offer / capture | 0.441 | 40 |

These ideal-edge bounds exclude CPU service/polling time and analog
metastability delays; they do not themselves set a CPU watchdog constant.
On 2026-09-20 the user explicitly approved a **1,000 microsecond** mailbox
watchdog for control, feed and capture. Measure from successful submit using
an injected monotonic real clock, independently of rendering. Process a
matching completion before testing expiry, including exactly at the deadline.
An incomplete expired operation is failure, never cancellation, quiescence or
permission to reuse borrowed state. Drain its response or confirm common reset
before reuse. This value exceeds the longest local bound by more than five
times; CPU service frequency and the enclosing Core deadline still require
adapter/integration checks. It is not a measured Pocket response time.

The local fixture uses CPU/audio periods 11.111 / 81.380 ns. False paths
terminate only at first toggle/status synchronizer stages and external-reset
release registers. Every second stage and local reset recovery/removal path
stays timed. Held request/response bundles have maximum delays 81.380 / 11.111
ns; only their asynchronous hold relationship is excepted, justified by
ownership through ACK. No clock-group false path may mask these bundle limits.
Audit every optimized bundle destination and actual data delay as well as
setup slack. Whole-Pocket integration must repeat this proof with its clocks,
routing, decoder and reset; the local fixture does not establish board timing.

Verify every word and illegal access, independent mailbox backpressure,
staging mutation, coherent copies across frame/policy changes, every reset
transfer stage and either reset origin, stale epochs, Full/retry and emergency
during pending/unread operations. Use actual JT51, shifted independent clocks,
unchanged native/host regressions, negative controls and a constrained dual-clock
fit. Whole-Pocket decode, CPU backend, journal and firmware acceptance follow.
