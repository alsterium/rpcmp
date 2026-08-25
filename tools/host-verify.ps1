$ErrorActionPreference = 'Stop'

$rpcmpVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $rpcmpVsWhere -PathType Leaf)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$rpcmpVsInstall = (& $rpcmpVsWhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath).Trim()
if (-not $rpcmpVsInstall) {
    throw 'Visual Studio C++ Build Tools were not found.'
}

$rpcmpDevCmd = Join-Path $rpcmpVsInstall 'Common7/Tools/VsDevCmd.bat'
$rpcmpCmake = (Get-Command cmake -ErrorAction Stop).Source
$rpcmpInvocation = 'call "{0}" -arch=x64 -host_arch=x64 >nul && set VSLANG=1033 && "{1}" --workflow --preset host-verify' -f `
    $rpcmpDevCmd, $rpcmpCmake

& $env:ComSpec /d /c $rpcmpInvocation
exit $LASTEXITCODE
