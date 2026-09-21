# Apply only to the HYB1 instance. CPU/audio clocks must not be clock-group cut.
# Vendor DCFIFO supplies its pointer/memory constraints. Its reset chains are
# explicitly synchronized by READ/WRITE_ACLR_SYNCH=ON in the RTL.
proc rpcmp_hybrid_required {pattern minimum maximum} {
    set nodes [get_registers $pattern]
    set count [get_collection_size $nodes]
    if {$count < $minimum || $count > $maximum} {
        error "HYB1 CDC endpoint count $count outside $minimum..$maximum: $pattern"
    }
    return $nodes
}
proc rpcmp_hybrid_cdc {prefix cpu_name audio_name} {
    set cpu [get_clocks $cpu_name]
    set audio [get_clocks $audio_name]
    if {[get_collection_size $cpu] != 1 || [get_collection_size $audio] != 1} {
        error "HYB1 requires exactly one CPU clock and one audio clock"
    }
    foreach name {clear_sync start_sync pause_sync} {
        set first [rpcmp_hybrid_required [format {%s%s[0]} $prefix $name] 1 1]
        set_false_path -from $cpu -to $first
    }
    set first [rpcmp_hybrid_required [format {%sack_sync[0]} $prefix] 1 1]
    set_false_path -from $audio -to $first
    set status [rpcmp_hybrid_required [format {%sstatus_meta[*]} $prefix] 6 7]
    set_false_path -from $audio -to $status
    # The source is an asynchronous assertion to the vendor's two write-reset
    # flops. Their synchronously released output and every consumer remain timed.
    set reset_source [rpcmp_hybrid_required [format {%sclear_sync[1]} $prefix] 1 1]
    foreach fifo {pcm_fifo fm_fifo} {
        foreach flop {dffe12a dffe13a} {
            set dest [rpcmp_hybrid_required [format {%s*%s*|wraclr|%s[0]} $prefix $fifo $flop] 1 1]
            set_false_path -from $reset_source -to $dest
        }
    }
}
