`timescale 1ns/1ps
module m4_mdx_core_tb;
    logic clk_cpu=0,clk_audio=0,reset_n=0;
    logic audio_mclk,audio_lrck,audio_dac,sequence_enqueued,sequence_complete,queue_fault,audio_fault;
    integer writes=0,timeout,max_samples=0;
    logic [15:0] observed [0:65];
    always #5 clk_cpu=~clk_cpu;
    always #7 clk_audio=~clk_audio;
    rpcmp_m4_mdx_core #(.CPU_TICKS_PER_SECOND(480000)) dut (.*);
    always @(posedge clk_audio) begin
        if(dut.device.cen && !dut.device.jt_wr_n) begin
            observed[writes]={dut.device.jt_a0,dut.device.jt_din}; writes=writes+1;
        end
        if(dut.selected_count>max_samples) max_samples=dut.selected_count;
    end
    initial begin
        repeat(5) @(negedge clk_cpu);reset_n=1;timeout=0;
        while(!sequence_complete && timeout<100000) begin @(posedge clk_cpu);timeout=timeout+1; end
        if(timeout==100000) $fatal(1,"sequence timeout enqueued=%0d writes=%0d",sequence_enqueued,writes);
        if(writes!=66 || max_samples==0) $fatal(1,"sequence evidence writes=%0d samples=%0d",writes,max_samples);
        if(queue_fault || audio_fault) $fatal(1,"fault queue=%0d audio=%0d",queue_fault,audio_fault);
        if(observed[0]!==16'h0012 || observed[1]!==16'h01c8 ||
           observed[62]!==16'h0020 || observed[63]!==16'h0155 ||
           observed[64]!==16'h0008 || observed[65]!==16'h0100)
            $fatal(1,"ordered endpoints");
        $display("m4_mdx_core_tb: PASS writes=%0d samples=%0d",writes,max_samples);$finish;
    end
endmodule
