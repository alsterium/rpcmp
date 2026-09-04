`timescale 1ns/1ps
module device_queue_tb;
    localparam logic [31:0] BASE = 32'h4000_0200;
    logic cpu_clk=0, dev_clk=0, reset_n=0, mmio_rd=0, mmio_wr=0;
    logic [31:0] mmio_addr=0, mmio_wr_data=0, mmio_rd_data;
    logic dev_valid, dev_ready=1;
    logic [1:0] dev_kind;
    logic [7:0] dev_address, dev_value;
    integer events=0, index;
    logic stop_after_four=0;
    logic [15:0] observed [0:31];
    always #5 cpu_clk=~cpu_clk;
    always #7 dev_clk=~dev_clk;
    rpcmp_device_queue dut (.cpu_reset_n(reset_n), .dev_reset_n(reset_n), .*);
    always @(posedge dev_clk) if (reset_n && dev_valid && dev_ready) begin
        observed[events]={dev_address,dev_value}; events=events+1;
        if (stop_after_four && events==4) dev_ready<=0;
    end
    task automatic write_word(input logic [31:0] a, input logic [31:0] v);
        @(negedge cpu_clk); mmio_addr=a; mmio_wr_data=v; mmio_wr=1;
        @(negedge cpu_clk); mmio_wr=0; mmio_addr=0; mmio_wr_data=0;
    endtask
    task automatic expect_mask(input logic [31:0] a, input logic [31:0] m,
                               input logic [31:0] e, input string label_text);
        mmio_addr=a; #1;
        if ((mmio_rd_data&m)!==e) $fatal(1,"%s expected %08x got %08x",label_text,e,mmio_rd_data&m);
        mmio_addr=0;
    endtask
    task automatic push(input logic [63:0] due, input logic [7:0] a, input logic [7:0] v);
        write_word(BASE+32'h18,due[31:0]); write_word(BASE+32'h1c,due[63:32]);
        write_word(BASE+32'h20,32'h8001_0000|{16'd0,a,v});
    endtask
    task automatic wait_count(input integer wanted);
        integer timeout; begin timeout=0;
        while(events<wanted && timeout<500) begin @(posedge dev_clk); timeout=timeout+1; end
        if(events!=wanted) $fatal(1,"event timeout wanted %0d got %0d",wanted,events);
        end
    endtask
    logic [63:0] future_due;
    initial begin
        repeat(3) @(negedge cpu_clk);
        expect_mask(BASE,32'hffff_ffff,32'h5251_4d31,"id");
        expect_mask(BASE+4,32'hffff_ffff,32'h0001_0008,"capability");
        reset_n=1;
        future_due=dut.now_tick+30; push(future_due,8'h20,8'hc7);
        repeat(2) @(posedge dev_clk); if(events!=0) $fatal(1,"early dispatch");
        wait_count(1); if(observed[0]!==16'h20c7) $fatal(1,"future payload");

        dev_ready=0;
        for(index=0;index<8;index=index+1) push(0,8'h40+index,index);
        expect_mask(BASE+8,32'h0000_010f,32'h0000_0108,"full");
        push(0,8'h55,8'haa); expect_mask(BASE+8,32'h200,32'h200,"overflow");
        write_word(BASE+12,1); expect_mask(BASE+8,32'h200,0,"overflow clear");

        reset_n=0; repeat(2) @(posedge cpu_clk); reset_n=1; dev_ready=0; events=0;
        for(index=0;index<8;index=index+1) push(0,8'h60+index,index);
        stop_after_four=1; dev_ready=1;
        wait_count(4);
        for(index=8;index<12;index=index+1) push(0,8'h60+index,index);
        stop_after_four=0; dev_ready=1;
        wait_count(12);
        for(index=0;index<12;index=index+1)
            if(observed[index]!=={8'h60+index[7:0],index[7:0]}) $fatal(1,"order %0d",index);

        write_word(BASE+32'h20,32'h8002_0000);
        expect_mask(BASE+8,32'h400,32'h400,"invalid");
        write_word(BASE+12,2); expect_mask(BASE+8,32'h400,0,"invalid clear");

        dev_ready=0; push(0,8'h08,8'h08); wait(dev_valid===1);
        repeat(3) @(posedge dev_clk);
        if({dev_address,dev_value}!==16'h0808) $fatal(1,"backpressure bundle");
        dev_ready=1; wait_count(13);

        push(dut.now_tick+100,8'h28,8'h40); reset_n=0;
        repeat(2) @(posedge dev_clk); reset_n=1; repeat(20) @(posedge dev_clk);
        if(dev_valid!==0) $fatal(1,"reset survivor");
        expect_mask(BASE+8,32'h0000_0f0f,0,"reset status");
        $display("device_queue_tb: PASS events=%0d",events); $finish;
    end
endmodule
