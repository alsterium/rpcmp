[CmdletBinding()]
param()

& (Join-Path $PSScriptRoot 'pocket-m2-build.ps1') -Variant stereo
exit $LASTEXITCODE
