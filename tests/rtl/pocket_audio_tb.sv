`timescale 1ns/1ps
module pocket_audio_tb;
    logic clk_audio=0, reset_n=0, clear_flags=0, src_valid=0;
    logic signed [17:0] src_left=0, src_right=0;
    logic audio_mclk, audio_lrck, audio_dac, underflow, overflow, clipped;
    logic [31:0] selected_count, frame_count;
    integer index;
    always #5 clk_audio=~clk_audio;
    always @(clk_audio) begin
        #1;
        if(audio_mclk!==clk_audio) $fatal(1,"mclk passthrough");
        if(reset_n && audio_lrck!==dut.serial_phase[7]) $fatal(1,"lrck phase");
        if(reset_n && dut.serial_phase[6:2]>=16 && audio_dac!==0) $fatal(1,"dummy slot");
    end

    rpcmp_pocket_audio #(.SRC_RATE_NUM(7),.SRC_RATE_DEN(1),.OUT_RATE(3)) dut (.*);

    task automatic source(input integer left_value, input integer right_value);
        @(negedge clk_audio); src_left=left_value; src_right=right_value; src_valid=1;
        @(negedge clk_audio); src_valid=0; src_left=0; src_right=0;
    endtask
    task automatic clear;
        @(negedge clk_audio); clear_flags=1;
        @(negedge clk_audio); clear_flags=0;
    endtask
    task automatic expect_word(input logic channel, input logic [15:0] expected);
        integer bit_index; begin
        wait(dut.serial_phase==(channel?128:0) && audio_lrck==channel);
        for(bit_index=0;bit_index<16;bit_index=bit_index+1) begin
            wait(dut.serial_phase==((channel?128:0)+(bit_index*4)));
            #1; if(audio_dac!==expected[15-bit_index])
                $fatal(1,"channel %0d bit %0d expected %0d got %0d",channel,bit_index,expected[15-bit_index],audio_dac);
            @(posedge clk_audio);
        end
        end
    endtask

    initial begin
        repeat(3) @(negedge clk_audio); reset_n=1;
        repeat(256) @(posedge clk_audio); #1;
        if(underflow!==0 || frame_count!==1) $fatal(1,"startup silence");

        source(100, -100); source(1234, -2345); source(40000, -40000);
        if(selected_count!==1 || clipped!==1 || overflow!==0) $fatal(1,"selection or clipping");
        wait(dut.serial_phase==8'hff); @(posedge clk_audio); #1;
        fork expect_word(0,16'h7fff); expect_word(1,16'h8000); join

        for(index=0;index<14;index=index+1) source(index,index);
        if(selected_count!==7) $fatal(1,"rational count %0d",selected_count);
        if(overflow!==1) $fatal(1,"overflow not sticky");
        wait(dut.serial_phase==8'hff); @(posedge clk_audio);
        repeat(256) @(posedge clk_audio); #1;
        if(underflow!==1) $fatal(1,"underflow not sticky");
        clear();
        if(underflow||overflow||clipped) $fatal(1,"clear flags");

        reset_n=0; #1;
        if(audio_dac!==0 || audio_lrck!==0 || selected_count!==0 || frame_count!==0)
            $fatal(1,"reset outputs");
        $display("pocket_audio_tb: PASS selected=7"); $finish;
    end
endmodule
