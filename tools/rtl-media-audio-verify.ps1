[CmdletBinding()]
param([switch]$OutputOnly)
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
$output = Join-Path $root 'out/sim/media-audio'
New-Item -ItemType Directory -Force $output | Out-Null
$sources = @('core/rtl/pocket/rpcmp_pocket_audio.sv',
             'core/rtl/pocket/rpcmp_media_output.sv',
             'core/rtl/pocket/rpcmp_pocket_media_audio.sv',
             'tests/rtl/pocket_media_audio_tb.sv',
             'tests/rtl/media_sample_position_tb.sv') | ForEach-Object { Join-Path $root $_ }
$tops = @('pocket_media_audio_tb', 'media_sample_position_tb')
$sources += @('core/rtl/pocket/rpcmp_native_completion.sv',
              'tests/rtl/native_completion_tb.sv') | ForEach-Object { Join-Path $root $_ }
$tops += 'native_completion_tb'
$sources += @('core/rtl/pocket/rpcmp_media_source_queue.sv',
              'tests/rtl/media_source_queue_tb.sv') | ForEach-Object { Join-Path $root $_ }
$tops += 'media_source_queue_tb'
if (-not $OutputOnly) {
    $generated = Join-Path $output ([guid]::NewGuid().ToString('N'))
    & python -B (Join-Path $root 'tools/jt51_hold_prepare.py') --jt51 (Join-Path $root 'out/research/jt51-985a573') --output $generated
    if ($LASTEXITCODE) { throw 'JT51 hold generation failed.' }
    $manifest = Get-Content (Join-Path $generated 'manifest.json') -Raw | ConvertFrom-Json
    $sources += @($manifest.files | ForEach-Object { Join-Path $generated $_.file })
    $sources += Join-Path $root 'core/rtl/pocket/rpcmp_jt51_media_source.sv'
    $sources += Join-Path $root 'core/rtl/pocket/rpcmp_jt51_media_audio.sv'
    $sources += Join-Path $root 'tests/rtl/jt51_media_audio_tb.sv'
    $tops += 'jt51_media_audio_tb'
    $sources += Join-Path $root 'tests/rtl/jt51_media_burst_tb.sv'
    $tops += 'jt51_media_burst_tb'
    $sources += Join-Path $root 'tests/rtl/jt51_source_receipt_tb.sv'
    $tops += 'jt51_source_receipt_tb'
    $sources += Join-Path $root 'tests/rtl/jt51_media_phase_tb.sv'
    $tops += 'jt51_media_phase_tb'
    $sources += @('jt51_lfo.v', 'jt51_acc.v', 'jt51_sh.v', 'jt51_lin2exp.v', 'jt51_exp2lin.v') |
        ForEach-Object { Join-Path $root "out/research/jt51-985a573/hdl/$_" }
    $sources += @('tests/rtl/jt51_lfo_pipeline_tb.sv', 'tests/rtl/jt51_acc_latency_tb.sv') |
        ForEach-Object { Join-Path $root $_ }
    $tops += @('jt51_lfo_pipeline_tb', 'jt51_acc_latency_tb')
}
Push-Location $output
try {
    if (-not (Test-Path 'work')) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    & $vlog -quiet -sv -work work @sources
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    foreach ($top in $tops) {
        $lines = @(& $vsim -c -quiet -lib work $top -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch "(?m)^# ${top}: PASS .+\r?$" -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "Media audio simulation failed: $top" }
    }
} finally { Pop-Location }
