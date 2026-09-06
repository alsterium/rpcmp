[CmdletBinding()]
param([switch]$CheckSetupOnly)

$ErrorActionPreference = 'Stop'
$rpcmpRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Assert-ToolVersion {
    param([string]$Name, [version]$Minimum, [string]$Exact = '')
    $rpcmpTool = (Get-Command $Name -ErrorAction Stop).Source
    $rpcmpVersionText = (& $rpcmpTool --version 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0 -or $rpcmpVersionText -notmatch '(\d+\.\d+\.\d+)') {
        throw "$Name did not report a usable version."
    }
    $rpcmpVersion = $Matches[1]
    if ([version]$rpcmpVersion -lt $Minimum -or ($Exact -and $rpcmpVersion -ne $Exact)) {
        throw "$Name version $rpcmpVersion is unsupported; minimum=$Minimum exact=$Exact. See docs/development/harness.md."
    }
    Write-Host "$Name $rpcmpVersion"
}

$rpcmpPresets = Get-Content -LiteralPath (Join-Path $rpcmpRoot 'CMakePresets.json') -Raw | ConvertFrom-Json
$rpcmpLlvm = ($rpcmpPresets.configurePresets | Where-Object name -EQ 'host-msvc').cacheVariables.RPCMP_LLVM_VERSION
Assert-ToolVersion cmake ([version]'3.25.0')
Assert-ToolVersion python ([version]'3.11.0')
Assert-ToolVersion clang-format ([version]$rpcmpLlvm) $rpcmpLlvm
Assert-ToolVersion clang-tidy ([version]$rpcmpLlvm) $rpcmpLlvm
& python -B (Join-Path $rpcmpRoot 'tools/check_harness.py') --root $rpcmpRoot
if ($LASTEXITCODE -ne 0) { throw 'Harness navigation or verification preset check failed.' }

$rpcmpVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $rpcmpVsWhere -PathType Leaf)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$rpcmpVsOutput = & $rpcmpVsWhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio discovery failed.' }
$rpcmpVsInstall = ($rpcmpVsOutput | Out-String).Trim()
if (-not $rpcmpVsInstall) {
    throw 'Visual Studio C++ Build Tools were not found.'
}

$rpcmpDevCmd = Join-Path $rpcmpVsInstall 'Common7/Tools/VsDevCmd.bat'
$rpcmpCmake = (Get-Command cmake -ErrorAction Stop).Source
$rpcmpInvocation = 'call "{0}" -arch=x64 -host_arch=x64 >nul && set VSLANG=1033 && "{1}" --workflow --preset host-verify' -f `
    $rpcmpDevCmd, $rpcmpCmake

if ($CheckSetupOnly) {
    $rpcmpInvocation = 'call "{0}" -arch=x64 -host_arch=x64 >nul && where cl && ninja --version' -f $rpcmpDevCmd
}

Push-Location $rpcmpRoot
try {
    & $env:ComSpec /d /c $rpcmpInvocation
    $rpcmpExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
if ($rpcmpExitCode -eq 0 -and $CheckSetupOnly) { Write-Host 'Host setup: PASS (no build or tests run)' }
exit $rpcmpExitCode
