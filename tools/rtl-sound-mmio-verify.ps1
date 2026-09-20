[CmdletBinding()]
param([int[]]$PhasePs = @(0, 173, 5555, 40690, 81379))
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
if ($PhasePs.Count -eq 0) { throw 'At least one clock phase is required.' }
$output = Join-Path $root 'out/sim/sound-mmio'
$generated = Join-Path $output ([guid]::NewGuid().ToString('N'))
& python -B (Join-Path $root 'tools/jt51_hold_prepare.py') --jt51 (Join-Path $root 'out/research/jt51-985a573') --output $generated
if ($LASTEXITCODE) { throw 'JT51 hold generation failed.' }
$manifest = Get-Content (Join-Path $generated 'manifest.json') -Raw | ConvertFrom-Json
$sources = @($manifest.files | ForEach-Object { Join-Path $generated $_.file })
$sources += @('core/rtl/pocket/rpcmp_media_output.sv', 'core/rtl/pocket/rpcmp_jt51_media_source.sv',
              'core/rtl/pocket/rpcmp_media_envelope.sv', 'core/rtl/pocket/rpcmp_native_completion.sv',
              'core/rtl/pocket/rpcmp_media_source_queue.sv', 'core/rtl/pocket/rpcmp_jt51_progress_audio.sv',
              'core/rtl/pocket/rpcmp_sound_session.sv', 'core/rtl/pocket/rpcmp_sound_mailbox.sv',
              'core/rtl/pocket/rpcmp_sound_mmio.sv', 'tests/rtl/sound_mmio_tb.sv',
              'tests/rtl/sound_mailbox_tb.sv') | ForEach-Object { Join-Path $root $_ }
Push-Location $output
try {
    if (-not (Test-Path 'work')) { & $vlib work; if ($LASTEXITCODE) { throw 'vlib failed.' } }
    & $vlog -quiet -sv -work work @sources
    if ($LASTEXITCODE) { throw 'vlog failed.' }
    $lines = @(& $vsim -c -quiet -lib work sound_mailbox_tb -do 'run -all; quit -code 0' 2>&1)
    $code = $LASTEXITCODE
    $lines | Write-Output
    $text = $lines | Out-String
    if ($code -ne 0 -or $text -notmatch '(?m)^# sound_mailbox_tb: PASS requests=64 responses=64 destination_stalls=64 source_stalls=64\r?$' -or
        $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw 'Sound mailbox backpressure simulation failed.' }
    foreach ($phase in $PhasePs) {
        if ($phase -lt 0 -or $phase -ge 81380) { throw 'Clock phase must be in [0, 81380) ps.' }
        $lines = @(& $vsim -c -quiet -lib work sound_mmio_tb "+PHASE_PS=$phase" -do 'run -all; quit -code 0' 2>&1)
        $code = $LASTEXITCODE
        $lines | Write-Output
        $text = $lines | Out-String
        if ($code -ne 0 -or $text -notmatch "(?m)^# sound_mmio_tb: PASS phase_ps=$phase .+ reset_stages=18 nonzero_bits=\d+\r?$" -or
            $text -notmatch '(?m)^# Errors: 0, Warnings: 0\r?$') { throw "Sound MMIO simulation failed at phase $phase." }
    }
} finally { Pop-Location }
