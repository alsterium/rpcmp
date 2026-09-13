`timescale 1ns/1ps
// Read final native register banks, not adapter handshakes, as the oracle.
module jt51_media_burst_tb;
    logic clk_audio=0, reset_n=0, stream_reset=1, media_enable=0;
    logic dev_valid=0, dev_ready, device_idle, jt_sample;
    logic [7:0] dev_address=0, dev_value=0;
    logic signed [15:0] jt_left, jt_right;
    logic [63:0] source_edge;
    logic source_edge_exhausted;
    logic marker_valid=0, receipt_ready=1, marker_ready, receipt_valid, receipt_marker, operation_exhausted;
    logic [63:0] operation_token, receipt_token, receipt_at_edge;
    logic automatic_holds=0;
    integer wall=0, media=0, held=0, issued=0, checked=0, cases=0;
    integer opindex, hold_remaining=0;
    integer data_holds=0, address_holds=0, busy_holds=0;
    logic [31:0] updated_tl=0, scans_seen=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_media_source dut(.*);
    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>4000000) $fatal(1,"bounded native burst timeout");
        if (media_enable) media=media+1;
        if (dev_valid && dev_ready) issued=issued+1;
        if (media_enable && !stream_reset && dut.cen_p1 && dut.sound.u_mmr.u_reg.up_tl_op) begin
            opindex={dut.sound.u_mmr.up_op,dut.sound.u_mmr.up_ch};
            if (dut.sound.u_mmr.op_din==opindex+1) updated_tl[opindex]=1;
        end
    end
    always @(negedge clk_audio) if (automatic_holds) begin
        if (hold_remaining!=0) begin
            hold_remaining=hold_remaining-1;
            media_enable=hold_remaining==0;
        end else if (media%73==0) begin
            hold_remaining=1+(media/73)%17; media_enable=0;
        end
        if (!media_enable) begin
            held=held+1;
            if (!dut.jt_wr_n && dut.jt_a0) data_holds=data_holds+1;
            if (!dut.jt_wr_n && !dut.jt_a0) address_holds=address_holds+1;
            if (dut.jt_dout[7]) busy_holds=busy_holds+1;
        end
    end
    task automatic send(input logic [7:0] address, value);
        // No idle padding: the producer uses only the declared ready handshake.
        @(negedge clk_audio); #1; dev_address=address; dev_value=value; dev_valid=1;
        do @(posedge clk_audio); while (!dev_ready);
        @(negedge clk_audio); #1; dev_valid=0;
    endtask
    task automatic native_reset;
        @(negedge clk_audio); #1; automatic_holds=0; stream_reset=1; media_enable=0;
        repeat(2048) @(negedge clk_audio);
        #1; stream_reset=0; media_enable=1; hold_remaining=0;
        updated_tl=0;
    endtask
    task automatic drain;
        integer ticks;
        ticks=0;
        while(ticks<1024) begin
            @(negedge clk_audio); #1;
            if (media_enable) ticks=ticks+1;
        end
    endtask
    task automatic check_operators;
        integer samples, index;
        samples=0;
        // Stage VII TL belongs to (scan-6)%32; the 32-stage ring returns a
        // stored value at that same phase. MUL is VI, DT1/AR are II, KS is III.
        while(samples<64) begin
            @(negedge clk_audio); #1;
            if(media_enable && dut.cen_p1) begin
                index=(int'(dut.sound.cycles)+26)%32;
                if(dut.sound.tl_VII!==7'(index+1))
                    $fatal(1,"TL not reflected case=%0d op=%0d expected=%0d got=%0d issued=%0d",
                           cases,index,index+1,dut.sound.tl_VII,issued);
                index=(int'(dut.sound.cycles)+27)%32;
                if(dut.sound.mul_VI!==4'(index)) $fatal(1,"MUL not reflected");
                index=(int'(dut.sound.cycles)+31)%32;
                if(dut.sound.dt1_II!==3'(index>>2) || dut.sound.arate_II!==5'(31-index))
                    $fatal(1,"DT1/AR not reflected");
                index=(int'(dut.sound.cycles)+30)%32;
                if(dut.sound.ks_III!==2'(index>>3)) $fatal(1,"KS not reflected");
                scans_seen[dut.sound.cycles]=1; samples=samples+1; checked=checked+1;
            end
        end
        if(updated_tl!==32'hffffffff) $fatal(1,"missing TL update strobes");
    endtask
    initial begin
        repeat(4) @(negedge clk_audio); reset_n=1;
        // Vary reset/start offsets, with and without holds. The bank checks
        // cover every one of the 32 operator scan positions.
        for(integer seed=0;seed<32;seed=seed+1) begin
            native_reset();
            repeat(seed*7) @(negedge clk_audio);
            #1; automatic_holds=seed[0];
            for(integer op=0;op<32;op=op+1) begin
                send(8'h60+op,op+1);
                send(8'h40+op,((op>>2)<<4)|(op&15));
                send(8'h80+op,((op>>3)<<6)|(31-op));
            end
            drain(); check_operators(); cases=cases+1;
        end
        if(issued!=32*96 || checked!=32*64 || scans_seen!==32'hffffffff ||
           data_holds==0 || address_holds==0 || busy_holds==0) $fatal(1,"missing burst/hold coverage");
        $display("jt51_media_burst_tb: PASS cases=%0d writes=%0d scans=%0d held=%0d address=%0d data=%0d busy=%0d",
                 cases,issued,checked,held,address_holds,data_holds,busy_holds);
        $finish;
    end
endmodule
