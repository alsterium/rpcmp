[CmdletBinding()]
param()

$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$template=Join-Path $root 'out\toolchain-smoke\core-template'
$jt51=Join-Path $root 'out\research\jt51-985a573'
$templateRevision='da3a021b1eaf742604d86d8dc9b33a6666263e6a'
$jt51Revision='985a573dcfc1ff135553a39f7eae21d18ba57cbe'
$quartus=if($env:RPCMP_QUARTUS_ROOT){$env:RPCMP_QUARTUS_ROOT}else{'C:\altera_lite\25.1std\quartus\bin64'}
$quartusSh=Join-Path $quartus 'quartus_sh.exe'
if((& git -C $template rev-parse HEAD).Trim() -ne $templateRevision){throw 'Template revision mismatch.'}
if((& git -C $jt51 rev-parse HEAD).Trim() -ne $jt51Revision){throw 'JT51 revision mismatch.'}
if(((& git -C $jt51 status --porcelain|Out-String).Trim())){throw 'Pinned JT51 checkout has local changes.'}
if((& $quartusSh --version 2>&1|Out-String) -notmatch '25\.1std\.0 Build 1129'){throw 'Quartus version mismatch.'}

$output=Join-Path $root 'out\pocket-m2';$build=Join-Path $output 'core-template';$archive=Join-Path $output 'template.zip'
$outputFull=[IO.Path]::GetFullPath($output).TrimEnd('\')+'\';$buildFull=[IO.Path]::GetFullPath($build)
if(-not $buildFull.StartsWith($outputFull,[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe build path.'}
New-Item -ItemType Directory -Force $output|Out-Null
if(Test-Path $buildFull){Remove-Item -LiteralPath $buildFull -Recurse -Force}
if(Test-Path $archive){Remove-Item -LiteralPath $archive -Force}
& git -C $template archive --format=zip --output=$archive $templateRevision
if($LASTEXITCODE){throw 'Template export failed.'}
Expand-Archive $archive $buildFull

$core=Join-Path $buildFull 'src\fpga\core'
foreach($name in @('rpcmp_device_queue.sv','rpcmp_pocket_audio.sv','rpcmp_jt51_audio.sv','rpcmp_m2_fixed_core.sv')){
    Copy-Item (Join-Path $root "core\rtl\pocket\$name") (Join-Path $core $name)
}
Copy-Item (Join-Path $jt51 'hdl') (Join-Path $core 'jt51') -Recurse

$topPath=Join-Path $core 'core_top.v';$qsfPath=Join-Path $buildFull 'src\fpga\ap_core.qsf'
$sdcPath=Join-Path $core 'core_constraints.sdc'
$top=[IO.File]::ReadAllText($topPath).Replace("`r`n","`n");$qsf=[IO.File]::ReadAllText($qsfPath).Replace("`r`n","`n")
$start=$top.IndexOf('// audio i2s silence generator');$end=$top.IndexOf('///////////////////////////////////////////////',$start)
if($start -lt 0 -or $end -lt 0){throw 'Audio overlay markers missing.'}
$top=$top.Substring(0,$start)+"// RPCMP M2 audio is instantiated after the audio PLL.`n`n"+$top.Substring($end)
$pllMarker=@'
mf_pllbase mp1 (
    .refclk         ( clk_74a ),
    .rst            ( 0 ),
    
    .outclk_0       ( clk_core_12288 ),
    .outclk_1       ( clk_core_12288_90deg ),
    
    .locked         ( pll_core_locked )
);
'@.Replace("`r`n","`n")
$instance=$pllMarker+@'

wire rpcmp_m2_enqueued;
wire rpcmp_m2_complete;
wire rpcmp_m2_queue_fault;
wire rpcmp_m2_audio_fault;
rpcmp_m2_fixed_core rpcmp_m2 (
    .clk_cpu(clk_74a), .clk_audio(clk_core_12288),
    .reset_n(reset_n && pll_core_locked_s),
    .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac),
    .sequence_enqueued(rpcmp_m2_enqueued), .sequence_complete(rpcmp_m2_complete),
    .queue_fault(rpcmp_m2_queue_fault), .audio_fault(rpcmp_m2_audio_fault)
);
'@.Replace("`r`n","`n")
if(([regex]::Matches($top,[regex]::Escape($pllMarker))).Count -ne 1){throw 'PLL marker mismatch.'}
$top=$top.Replace($pllMarker,$instance)
$gray=@'
                vidout_rgb[23:16] <= 8'd60;
                vidout_rgb[15:8]  <= 8'd60;
                vidout_rgb[7:0]   <= 8'd60;
'@.Replace("`r`n","`n")
$color=@'
                if(rpcmp_m2_queue_fault || rpcmp_m2_audio_fault)
                    vidout_rgb <= 24'hff0000;
                else if(rpcmp_m2_complete)
                    vidout_rgb <= 24'h00c000;
                else
                    vidout_rgb <= 24'h0020c0;
'@.Replace("`r`n","`n")
if(([regex]::Matches($top,[regex]::Escape($gray))).Count -ne 1){throw 'Video marker mismatch.'}
$top=$top.Replace($gray,$color)
$qsfMarker='set_global_assignment -name VERILOG_FILE core/core_top.v'
$qsfSources=@'
set_global_assignment -name SYSTEMVERILOG_FILE core/rpcmp_device_queue.sv
set_global_assignment -name SYSTEMVERILOG_FILE core/rpcmp_pocket_audio.sv
set_global_assignment -name SYSTEMVERILOG_FILE core/rpcmp_jt51_audio.sv
set_global_assignment -name SYSTEMVERILOG_FILE core/rpcmp_m2_fixed_core.sv
set_global_assignment -name QIP_FILE core/jt51/jt51.qip
set_global_assignment -name VERILOG_FILE core/core_top.v
'@.Replace("`r`n","`n")
if(([regex]::Matches($qsf,[regex]::Escape($qsfMarker))).Count -ne 1){throw 'QSF marker mismatch.'}
$qsf=$qsf.Replace($qsfMarker,$qsfSources)
[IO.File]::WriteAllText($topPath,$top,[Text.UTF8Encoding]::new($false));[IO.File]::WriteAllText($qsfPath,$qsf,[Text.UTF8Encoding]::new($false))

$sdc=@'
# RPCMP M2 clock-domain and external-interface constraints.
# The APF shell provides no board-level input/output delay contract for these
# asynchronous bridge and scaler interfaces, so they are explicitly exempted.
set audio_clock {ic|mp1|mf_pllbase_inst|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk}
set audio_clock_90 {ic|mp1|mf_pllbase_inst|altera_pll_i|general[1].gpll~PLL_OUTPUT_COUNTER|divclk}
set_clock_groups -asynchronous \
 -group { bridge_spiclk } \
 -group { clk_74a } \
 -group { clk_74b } \
 -group $audio_clock \
 -group $audio_clock_90

set_false_path -from [get_ports {bridge_1wire bridge_spimiso bridge_spimosi bridge_spiss}]
set_false_path -to [get_ports {bridge_1wire bridge_spimiso bridge_spimosi scal_*}]
'@.Replace("`r`n","`n")
[IO.File]::WriteAllText($sdcPath,$sdc,[Text.UTF8Encoding]::new($false))

$project=Join-Path $buildFull 'src\fpga';Push-Location $project
try{& $quartusSh --flow compile ap_core;if($LASTEXITCODE){throw 'Quartus compile failed.'}}finally{Pop-Location}
$reports=Join-Path $project 'output_files';$rbf=Join-Path $reports 'ap_core.rbf';$sof=Join-Path $reports 'ap_core.sof'
if(!(Test-Path $rbf) -or !(Test-Path $sof)){throw 'Programming artifacts missing.'}
$flow=[IO.File]::ReadAllText((Join-Path $reports 'ap_core.flow.rpt'))
if($flow -notmatch 'Flow Status\s*;\s*Successful'){throw 'Quartus flow was not successful.'}
$sta=[IO.File]::ReadAllText((Join-Path $reports 'ap_core.sta.rpt'))
$staSummary=[IO.File]::ReadAllText((Join-Path $reports 'ap_core.sta.summary'))
$fitSummary=[IO.File]::ReadAllText((Join-Path $reports 'ap_core.fit.summary'))
if($sta -notmatch 'Design is fully constrained for setup requirements' -or $sta -notmatch 'Design is fully constrained for hold requirements'){throw 'Timing constraints are incomplete.'}
if($sta -notmatch 'Unconstrained Input Ports\s*;\s*0\s*;\s*0' -or $sta -notmatch 'Unconstrained Output Ports\s*;\s*0\s*;\s*0'){throw 'Unconstrained external ports remain.'}
if($sta -notmatch 'Worst-Case MTBF of Design is 1e\+09 years'){throw 'RPCMP synchronizers were not recognized.'}
$slacks=@([regex]::Matches($staSummary,'(?m)^Slack\s*:\s*(-?\d+(?:\.\d+)?)\s*$')|ForEach-Object {[double]$_.Groups[1].Value})
if($slacks.Count -eq 0 -or ($slacks|Measure-Object -Minimum).Minimum -lt 0){throw 'Timing slack is negative or missing.'}
function Read-Fit([string]$name){$match=[regex]::Match($fitSummary,"(?m)^$([regex]::Escape($name))\s*:\s*(.+)$");if(-not $match.Success){throw "Missing fitter field: $name"};$match.Groups[1].Value.Trim()}
$evidence=[ordered]@{
    Quartus='25.1std.0 Build 1129';TemplateRevision=$templateRevision;JT51Revision=$jt51Revision
    LogicALMs=(Read-Fit 'Logic utilization (in ALMs)');Registers=(Read-Fit 'Total registers')
    BlockMemoryBits=(Read-Fit 'Total block memory bits');RamBlocks=(Read-Fit 'Total RAM Blocks')
    DspBlocks=(Read-Fit 'Total DSP Blocks');PLLs=(Read-Fit 'Total PLLs')
    MinimumSlackNs=($slacks|Measure-Object -Minimum).Minimum
    NativeRbfBytes=(Get-Item $rbf).Length;NativeRbfSHA256=(Get-FileHash $rbf -Algorithm SHA256).Hash
    SofBytes=(Get-Item $sof).Length;SofSHA256=(Get-FileHash $sof -Algorithm SHA256).Hash
}
$evidence|ConvertTo-Json|Set-Content (Join-Path $output 'build-evidence.json') -Encoding utf8NoBOM
Write-Output 'Pocket M2 Quartus validation: PASS'
[pscustomobject]$evidence|Format-List
