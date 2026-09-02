[CmdletBinding()]
param()

& (Join-Path $PSScriptRoot 'pocket-m2-build.ps1') -Variant m4
exit $LASTEXITCODE
