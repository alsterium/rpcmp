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
$rtlSource = Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_spike_regs.sv'
$testSource = Join-Path $repositoryRoot 'tests\rtl\pocket_spike_tb.sv'

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
Push-Location $outputDirectory

try {
    if (-not (Test-Path -LiteralPath $workLibrary)) {
        & $vlib work
        if ($LASTEXITCODE -ne 0) {
            throw 'vlib failed.'
        }
    }

    & $vlog -quiet -sv -work work $rtlSource $testSource
    if ($LASTEXITCODE -ne 0) {
        throw 'vlog failed.'
    }

    if (-not $licenseServer) {
        throw 'RTL compilation passed, but simulation requires SALT_LICENSE_SERVER to reference a valid Questa Starter license.'
    }

    $simulationOutput = @(
        & $vsim -c -quiet -lib work pocket_spike_tb -do 'onerror {quit -code 1}; run -all; quit -code 0' 2>&1
    )
    $simulationExitCode = $LASTEXITCODE
    $simulationOutput | Write-Output
    if ($simulationExitCode -ne 0) {
        throw 'vsim failed.'
    }
    $simulationText = $simulationOutput | Out-String
    if ($simulationText -notmatch '(?m)^# pocket_spike_tb: PASS\r?$') {
        throw 'vsim completed without the pocket_spike_tb PASS marker.'
    }
    if ($simulationText -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') {
        throw 'vsim completed without the required zero-error, zero-warning summary.'
    }
} finally {
    Pop-Location
}
