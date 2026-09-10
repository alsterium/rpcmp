# M5 placement explanation after the four-row hardware matrix

Investigation: 2026-09-10. This review strengthens the explanation of the
historical failure recipe; it does not promote openfpgaOS to production.

## What the evidence now establishes

The historical packaging commits `b064a27` (empty terminal call, `0.5.36`)
and `32e168a` (skipped empty call, `0.5.37`) select their respective diagnostic
OS images but both select `SAFE_MEMSET_RBF`. Inspection with
`git show <commit>:tools/pocket_package.py` confirms the profile branch,
OS hash check, fixed RBF hash check and bit reversal before packaging.
These are the historical revisions, not just today's packaging implementation.

The preserved native RBF at
`out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/bld/rpcmp90fcmem/output_files/ap_core.rbf`
still hashes to `fa75e3cf3fe465090924d28df5616170cd2f4eefd9f4d68c89a72cb2cd93dbd5`,
the value enforced by both historical recipes. The job-local `firmware.mif`
hash is `a84afd867a4cdb2cc6bb4319f7be4252a5ab2d39cb857a9735c942d7c2ead567`.
Its complete decoded ROM passes the same-ELF check with the recovered safe
control and accepted OS. The retained assembler report ends with 0 errors,
0 warnings. A saved MIF/report alongside an RBF is provenance evidence, not
an independent extraction of that RBF's initialized memory contents.

The retained `firmware-0.5.36.elf` `.osdata` exactly equals the corresponding
diagnostic OS payload; its complete OS hash equals the historical recipe's
`8c6a0a5a46a3d7d3832560c7da889abe145aa47e350332960f4ec4ca43b8a3c7`.
The footer carries entry `0x1032c780` and BSS `0x10341550..0x1038da40`.
Its IRQ and syscall symbols are `0x103369c0` and `0x10335e80`, whereas the
preserved safe ROM calls `0x10336980` and `0x10335e40`. OSE2 metadata cannot
repair these fixed instruction targets or the other OS references recorded
by the [reference audit](pocket-m5-control-recovery.md).

The actual historical ELF/OS plus preserved safe MIF was rejected by
`pocket_firmware_pair.verify` with
`boot MIF does not match linked ELF (including OS references)`.
The recovered safe control plus that same MIF passed. This negative inspection
uses preserved artifacts; authored unit fixtures remain the repository tests.

The [four-row hardware report](pocket-m5-placement-hardware-check.md) now
demonstrates that coherent OS code/BSS shifts and independent app code/BSS
shifts by `0x40` all survive initial playback, stereo one-minute playback,
three warm starts and one cold start on firmware 2.6. This rules out treating
every such displacement as inherently unbootable on the tested substrate.

## Boundaries of the conclusion

The recorded historical recipe combines incompatible ROM/OS addresses. That
is a concrete software compatibility defect and a supported explanation for
the historical symptoms, rather than evidence of an unexplained cache fault.
The current same-link gate prevents this particular combination in the new
OS packages, and the coherent shifted candidates have passed hardware.

No `0.5.x` core ZIP was found when inspecting ZIP core metadata under the
repository's `out/` tree. This search says nothing about backups elsewhere.
The historical whole-ZIP hashes in the spike log therefore have not been
reproduced or checked against retained ZIP bytes. Exact attribution of the
old hardware run remains incomplete. Also, its older FPGA/cache configuration
differs from the current M5 integration; current hardware success does not
retroactively execute that older configuration.

Clean-source reconstruction of the old safe control is still unresolved.
The accepted bytes were recovered from retained objects, with main/memtest
objects substituted as documented. The current mutable sources beside those
objects are not a source-provenance record. This review adds no clean rebuild
claim and does not overwrite those objects. New placement probes instead use
the separately accepted fail-closed baseline and verified identical inputs.

ADR-0008 gate 4 has positive evidence for the initial four-row matrix. Gate 1
has a concrete bounded mechanism and successful prevention evidence, but this
does not establish arbitrary-layout stability or close every historical
provenance gap. Full production promotion remains withheld. Additional offsets
or a fallback SoC should be driven by an unresolved mechanism or a new failure,
not by presenting the present evidence as proof of arbitrary placement safety.

## Next bounded hardware slice

The next independent M5 requirement is admission-failure silence. The
[failure handoff](pocket-m5-failure-hardware-check.md) fixes the accepted
ROM/OS/app and changes only input data for argument, container and track
selection errors. It exercises existing terminal failure paths before
playback; it does not claim an APF transport timeout, in-flight queue fault,
continuous heartbeat trace or reset-controller failure test. Those remain
separate, as do integrated CDC/external-I/O policy and future PCM/UI reserves.

This investigation changed no runtime or RTL. Host verification covers the
new packaging tooling and current navigation; older RBF/report inspection is
not described as a new synthesis run.
