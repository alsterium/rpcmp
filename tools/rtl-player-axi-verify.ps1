[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PreparedTree, [int[]]$PhasePs = @(0, 5555, 81379))
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tree = (Resolve-Path -LiteralPath $PreparedTree).Path
$output = Join-Path $repo 'out/sim/player-axi'
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
& python -B (Join-Path $repo 'tools/player_sound_fixture.py') --tree $tree --output $output
if ($LASTEXITCODE) { throw 'Fixture extraction failed' }
$generated = Join-Path $tree 'rpcmp-jt51-hold'
$manifest = Get-Content (Join-Path $generated 'manifest.json') -Raw | ConvertFrom-Json
$sources = @($manifest.files | ForEach-Object { Join-Path $generated $_.file })
$sources += @('rpcmp_media_output', 'rpcmp_jt51_media_source', 'rpcmp_media_envelope',
    'rpcmp_native_completion', 'rpcmp_media_source_queue', 'rpcmp_jt51_progress_audio',
    'rpcmp_sound_session', 'rpcmp_sound_mailbox', 'rpcmp_sound_mmio', 'rpcmp_output_journal') |
    ForEach-Object { Join-Path $repo "core/rtl/pocket/$_.sv" }
$sources += @((Join-Path $tree 'src/fpga/targets/pocket/apf/common.v'),
    (Join-Path $tree 'src/fpga/common/axi_periph_slave.v'), (Join-Path $tree 'src/fpga/common/sync_fifo.v'),
    (Join-Path $tree 'src/fpga/targets/pocket/snac_shifter.v'),
    (Join-Path $repo 'tests/rtl/apf_unused_psx.sv'), (Join-Path $repo 'tests/rtl/player_axi_tb.sv'))
# The fixture drives AXI directly; unrelated CPU ROM is initialized explicitly.
@'
WIDTH=32;
DEPTH=8192;
ADDRESS_RADIX=HEX;
DATA_RADIX=HEX;
CONTENT BEGIN
[0000..1FFF] : 00000000;
END;
'@ | Set-Content (Join-Path $output 'firmware.mif')
Push-Location $output
try {
    if (-not (Test-Path work)) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed' } }
    & $vlog -quiet -sv -work work +define+INCLUDE_RPCMP_JT51 +define+INCLUDE_RPCMP_PLAYER +define+EXCLUDE_GPU "+incdir+$output" @sources
    if ($LASTEXITCODE) { throw 'Player AXI compilation failed' }
    foreach ($phase in $PhasePs) {
        $lines = @(& $vsim -c -quiet -L altera_mf_ver -lib work player_axi_tb "+PHASE_PS=$phase" -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $log = $lines | Out-String
        if ($code -ne 0 -or $log -notmatch "(?m)^# player_axi_tb: PASS phase=$phase reads=\d+ writes=\d+\r?$" -or
            $log -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "Player AXI simulation failed at phase $phase" }
    }
} finally { Pop-Location }
