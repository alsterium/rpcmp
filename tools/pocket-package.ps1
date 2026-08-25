[CmdletBinding()]
param(
    [string]$RbfPath
)

$ErrorActionPreference = 'Stop'

function Assert-True {
    param(
        [bool]$Condition,
        [string]$Message
    )

    if (-not $Condition) {
        throw $Message
    }
}

function Convert-ToBridgeAddress {
    param(
        [object]$Value
    )

    if ($Value -is [string] -and $Value -match '^0x[0-9A-Fa-f]{1,8}$') {
        return [Convert]::ToUInt32($Value.Substring(2), 16)
    }
    if ($Value -isnot [string] -and [uint64]$Value -le [uint32]::MaxValue) {
        return [uint32]$Value
    }
    throw "Invalid 32-bit BRIDGE address: $Value"
}

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$definitionRoot = Join-Path $repositoryRoot 'core\platform\pocket\apf'
if (-not $RbfPath) {
    $RbfPath = Join-Path $repositoryRoot 'out\pocket-spike\core-template\src\fpga\output_files\ap_core.rbf'
}
$resolvedRbfPath = (Resolve-Path -LiteralPath $RbfPath).Path

$definitionRoots = [ordered]@{
    'audio.json' = 'audio'
    'core.json' = 'core'
    'data.json' = 'data'
    'input.json' = 'input'
    'interact.json' = 'interact'
    'variants.json' = 'variants'
    'video.json' = 'video'
}
$definitions = @{}

foreach ($definitionName in $definitionRoots.Keys) {
    $definitionPath = Join-Path $definitionRoot $definitionName
    Assert-True (Test-Path -LiteralPath $definitionPath) "Missing APF definition: $definitionName"
    try {
        $document = Get-Content -Raw -LiteralPath $definitionPath | ConvertFrom-Json -Depth 32
    } catch {
        throw "Invalid JSON in ${definitionName}: $($_.Exception.Message)"
    }

    $properties = @($document.PSObject.Properties)
    $expectedRoot = $definitionRoots[$definitionName]
    Assert-True ($properties.Count -eq 1) "$definitionName must contain exactly one root object."
    Assert-True ($properties[0].Name -ceq $expectedRoot) "$definitionName must use the $expectedRoot root object."
    Assert-True ($properties[0].Value.magic -ceq 'APF_VER_1') "$definitionName must declare APF_VER_1."
    $definitions[$expectedRoot] = $properties[0].Value
}

$core = $definitions.core
$metadata = $core.metadata
Assert-True (@($metadata.platform_ids).Count -eq 0) 'The M0 spike must remain standalone without platform assets.'
Assert-True ($metadata.shortname -ceq 'RPCMP') 'core.metadata.shortname must be RPCMP.'
Assert-True ($metadata.author -ceq 'alsterium') 'core.metadata.author must be alsterium.'
Assert-True ($metadata.version -ceq '0.0.0-m0.2') 'core.metadata.version must match the current M0.2 package.'
Assert-True ($metadata.shortname.Length -le 31) 'core.metadata.shortname exceeds 31 characters.'
Assert-True ($metadata.description.Length -le 63) 'core.metadata.description exceeds 63 characters.'
Assert-True ($metadata.author.Length -le 31) 'core.metadata.author exceeds 31 characters.'
Assert-True ($metadata.url.Length -le 63) 'core.metadata.url exceeds 63 characters.'
Assert-True ($metadata.version.Length -le 31) 'core.metadata.version exceeds 31 characters.'
Assert-True ($metadata.date_release -match '^\d{4}-\d{2}-\d{2}$') 'core.metadata.date_release must use YYYY-MM-DD.'
Assert-True ($core.framework.target_product -ceq 'Analogue Pocket') 'framework.target_product must be Analogue Pocket.'
Assert-True (@($core.cores).Count -eq 1) 'The M0 package must contain exactly one core entry.'
Assert-True ([uint32]$core.cores[0].id -eq 0) 'The M0 bitstream ID must be zero.'
Assert-True ($core.cores[0].filename -ceq 'bitstream.rbf_r') 'The core entry must reference bitstream.rbf_r.'

