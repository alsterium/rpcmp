[CmdletBinding()]
param([string]$Jt51 = 'out/research/jt51-985a573', [int[]]$PhasePs = @(0, 5555, 81379),
      [switch]$VoiceOnly, [string]$PreparedTree)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$native = (Resolve-Path -LiteralPath $Jt51).Path
$output = Join-Path $repo 'out/sim/hybrid'
if ($PreparedTree) { $output = Join-Path $repo 'out/sim/hybrid-axi' }
if ($PreparedTree -and $VoiceOnly) { throw 'VoiceOnly is for the standalone transport' }
$questa = if ($env:RPCMP_QUESTA_ROOT) { $env:RPCMP_QUESTA_ROOT } else { 'C:/altera_lite/25.1std/questa_fse/win64' }
$vlib = Join-Path $questa 'vlib.exe'
$vlog = Join-Path $questa 'vlog.exe'
$vsim = Join-Path $questa 'vsim.exe'
$version = (& $vsim -version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $version -notmatch '2025\.2') { throw 'Expected Questa 2025.2' }
if (-not $env:SALT_LICENSE_SERVER) {
    $env:SALT_LICENSE_SERVER = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER', 'User')
}
if (-not $env:SALT_LICENSE_SERVER) { throw 'SALT_LICENSE_SERVER required' }
if ($PhasePs.Count -eq 0) { throw 'At least one clock phase is required' }
foreach ($phase in $PhasePs) {
    if ($phase -lt 0 -or $phase -ge 81380) { throw 'Clock phase must be in [0, 81380) ps' }
}
$prepare = @'
import sys
sys.path.insert(0, sys.argv[1])
from mdx_hybrid_probe import prepare_jt51_paused
for path in prepare_jt51_paused(sys.argv[2], sys.argv[3]):
    print(path)
'@
$sources = @(& python -B -c $prepare $PSScriptRoot $native (Join-Path $output 'jt51-wide'))
if ($LASTEXITCODE) { throw 'Pinned JT51 preparation failed' }
$sources += @('rpcmp_hybrid_mixer','rpcmp_hybrid_audio','rpcmp_hybrid_mmio') |
    ForEach-Object { Join-Path $repo "core/rtl/pocket/$_.sv" }
$defines = @()
if ($PreparedTree) {
    $tree = (Resolve-Path -LiteralPath $PreparedTree).Path
    & python -B (Join-Path $repo 'tools/player_sound_fixture.py') --tree $tree --output $output --hybrid
    if ($LASTEXITCODE) { throw 'HYB1 binding extraction failed' }
    $sources += @('src/fpga/targets/pocket/apf/common.v','src/fpga/common/axi_periph_slave.v',
                  'src/fpga/common/sync_fifo.v','src/fpga/targets/pocket/snac_shifter.v') |
        ForEach-Object { Join-Path $tree $_ }
    $sources += @('apf_unused_psx.sv','hybrid_axi_tb.sv') | ForEach-Object { Join-Path $repo "tests/rtl/$_" }
    $defines = @('+define+INCLUDE_RPCMP_JT51','+define+INCLUDE_RPCMP_PLAYER','+define+EXCLUDE_GPU',"+incdir+$output")
    # AXI peripheral fixture does not execute the unrelated CPU ROM.
    @'
WIDTH=32;
DEPTH=8192;
ADDRESS_RADIX=HEX;
DATA_RADIX=HEX;
CONTENT BEGIN
[0000..1FFF] : 00000000;
END;
'@ | Set-Content (Join-Path $output 'firmware.mif')
} else {
    $sources += @('hybrid_mixer_tb','hybrid_stream_tb') | ForEach-Object { Join-Path $repo "tests/rtl/$_.sv" }
}
Push-Location $output
try {
    if (-not (Test-Path work)) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed' } }
    & $vlog -quiet -sv -work work @defines @sources
    if ($LASTEXITCODE) { throw 'Hybrid compilation failed' }
    $runs = @(@{ Name='hybrid_mixer_tb'; Arguments=@(); Marker='hybrid_mixer_tb: PASS samples=8' })
    if ($PreparedTree) { $runs = @() }
    foreach ($phase in $PhasePs) {
        if ($PreparedTree) {
            $runs += @{ Name='hybrid_axi_tb'; Arguments=@("+PHASE_PS=$phase"); Marker="hybrid_axi_tb: PASS phase=$phase " }
            continue
        }
        if (!$VoiceOnly) {
            $runs += @{ Name='hybrid_stream_tb'; Arguments=@("+PHASE_PS=$phase"); Marker="hybrid_stream_tb: PASS phase=$phase " }
            $runs += @{ Name='hybrid_stream_tb'; Arguments=@("+PHASE_PS=$phase",'+PAUSE'); Marker="hybrid_stream_tb: PAUSE PASS phase=$phase " }
        }
        $runs += @{ Name='hybrid_stream_tb'; Arguments=@("+PHASE_PS=$phase",'+VOICE'); Marker="hybrid_stream_tb: VOICE PASS phase=$phase " }
    }
    foreach ($run in $runs) {
        $extra = $run.Arguments
        $lines = @(& $vsim -c -quiet -L altera_mf_ver -lib work $run.Name @extra -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $log = $lines | Out-String
        if ($code -ne 0 -or !$log.Contains($run.Marker) -or $log -match '(?im)^# (ERROR:|\*\* (Error|Fatal):)' -or
            $log -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "Hybrid simulation failed: $($run.Name) $extra" }
    }
} finally { Pop-Location }
