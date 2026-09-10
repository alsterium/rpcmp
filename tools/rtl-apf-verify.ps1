[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PreparedTree)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tree = (Resolve-Path -LiteralPath $PreparedTree).Path
$pocket = Join-Path $tree 'src/fpga/targets/pocket'
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
$output = Join-Path $repo 'out/sim/apf-lifecycle'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    if (-not (Test-Path work)) {
        & $vlib work
        if ($LASTEXITCODE -ne 0) { throw 'vlib failed' }
    }
    & $vlog -quiet -sv -work work (Join-Path $pocket 'apf/common.v') (Join-Path $pocket 'core_bridge_cmd.v') (Join-Path $repo 'tests/rtl/apf_lifecycle_tb.sv')
    if ($LASTEXITCODE -ne 0) { throw 'APF compilation failed' }
    $result = @(& $vsim -c -quiet -lib work apf_lifecycle_tb -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $result | Write-Output
    $text = $result | Out-String
    if ($code -ne 0 -or $text -notmatch '(?m)^# apf_lifecycle_tb: PASS\r?$' -or
        $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'APF simulation failed' }
} finally { Pop-Location }
