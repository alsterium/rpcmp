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

    if (-not $env:SALT_LICENSE_SERVER) {
        throw 'RTL compilation passed, but simulation requires SALT_LICENSE_SERVER to reference a valid Questa Starter license.'
    }

    & $vsim -c -quiet -lib work pocket_spike_tb -do 'onerror {quit -code 1}; run -all; quit -code 0'
    if ($LASTEXITCODE -ne 0) {
        throw 'vsim failed.'
    }
} finally {
    Pop-Location
}
