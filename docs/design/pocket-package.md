# Pocket M0 Package

## 1. Purpose and status

This document defines the installable APF package for the M0 register-boundary target experiment. It is deliberately identified as version `0.0.0-m0` with description `M0 register-boundary target spike.` It is not an RPCMP music-player release and does not supersede ADR-0004.

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

The spike has an empty `platform_ids` list, so it does not add `Platforms`, `Assets`, or `Saves`. The optional core icon is also omitted. The ZIP filename is `alsterium.RPCMP_0.0.0-m0_2026-08-25.zip` and has no extra wrapper directory.

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

On 2026-08-25, package validation passed for the integrated 794,760-byte RBF:

- source RBF SHA-256: `CAEB5E867AD27E416227BB4967EB0EDEED9D0AADA8B942FB416A2490BF527DEB`;
- reversed RBF_R SHA-256: `E3918139E8D614378C16DE1442B40CBC7C5CA1E6AC8A682B9C93F02B78617B0F`;
- evidence ZIP SHA-256: `0AF4E57814F13C47C22DB6380BB36E37CA237B436F94E3791546D37DDD67D7E6`.

The ZIP hash is expected to change when regenerated; the source/reversed pair identifies the byte-level conversion that passed the exact round-trip check.

## 4. Pocket Interact experiment

The package uses APF's generic Core Settings UI only as a hardware-spike adapter. It does not establish RPCMP's replaceable player UI.

After a successful boot on Pocket:

1. open Core Settings and confirm snapshot sequence, counter, last command ID, event sequence, and event value initially read zero;
2. leave `Command ID` at 1 and select `Advance 1000`;
3. confirm snapshot sequence, last command ID, and event sequence read 1, while counter/event value read 1000;
4. select `Advance 1000` again without changing the ID and confirm the values do not change;
5. change `Command ID` to 2, select the action, and confirm sequences read 2 and values read 2000.

On 2026-08-25, the package was installed and executed on Pocket and the following partial result was reported:

- the first `Advance 1000` with command ID 1 made the visible value 1000;
- after exiting and relaunching the core, the first action again made the value 1000;
- repeating the action without changing command ID 1 caused no change.

This confirms that the packaged core reaches its Interact control, the integrated BRIDGE path can update observable state, relaunch returns the spike to its initial command state, and duplicate command ID 1 is rejected. The command-ID-2 case and the exact snapshot, last-command, event-sequence, and event-value readouts have not yet been reported, so the complete experiment remains open.

## 5. Remaining release gaps

- Pocket package launch and the command-ID-1 Interact path are partially verified as recorded above. Continuous heartbeat survival, command ID 2, and the complete snapshot/event readouts remain unverified.
- The package contains the official template's gray video and silence audio, not an RPCMP UI or player.
- Timing remains incompletely constrained as recorded in `pocket-template-integration.md`.
- There are no data slots, `.rpcmlib` assets, save files, platform metadata, or production input mapping.
- No release archive or generated FPGA artifact is tracked by Git.
