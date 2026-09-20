# HYB1 only. core_constraints.sdc keeps this CPU/audio clock pair uncut.
source [file normalize [file join [file dirname [info script]] ../../core/rtl/pocket/rpcmp_hybrid_cdc.tcl]]
proc hybrid_shell_clock {names} {
    set matches {}
    foreach_in_collection clock [get_clocks *] {
        set name [get_clock_info -name $clock]
        if {[lsearch -exact $names $name] >= 0} { lappend matches $name }
    }
    if {[llength $matches] != 1} { error "missing/ambiguous HYB1 shell clock: $names" }
    return [lindex $matches 0]
}
set hybrid_cpu [hybrid_shell_clock {
    ic|mp_ram|altera_pll_i|outclk_wire[0]
    ic|mp_ram|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk
}]
set hybrid_audio [hybrid_shell_clock {
    ic|mp1|mf_pllbase_inst|altera_pll_i|outclk_wire[0]
    ic|mp1|mf_pllbase_inst|altera_pll_i|general[0].gpll~PLL_OUTPUT_COUNTER|divclk
}]
rpcmp_hybrid_cdc {*rpcmp_sound*|} $hybrid_cpu $hybrid_audio
# The unrelated inherited PCM peripheral is still present, but does not drive
# AUDIO in this variant. Preserve its existing scoped exception, not a clock cut.
set hybrid_legacy [get_cells -compatibility_mode {*audio_out*}]
if {[get_collection_size $hybrid_legacy] == 0} { error "missing legacy PCM cells" }
set_false_path -from [get_clocks $hybrid_cpu] -to [get_clocks $hybrid_audio] -through $hybrid_legacy
set_false_path -from [get_clocks $hybrid_audio] -to [get_clocks $hybrid_cpu] -through $hybrid_legacy
