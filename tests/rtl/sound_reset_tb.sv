`timescale 1ns/1ps
module sound_reset_tb;
    logic cpu_clk=0, audio_clk=0, reset_n=0;
    logic [31:0] mmio_addr=0, mmio_wr_data=0, mmio_rd_data;
    logic mmio_rd=0, mmio_wr=0;
    logic sound_reset_cpu_n, sound_reset_audio_n;
    int audio_low_cycles;

    always #5 cpu_clk = ~cpu_clk;
    always #19 audio_clk = ~audio_clk;

    rpcmp_sound_reset #(.AUDIO_RESET_CYCLES(8)) dut (
        .cpu_reset_n(reset_n), .audio_reset_n(reset_n), .*
    );

    task automatic write_reg(input logic [31:0] address, input logic [31:0] value);
        @(negedge cpu_clk); mmio_addr=address; mmio_wr_data=value; mmio_wr=1;
        @(negedge cpu_clk); mmio_wr=0;
    endtask

    initial begin
        repeat(3) @(posedge cpu_clk); reset_n=1;
        #1; mmio_addr=32'h4000_0240; #1;
        if(mmio_rd_data!==32'h5253_4331) $fatal(1,"bad id");
        mmio_addr=32'h4000_0244; #1;
        if(mmio_rd_data!==32'h0001_0008) $fatal(1,"bad capability");

        write_reg(32'h4000_024c,1);
        if(sound_reset_cpu_n!==0) $fatal(1,"cpu reset not asserted");
        wait(sound_reset_audio_n===0);
        audio_low_cycles=0;
        while(sound_reset_audio_n===0) begin
            @(posedge audio_clk); audio_low_cycles++;
        end
        if(audio_low_cycles<8) $fatal(1,"audio reset too short: %0d",audio_low_cycles);
        wait(sound_reset_cpu_n===1);
        mmio_addr=32'h4000_0248; #1;
        if(mmio_rd_data[0]!==0 || mmio_rd_data[31:16]!==1) $fatal(1,"completion missing");

        write_reg(32'h4000_024c,1);
        wait(sound_reset_audio_n===0);
        repeat(3) @(posedge audio_clk);
        write_reg(32'h4000_024c,1);
        wait(sound_reset_cpu_n===1);
        mmio_addr=32'h4000_0248; #1;
        if(mmio_rd_data[31:16]!==2) $fatal(1,"generation mismatch");

        write_reg(32'h4000_024c,32'h4);
        mmio_addr=32'h4000_0248; #1;
        if(mmio_rd_data[1]!==1) $fatal(1,"invalid not sticky");
        write_reg(32'h4000_024c,32'h2);
        mmio_addr=32'h4000_0248; #1;
        if(mmio_rd_data[1]!==0) $fatal(1,"invalid not cleared");
        $display("sound_reset_tb: PASS");
        $finish;
    end
endmodule