Assert-True (@($definitions.data.data_slots).Count -eq 0) 'M0 must not package data slots.'
Assert-True (@($definitions.input.controllers).Count -eq 0) 'M0 must not package controller mappings.'
Assert-True (@($definitions.variants.variant_list).Count -eq 0) 'M0 must not package variants.'
Assert-True (@($definitions.video.scaler_modes).Count -eq 1) 'M0 must define exactly one scaler mode.'
$scalerMode = $definitions.video.scaler_modes[0]
Assert-True ($scalerMode.width -eq 320 -and $scalerMode.height -eq 240) 'The template video mode must remain 320x240.'
Assert-True ($scalerMode.aspect_w -eq 4 -and $scalerMode.aspect_h -eq 3) 'The template video aspect must remain 4:3.'

$expectedInteract = @{
    3 = @{ Name = 'Seq'; Type = 'number_u32'; Address = [uint32]0x1000000C; Enabled = $false }
    4 = @{ Name = 'Count'; Type = 'number_u32'; Address = [uint32]0x10000010; Enabled = $false }
    5 = @{ Name = 'Last'; Type = 'number_u32'; Address = [uint32]0x10000018; Enabled = $false }
    6 = @{ Name = 'ESeq'; Type = 'number_u32'; Address = [uint32]0x1000001C; Enabled = $false }
    7 = @{ Name = 'EVal'; Type = 'number_u32'; Address = [uint32]0x10000020; Enabled = $false }
    8 = @{ Name = 'Run 1'; Type = 'action'; Address = [uint32]0x10000028; Enabled = $true }
    9 = @{ Name = 'Run 2'; Type = 'action'; Address = [uint32]0x1000002C; Enabled = $true }
}
$interactVariables = @($definitions.interact.variables)
Assert-True ($interactVariables.Count -eq $expectedInteract.Count) 'Unexpected M0 Interact variable count.'
$seenIds = [Collections.Generic.HashSet[uint16]]::new()
foreach ($variable in $interactVariables) {
    $variableId = [uint16]$variable.id
    Assert-True ($seenIds.Add($variableId)) "Duplicate Interact ID: $variableId"
    Assert-True ($variable.name.Length -le 5) "M0.2 Interact name exceeds the observed five-character display budget: $($variable.name)"
    Assert-True ($expectedInteract.ContainsKey([int]$variableId)) "Unexpected Interact ID: $variableId"
    $expected = $expectedInteract[[int]$variableId]
    $address = Convert-ToBridgeAddress $variable.address
    Assert-True (($address -shr 24) -ne 0xF8) "Interact ID $variableId enters the APF-reserved region."
    Assert-True ($variable.name -ceq $expected.Name) "Unexpected Interact name for ID $variableId."
    Assert-True ($variable.type -ceq $expected.Type) "Unexpected Interact type for ID $variableId."
    Assert-True ($address -eq $expected.Address) "Unexpected Interact address for ID $variableId."
    Assert-True ([bool]$variable.enabled -eq $expected.Enabled) "Unexpected Interact enabled state for ID $variableId."
}
$runId1 = $interactVariables | Where-Object { $_.id -eq 8 }
$runId2 = $interactVariables | Where-Object { $_.id -eq 9 }
Assert-True ($runId1.value -eq 1) 'Run ID 1 must write opcode one.'
Assert-True ($runId2.value -eq 1) 'Run ID 2 must write opcode one.'

$reverseTable = [byte[]]::new(256)
for ($byteValue = 0; $byteValue -lt 256; $byteValue++) {
    $reversedValue = 0
    for ($bit = 0; $bit -lt 8; $bit++) {
        if (($byteValue -band (1 -shl $bit)) -ne 0) {
            $reversedValue = $reversedValue -bor (1 -shl (7 - $bit))
        }
    }
    $reverseTable[$byteValue] = [byte]$reversedValue
}

$reverseCases = @{
    0x00 = 0x00
    0x01 = 0x80
    0x02 = 0x40
    0x55 = 0xAA
    0x80 = 0x01
    0xFF = 0xFF
}
foreach ($case in $reverseCases.GetEnumerator()) {
    Assert-True ($reverseTable[[int]$case.Key] -eq [byte]$case.Value) "Bit-reversal self-test failed for 0x$($case.Key.ToString('X2'))."
}

