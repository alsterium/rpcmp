[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PreparedTree,
      [int[]]$PhasePs = @(0, 173, 5555, 11110))
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tree = (Resolve-Path -LiteralPath $PreparedTree).Path
$pocket = Join-Path $tree 'src/fpga/targets/pocket'
$common = Join-Path $tree 'src/fpga/common'
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
$output = Join-Path $repo 'out/sim/apf-flush'
& python -B (Join-Path $repo 'tools/apf_flush_fixture.py') --tree $tree --output $output
if ($LASTEXITCODE) { throw 'Fixture generation failed' }
# The CPU is driven by AXI transactions, not boot code. Initialize the unrelated
# peripheral BRAM explicitly so vendor simulation has no missing-ROM warning.
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
    $sources = @((Join-Path $pocket 'apf/common.v'), (Join-Path $pocket 'core_bridge_cmd.v'),
                 (Join-Path $common 'axi_periph_slave.v'), (Join-Path $common 'sync_fifo.v'),
                 (Join-Path $pocket 'snac_shifter.v'),
                 (Join-Path $repo 'tests/rtl/apf_unused_psx.sv'),
                 (Join-Path $repo 'tests/rtl/apf_lifecycle_tb.sv'),
                 (Join-Path $repo 'tests/rtl/apf_flush_tb.sv'))
    foreach ($source in $sources) {
        & $vlog -quiet -work work +define+RPCMP_APF_FLUSH +define+INCLUDE_RPCMP_JT51 "+incdir+$output" $source
        if ($LASTEXITCODE) { throw "APF flush compilation failed: $source" }
    }
    foreach ($phase in $PhasePs) {
        $lines = @(& $vsim -c -quiet -L altera_mf_ver -lib work apf_flush_tb "-gPHASE_PS=$phase" -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $log = $lines | Out-String
        if ($code -ne 0 -or $log -notmatch "(?m)^# apf_flush_tb: PASS phase=$phase\r?$" -or
            $log -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "APF flush simulation failed at phase $phase" }
    }
    $lines = @(& $vsim -c -quiet -L altera_mf_ver -lib work apf_flush_tb -gFLUSH_ENABLE=0 -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $log = $lines | Out-String
    if ($code -ne 0 -or $log -notmatch '(?m)^# apf_flush_tb: PASS disabled\r?$' -or
        $log -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Disabled capability simulation failed' }
} finally { Pop-Location }
