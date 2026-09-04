`timescale 1ns/1ps
module openfpgaos_sound_tb;
    logic cpu_clk=0, audio_clk=0, reset_n=0;
    logic [31:0] mmio_addr=0, mmio_wr_data=0, mmio_rd_data;
    logic mmio_rd=0, mmio_wr=0;
    logic audio_mclk, audio_lrck, audio_dac, sound_fault;
    int timeout;

    always #5 cpu_clk=~cpu_clk;
    always #19 audio_clk=~audio_clk;

    rpcmp_openfpgaos_sound dut (
        .cpu_reset_n(reset_n), .audio_reset_n(reset_n), .*
    );

    task automatic write_reg(input logic [31:0] address, input logic [31:0] value);
        @(negedge cpu_clk); mmio_addr=address; mmio_wr_data=value; mmio_wr=1;
        @(negedge cpu_clk); mmio_wr=0;
    endtask

    task automatic expect_read(input logic [31:0] address, input logic [31:0] value);
        mmio_addr=address; mmio_rd=1; #1;
        if(mmio_rd_data!==value) $fatal(1,"read %08x got %08x expected %08x",address,mmio_rd_data,value);
        mmio_rd=0;
    endtask

    initial begin
        repeat(4) @(posedge cpu_clk); reset_n=1;
        expect_read(32'h4000_0200,32'h5251_4d31);
        expect_read(32'h4000_0204,32'h0001_0008);
        expect_read(32'h4000_0240,32'h5253_4331);
        expect_read(32'h4000_0244,32'h0001_0100);

        write_reg(32'h4000_0220,32'h0000_0000);
        mmio_addr=32'h4000_0208; #1;
        if(mmio_rd_data[10]!==1) $fatal(1,"queue invalid missing");

        write_reg(32'h4000_024c,32'h1);
        timeout=0; mmio_addr=32'h4000_0248;
        while((mmio_rd_data[0]!==0 || mmio_rd_data[31:16]===0) && timeout<5000) begin
            @(posedge cpu_clk); timeout++;
        end
        if(timeout==5000) $fatal(1,"sound reset timeout");
        mmio_addr=32'h4000_0208; #1;
        if(mmio_rd_data!==0) $fatal(1,"queue survived sound reset: %08x",mmio_rd_data);
        mmio_addr=32'h4000_0250; #1;
        if(mmio_rd_data[2:0]!==0) $fatal(1,"audio fault after reset");
        @(negedge audio_clk); #1;
        if(audio_mclk!==audio_clk) $fatal(1,"audio mclk mismatch");
        if(sound_fault!==0) $fatal(1,"unexpected sound fault");

        $display("openfpgaos_sound_tb: PASS");
        $finish;
    end
endmodule