$outputRoot = Join-Path $repositoryRoot 'out\package'
$sdRoot = Join-Path $outputRoot 'sd'
$coreFolderName = "$($metadata.author).$($metadata.shortname)"
Assert-True ($coreFolderName -ceq 'alsterium.RPCMP') 'Core folder identity drifted from alsterium.RPCMP.'
$coreOutput = Join-Path $sdRoot "Cores\$coreFolderName"
$outputRootFull = [IO.Path]::GetFullPath($outputRoot).TrimEnd('\') + '\'
$sdRootFull = [IO.Path]::GetFullPath($sdRoot)
Assert-True ($sdRootFull.StartsWith($outputRootFull, [StringComparison]::OrdinalIgnoreCase)) "Refusing to prepare a package outside $outputRootFull"

New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if (Test-Path -LiteralPath $sdRootFull) {
    Remove-Item -LiteralPath $sdRootFull -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $coreOutput | Out-Null

foreach ($definitionName in $definitionRoots.Keys) {
    Copy-Item -LiteralPath (Join-Path $definitionRoot $definitionName) -Destination (Join-Path $coreOutput $definitionName)
}

$sourceBytes = [IO.File]::ReadAllBytes($resolvedRbfPath)
Assert-True ($sourceBytes.Length -gt 0) 'The Quartus RBF is empty.'
$reversedBytes = [byte[]]::new($sourceBytes.Length)
for ($index = 0; $index -lt $sourceBytes.Length; $index++) {
    $reversedBytes[$index] = $reverseTable[$sourceBytes[$index]]
}
$reversedPath = Join-Path $coreOutput 'bitstream.rbf_r'
[IO.File]::WriteAllBytes($reversedPath, $reversedBytes)
Assert-True ((Get-Item -LiteralPath $reversedPath).Length -eq $sourceBytes.Length) 'RBF_R length differs from the source RBF.'
for ($index = 0; $index -lt $sourceBytes.Length; $index++) {
    if ($reverseTable[$reversedBytes[$index]] -ne $sourceBytes[$index]) {
        throw "RBF_R round-trip validation failed at byte $index."
    }
}

$expectedFiles = @($definitionRoots.Keys) + 'bitstream.rbf_r' | Sort-Object
$actualFiles = @(Get-ChildItem -LiteralPath $coreOutput -File | Select-Object -ExpandProperty Name | Sort-Object)
$fileDifference = @(Compare-Object -ReferenceObject $expectedFiles -DifferenceObject $actualFiles)
Assert-True ($fileDifference.Count -eq 0) 'Generated core folder contains missing or unexpected files.'

$zipName = "$coreFolderName`_$($metadata.version)_$($metadata.date_release).zip"
$zipPath = Join-Path $outputRoot $zipName
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath (Join-Path $sdRoot 'Cores') -DestinationPath $zipPath -CompressionLevel Optimal

$expectedEntries = $expectedFiles |
    ForEach-Object { "Cores/$coreFolderName/$_" } |
    Sort-Object
$archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
try {
    $archiveEntries = @(
        $archive.Entries |
            ForEach-Object { $_.FullName.Replace('\', '/') } |
            Where-Object { -not $_.EndsWith('/') } |
            Sort-Object
    )
    foreach ($entryName in $archiveEntries) {
        Assert-True (-not $entryName.StartsWith('/')) "ZIP entry must be relative: $entryName"
        Assert-True (-not $entryName.Contains('../')) "ZIP entry escapes its base folder: $entryName"
        Assert-True ($entryName.StartsWith('Cores/')) "ZIP contains an unsupported base folder: $entryName"
    }
    $entryDifference = @(Compare-Object -ReferenceObject $expectedEntries -DifferenceObject $archiveEntries)
    Assert-True ($entryDifference.Count -eq 0) 'ZIP layout differs from the validated SD tree.'
    $bitstreamEntry = $archive.Entries | Where-Object { $_.FullName.Replace('\', '/') -ceq "Cores/$coreFolderName/bitstream.rbf_r" }
    Assert-True ($bitstreamEntry.Length -eq $sourceBytes.Length) 'ZIP bitstream length differs from the source RBF.'
} finally {
    $archive.Dispose()
}

$sourceHash = Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedRbfPath
$reversedHash = Get-FileHash -Algorithm SHA256 -LiteralPath $reversedPath
$zipHash = Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath
Write-Output 'Pocket package validation: PASS'
[pscustomobject]@{
    SourceRbfBytes = $sourceBytes.Length
    SourceRbfSHA256 = $sourceHash.Hash
    ReversedRbfSHA256 = $reversedHash.Hash
    ZipPath = $zipPath
    ZipSHA256 = $zipHash.Hash
} | Format-List
