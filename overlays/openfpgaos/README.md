# M5 audio boot-ROM repair build

This is the experimental M5 integration profile governed by ADR-0008.
The OS and boot ROM use the hardware-accepted safe-memset pair. The integrated
bitstream still requires hardware acceptance and the ADR's promotion gates.

Run from the repository root in PowerShell. Preparation requires a new output
directory and clean pinned upstream/JT51 checkouts. The boot MIF is a local
build artifact; both its file hash and decoded instruction-byte hash are
checked against the documented safe control. Never substitute the latest
research `boot.bin` merely because the filename matches.

```powershell
python -B tools/pocket_m5_audio_prepare.py --repo F:/source/rpcmp --openfpgaos out/research/openfpgaCore-618a3eb --jt51 out/research/jt51-985a573 --vexii-netlist out/research/openfpgaCore-618a3eb-lf/src/fpga/vendor/vexriscv/VexiiRiscv/VexiiRiscv_rpcmp.v --boot-mif out/research/openfpgaCore-618a3eb-lf/src/fpga/targets/pocket/bld/rpcmp90fcmem/firmware.mif --output out/build/openfpgaos-m5-audio-bootfix
docker run --rm -v F:/source/rpcmp:/workspace/rpcmp -w /workspace/rpcmp/out/build/openfpgaos-m5-audio-bootfix/src/fpga/targets/pocket rpcmp-openfpgaos-toolchain:14.2.0-3 make bld/rpcmp-m5-bootfix/ap_core.qsf VARIANT=rpcmp JOB=rpcmp-m5-bootfix QPROCS=4
```

Convert `/workspace/rpcmp` to `F:/source/rpcmp` in the generated job QSF for
native Windows Quartus. Preserve its `SEARCH_PATH` to the Pocket source
directory: this resolves the RTL's bare `firmware.mif` and `./apf/build_id.mif`
references. From the job directory run Quartus 25.1std.0 Build 1129:

```powershell
& C:/altera_lite/25.1std/quartus/bin64/quartus_sh.exe --flow compile ap_core
```

Require both MIFs in the map report's input-file table, no missing-memory-file
warning, successful compilation, and nonnegative timing before packaging.
The packager additionally pins the reviewed repair RBF; a newly differing
RBF must be reviewed before updating that pin. File extraction and rebuilding
are reproducible procedures, not a claim of byte-identical Quartus output.

From the repository root:

```powershell
python -B tools/pocket_m5_audio_package.py --repo F:/source/rpcmp --sdk out/research/openfpgaSDK-a408ddc --elf out/build/pocket-openfpgaos-m5-audio/rpcmp-m5-audio.elf --rbf out/build/openfpgaos-m5-audio-bootfix/src/fpga/targets/pocket/bld/rpcmp-m5-bootfix/output_files/ap_core.rbf --output out/build/pocket-m5-audio-bootfix-package --zip out/build/rpcmp-m5-audio-bootfix.zip
```

The ZIP and adjacent evidence JSON use dedicated repair paths. Copy the ZIP's
contents to the SD root and launch `RPCMP M5 Audio Probe`, version
`0.9.1-m5-audio`. Record the exact last visible text on failure; on success,
record `M5 AUDIO: PASS`, audio observations, a warm relaunch, and a full
power-off cold start. The original `0.8.0-m5-bss` control remains immutable.
