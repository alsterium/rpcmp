[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$expectedTemplateRevision = 'da3a021b1eaf742604d86d8dc9b33a6666263e6a'
$expectedQuartusVersion = '25.1std.0 Build 1129'
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$templateRoot = if ($env:RPCMP_OPENFPGA_TEMPLATE) {
    $env:RPCMP_OPENFPGA_TEMPLATE
} else {
    Join-Path $repositoryRoot 'out\toolchain-smoke\core-template'
}
$quartusRoot = if ($env:RPCMP_QUARTUS_ROOT) {
    $env:RPCMP_QUARTUS_ROOT
} else {
    'C:\altera_lite\25.1std\quartus\bin64'
}

$quartusSh = Join-Path $quartusRoot 'quartus_sh.exe'
if (-not (Test-Path -LiteralPath $quartusSh)) {
    throw 'Quartus was not found. Set RPCMP_QUARTUS_ROOT to the directory containing quartus_sh.exe.'
}

if (-not (Test-Path -LiteralPath (Join-Path $templateRoot '.git'))) {
    throw 'The official openFPGA template clone was not found. Set RPCMP_OPENFPGA_TEMPLATE to its repository root.'
}

$templateRevision = (& git -C $templateRoot rev-parse HEAD 2>&1 | Out-String).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect the openFPGA template revision:`n$templateRevision"
}
if ($templateRevision -ne $expectedTemplateRevision) {
    throw "Expected openFPGA template $expectedTemplateRevision, got $templateRevision."
}

$quartusVersion = (& $quartusSh --version 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to query Quartus version:`n$quartusVersion"
}
if ($quartusVersion -notmatch [regex]::Escape($expectedQuartusVersion)) {
    throw "Expected Quartus $expectedQuartusVersion, got:`n$quartusVersion"
}

$outputRoot = Join-Path $repositoryRoot 'out\pocket-spike'
$buildDirectory = Join-Path $outputRoot 'core-template'
$archivePath = Join-Path $outputRoot 'core-template.zip'
$outputRootFull = [IO.Path]::GetFullPath($outputRoot).TrimEnd('\') + '\'
$buildDirectoryFull = [IO.Path]::GetFullPath($buildDirectory)

if (-not $buildDirectoryFull.StartsWith($outputRootFull, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to prepare a build outside $outputRootFull"
}

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path -LiteralPath $buildDirectoryFull) {
    Remove-Item -LiteralPath $buildDirectoryFull -Recurse -Force
}
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}

& git -C $templateRoot archive --format=zip --output=$archivePath $expectedTemplateRevision
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to export the pinned openFPGA template.'
}
Expand-Archive -LiteralPath $archivePath -DestinationPath $buildDirectoryFull

$generatedRtl = Join-Path $buildDirectoryFull 'src\fpga\core\rpcmp_spike_regs.sv'
$projectRtl = Join-Path $repositoryRoot 'core\rtl\pocket\rpcmp_spike_regs.sv'
Copy-Item -LiteralPath $projectRtl -Destination $generatedRtl

$coreTopPath = Join-Path $buildDirectoryFull 'src\fpga\core\core_top.v'
$qsfPath = Join-Path $buildDirectoryFull 'src\fpga\ap_core.qsf'
$coreTop = [IO.File]::ReadAllText($coreTopPath).Replace("`r`n", "`n")
$qsf = [IO.File]::ReadAllText($qsfPath).Replace("`r`n", "`n")

$muxBefore = @'
    32'h10xxxxxx: begin
        // example
        // bridge_rd_data <= example_device_data;
        bridge_rd_data <= 0;
    end
'@.Replace("`r`n", "`n")
$muxAfter = @'
    32'h10xxxxxx: begin
        bridge_rd_data <= rpcmp_bridge_rd_data;
    end
