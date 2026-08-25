# Pocket M0 Package

## 1. Purpose and status

This document defines the installable APF package for the M0 register-boundary target experiment. It is deliberately identified as version `0.0.0-m0.2` with description `M0 register-boundary target spike.` It is not an RPCMP music-player release and does not supersede ADR-0004.

The definitions in `core/platform/pocket/apf` are project-owned rather than copied from the official template. They follow the current official definition roots and `APF_VER_1` magic while retaining the template's proven 320x240 video and framework requirement 1.1.

## 2. Package shape

The generated ZIP contains only this SD tree:

```text
Cores/
  alsterium.RPCMP/
    audio.json
    bitstream.rbf_r
    core.json
    data.json
    input.json
    interact.json
    variants.json
    video.json
```

The spike has an empty `platform_ids` list, so it does not add `Platforms`, `Assets`, or `Saves`. The optional core icon is also omitted. The ZIP filename is `alsterium.RPCMP_0.0.0-m0.2_2026-08-25.zip` and has no extra wrapper directory.

## 3. Generation and validation

Run after `tools/pocket-spike-build.ps1` has produced the integrated `ap_core.rbf`:

```powershell
pwsh -File tools/pocket-package.ps1
```

The script:

- parses every JSON definition and checks its exact root object and `APF_VER_1` magic;
- validates bounded metadata, empty future-feature lists, the bitstream ID/filename, and the 320x240 scaler mode;
- checks all Interact IDs, types, access modes, and addresses against the provisional register contract and rejects APF-reserved `0xF8xxxxxx` addresses;
- self-tests byte bit reversal, converts the RBF without changing length, and reverses the result again in memory to prove an exact round trip;
- rejects missing or unexpected core files and unsupported ZIP base folders;
- emits source RBF, reversed RBF, and ZIP SHA-256 evidence.

Generated files live below ignored `out/package`. The official build ID embeds compile time, and ZIP metadata also carries timestamps, so package hashes identify an evidence run rather than a reproducible release artifact.

On 2026-08-25, package validation passed for the integrated `0.0.0-m0.2` 794,412-byte RBF:

- source RBF SHA-256: `E2E1112EB24AA46B7E003ABDF89482C54101D2F16F25E8DEF47D5E0C2F5177A9`;
- reversed RBF_R SHA-256: `B275C5547A6E3BF234B55BC9037C1C312D809CA534D3C4E1024BD664307E5FAA`;
- evidence ZIP SHA-256: `8476E8FE62D330C0EF724B369288328388CC3A38ECE0079C5B349ABE2C0D95CA`.

The ZIP hash is expected to change when regenerated; the source/reversed pair identifies the byte-level conversion that passed the exact round-trip check.

## 4. Pocket Interact experiment

The package uses APF's generic Core Settings UI only as a hardware-spike adapter. It does not establish RPCMP's replaceable player UI.

After a successful boot on Pocket:

1. open Core Settings and confirm `Seq`, `Count`, `Last`, `ESeq`, and `EVal` initially read hexadecimal value `0x00000000`;
2. select `Run 1` once and confirm `Seq`, `Last`, and `ESeq` read `0x00000001`, while `Count` and `EVal` read `0x000003E8`;
3. select `Run 1` again and confirm every value remains unchanged;
4. select `Run 2` and confirm the three sequence/ID readouts are `0x00000002` and the two value readouts are `0x000007D0`.

`number_u32` is a hexadecimal readout in APF. Thus decimal 1000 is `0x000003E8`, and decimal 2000 is `0x000007D0`; the firmware may omit the prefix or leading zeroes when rendering them. Every `0.0.0-m0.2` label is at most five characters. `Run 1` and `Run 2` use separate write-only register addresses, and each action atomically supplies its command ID and advances the counter in one APF write.

On 2026-08-25, package version `0.0.0-m0` was installed and executed on Pocket and the following partial result was reported:

- the first `Advance 1000` with command ID 1 made the visible value 1000;
- after exiting and relaunching the core, the first action again made the value 1000;
- repeating the action without changing command ID 1 caused no change.

This confirms that the packaged core reaches its Interact control, the integrated BRIDGE path can update observable state, relaunch returns the spike to its initial command state, and duplicate command ID 1 is rejected.

Version `0.0.0-m0.1` was then rejected by on-device testing: every readout label except `Counter` was truncated, and the separate `Set ID`/`Advance` actions produced no apparent changes in the displayed values. Because most values were obscured, that observation does not isolate whether the failure was write ordering or display refresh. Version `0.0.0-m0.2` removes both uncertainties with five-character labels and one-write `Run` actions; it requires a new bitstream and remains unverified on Pocket.

## 5. Remaining release gaps

- Pocket package `0.0.0-m0` launch and the command-ID-1 Interact path are partially verified as recorded above. The revised `0.0.0-m0.2` atomic controls, continuous heartbeat survival, command ID 2, and the complete snapshot/event readouts remain unverified.
- The package contains the official template's gray video and silence audio, not an RPCMP UI or player.
- Timing remains incompletely constrained as recorded in `pocket-template-integration.md`.
- There are no data slots, `.rpcmlib` assets, save files, platform metadata, or production input mapping.
- No release archive or generated FPGA artifact is tracked by Git.
