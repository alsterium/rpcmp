[CmdletBinding()]
param([switch]$SessionOnly)
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
$output = Join-Path $root 'out/sim/enveloped-audio'
$generated = Join-Path $output ([guid]::NewGuid().ToString('N'))
& python -B (Join-Path $root 'tools/jt51_hold_prepare.py') --jt51 (Join-Path $root 'out/research/jt51-985a573') --output $generated
if ($LASTEXITCODE) { throw 'JT51 hold generation failed.' }
$manifest = Get-Content (Join-Path $generated 'manifest.json') -Raw | ConvertFrom-Json
$sources = @($manifest.files | ForEach-Object { Join-Path $generated $_.file })
$sources += @('core/rtl/pocket/rpcmp_media_output.sv', 'core/rtl/pocket/rpcmp_pocket_media_audio.sv',
              'core/rtl/pocket/rpcmp_jt51_media_source.sv', 'core/rtl/pocket/rpcmp_jt51_media_audio.sv',
              'core/rtl/pocket/rpcmp_media_envelope.sv', 'core/rtl/pocket/rpcmp_jt51_enveloped_audio.sv',
              'core/rtl/pocket/rpcmp_native_completion.sv',
              'core/rtl/pocket/rpcmp_media_source_queue.sv',
              'core/rtl/pocket/rpcmp_jt51_progress_audio.sv', 'tests/rtl/jt51_progress_audio_tb.sv',
              'core/rtl/pocket/rpcmp_sound_session.sv', 'tests/rtl/sound_session_tb.sv',
              'tests/rtl/jt51_enveloped_audio_tb.sv', 'tests/rtl/jt51_source_queue_tb.sv') | ForEach-Object { Join-Path $root $_ }
Push-Location $output
try {
    if (-not (Test-Path 'work')) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    & $vlog -quiet -sv -work work @sources
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    if (-not $SessionOnly) {
        $lines = @(& $vsim -c -quiet -lib work jt51_enveloped_audio_tb -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch '(?m)^# jt51_enveloped_audio_tb: PASS .+positions=3198 token_samples=\d+ token_frames=\d+ multicast=34 restore=960 resets=13\r?$' -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Enveloped audio simulation failed.' }
        $lines = @(& $vsim -c -quiet -lib work jt51_source_queue_tb -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch '(?m)^# jt51_source_queue_tb: PASS writes=8192 receipts=8195 .+ reset_fault=2\r?$' -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Scheduled source integration failed.' }
        $lines = @(& $vsim -c -quiet -lib work jt51_progress_audio_tb -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch '(?m)^# jt51_progress_audio_tb: PASS .+ fault_recovery=1\r?$' -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Audible progress integration failed.' }
    }
    $lines = @(& $vsim -c -quiet -lib work sound_session_tb -do 'run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $text = $lines | Out-String
    if ($code -ne 0 -or $text -notmatch '(?m)^# sound_session_tb: PASS phases=256 reset_max=2050 start_max=259 control_max=257 .+ emergencies=5 nonzero_bits=\d+\r?$' -or
        $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Sound session integration failed.' }
} finally { Pop-Location }
