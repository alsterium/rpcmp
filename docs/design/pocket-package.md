# Pocket M0 Package

## 1. Purpose and status

This document defines the installable APF package for the M0 register-boundary target experiment. It is deliberately identified as version `0.0.0-m0.4` with description `M0 register-boundary target spike.` It is not an RPCMP music-player release and does not supersede ADR-0004.

The definitions in `core/platform/pocket/apf` are project-owned rather than copied from the official template. They follow the current official definition roots and `APF_VER_1` magic while retaining the template's proven 320x240 video and framework requirement 1.1.

## 2. Package shape

The generated ZIP contains only this SD tree:

```text
Cores/
  alsterium.RPCMP/
    audio.json
    core.json
    data.json
    input.json
    interact.json
    m004.rbf_r
    variants.json
    video.json
```

The spike has an empty `platform_ids` list, so it does not add `Platforms`, `Assets`, or `Saves`. The optional core icon is also omitted. The ZIP filename is `alsterium.RPCMP_0.0.0-m0.4_2026-08-26.zip` and has no extra wrapper directory.

The package uses a version-specific bitstream filename. Version m0.4 points to `m004.rbf_r`, so an older same-named artifact cannot be silently selected from the core directory or a filename-based cache.

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

On 2026-08-26, package validation passed for the integrated `0.0.0-m0.4` 798,412-byte RBF:

- source RBF SHA-256: `BBBC7C81313101D76E223D61BA4E9849A34FA91A007C811CA9A18E6814C11F5E`;
- reversed RBF_R SHA-256: `61CF85973FBC9BF201A9196DEFD732319E5CCBB51A83655EA2A328F05691D761`;
- evidence ZIP SHA-256: `34CE0DCA58A4B2B2F22491D54DBDA737699E5CBF8B6967EBA0E90AFCB3642E19`.

The ZIP hash is expected to change when regenerated; the source/reversed pair identifies the byte-level conversion that passed the exact round-trip check.

## 4. Pocket Interact experiment

The package uses APF's generic Core Settings UI only as a hardware-spike adapter. It does not establish RPCMP's replaceable player UI.

After a successful boot on Pocket:

1. open Core Settings and confirm `Build` reads `0x4D303034`; this is the m0.4 bitstream signature;
2. confirm `Seq`, `Count`, `Last`, `ESeq`, `EVal`, `WCnt`, `WAdr`, and `WDat` initially read hexadecimal value `0x00000000`;
3. select `Run 1` once and confirm `WCnt=1`, `WAdr=0x00F00010`, and `WDat=0x00000040`;
4. confirm `Seq`, `Last`, and `ESeq` read `1`, while `Count` and `EVal` read `0x000003E8`;
5. select `Run 1` again and confirm `WCnt=2` while the command/event values remain unchanged because command ID 1 is a duplicate;
6. select `Run 2` and confirm `WCnt=3`, `WAdr=0x00F00018`, and `WDat=0`, then confirm the three sequence/ID readouts are `2` and the two value readouts are `0x000007D0`.

`number_u32` is a hexadecimal readout in APF. Thus decimal 1000 is `0x000003E8`, and decimal 2000 is `0x000007D0`; the firmware may omit the prefix or leading zeroes when rendering them. Every `0.0.0-m0.4` label is at most five characters. `Run 1` and `Run 2` mirror the official Interact sample's action addresses and values; the RTL accepts their address write strobes regardless of data. The write diagnostics make the APF transaction visible even if the command is later rejected as duplicate or stale.

On 2026-08-25, package version `0.0.0-m0` was installed and executed on Pocket and the following partial result was reported:

- the first `Advance 1000` with command ID 1 made the visible value 1000;
- after exiting and relaunching the core, the first action again made the value 1000;
- repeating the action without changing command ID 1 caused no change.

This confirms that the packaged core reaches its Interact control, the integrated BRIDGE path can update observable state, relaunch returns the spike to its initial command state, and duplicate command ID 1 is rejected.

Version `0.0.0-m0.1` was then rejected by on-device testing: every readout label except `Counter` was truncated, and the separate `Set ID`/`Advance` actions produced no apparent changes in the displayed values. Because most values were obscured, that observation does not isolate whether the failure was write ordering or display refresh.

Version `0.0.0-m0.2` used five-character labels and one-write `Run` actions. On-device testing confirmed that selecting and confirming both actions left `Seq`, `Count`, and the other readouts unchanged, so m0.2 is rejected. Its project-specific addresses and strict `bridge_wr_data == 1` comparison deviated from the official action example and provided no transaction-level observation.

Version `0.0.0-m0.3` mirrored two official action entries and added the build/write diagnostics above. On-device testing displayed the new `Build` entry but read `0`, whereas the m0.3 RTL returns `0x4D303033` independently of mutable state. Version `0.0.0-m0.3.1` then forced selection through unique filename `m003.rbf_r`, but `Build` still read zero. That result rejects the mixed-install hypothesis.

Inspection of the pinned official `io_bridge_peripheral` established the direct cause: APF buffers `bridge_rd_data` first and pulses `bridge_rd` afterward, while the m0.3 RTL emitted nonzero data only while `bridge_rd` was asserted. Version `0.0.0-m0.4` follows the official core pattern by decoding read data continuously from `bridge_addr`, and its RTL test explicitly samples every read once before the strobe and once during it.

On 2026-08-26, the m0.4 package was installed and exercised on Pocket. `Build` read `0x4D303034`, confirming that the uniquely named m0.4 bitstream was active. A direct `Run 1` then `Run 2` sequence produced:

| Action | `Seq` | `Count` | `Last` | `WCnt` | `WAdr` | `WDat` | `ESeq` | `EVal` |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `Run 1` | `0x1` | `0x3E8` | `0x1` | `0x1` | `0x00F00010` | `0x40` | `0x1` | `0x3E8` |
| `Run 2` | `0x2` | `0x7D0` | `0x2` | `0x2` | `0x00F00018` | `0x0` | `0x2` | `0x7D0` |

This verifies on Pocket that the corrected pre-strobe read path exposes the expected m0.4 signature, both official-pattern writes reach their exact addresses and data values, command IDs 1 and 2 are accepted in order, and the snapshot-equivalent and fake-event readouts advance atomically to 1,000 and 2,000. The m0.4 run did not repeat `Run 1`, so duplicate rejection for this exact package remains covered by RTL simulation rather than this hardware observation.

## 5. Remaining release gaps

- Pocket package `0.0.0-m0` supplied the initial partial command-ID-1 evidence. Versions m0.1 and m0.2 are rejected, and m0.3 and m0.3.1 exposed the BRIDGE read-timing defect. The revised `0.0.0-m0.4` package, corrected read path, official-pattern controls and diagnostics, command IDs 1 and 2, and complete snapshot-equivalent/event readouts are verified as recorded above. Duplicate rejection on m0.4 and extended continuous-heartbeat observation remain unverified on hardware.
- The package contains the official template's gray video and silence audio, not an RPCMP UI or player.
- Timing remains incompletely constrained as recorded in `pocket-template-integration.md`.
- There are no data slots, `.rpcmlib` assets, save files, platform metadata, or production input mapping.
- No release archive or generated FPGA artifact is tracked by Git.
