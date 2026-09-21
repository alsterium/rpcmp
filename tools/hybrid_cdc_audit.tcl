# Run quartus_sta -t <this file> in a fitted HYB1 project directory.
project_open ap_core
create_timing_netlist -model slow -temperature 85 -voltage 1100
read_sdc
set evidence [open hybrid-cdc-paths.txt w]
set sync_pairs {{cpu_reset_pipe[0]} {cpu_reset_pipe[1]} {audio_reset_pipe[0]} {audio_reset_pipe[1]} {clear_sync[0]} {clear_sync[1]} {start_sync[0]} {start_sync[1]} {pause_sync[0]} {pause_sync[1]} {ack_sync[0]} {ack_sync[1]}}
# Inspect each status bit, including any fitter-created copy of its second
# stage. A whole-bus count would reject valid replication, while simply
# increasing that count could hide a missing bit.
for {set bit 0} {$bit < 7} {incr bit} {
    lappend sync_pairs [format {status_meta[%d]} $bit] [format {status_sync[%d]} $bit]
}
foreach corner {{slow 85} {slow 0} {fast 85} {fast 0}} {
    lassign $corner model temperature
    set_operating_conditions -model $model -temperature $temperature -voltage 1100
    update_timing_netlist
    # Only the intended one-bit control and vendor Gray-pointer crossings may
    # be cut. Subsequent synchronizer stages must retain setup AND hold timing.
    foreach {first second} $sync_pairs {
        set a [rpcmp_hybrid_required "*rpcmp_sound*|$first" 1 1]
        set b [rpcmp_hybrid_required "*rpcmp_sound*|$second" 1 2]
        foreach analysis {setup hold} {
            set paths [get_timing_paths -$analysis -from $a -to $b -npaths 20 -nworst 1]
            if {[get_collection_size $paths] != [get_collection_size $b]} {error "incomplete sync timing $first -> $second"}
            foreach_in_collection p $paths {
                set slack [get_path_info -slack $p]
                if {$slack <= 0} {error "sync $analysis violation $first -> $second"}
                puts $evidence "$model $temperature $first->$second $analysis=$slack"
            }
        }
    }
    # Vendor pointers include the wrap bit; the fitter may replicate a source
    # register for local full/empty logic. Retain every copy in the path query.
    foreach fifo {pcm_fifo fm_fifo} width {13 11} {
        foreach pointer {rdptr_g delayed_wrptr_g} {
            set sources [rpcmp_hybrid_required "*rpcmp_sound*|*$fifo*|$pointer\[*\]" $width [expr {$width * 2}]]
            set targets [get_registers "*rpcmp_sound*|*$fifo*|*"]
            set paths [get_timing_paths -false_path -setup -from $sources -to $targets -npaths 100 -pairs_only]
            set bits {}
            set maximum 0
            foreach_in_collection p $paths {
                set delay [get_path_info -data_delay $p]
                set dest [get_node_info -name [get_path_info -to $p]]
                set src [get_node_info -name [get_path_info -from $p]]
                if {![regexp {\[([0-9]+)\](~DUPLICATE)?$} $src ignore bit]} {error "unknown Gray source: $src"}
                if {$bit >= $width} {error "Gray bit outside expected width"}
                dict set bits $bit 1
                if {![string match *dgrp* $dest] && ![string match *dgwp* $dest]} {error "unexpected Gray destination: $dest"}
                if {$delay > $maximum} {set maximum $delay}
            }
            if {[dict size $bits] != $width} {error "incomplete Gray bus: $fifo $pointer"}
            # Conservative absolute delay bound, stricter than a Gray-bit skew
            # bound: every route must fit the faster 90 MHz source period.
            if {$maximum >= 11.111} {error "Gray route exceeds fast clock period"}
            puts $evidence "$model $temperature $fifo $pointer bits=$width max_data_ns=$maximum limit=11.111"
        }
    }
    foreach {launch latch} [list $hybrid_cpu $hybrid_audio $hybrid_audio $hybrid_cpu] {
        set paths [get_timing_paths -setup -from [get_clocks $launch] -to [get_clocks $latch] -npaths 10000 -nworst 1]
        if {[get_collection_size $paths] != 0} {error "unexpected unprotected CPU/audio crossing"}
    }
    flush $evidence
}
report_exceptions -setup -file hybrid-cdc-exceptions.rpt
report_metastability -nchains 1000 -file hybrid-cdc-metastability.rpt
close $evidence
delete_timing_netlist
project_close
puts "PASS HYB3 pause/status synchronizers and all 48 Gray-pointer bits in four corners"
