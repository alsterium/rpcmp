`timescale 1ns/1ps
module native_completion_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, media_enable=1;
    logic [63:0] source_edge=0, receipt_token=0, receipt_at_edge=0, native_prefix;
    logic receipt_valid=0, receipt_ready, completion_fault;
    logic [63:0] tokens[256], positions[256];
    integer first=0, last=0, accepted=0, retired=0, full_cycles=0, held_cycles=0;
    integer simultaneous=0, reset_cases=0, overflow_cases=0, wall=0;
    logic [63:0] previous=0, old_edge;
    logic old_enable, incoming, overflowing, old_full;
    always #5 clk_audio=~clk_audio;
    rpcmp_native_completion dut(.*);

    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>100000) $fatal(1,"bounded completion test timeout");
        old_edge=source_edge; old_enable=media_enable;
        incoming=receipt_valid && receipt_ready;
        overflowing=incoming && receipt_at_edge>64'hffffffffffffeb66;
        old_full=last-first==32;
        if (!reset_n || stream_reset) begin
            if (receipt_ready) $fatal(1,"reset admitted a receipt");
            first=0; last=0; previous=0;
        end else begin
            if (last-first>32) $fatal(1,"completion FIFO exceeded capacity");
            if (old_full && !receipt_ready) full_cycles=full_cycles+1;
            if (!media_enable) held_cycles=held_cycles+1;
            if (incoming && !overflowing) begin
                if (last==256) $fatal(1,"test oracle storage full");
                tokens[last]=receipt_token; positions[last]=receipt_at_edge;
                last=last+1; accepted=accepted+1;
            end
            if (overflowing) overflow_cases=overflow_cases+1;
        end
        #1;
        if (!reset_n || stream_reset) begin
            if (native_prefix!==0 || completion_fault) $fatal(1,"reset retained completion state");
        end else begin
            if (native_prefix!==previous) begin
                if (!old_enable || first==last || native_prefix!==tokens[first])
                    $fatal(1,"completion prefix advanced out of receipt order or during hold");
                // Subtraction avoids wrapping the independent expected age.
                if (old_edge<positions[first] || old_edge-positions[first]<5273)
                    $fatal(1,"early native completion");
                first=first+1; retired=retired+1;
                if(incoming) simultaneous=simultaneous+1;
            end else if (old_full && incoming && !overflowing)
                $fatal(1,"full queue accepted without retiring its old head");
            if (overflowing && (!completion_fault || receipt_ready))
                $fatal(1,"overflow was accepted as a successful completion");
            previous=native_prefix;
        end
    end
    task automatic tick;
        @(negedge clk_audio); #1;
    endtask
    task automatic reset_stream;
        tick(); stream_reset=1; receipt_valid=0; source_edge=0; media_enable=1;
        repeat(3) tick();
        stream_reset=0; reset_cases=reset_cases+1;
    endtask
    task automatic offer(input logic [63:0] token, position);
        tick(); receipt_token=token; receipt_at_edge=position; receipt_valid=1;
        do @(posedge clk_audio); while(!receipt_ready);
        tick(); receipt_valid=0;
    endtask
    task automatic drain(input logic [63:0] position);
        tick(); source_edge=position; media_enable=1;
        repeat(70) tick();
        if (first!=last) $fatal(1,"eligible receipts failed to retire within two edges per entry");
    endtask
    initial begin
        repeat(4) tick(); reset_n=1;
        reset_stream();
        // Receipt admission may finish while paused; the prefix must not move.
        media_enable=0; source_edge=1000;
        for(integer n=1;n<=32;n=n+1) offer(64'h8000000000000000+64'(n),64'(n));
        if(receipt_ready) $fatal(1,"32 held receipts did not fill the queue");
        source_edge=10000; receipt_valid=1; receipt_token=64'h8000000000000021; receipt_at_edge=33;
        repeat(19) tick();
        if(native_prefix!==0 || receipt_ready) $fatal(1,"held full queue retired");
        media_enable=1;
        do @(posedge clk_audio); while(!receipt_ready);
        tick(); receipt_valid=0;
        drain(10000);
        if(native_prefix!==64'h8000000000000021) $fatal(1,"full-width prefix lost");
        // A receipt exactly at the last safe addition remains valid and due at MAX.
        reset_stream();
        offer(64'hfffffffffffffffe,64'hffffffffffffeb66);
        source_edge=64'hfffffffffffffffe;
        repeat(8) tick();
        if (native_prefix!==0 || completion_fault) $fatal(1,"last safe due position rounded or wrapped");
        drain(64'hffffffffffffffff);
        if (native_prefix!==64'hfffffffffffffffe || completion_fault) $fatal(1,"last safe receipt lost");
        reset_stream(); media_enable=0;
        offer(1,64'hffffffffffffeb67);
        repeat(8) tick();
        if (!completion_fault || receipt_ready || native_prefix!==0) $fatal(1,"overflow not sticky");
        reset_stream(); source_edge=10000;
        // More than two RAM pointer wraps, while admission overlaps retirement.
        for(integer n=1;n<=96;n=n+1) offer(64'(n),64'(n));
        drain(10000);
        if(native_prefix!==96) $fatal(1,"wrapped queue lost its final receipt");
        // Reset occupied RAM before, during and after head prefetch.
        for(integer delay=0;delay<3;delay=delay+1) begin
            reset_stream(); offer(1,0);
            repeat(delay) tick();
            stream_reset=1; repeat(3) tick(); stream_reset=0; reset_cases=reset_cases+1;
            drain(10000);
            if(native_prefix!==0) $fatal(1,"old RAM head survived reset");
        end
        if (accepted-retired!=3 || accepted<100 || full_cycles<19 || held_cycles<64 ||
            simultaneous==0 || overflow_cases!=1 || reset_cases!=10)
            $fatal(1,"missing completion boundary coverage");
        $display("native_completion_tb: PASS accepted=%0d retired=%0d full=%0d held=%0d simultaneous=%0d resets=%0d overflow=%0d",
                 accepted,retired,full_cycles,held_cycles,simultaneous,reset_cases,overflow_cases);
        $finish;
    end
endmodule
