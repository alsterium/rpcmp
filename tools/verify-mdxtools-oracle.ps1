$ErrorActionPreference = 'Stop'

$rpcmpRoot = Split-Path -Parent $PSScriptRoot
$rpcmpOracleRoot = Join-Path $rpcmpRoot 'out/research/mdxtools'
$rpcmpExpectedCommit = '9c8539fec2757fcf7c85d1986171b50ebe2ef1e5'
$rpcmpActualCommit = (& git -C $rpcmpOracleRoot rev-parse HEAD).Trim()
if ($rpcmpActualCommit -ne $rpcmpExpectedCommit) {
    throw "mdxtools commit mismatch: expected $rpcmpExpectedCommit, got $rpcmpActualCommit"
}

$rpcmpMount = "type=bind,source=$rpcmpRoot,target=/work"
$rpcmpBuildAndRun = @'
gcc -std=c11 -Iout/research/mdxtools \
  tools/research/mdxtools_oracle.c \
  out/research/mdxtools/mdx.c \
  out/research/mdxtools/mdx_driver.c \
  out/research/mdxtools/fm_driver.c \
  out/research/mdxtools/fm_opm_driver.c \
  out/research/mdxtools/timer_driver.c \
  out/research/mdxtools/adpcm_driver.c \
  -o /tmp/mdxtools_oracle && \
/tmp/mdxtools_oracle tests/fixtures/mdx/oracle-fm.mdx.hex
'@
$rpcmpActual = @(& docker run --rm --mount $rpcmpMount -w /work `
    rpcmp-openfpgaos-toolchain:14.2.0-3 sh -c $rpcmpBuildAndRun)
if ($LASTEXITCODE -ne 0) {
    throw "mdxtools oracle container failed with exit code $LASTEXITCODE"
}
$rpcmpTracePath = Join-Path $rpcmpRoot 'tests/fixtures/mdx/oracle-fm.mdxtools.trace'
$rpcmpExpected = @(Get-Content -LiteralPath $rpcmpTracePath | Where-Object {
        $_ -and -not $_.StartsWith('#')
    })
$rpcmpDifference = Compare-Object -ReferenceObject $rpcmpExpected -DifferenceObject $rpcmpActual
if ($rpcmpDifference) {
    $rpcmpDifference | Format-Table | Out-String | Write-Host
    throw 'mdxtools oracle trace differs from the approved fixture'
}

Write-Host "ORACLE:PASS commit=$rpcmpActualCommit events=$($rpcmpActual.Count)"
