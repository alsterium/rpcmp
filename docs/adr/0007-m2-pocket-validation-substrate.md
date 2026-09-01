# ADR-0007: Select a hardware sequencer for M2 Pocket validation

- Status: accepted
- Date: 2026-09-01
- Deciders: RPCMP maintainers

## Context

M2 must carry one frozen, self-authored 17-operation sequence through the v1
MMIO queue, JT51, and Pocket AUDIO on hardware. It does not need arbitrary
library access, parsing, UI, or a general-purpose runtime. The broader Pocket
product substrate remains gated by ADR-0004, including the unresolved
openfpgaOS placement sensitivity and project-owned soft-CPU comparison.

Using openfpgaOS merely to emit this immutable fixture would reintroduce those
unresolved variables. The pinned official openFPGA template has already booted
on Pocket and supplies the required APF command, heartbeat, reset, video, PLL,
and safe unused-pin shell. A small project-owned state machine can express the
same frozen values as the byte-exact host fixture and exercise the production
queue without becoming a future parser or player runtime.

## Decision

- Use a project-owned synthesizable state machine as the M2 hardware-validation
  execution substrate only.
- Run it from the template's 74.25 MHz clock and convert the fixture's 1,000
  ticks/second into absolute queue due ticks with integer constants.
- Submit every operation through Core-to-RTL Device Queue v1's staged MMIO
  registers. Do not bypass its depth, due-time, CDC, reset, or backpressure.
- Run JT51 and Pocket AUDIO from the template's exact 12.288 MHz PLL output.
- Reuse the pinned official template revision
  `da3a021b1eaf742604d86d8dc9b33a6666263e6a` for APF integration. Preserve its
  Host/Target boot behavior, continuous heartbeat, video timing, and physical
  pin defaults.
- Keep the resulting bitstream and package local-only under ADR-0006. The build
  consumes a clean JT51 checkout at
  `985a573dcfc1ff135553a39f7eae21d18ba57cbe`; no GPL source or binary is added
  to this repository.
- Do not reuse the fixed state machine for MDX or later runtime work. ADR-0004
  remains accepted and unsuperseded for the production Pocket substrate.

## Consequences

M2 can reach an audible Pocket test without claiming that C++ Core, library,
UI, storage, or a general player runtime executes in this shell. The host
scheduler remains the semantic source of truth and its exact fixture is checked
independently. Any divergence between the host fixture and RTL literal list is
a test/build failure.

This closes the execution-location decision needed for M2 local validation,
not the product-wide substrate gate. The integrated build must still pass
simulation, exact-source checks, Quartus fit/timing/resource gates, APF package
validation, and Pocket reset/relaunch/sustained-run checks.

## Alternatives considered

- **Stripped openfpgaOS:** feasible in resources, but its locally rebuilt
  bitstreams and nearby firmware layouts have unresolved boot sensitivity.
- **Project-owned soft CPU:** remains a production comparison candidate, but
  implementing storage, firmware memory, and a toolchain adds no evidence to a
  frozen M2 sequence.
- **Drive JT51 directly from a ROM:** smaller, but would bypass the accepted v1
  MMIO queue and weaken the hardware transport proof.
