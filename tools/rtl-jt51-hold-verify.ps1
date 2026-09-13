[CmdletBinding()]
param([switch]$CenOnly)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$jt51 = Join-Path $root 'out/research/jt51-985a573'
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
$output = Join-Path $root 'out/sim/jt51-hold'
$generated = Join-Path $output ([guid]::NewGuid().ToString('N'))
& python -B (Join-Path $root 'tools/jt51_hold_prepare.py') --jt51 $jt51 --output $generated
if ($LASTEXITCODE -ne 0) { throw 'JT51 hold generation failed.' }
$manifest = Get-Content (Join-Path $generated 'manifest.json') -Raw | ConvertFrom-Json
$sources = @($manifest.files | ForEach-Object { Join-Path $jt51 "hdl/$($_.file)" })
$sources += @($manifest.files | ForEach-Object { Join-Path $generated $_.file })
$sources += Join-Path $root 'tests/rtl/jt51_hold_tb.sv'
Push-Location $output
try {
    if (-not (Test-Path 'work')) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    $compileArgs = @('-quiet','-sv','-work','work')
    if ($CenOnly) { $compileArgs += '+define+RPCMP_HOLD_CEN_ONLY' }
    & $vlog @compileArgs @sources
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    $lines = @(& $vsim -c -quiet -lib work jt51_hold_tb -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $text = $lines | Out-String
    if ($CenOnly) {
        if ($text -notmatch '(?m)^# \*\* Fatal: MMR changed while held at media=2049 hold=1\r?$' -or
            $text -notmatch '(?m)^# Errors: 1, Warnings: 0\r?$' -or $text -match 'jt51_hold_tb: PASS') {
            throw "cen-only negative control did not reproduce the hold failure (exit $code)."
        }
        Write-Output 'jt51_hold negative control: expected MMR hold failure reproduced'
    } elseif ($code -ne 0 -or $text -notmatch '(?m)^# jt51_hold_tb: PASS .*phases=256\r?$' -or
              $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') {
        throw 'JT51 full-state hold simulation failed.'
    }
} finally { Pop-Location }
