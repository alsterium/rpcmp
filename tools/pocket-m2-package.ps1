[CmdletBinding()]
param([ValidateSet('m2','m4','stereo')][string]$Variant='m2')

$ErrorActionPreference='Stop'
function Assert([bool]$condition,[string]$message){if(-not $condition){throw $message}}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$definitions=Join-Path $root 'core\platform\pocket\m2-apf'
$rbf=Join-Path $root "out\pocket-$Variant\core-template\src\fpga\output_files\ap_core.rbf"
Assert (Test-Path -LiteralPath $rbf) 'Run tools/pocket-m2-build.ps1 first.'

$roots=[ordered]@{'audio.json'='audio';'core.json'='core';'data.json'='data';'input.json'='input';'interact.json'='interact';'variants.json'='variants';'video.json'='video'}
$docs=@{}
foreach($name in $roots.Keys){
    $doc=Get-Content -Raw (Join-Path $definitions $name)|ConvertFrom-Json -Depth 32
    $properties=@($doc.PSObject.Properties)
    Assert ($properties.Count -eq 1 -and $properties[0].Name -ceq $roots[$name]) "Invalid root in $name."
    Assert ($properties[0].Value.magic -ceq 'APF_VER_1') "Invalid magic in $name."
    $docs[$roots[$name]]=$properties[0].Value
}
$core=$docs.core;$metadata=$core.metadata
Assert ($metadata.shortname -ceq 'RPCMP M2' -and $metadata.version -ceq '0.0.0-m2-local') 'M2 identity mismatch.'
Assert ($metadata.shortname.Length -le 31 -and $metadata.description.Length -le 63 -and $metadata.author.Length -le 31 -and $metadata.url.Length -le 63 -and $metadata.version.Length -le 31) 'Metadata exceeds APF limits.'
Assert (@($core.cores).Count -eq 1 -and $core.cores[0].filename -ceq 'm2fixed.rbf_r') 'M2 core entry mismatch.'
Assert (@($docs.data.data_slots).Count -eq 0 -and @($docs.input.controllers).Count -eq 0 -and @($docs.interact.variables).Count -eq 0 -and @($docs.variants.variant_list).Count -eq 0) 'M2 package must have no runtime inputs or assets.'
Assert (@($docs.video.scaler_modes).Count -eq 1 -and $docs.video.scaler_modes[0].width -eq 320 -and $docs.video.scaler_modes[0].height -eq 240) 'M2 video mode mismatch.'

