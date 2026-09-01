[CmdletBinding()]
param()

$ErrorActionPreference='Stop'
function Assert([bool]$condition,[string]$message){if(-not $condition){throw $message}}
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$definitions=Join-Path $root 'core\platform\pocket\m2-apf'
$rbf=Join-Path $root 'out\pocket-m2\core-template\src\fpga\output_files\ap_core.rbf'
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

$output=Join-Path $root 'out\pocket-m2\package';$sd=Join-Path $output 'sd';$folder='alsterium.RPCMP M2';$coreOut=Join-Path $sd "Cores\$folder"
$outputFull=[IO.Path]::GetFullPath($output).TrimEnd('\')+'\';$sdFull=[IO.Path]::GetFullPath($sd)
Assert ($sdFull.StartsWith($outputFull,[StringComparison]::OrdinalIgnoreCase)) 'Unsafe package path.'
if(Test-Path -LiteralPath $sdFull){Remove-Item -LiteralPath $sdFull -Recurse -Force}
New-Item -ItemType Directory -Force $coreOut|Out-Null
foreach($name in $roots.Keys){Copy-Item (Join-Path $definitions $name) (Join-Path $coreOut $name)}

$table=[byte[]]::new(256)
for($value=0;$value -lt 256;$value++){$reversed=0;for($bit=0;$bit -lt 8;$bit++){if($value -band (1 -shl $bit)){$reversed=$reversed -bor (1 -shl (7-$bit))}};$table[$value]=[byte]$reversed}
Assert ($table[1] -eq 128 -and $table[0x55] -eq 0xaa) 'Bit-reversal self-test failed.'
$native=[IO.File]::ReadAllBytes($rbf);$converted=[byte[]]::new($native.Length)
for($index=0;$index -lt $native.Length;$index++){$converted[$index]=$table[$native[$index]]}
$reversedPath=Join-Path $coreOut 'm2fixed.rbf_r';[IO.File]::WriteAllBytes($reversedPath,$converted)
for($index=0;$index -lt $native.Length;$index++){if($table[$converted[$index]] -ne $native[$index]){throw "RBF round trip failed at $index."}}

$expected=@($roots.Keys)+'m2fixed.rbf_r'|Sort-Object;$actual=@(Get-ChildItem $coreOut -File|ForEach-Object Name|Sort-Object)
Assert (@(Compare-Object $expected $actual).Count -eq 0) 'Package allowlist mismatch.'
$zip=Join-Path $output 'alsterium.RPCMP-M2_0.0.0-m2-local_2026-09-01.zip'
if(Test-Path $zip){Remove-Item -LiteralPath $zip -Force}
Compress-Archive -LiteralPath (Join-Path $sd 'Cores') -DestinationPath $zip -CompressionLevel Optimal
$nativeHash=(Get-FileHash $rbf -Algorithm SHA256).Hash;$reversedHash=(Get-FileHash $reversedPath -Algorithm SHA256).Hash;$zipHash=(Get-FileHash $zip -Algorithm SHA256).Hash
Write-Output 'Pocket M2 package validation: PASS'
[pscustomobject]@{NativeRbfBytes=$native.Length;NativeRbfSHA256=$nativeHash;ReversedRbfSHA256=$reversedHash;ZipPath=$zip;ZipSHA256=$zipHash}|Format-List
