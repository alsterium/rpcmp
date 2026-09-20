`timescale 1ns/1ps
module player_axi_tb;
    reg clk_cpu=0, clk_core_12288=0, reset_n=0;
    always #5.556 clk_cpu=~clk_cpu;
    integer phase=0;
    initial begin
        if (!$value$plusargs("PHASE_PS=%d",phase)) phase=0;
        #(phase*0.001);
        forever #40.690 clk_core_12288=~clk_core_12288;
    end
    reg s_axi_arvalid=0, s_axi_awvalid=0, s_axi_wvalid=0;
    reg [31:0] s_axi_araddr=0, s_axi_awaddr=0, s_axi_wdata=0;
    wire s_axi_arready, s_axi_awready, s_axi_wready, s_axi_rvalid, s_axi_rlast, s_axi_bvalid;
    wire [31:0] s_axi_rdata;
    wire [1:0] s_axi_rresp, s_axi_bresp;
    reg [7:0] s_axi_arlen=0, s_axi_awlen=0;
    reg [1:0] s_axi_awburst=1;
    reg [3:0] s_axi_wstrb=15;
    reg s_axi_wlast=1, s_axi_rready=1, s_axi_bready=1;
    wire audio_mclk, audio_lrck, audio_dac;
    `include "player_bindings.svh"
    localparam [31:0] BASE=32'h40000400;
    integer reads=0, writes=0, submits=0;
    reg [31:0] value, epoch;
    always @(posedge clk_cpu)
        if (rpcmp_mmio_wr && !rpcmp_mmio_error && rpcmp_mmio_addr==BASE+'h44) submits<=submits+1;

    task automatic rd(input [31:0] address, output [31:0] data, input [1:0] response=0, input integer stall=0);
        @(negedge clk_cpu); s_axi_araddr=address; s_axi_arvalid=1; s_axi_rready=0;
        do @(posedge clk_cpu); while(!s_axi_arready);
        @(negedge clk_cpu); s_axi_arvalid=0;
        do @(posedge clk_cpu); while(!s_axi_rvalid);
        data=s_axi_rdata;
        if (s_axi_rresp!==response || !s_axi_rlast) $fatal(1,"read response %h: %h/%h",address,s_axi_rresp,response);
        repeat(stall) begin
            @(negedge clk_cpu);
            if (!s_axi_rvalid || s_axi_rdata!==data || s_axi_rresp!==response || !s_axi_rlast)
                $fatal(1,"read response changed under backpressure");
        end
        @(negedge clk_cpu); s_axi_rready=1;
        @(posedge clk_cpu);
        reads=reads+1;
    endtask
    task automatic assert_read(input [31:0] address, data, input [1:0] response=0);
        reg [31:0] actual;
        rd(address,actual,response,5);
        if(actual!==data) $fatal(1,"read %h expected %h actual %h",address,data,actual);
    endtask
    task automatic wr(input [31:0] address, data, input [1:0] response=0,
                      input [3:0] mask=15, input integer delay_cycles=0, input integer stall=0);
        @(negedge clk_cpu); s_axi_awaddr=address; s_axi_awvalid=1; s_axi_bready=0;
        s_axi_wdata=data; s_axi_wstrb=mask; s_axi_wvalid=delay_cycles==0;
        do @(posedge clk_cpu); while(!s_axi_awready);
        if(delay_cycles==0 && !s_axi_wready) $fatal(1,"bundled AW/W not accepted");
        @(negedge clk_cpu); s_axi_awvalid=0;
        if(delay_cycles!=0) begin
            repeat(delay_cycles) @(negedge clk_cpu);
            s_axi_wvalid=1;
            do @(posedge clk_cpu); while(!s_axi_wready);
            @(negedge clk_cpu);
        end
        s_axi_wvalid=0;
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        if(s_axi_bresp!==response) $fatal(1,"write %h expected %h actual %h",address,response,s_axi_bresp);
        repeat(stall) begin
            @(negedge clk_cpu);
            if(!s_axi_bvalid || s_axi_bresp!==response) $fatal(1,"write response changed under backpressure");
        end
        @(negedge clk_cpu); s_axi_bready=1;
        @(posedge clk_cpu);
        writes=writes+1;
    endtask
    task automatic wait_status(input integer bit_number);
        for(integer n=0;n<10000;n=n+1) begin
            rd(BASE+'hc,value);
            if(value[bit_number]) return;
        end
        $fatal(1,"mailbox timeout %0d",bit_number);
    endtask
    task automatic held_write_response;
        @(negedge clk_cpu);
        s_axi_awaddr=BASE+'h20; s_axi_awvalid=1; s_axi_wdata=21; s_axi_wvalid=1; s_axi_bready=0;
        do @(posedge clk_cpu); while(!(s_axi_awready && s_axi_wready));
        @(negedge clk_cpu); s_axi_awvalid=0; s_axi_wvalid=0;
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        @(negedge clk_cpu); s_axi_awvalid=1; s_axi_awaddr=BASE+'h24;
        repeat(12) begin
            @(negedge clk_cpu);
            if(s_axi_awready || !s_axi_bvalid || s_axi_bresp!==0) $fatal(1,"held B overwritten by another write");
        end
        s_axi_bready=1;
        do @(posedge clk_cpu); while(!s_axi_awready);
        @(negedge clk_cpu); s_axi_awvalid=0; s_axi_wvalid=1; s_axi_wdata=22;
        do @(posedge clk_cpu); while(!s_axi_wready);
        @(negedge clk_cpu); s_axi_wvalid=0;
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        if(s_axi_bresp!==0) $fatal(1,"second write failed");
        assert_read(BASE+'h20,21); assert_read(BASE+'h24,22);
    endtask
    task automatic rejected_bursts;
        @(negedge clk_cpu); s_axi_araddr=BASE; s_axi_arlen=2; s_axi_arvalid=1;
        do @(posedge clk_cpu); while(!s_axi_arready);
        @(negedge clk_cpu); s_axi_arvalid=0;
        for(integer n=0;n<3;n=n+1) begin
            do @(posedge clk_cpu); while(!s_axi_rvalid);
            if(s_axi_rresp!==2 || s_axi_rdata!==0 || s_axi_rlast!==(n==2)) $fatal(1,"unsupported read burst");
            @(negedge clk_cpu);
        end
        s_axi_arlen=0;
        s_axi_bready=0; s_axi_awaddr=BASE+'h20; s_axi_awlen=1; s_axi_awvalid=1;
        do @(posedge clk_cpu); while(!s_axi_awready);
        @(negedge clk_cpu); s_axi_awvalid=0;
        for(integer n=0;n<2;n=n+1) begin
            s_axi_wdata=32'hbad00000+n; s_axi_wlast=n==1; s_axi_wvalid=1;
            do @(posedge clk_cpu); while(!s_axi_wready);
            @(negedge clk_cpu); s_axi_wvalid=0;
            repeat(2) @(negedge clk_cpu);
        end
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        if(s_axi_bresp!==2) $fatal(1,"unsupported write burst");
        @(negedge clk_cpu); s_axi_awlen=0; s_axi_wlast=1; s_axi_bready=1;
        assert_read(BASE+'h20,21); assert_read(BASE+'h24,22);
    endtask
    initial begin
        repeat(10) @(negedge clk_cpu); reset_n=1;
        repeat(50) @(negedge clk_cpu);
        assert_read(BASE,32'h52534d31); assert_read(BASE+4,32'h10001); assert_read(BASE+8,15);
        assert_read(BASE+'h18,12288000); assert_read(BASE+'h1c,48000);
        rd(32'h40000098,value);
        if ((value & 32'h7efffc10)!=0 || !value[8] || !value[24])
            $fatal(1,"CPU framebuffer profile advertised GPU or lost unrelated capabilities");
        assert_read(32'h40000200,0); assert_read(32'h40000800,0); assert_read(32'h40000c00,0);
        for(integer n=0;n<10;n=n+1) begin
            wr(BASE+'h20,32'h87650000+n,0,15,n%3,7);
            assert_read(BASE+'h20,32'h87650000+n);
        end
        for(integer n=0;n<15;n=n+1) begin
            wr(BASE+'h20,32'hffffffff,2,4'(n));
            assert_read(BASE+'h20,32'h87650009);
        end
        for(integer n=1;n<4;n=n+1) begin
            wr(BASE+'h20+n,99,2); assert_read(BASE+n,0,2);
        end
        wr(BASE,1,2); assert_read(BASE+'h7c,0,2); assert_read(BASE+'h180,0,2);
        held_write_response(); rejected_bursts();
        s_axi_awburst=2; wr(BASE+'h20,88,2); s_axi_awburst=1;
        s_axi_wlast=0; wr(BASE+'h20,88,2); s_axi_wlast=1;
        assert_read(BASE+'h20,21);
        // Actual Reset traverses AXI, both copied clock crossings and JT51.
        wr(BASE+'h20,41); wr(BASE+'h24,32'h12345678);
        wr(BASE+'h28,9); wr(BASE+'h2c,0); wr(BASE+'h30,0);
        wr(BASE+'h34,0); wr(BASE+'h38,0); wr(BASE+'h3c,0); wr(BASE+'h40,0);
        wr(BASE+'h44,1); wr(BASE+'h44,1,2);
        wait_status(1);
        assert_read(BASE+'h180,41); assert_read(BASE+'h184,32'h12345678);
        assert_read(BASE+'h188,9); assert_read(BASE+'h1ac,1);
        rd(BASE+'h1b0,epoch); if(!epoch) $fatal(1,"zero reset epoch");
        if(submits!=1) $fatal(1,"control submitted more than once");
        wr(BASE+'h48,1);
        wr(BASE+'h80,9); wr(BASE+'h84,0); wr(BASE+'h88,epoch); wr(BASE+'h8c,0);
        wr(BASE+'h90,0); wr(BASE+'h94,0); wr(BASE+'h98,256); wr(BASE+'h9c,0);
        wr(BASE+'ha0,0); wr(BASE+'ha4,0); wr(BASE+'ha8,1); wr(BASE+'hac,0);
        wr(BASE+'hb0,1); wait_status(3);
        assert_read(BASE+'hb8,1); assert_read(BASE+'hbc,9); assert_read(BASE+'hc4,epoch);
        wr(BASE+'hb4,1);
        wr(BASE+'h100,77); wr(BASE+'h104,0); wr(BASE+'h108,epoch); wr(BASE+'h10c,0);
        wr(BASE+'h110,1); wait_status(5);
        assert_read(BASE+'h200,77); assert_read(BASE+'h208,epoch); assert_read(BASE+'h280,1);
        wr(BASE+'h114,1);
        wr(BASE+'h300,55); wr(BASE+'h304,0); wr(BASE+'h308,epoch); wr(BASE+'h30c,0);
        wr(BASE+'h310,0); wr(BASE+'h314,0); wr(BASE+'h318,0); wr(BASE+'h31c,1);
        wait_status(13); assert_read(BASE+'h340,55); assert_read(BASE+'h348,epoch); assert_read(BASE+'h358,2);
        wr(BASE+'h320,1);
        // APF reset discards staged policy/control and retained responses.
        @(negedge clk_cpu); reset_n=0;
        repeat(20) @(negedge clk_cpu); reset_n=1;
        repeat(50) @(negedge clk_cpu);
        assert_read(BASE,32'h52534d31); assert_read(BASE+'h20,0); assert_read(BASE+'h180,0,2);
        if(audio_dac!==0 || rpcmp_sound_fault) $fatal(1,"startup not silent");
        $display("player_axi_tb: PASS phase=%0d reads=%0d writes=%0d",phase,reads,writes);
        $finish;
    end
    initial begin #2000000; $fatal(1,"test watchdog"); end
endmodule