$output=Join-Path $root "out\pocket-$Variant\package";$sd=Join-Path $output 'sd'
$folder=switch($Variant){'m4' {'alsterium.RPCMP M4'} 'stereo' {'alsterium.RPCMP Stereo'} default {'alsterium.RPCMP M2'}}
$coreOut=Join-Path $sd "Cores\$folder"
$outputFull=[IO.Path]::GetFullPath($output).TrimEnd('\')+'\';$sdFull=[IO.Path]::GetFullPath($sd)
Assert ($sdFull.StartsWith($outputFull,[StringComparison]::OrdinalIgnoreCase)) 'Unsafe package path.'
if(Test-Path -LiteralPath $sdFull){Remove-Item -LiteralPath $sdFull -Recurse -Force}
New-Item -ItemType Directory -Force $coreOut|Out-Null
foreach($name in $roots.Keys){Copy-Item (Join-Path $definitions $name) (Join-Path $coreOut $name)}
if($Variant -eq 'm4'){
    $corePath=Join-Path $coreOut 'core.json';$coreDoc=Get-Content -Raw $corePath|ConvertFrom-Json -Depth 32
    $coreDoc.core.metadata.shortname='RPCMP M4';$coreDoc.core.metadata.description='Local M4 MDX-to-YM2151 audio test.'
    $coreDoc.core.metadata.version='0.7.0-m4-local';$coreDoc.core.metadata.date_release='2026-09-02'
    $coreDoc.core.cores[0].name='m4-mdx';$coreDoc.core.cores[0].filename='m4mdx.rbf_r'
    $coreDoc|ConvertTo-Json -Depth 32|Set-Content $corePath -Encoding ascii
}
elseif($Variant -eq 'stereo'){
    $corePath=Join-Path $coreOut 'core.json';$coreDoc=Get-Content -Raw $corePath|ConvertFrom-Json -Depth 32
    $coreDoc.core.metadata.shortname='RPCMP Stereo';$coreDoc.core.metadata.description='Local YM2151 stereo channel mapping test.'
    $coreDoc.core.metadata.version='0.7.1-stereo-local';$coreDoc.core.metadata.date_release='2026-09-02'
    $coreDoc.core.cores[0].name='stereo-probe';$coreDoc.core.cores[0].filename='stereo.rbf_r'
    $coreDoc|ConvertTo-Json -Depth 32|Set-Content $corePath -Encoding ascii
}
$packagedCore=Get-Content -Raw (Join-Path $coreOut 'core.json')|ConvertFrom-Json -Depth 32
$expectedShortname=switch($Variant){'m4' {'RPCMP M4'} 'stereo' {'RPCMP Stereo'} default {'RPCMP M2'}}
$expectedVersion=switch($Variant){'m4' {'0.7.0-m4-local'} 'stereo' {'0.7.1-stereo-local'} default {'0.0.0-m2-local'}}
$expectedRbf=switch($Variant){'m4' {'m4mdx.rbf_r'} 'stereo' {'stereo.rbf_r'} default {'m2fixed.rbf_r'}}
Assert ($packagedCore.core.magic -ceq 'APF_VER_1' -and
        $packagedCore.core.metadata.shortname -ceq $expectedShortname -and
        $packagedCore.core.metadata.version -ceq $expectedVersion -and
        $packagedCore.core.cores[0].filename -ceq $expectedRbf) 'Packaged core identity mismatch.'

$table=[byte[]]::new(256)
for($value=0;$value -lt 256;$value++){$reversed=0;for($bit=0;$bit -lt 8;$bit++){if($value -band (1 -shl $bit)){$reversed=$reversed -bor (1 -shl (7-$bit))}};$table[$value]=[byte]$reversed}
Assert ($table[1] -eq 128 -and $table[0x55] -eq 0xaa) 'Bit-reversal self-test failed.'
$native=[IO.File]::ReadAllBytes($rbf);$converted=[byte[]]::new($native.Length)
for($index=0;$index -lt $native.Length;$index++){$converted[$index]=$table[$native[$index]]}
$rbfName=$expectedRbf
$reversedPath=Join-Path $coreOut $rbfName;[IO.File]::WriteAllBytes($reversedPath,$converted)
for($index=0;$index -lt $native.Length;$index++){if($table[$converted[$index]] -ne $native[$index]){throw "RBF round trip failed at $index."}}

$expected=@($roots.Keys)+$rbfName|Sort-Object;$actual=@(Get-ChildItem $coreOut -File|ForEach-Object Name|Sort-Object)
Assert (@(Compare-Object $expected $actual).Count -eq 0) 'Package allowlist mismatch.'
$zipName=switch($Variant){'m4' {'alsterium.RPCMP-M4_0.7.0-m4-local_2026-09-02.zip'} 'stereo' {'alsterium.RPCMP-Stereo_0.7.1-stereo-local_2026-09-02.zip'} default {'alsterium.RPCMP-M2_0.0.0-m2-local_2026-09-01.zip'}}
$zip=Join-Path $output $zipName
if(Test-Path $zip){Remove-Item -LiteralPath $zip -Force}
Compress-Archive -LiteralPath (Join-Path $sd 'Cores') -DestinationPath $zip -CompressionLevel Optimal
$nativeHash=(Get-FileHash $rbf -Algorithm SHA256).Hash;$reversedHash=(Get-FileHash $reversedPath -Algorithm SHA256).Hash;$zipHash=(Get-FileHash $zip -Algorithm SHA256).Hash
Write-Output "Pocket $($Variant.ToUpperInvariant()) package validation: PASS"
[pscustomobject]@{NativeRbfBytes=$native.Length;NativeRbfSHA256=$nativeHash;ReversedRbfSHA256=$reversedHash;ZipPath=$zip;ZipSHA256=$zipHash}|Format-List
