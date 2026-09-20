# Player-only CPU/audio crossings. core_constraints.sdc deliberately does NOT
# clock-group-cut this pair: that would override the held-bundle route limits.
# Other shell clock groups and all second synchronizer stages remain timed.
proc rpcmp_required_registers {pattern} {
    set result [get_registers $pattern]
    if {[get_collection_size $result] == 0} {
        error "missing required player CDC endpoint $pattern"
    }
    return $result
}
proc rpcmp_required_clock {names} {
    set matches {}
    foreach_in_collection clock [get_clocks *] {
        set name [get_clock_info -name $clock]
        if {[lsearch -exact $names $name] >= 0} { lappend matches $name }
    }
    if {[llength $matches] != 1} { error "missing or ambiguous player clock: $names" }
    return [get_clocks [lindex $matches 0]]
}
# Timing-driven synthesis uses PLL RTL port names; fit/STA use device counters.
# Require exactly one observed spelling of each clock, at either netlist stage.
set rpcmp_cpu_clock [rpcmp_required_clock {
    ic|mp_ram|altera_pll_i|outclk_wire[0]
    ic|mp_ram|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk
}]
set rpcmp_audio_clock [rpcmp_required_clock {
    ic|mp1|mf_pllbase_inst|altera_pll_i|outclk_wire[0]
    ic|mp1|mf_pllbase_inst|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk
}]
foreach box {control_mailbox feed_mailbox capture_mailbox journal_mailbox} {
    set prefix "*rpcmp_sound*${box}*"
    set_false_path -from $rpcmp_cpu_clock -to [rpcmp_required_registers "$prefix|request_meta"]
    set_false_path -from $rpcmp_audio_clock -to [rpcmp_required_registers "$prefix|ack_meta"]
    set request_from [rpcmp_required_registers "$prefix|request_held*"]
    set request_to [rpcmp_required_registers "$prefix|dst_request*"]
    set response_from [rpcmp_required_registers "$prefix|response_held*"]
    set response_to [rpcmp_required_registers "$prefix|src_response*"]
    # One receiver period, before the third-edge copy. Ownership keeps every
    # bundle unchanged through ACK, so asynchronous hold pairing is inapplicable.
    set_max_delay -from $request_from -to $request_to 81.380
    set_max_delay -from $response_from -to $response_to 11.111
    set_false_path -hold -from $request_from -to $request_to
    set_false_path -hold -from $response_from -to $response_to
}
foreach name {fault_meta inhibited_meta online_meta inhibit_ack_meta} {
    set_false_path -from $rpcmp_audio_clock -to [rpcmp_required_registers "*rpcmp_sound*|$name"]
}
set_false_path -from $rpcmp_cpu_clock -to [rpcmp_required_registers {*rpcmp_sound*|inhibit_meta}]

# Preserve the inherited exclusion ONLY for the separate legacy PCM serializer
# and FIFO. It does not drive Pocket AUDIO in this profile. The clock endpoints
# keep all same-domain legacy paths timed; the through filter cannot cover the
# new rpcmp_sound owner. This narrows the previous blanket CPU/audio exclusion,
# and does not claim a new proof of the legacy reset/FIFO crossings.
set rpcmp_legacy_audio [get_cells -compatibility_mode {*audio_out*}]
if {[get_collection_size $rpcmp_legacy_audio] == 0} { error "missing legacy audio cells" }
set_false_path -from $rpcmp_cpu_clock -to $rpcmp_audio_clock -through $rpcmp_legacy_audio
set_false_path -from $rpcmp_audio_clock -to $rpcmp_cpu_clock -through $rpcmp_legacy_audio
