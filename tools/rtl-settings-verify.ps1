[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
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
$output = Join-Path $repo 'out/sim/settings'
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    if (-not (Test-Path work)) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed' } }
    & $vlog -quiet -sv -work work (Join-Path $repo 'core/rtl/pocket/rpcmp_settings_ram.sv') `
        (Join-Path $repo 'tests/rtl/settings_ram_tb.sv')
    if ($LASTEXITCODE) { throw 'Settings RAM compilation failed' }
    $lines = @(& $vsim -c -quiet -L altera_mf_ver -lib work settings_ram_tb -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $log = $lines | Out-String
    if ($code -ne 0 -or $log -notmatch '(?m)^# settings_ram_tb: PASS commands=\d+\r?$' -or
        $log -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Settings RAM simulation failed' }
} finally { Pop-Location }
