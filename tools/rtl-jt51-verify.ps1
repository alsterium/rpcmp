[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$jt51 = Join-Path $root 'out\research\jt51-985a573'
$expected = '985a573dcfc1ff135553a39f7eae21d18ba57cbe'
$actual = (& git -C $jt51 rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actual -ne $expected) { throw "JT51 revision mismatch: $actual" }
if ((& git -C $jt51 status --porcelain | Out-String).Trim()) { throw 'JT51 checkout has local changes.' }

$questa = if ($env:RPCMP_QUESTA_ROOT) { $env:RPCMP_QUESTA_ROOT } else { 'C:\altera_lite\25.1std\questa_fse\win64' }
$vlib = Join-Path $questa 'vlib.exe'; $vlog = Join-Path $questa 'vlog.exe'; $vsim = Join-Path $questa 'vsim.exe'
$version = (& $vsim -version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $version -notmatch '2025\.2') { throw "Expected Questa 2025.2:`n$version" }
$license = $env:SALT_LICENSE_SERVER
if (-not $license) { $license = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER','User') }
if (-not $license) { $license = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER','Machine') }
if (-not $license) { throw 'SALT_LICENSE_SERVER must reference a valid Questa Starter license.' }
$env:SALT_LICENSE_SERVER = $license

$output = Join-Path $root 'out\sim\jt51-real'; $work = Join-Path $output 'work'
New-Item -ItemType Directory -Force $output | Out-Null
$sources = Get-Content (Join-Path $jt51 'hdl\jt51.f') | ForEach-Object { Join-Path $jt51 "hdl\$_" }
Push-Location $output
try {
    if (-not (Test-Path $work)) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    & $vlog -quiet -sv -work work $sources `
        (Join-Path $root 'core\rtl\pocket\rpcmp_pocket_audio.sv') `
        (Join-Path $root 'core\rtl\pocket\rpcmp_jt51_audio.sv') `
        (Join-Path $root 'core\rtl\pocket\rpcmp_device_queue.sv') `
        (Join-Path $root 'core\rtl\pocket\rpcmp_m2_fixed_core.sv') `
        (Join-Path $root 'core\rtl\pocket\rpcmp_m4_mdx_core.sv') `
        (Join-Path $root 'tests\rtl\jt51_audio_tb.sv') `
        (Join-Path $root 'tests\rtl\m2_fixed_core_tb.sv') `
        (Join-Path $root 'tests\rtl\m4_mdx_core_tb.sv')
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    foreach($simulation in @(@{Top='jt51_audio_tb';Marker='jt51_audio_tb: PASS writes=30'},@{Top='m2_fixed_core_tb';Marker='m2_fixed_core_tb: PASS writes=30'},@{Top='m4_mdx_core_tb';Marker='m4_mdx_core_tb: PASS writes=66'})) {
        $lines = @(& $vsim -c -quiet -lib work $simulation.Top -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE; $lines | Write-Output; $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch "(?m)^# $([regex]::Escape($simulation.Marker)).*\r?$" -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "Real JT51 simulation failed: $($simulation.Top)" }
    }
} finally { Pop-Location }
