`timescale 1ns/1ps
module media_source_queue_tb;
    logic clk_audio=0, reset_n=0, stream_reset=1, media_enable=0;
    logic [63:0] source_edge=0;
    logic item_valid=0, item_marker=0, item_end=0;
    logic [63:0] item_at=0, item_until=0;
    logic [7:0] item_address=0, item_value=0;
    logic [2:0] item_status;
    logic [6:0] queued;
    logic supply_fault, dev_valid, marker_valid;
    logic [7:0] dev_address, dev_value;
    logic dev_ready=0, marker_ready=0;
    logic [145:0] model[$];
    logic [145:0] expected;
    logic [63:0] model_until=0;
    logic model_end=0, expected_fault=0, check_model=1;
    integer accepted=0, dispatched=0, full=0, rejected=0, simultaneous=0, held=0, resets=0, faults=0;
    integer wall=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_media_source_queue dut(.*);

    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>20000) $fatal(1,"source queue timeout");
        if (check_model) begin
            if (!reset_n || stream_reset) begin
                model.delete(); model_until=0; model_end=0; expected_fault=0;
                if (dev_valid || marker_valid || item_status==1) $fatal(1,"reset handshake");
            end else begin
                if (queued!==7'(model.size()) || supply_fault!==expected_fault)
                    $fatal(1,"queue ownership/fault mismatch");
                if (!expected_fault && media_enable && model.size()==0 && dev_ready &&
                    !model_end && source_edge>=model_until) begin expected_fault=1; faults=faults+1; end
                if (dev_valid || marker_valid) begin
                    if (!media_enable || supply_fault || model.size()==0) $fatal(1,"invalid dispatch offer");
                    expected=model[0];
                    if (source_edge<expected[143:80] || marker_valid!==expected[145] ||
                        dev_valid===expected[145] || {dev_address,dev_value}!==expected[15:0])
                        $fatal(1,"changed/reordered/early dispatch");
                end
                if ((dev_valid && dev_ready) || (marker_valid && marker_ready)) begin
                    expected=model.pop_front(); dispatched=dispatched+1;
                    if (expected[145]) begin model_end=expected[144]; model_until=expected[79:16]; end
                    if (item_status==1) simultaneous=simultaneous+1;
                end
                if (item_status==1) begin
                    model.push_back({item_marker,item_end,item_at,item_until,item_address,item_value});
                    accepted=accepted+1; if (!media_enable) held=held+1;
                end
                if (item_status==2) full=full+1;
                if (item_status==3 || item_status==4) rejected=rejected+1;
            end
            #1;
            if (queued!==7'(model.size()) || supply_fault!==expected_fault)
                $fatal(1,"post-edge queue ownership/fault mismatch");
        end
    end
    task automatic edges(input integer n);
        repeat(n) @(negedge clk_audio);
        #1;
    endtask
    task automatic reset_stream;
        @(negedge clk_audio); #1;
        stream_reset=1; media_enable=0; item_valid=0; dev_ready=0; marker_ready=0; source_edge=0;
        edges(3); stream_reset=0; edges(2); resets=resets+1;
    endtask
    task automatic offer(input logic marker, ending, input logic [63:0] at_edge, until_edge,
                         input logic [7:0] address, value, input logic [2:0] status);
        @(negedge clk_audio); #1;
        item_valid=1; item_marker=marker; item_end=ending; item_at=at_edge;
        item_until=until_edge; item_address=address; item_value=value;
        #1; if (item_status!==status) $fatal(1,"admission expected=%0d got=%0d",status,item_status);
        @(negedge clk_audio); #1; item_valid=0;
    endtask
    task automatic drain;
        while(queued!=0) edges(1);
        edges(3);
    endtask
    initial begin
        edges(4); reset_n=1; reset_stream();
        // Wrong tick/payload/marker range reject before any state changes.
        offer(0,0,1,0,8'h20,8'hc7,3); offer(0,1,0,0,8'h20,0,3);
        offer(0,0,0,1,8'h20,0,3); offer(1,0,0,0,0,0,3);
        offer(1,1,0,1,0,0,3); offer(1,0,0,100,1,0,3);
        offer(1,0,0,100,0,1,3);
        for(integer i=0;i<64;i=i+1) offer(0,0,0,0,8'(i),8'(255-i),1);
        offer(0,0,0,0,8'h80,1,2); offer(0,0,1,0,0,0,3);
        // A full queue rejects on a simultaneous pop; retry then succeeds.
        media_enable=1; dev_ready=1; marker_ready=1;
        item_valid=1; item_marker=0; item_end=0; item_at=0; item_until=0; item_address=8'h80; item_value=1;
        #1; if (item_status!=2) $fatal(1,"full pop incorrectly bypassed admission");
        edges(1); #1; if (item_status!=1) $fatal(1,"full retry not admitted");
        edges(1); item_valid=0;
        offer(1,0,0,64'h8000000000000000,0,0,1);
        drain(); if (supply_fault) $fatal(1,"sealed idle faulted");
        // Future head, high bits and freeze; no default/wrapped timestamp.
        offer(0,0,64'h8000000000000000,0,8'h42,8'hab,1);
        offer(1,0,64'h8000000000000000,64'hffffffffffffffff,0,0,1);
        edges(5); if (queued!=2 || dev_valid) $fatal(1,"future head dispatched");
        source_edge=64'h7fffffffffffffff; edges(4);
        if (dev_valid) $fatal(1,"future head rounded early");
        media_enable=0; source_edge=64'h8000000000000000; edges(5);
        if (dev_valid) $fatal(1,"held dispatch");
        media_enable=1; drain();
        offer(1,1,64'hffffffffffffffff,0,0,0,1);
        offer(0,0,64'hffffffffffffffff,0,0,0,4);
        source_edge=64'hffffffffffffffff; drain();
        if (supply_fault) $fatal(1,"closed source underrun");

        reset_stream();
        // Pointer wrap and admission while native backpressure changes.
        for(integer i=0;i<300;i=i+1) begin
            media_enable=1; dev_ready=0; marker_ready=0;
            offer(0,0,0,0,8'(i),8'(i*7),1);
            dev_ready=1; marker_ready=1;
            while(queued!=0) edges(1);
            dev_ready=0; marker_ready=0;
            edges(3); if (supply_fault) $fatal(1,"busy/receipt wait counted as missing input");
        end
        // Empty eligible edge is a fault even when a refill arrives on it.
        item_valid=1; item_marker=0; item_end=0; item_at=0; item_until=0; item_address=8'h08; item_value=0;
        dev_ready=1; marker_ready=1; edges(1); item_valid=0;
        if (!supply_fault) $fatal(1,"late refill concealed underrun");
        offer(1,1,0,0,0,0,4); edges(4);

        reset_stream();
        offer(1,0,0,100,0,0,1); media_enable=1; dev_ready=1; marker_ready=1;
        drain(); source_edge=99; edges(3);
        if (supply_fault) $fatal(1,"coverage ended early");
        media_enable=0; source_edge=100; edges(3);
        if (supply_fault) $fatal(1,"pause created underrun");
        media_enable=1; edges(1);
        if (!supply_fault) $fatal(1,"missing exact coverage deadline fault");

        reset_stream();
        offer(0,0,0,0,8'h20,8'hc7,1);
        edges(2);
        // Cached first write and next marker are accepted/dispatched together.
        media_enable=1; dev_ready=1; marker_ready=1;
        item_valid=1; item_marker=1; item_end=1; item_at=0; item_until=0; item_address=0; item_value=0;
        edges(1); item_valid=0; drain();

        // Reset after enqueue and after cache prefetch; fresh stream starts at zero.
        for(integer delay=0;delay<3;delay=delay+1) begin
            reset_stream(); offer(0,0,0,0,8'h08,8'h78,1); edges(delay);
            reset_stream(); offer(1,1,0,0,0,0,1);
            media_enable=1; dev_ready=1; marker_ready=1; drain();
        end
        if (accepted<370 || dispatched<360 || full<2 || rejected<10 || held<64 || faults!=2 || simultaneous==0)
            $fatal(1,"missing queue coverage: %0d %0d %0d %0d %0d %0d %0d",accepted,dispatched,full,rejected,held,faults,simultaneous);
        $display("media_source_queue_tb: PASS accepted=%0d dispatched=%0d full=%0d rejected=%0d simultaneous=%0d held=%0d resets=%0d faults=%0d",
                 accepted,dispatched,full,rejected,simultaneous,held,resets,faults);
        $finish;
    end
endmodule
