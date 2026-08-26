$ErrorActionPreference = 'Stop'

$rpcmpRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$rpcmpDocker = (Get-Command docker -ErrorAction Stop).Source
$rpcmpImage = 'rpcmp-openfpgaos-toolchain:14.2.0-3'
$rpcmpDockerfile = Join-Path $rpcmpRoot 'tools/docker/Dockerfile.openfpgaos-spike'
$rpcmpSdkRoot = Join-Path $rpcmpRoot 'out/research/openfpgaSDK-a408ddc'
$rpcmpExpectedSdkRevision = 'a408ddc12aed0dfaa4aa22c06af82f829db77126'

if (-not (Test-Path -LiteralPath $rpcmpSdkRoot -PathType Container)) {
    throw "Pinned SDK checkout is missing: $rpcmpSdkRoot"
}

$rpcmpActualSdkRevision = (& git -C $rpcmpSdkRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $rpcmpActualSdkRevision -ne $rpcmpExpectedSdkRevision) {
    throw "SDK revision mismatch: expected $rpcmpExpectedSdkRevision, got $rpcmpActualSdkRevision"
}

$rpcmpSdkStatus = (& git -C $rpcmpSdkRoot status --porcelain=v1 --untracked-files=all)
if ($LASTEXITCODE -ne 0 -or $rpcmpSdkStatus) {
    throw 'Pinned SDK checkout has local changes; refusing mixed-revision evidence.'
}

& $rpcmpDocker build --pull --provenance=false --tag $rpcmpImage --file $rpcmpDockerfile `
    (Split-Path $rpcmpDockerfile)
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$rpcmpMount = "type=bind,source=$rpcmpRoot,target=/workspace/rpcmp"
& $rpcmpDocker run --rm --mount $rpcmpMount --workdir /workspace/rpcmp $rpcmpImage `
    make --file spikes/pocket/openfpgaos/Makefile clean verify
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $rpcmpDocker run --rm --mount $rpcmpMount --workdir /workspace/rpcmp $rpcmpImage `
    make --file spikes/pocket/openfpgaos/desktop.mk clean-desktop prepare-desktop
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $rpcmpDocker run --rm --mount $rpcmpMount `
    --workdir /workspace/rpcmp/out/build/pocket-openfpgaos-desktop $rpcmpImage `
    make --file /workspace/rpcmp/spikes/pocket/openfpgaos/desktop.mk verify
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $rpcmpDocker run --rm --mount $rpcmpMount --workdir /workspace/rpcmp $rpcmpImage `
    python3 tools/pocket_package.py `
    --repo /workspace/rpcmp `
    --sdk /workspace/rpcmp/out/research/openfpgaSDK-a408ddc `
    --elf /workspace/rpcmp/out/build/pocket-openfpgaos/rpcmp-probe.elf `
    --output /workspace/rpcmp/out/build/pocket-openfpgaos-package `
    --zip /workspace/rpcmp/out/build/rpcmp-openfpgaos-probe.zip
exit $LASTEXITCODE
