`timescale 1ns/1ps
module hybrid_axi_tb;
    reg clk_cpu=0, clk_core_12288=0, reset_n=0;
    always #5.556 clk_cpu=~clk_cpu;
    integer phase=0;
    initial begin
        if (!$value$plusargs("PHASE_PS=%d",phase)) phase=0;
        #(phase*0.001); forever #40.690 clk_core_12288=~clk_core_12288;
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
    integer reads=0, writes=0, consumed=0;
    reg [31:0] value;
    reg check_pcm=0;
    always @(posedge clk_core_12288) if (check_pcm && rpcmp_sound.pcm_pop) begin
        if ($signed(rpcmp_sound.pcm_head[63:32]) !== 4096+consumed ||
            $signed(rpcmp_sound.pcm_head[31:0]) !== -4096-consumed) $fatal(1,"AXI PCM order/value");
        consumed=consumed+1;
    end
    task automatic rd(input [31:0] address, output [31:0] data, input [1:0] response=0);
        @(negedge clk_cpu); s_axi_araddr=address; s_axi_arvalid=1; s_axi_rready=0;
        do @(posedge clk_cpu); while(!s_axi_arready);
        @(negedge clk_cpu); s_axi_arvalid=0;
        do @(posedge clk_cpu); while(!s_axi_rvalid);
        data=s_axi_rdata;
        if (s_axi_rresp!==response || !s_axi_rlast) $fatal(1,"AXI read response %h",address);
        repeat(5) begin
            @(negedge clk_cpu);
            if (!s_axi_rvalid || s_axi_rdata!==data || s_axi_rresp!==response || !s_axi_rlast)
                $fatal(1,"R response changed under backpressure");
        end
        @(negedge clk_cpu); s_axi_rready=1; @(posedge clk_cpu); reads=reads+1;
    endtask
    task automatic wr(input [31:0] address, data, input [1:0] response=0,
                      input [3:0] mask=15, input integer delay_cycles=0);
        @(negedge clk_cpu); s_axi_awaddr=address; s_axi_awvalid=1; s_axi_bready=0;
        s_axi_wdata=data; s_axi_wstrb=mask; s_axi_wvalid=delay_cycles==0;
        do @(posedge clk_cpu); while(!s_axi_awready);
        if(delay_cycles==0 && !s_axi_wready) $fatal(1,"bundled AW/W not accepted");
        @(negedge clk_cpu); s_axi_awvalid=0;
        if(delay_cycles!=0) begin
            repeat(delay_cycles) @(negedge clk_cpu); s_axi_wvalid=1;
            do @(posedge clk_cpu); while(!s_axi_wready);
            @(negedge clk_cpu);
        end
        s_axi_wvalid=0;
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        if(s_axi_bresp!==response) $fatal(1,"AXI write %h expected %h actual %h",address,response,s_axi_bresp);
        // A new request must not steal ownership while B is held by the master.
        @(negedge clk_cpu); s_axi_awaddr=BASE+'h14; s_axi_awvalid=1;
        s_axi_wdata='hbad0; s_axi_wvalid=1;
        repeat(7) begin
            @(negedge clk_cpu);
            if(!s_axi_bvalid || s_axi_bresp!==response || s_axi_awready || s_axi_wready)
                $fatal(1,"B response/ownership changed under backpressure");
        end
        @(negedge clk_cpu); s_axi_awvalid=0; s_axi_wvalid=0;
        s_axi_bready=1; @(posedge clk_cpu); writes=writes+1;
    endtask
    task automatic ready;
        do begin rd(BASE+4,value); end while(!value[0]);
        if(value!=1) $fatal(1,"not cleared: %h",value);
    endtask
    initial begin
        repeat(10) @(negedge clk_cpu); reset_n=1;
        repeat(20) @(negedge clk_cpu);
        // In particular this precedes any write: AXI has no read WSTRB.
        rd(BASE,value); if(value!='h48594231) $fatal(1,"wrong HYB1 binding");
        ready(); wr(BASE+8,2,2); wr(BASE+'h14,1,2);
        for(integer n=0;n<15;n=n+1) begin
            wr(BASE+'h10,99,2,4'(n)); wr(BASE+'h14,1,2);
            rd(BASE,value); if(value!='h48594231) $fatal(1,"read inherited bad WSTRB");
        end
        for(integer n=1;n<4;n=n+1) begin wr(BASE+'h10+n,99,2); rd(BASE+n,value,2); end
        wr(BASE+'h110,99,2); rd(BASE+'h100,value,2);
        s_axi_awburst=2; wr(BASE+'h10,99,2); s_axi_awburst=1;
        s_axi_wlast=0; wr(BASE+'h10,99,2); s_axi_wlast=1;
        // Unsupported read bursts return SLVERR on every beat without MMIO work.
        @(negedge clk_cpu); s_axi_araddr=BASE; s_axi_arlen=2; s_axi_arvalid=1;
        do @(posedge clk_cpu); while(!s_axi_arready);
        @(negedge clk_cpu); s_axi_arvalid=0;
        for(integer n=0;n<3;n=n+1) begin
            do @(posedge clk_cpu); while(!s_axi_rvalid);
            if(s_axi_rresp!==2 || s_axi_rdata!==0 || s_axi_rlast!==(n==2)) $fatal(1,"read burst accepted");
            @(negedge clk_cpu);
        end
        s_axi_arlen=0;
        // Reject and drain a three-beat write burst without staging or committing PCM.
        @(negedge clk_cpu); s_axi_awaddr=BASE+'h10; s_axi_awlen=2; s_axi_awvalid=1; s_axi_bready=0;
        do @(posedge clk_cpu); while(!s_axi_awready);
        @(negedge clk_cpu); s_axi_awvalid=0;
        for(integer n=0;n<3;n=n+1) begin
            s_axi_wvalid=1; s_axi_wlast=(n==2); s_axi_wdata=123+n; s_axi_wstrb=15;
            do @(posedge clk_cpu); while(!s_axi_wready);
            @(negedge clk_cpu); s_axi_wvalid=0;
            repeat(2) @(negedge clk_cpu);
        end
        do @(posedge clk_cpu); while(!s_axi_bvalid);
        if(s_axi_bresp!==2) $fatal(1,"write burst accepted");
        @(negedge clk_cpu); s_axi_bready=1; @(posedge clk_cpu);
        s_axi_awlen=0; s_axi_wlast=1;
        wr(BASE+'h14,1,2);
        rd(BASE+'h18,value); if(value!=4096) $fatal(1,"write burst changed PCM FIFO");
        wr(BASE+'h20,125); wr(BASE+'h24,'h10000);
        for(integer n=0;n<125;n=n+1) begin
            wr(BASE+'h10,4096+n,0,15,n%3); wr(BASE+'h14,-4096-n,0,15,n%4);
        end
        rd(BASE+'h18,value); if(value!=3971) $fatal(1,"repeated/lost AXI commits");
        check_pcm=1; wr(BASE+8,2);
        do begin rd(BASE+4,value); end while(!value[3] && value[7:4]==0);
        if(value[7:4]!=0 || consumed!=125) $fatal(1,"AXI playback failed %h count %0d",value,consumed);
        check_pcm=0; wr(BASE+8,1); ready();
        rd(BASE+'h18,value); if(value!=4096) $fatal(1,"clear did not flush");
        wr(BASE+'h14,1,2); wr(BASE+'h24,1,2);
        $display("hybrid_axi_tb: PASS phase=%0d reads=%0d writes=%0d frames=%0d",phase,reads,writes,consumed);
        $finish;
    end
    initial begin #10000000; $fatal(1,"AXI watchdog"); end
endmodule
