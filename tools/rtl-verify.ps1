[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$questaCandidates = @()

if ($env:RPCMP_QUESTA_ROOT) {
    $questaCandidates += $env:RPCMP_QUESTA_ROOT
}

$questaCandidates += 'C:\altera_lite\25.1std\questa_fse\win64'
$questaRoot = $questaCandidates |
    Where-Object { Test-Path -LiteralPath (Join-Path $_ 'vsim.exe') } |
    Select-Object -First 1

if (-not $questaRoot) {
    throw 'Questa was not found. Set RPCMP_QUESTA_ROOT to the directory containing vsim.exe.'
}

$vlib = Join-Path $questaRoot 'vlib.exe'
$vlog = Join-Path $questaRoot 'vlog.exe'
$vsim = Join-Path $questaRoot 'vsim.exe'
$licenseServer = $env:SALT_LICENSE_SERVER
if (-not $licenseServer) {
    $licenseServer = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER', 'User')
}
if (-not $licenseServer) {
    $licenseServer = [Environment]::GetEnvironmentVariable('SALT_LICENSE_SERVER', 'Machine')
}
if ($licenseServer) {
    $env:SALT_LICENSE_SERVER = $licenseServer
}
$versionText = (& $vsim -version 2>&1 | Out-String)

if ($LASTEXITCODE -ne 0) {
    throw "Unable to query Questa version:`n$versionText"
}

if ($versionText -notmatch '2025\.2') {
    throw "RPCMP M0 pins Questa Altera Starter 2025.2, but found:`n$versionText"
}

$outputDirectory = Join-Path $repositoryRoot 'out\sim\pocket-spike'
$workLibrary = Join-Path $outputDirectory 'work'
$rtlSources = @((Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_spike_regs.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_device_queue.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_sound_reset.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_pocket_audio.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_jt51_audio.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_openfpgaos_sound.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_m2_fixed_core.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_m4_mdx_core.sv'), (Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_stereo_probe_core.sv'))
$testSources = @((Join-Path $repositoryRoot 'tests\rtl\pocket_spike_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\device_queue_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\sound_reset_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\pocket_audio_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\jt51_model.sv'), (Join-Path $repositoryRoot 'tests\rtl\jt51_audio_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\openfpgaos_sound_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\m2_fixed_core_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\m4_mdx_core_tb.sv'), (Join-Path $repositoryRoot 'tests\rtl\stereo_probe_core_tb.sv'))

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
Push-Location $outputDirectory

try {
    if (-not (Test-Path -LiteralPath $workLibrary)) {
        & $vlib work
        if ($LASTEXITCODE -ne 0) {
            throw 'vlib failed.'
        }
    }

    & $vlog -quiet -sv -work work $rtlSources $testSources
    if ($LASTEXITCODE -ne 0) {
        throw 'vlog failed.'
    }

    if (-not $licenseServer) {
        throw 'RTL compilation passed, but simulation requires SALT_LICENSE_SERVER to reference a valid Questa Starter license.'
    }

    foreach ($simulation in @(@{Top='pocket_spike_tb'; Marker='pocket_spike_tb: PASS'}, @{Top='device_queue_tb'; Marker='device_queue_tb: PASS'}, @{Top='sound_reset_tb'; Marker='sound_reset_tb: PASS'}, @{Top='pocket_audio_tb'; Marker='pocket_audio_tb: PASS'}, @{Top='jt51_audio_tb'; Marker='jt51_audio_tb: PASS'}, @{Top='openfpgaos_sound_tb'; Marker='openfpgaos_sound_tb: PASS'}, @{Top='m2_fixed_core_tb'; Marker='m2_fixed_core_tb: PASS'}, @{Top='m4_mdx_core_tb'; Marker='m4_mdx_core_tb: PASS'}, @{Top='stereo_probe_core_tb'; Marker='stereo_probe_core_tb: PASS'})) {
        $simulationOutput = @(& $vsim -c -quiet -lib work $simulation.Top -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1)
        $simulationExitCode = $LASTEXITCODE
        $simulationOutput | Write-Output
        if ($simulationExitCode -ne 0) { throw "vsim failed for $($simulation.Top)." }
        $simulationText = $simulationOutput | Out-String
        if ($simulationText -notmatch "(?m)^# $([regex]::Escape($simulation.Marker)).*\r?$") { throw "missing PASS for $($simulation.Top)." }
        if ($simulationText -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "non-clean summary for $($simulation.Top)." }
    }
} finally {
    Pop-Location
}
