[CmdletBinding()]
param()

& (Join-Path $PSScriptRoot 'pocket-m2-package.ps1') -Variant m4
exit $LASTEXITCODE