'@.Replace("`r`n", "`n")
$instanceMarker = 'core_bridge_cmd icb ('
$instanceBlock = @'
    wire    [31:0]  rpcmp_bridge_rd_data;
    wire            rpcmp_liveness;
    wire            rpcmp_device_event_valid;
    wire    [63:0]  rpcmp_device_event_value;

rpcmp_spike_regs rpcmp_regs (
    .clk                ( clk_74a ),
    .reset_n            ( reset_n ),
    .bridge_addr        ( bridge_addr ),
    .bridge_rd          ( bridge_rd ),
    .bridge_wr          ( bridge_wr ),
    .bridge_wr_data     ( bridge_wr_data ),
    .bridge_rd_data     ( rpcmp_bridge_rd_data ),
    .liveness           ( rpcmp_liveness ),
    .device_event_valid ( rpcmp_device_event_valid ),
    .device_event_value ( rpcmp_device_event_value )
);

core_bridge_cmd icb (
'@.Replace("`r`n", "`n")
$qsfMarker = 'set_global_assignment -name VERILOG_FILE core/core_top.v'
$qsfReplacement = @'
set_global_assignment -name SYSTEMVERILOG_FILE core/rpcmp_spike_regs.sv
set_global_assignment -name VERILOG_FILE core/core_top.v
'@.Replace("`r`n", "`n")

if ([regex]::Matches($coreTop, [regex]::Escape($muxBefore)).Count -ne 1) {
    throw 'The pinned core_top bridge-mux marker did not occur exactly once.'
}
if ([regex]::Matches($coreTop, [regex]::Escape($instanceMarker)).Count -ne 1) {
    throw 'The pinned core_top command-handler marker did not occur exactly once.'
}
if ([regex]::Matches($qsf, [regex]::Escape($qsfMarker)).Count -ne 1) {
    throw 'The pinned QSF source marker did not occur exactly once.'
}

$coreTop = $coreTop.Replace($muxBefore, $muxAfter).Replace($instanceMarker, $instanceBlock)
$qsf = $qsf.Replace($qsfMarker, $qsfReplacement)
[IO.File]::WriteAllText($coreTopPath, $coreTop, [Text.UTF8Encoding]::new($false))
[IO.File]::WriteAllText($qsfPath, $qsf, [Text.UTF8Encoding]::new($false))

$quartusProject = Join-Path $buildDirectoryFull 'src\fpga'
Push-Location $quartusProject
try {
    & $quartusSh --flow compile ap_core
    if ($LASTEXITCODE -ne 0) {
        throw 'Quartus compile failed.'
    }
} finally {
    Pop-Location
}

$sofPath = Join-Path $quartusProject 'output_files\ap_core.sof'
$rbfPath = Join-Path $quartusProject 'output_files\ap_core.rbf'
if (-not (Test-Path -LiteralPath $sofPath) -or -not (Test-Path -LiteralPath $rbfPath)) {
    throw 'Quartus completed without the expected SOF and RBF artifacts.'
}

$reportDirectory = Join-Path $quartusProject 'output_files'
$warningBaseline = @{
    'ap_core.map.rpt' = 159
    'ap_core.fit.rpt' = 16
    'ap_core.asm.rpt' = 2
    'ap_core.sta.rpt' = 15
}
foreach ($reportName in $warningBaseline.Keys) {
    $reportPath = Join-Path $reportDirectory $reportName
    $expectedWarnings = $warningBaseline[$reportName]
    $reportText = [IO.File]::ReadAllText($reportPath)
    if ($reportText -notmatch "was successful\. 0 errors, $expectedWarnings warnings") {
        throw "$reportName no longer matches the accepted zero-error warning baseline of $expectedWarnings."
    }
}

$artifacts = foreach ($artifactPath in @($sofPath, $rbfPath)) {
    $artifact = Get-Item -LiteralPath $artifactPath
    $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $artifactPath
    [pscustomobject]@{
        Name = $artifact.Name
        Bytes = $artifact.Length
        SHA256 = $hash.Hash
    }
}
$artifacts | Format-List
