param([Parameter(Mandatory = $true)][string]$PreparedRenderer)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$jni = Join-Path (Resolve-Path $PreparedRenderer).Path 'jni'
if (!(Test-Path (Join-Path $jni 'mxdrvg/mxdrvg_core.h'))) { throw 'Prepare the pinned renderer first' }
$version = (& clang-tidy --version | Out-String)
if ($LASTEXITCODE -ne 0 -or $version -notmatch 'version 22\.1\.8([^0-9]|$)') { throw 'LLVM 22.1.8 required' }
& clang-tidy (Join-Path $repo 'spikes/pocket/openfpgaos/hybrid_renderer.cpp') --config-file (Join-Path $repo '.clang-tidy') -- -std=c++17 -isystem $jni -fsigned-char
if ($LASTEXITCODE -ne 0) { throw 'Reference-backed renderer static analysis failed' }
