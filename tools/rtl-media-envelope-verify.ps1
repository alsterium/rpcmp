[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$questa = if ($env:RPCMP_QUESTA_ROOT) { $env:RPCMP_QUESTA_ROOT } else { 'C:/altera_lite/25.1std/questa_fse/win64' }
$vlib = Join-Path $questa 'vlib.exe'
$vlog = Join-Path $questa 'vlog.exe'
$vsim = Join-Path $questa 'vsim.exe'
$version = (& $vsim -version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $version -notmatch '2025\.2') { throw 'Questa 2025.2 is required.' }
$license = $env:SALT_LICENSE_SERVER
if (-not $license) { $license = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER','User') }
if (-not $license) { $license = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER','Machine') }
if (-not $license) { throw 'SALT_LICENSE_SERVER must reference a valid Questa Starter license.' }
$env:SALT_LICENSE_SERVER = $license
$output = Join-Path $root 'out/sim/media-envelope'
New-Item -ItemType Directory -Force $output | Out-Null
Push-Location $output
try {
    if (-not (Test-Path 'work')) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    & $vlog -quiet -sv -work work (Join-Path $root 'core/rtl/pocket/rpcmp_media_envelope.sv') (Join-Path $root 'tests/rtl/media_envelope_tb.sv')
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    $lines = @(& $vsim -c -quiet -lib work media_envelope_tb -do 'run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $text = $lines | Out-String
    if ($code -ne 0 -or $text -notmatch '(?m)^# media_envelope_tb: PASS .+restore=960\r?$' -or
        $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Media envelope simulation failed.' }
} finally { Pop-Location }
