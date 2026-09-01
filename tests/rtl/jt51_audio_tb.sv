`timescale 1ns/1ps
module jt51_audio_tb;
    logic clk_audio=0,reset_n=0,clear_audio_flags=0,dev_valid=0;
    logic dev_ready; logic [1:0] dev_kind=0; logic [7:0] dev_address=0,dev_value=0;
    logic audio_mclk,audio_lrck,audio_dac,audio_underflow,audio_overflow,audio_clipped;
    logic device_idle;
    logic [31:0] selected_count,frame_count;
    integer writes=0,index,timeout;
    logic [15:0] observed [0:31];
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_audio dut (.*);

    always @(posedge clk_audio) if(dut.cen && !dut.jt_wr_n) begin
        observed[writes]={dut.jt_a0,dut.jt_din}; writes=writes+1;
    end
    task automatic send(input logic [1:0] kind,input logic [7:0] address,input logic [7:0] value);
        begin
        wait(dev_ready); @(negedge clk_audio); dev_kind=kind;dev_address=address;dev_value=value;dev_valid=1;
        @(negedge clk_audio); dev_valid=0;
        wait(!dev_ready); wait(dev_ready);
        end
    endtask

    initial begin
        repeat(4) @(negedge clk_audio); reset_n=1;
        send(0,0,0);
        send(1,8'h20,8'hc7); send(1,8'h28,8'h3c); send(1,8'h30,8'h00);
        send(1,8'h40,8'h01); send(1,8'h60,8'h00); send(1,8'h80,8'h1f);
        send(1,8'ha0,8'h00); send(1,8'hc0,8'h00); send(1,8'he0,8'h0f);
        send(1,8'h68,8'h7f); send(1,8'h70,8'h7f); send(1,8'h78,8'h7f);
        send(1,8'h08,8'h08); send(1,8'h28,8'h40); send(1,8'h08,8'h00);
        if(writes!=30) $fatal(1,"write count %0d",writes);
        for(index=0;index<15;index=index+1) begin
            if(observed[index*2][15:8]!==8'h00 || observed[index*2+1][15:8]!==8'h01)
                $fatal(1,"a0 order %0d",index);
        end
        timeout=0;
        while(selected_count<8 && timeout<200000) begin @(posedge clk_audio); timeout=timeout+1; end
        if(timeout==200000 || frame_count==0) $fatal(1,"audio did not advance");
        if(audio_mclk!==clk_audio) $fatal(1,"mclk");
        send(0,0,0);
        if(selected_count!==0 || frame_count!==0 || audio_dac!==0) $fatal(1,"device reset audio");
        $display("jt51_audio_tb: PASS writes=%0d",writes); $finish;
    end
endmodule
