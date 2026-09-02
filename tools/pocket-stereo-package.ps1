[CmdletBinding()]
param()

& (Join-Path $PSScriptRoot 'pocket-m2-package.ps1') -Variant stereo
exit $LASTEXITCODE
