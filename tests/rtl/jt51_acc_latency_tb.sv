`timescale 1ns/1ps
// Isolated pinned accumulator, all carriers enabled. No inferred latency constants
// are taken from its internals: inject one exact impulse at each scan position.
module jt51_acc_latency_tb;
    logic clk=0, rst=1;
    integer scan_edge=0, injected_phase=0, checks=0, impulses=0;
    integer expected_boundary, min_delay=1000, max_delay=0, delay;
    wire [4:0] phase=5'(scan_edge);
    wire signed [13:0] op_out=scan_edge==96+injected_phase ? 14'sd1024 : 14'sd0;
    wire signed [15:0] left, right, xleft, xright;
    always #5 clk=~clk;
    jt51_acc dut (
        .rst(rst), .clk(clk), .cen(1'b1),
        .m1_enters(phase[4:3]==0), .m2_enters(phase[4:3]==1),
        .c1_enters(phase[4:3]==2), .c2_enters(phase[4:3]==3),
        .op31_acc(1'b0), .rl_I(2'b11), .con_I(3'd7), .op_out(op_out),
        .ne(1'b0), .noise_mix(12'sd0), .left(left), .right(right), .xleft(xleft), .xright(xright)
    );
    always @(posedge clk) begin
        if (rst) scan_edge<=0;
        else begin
            // cur=0 is the actual top-level sample phase, read before this edge.
            if (scan_edge>=96 && phase==0) begin
                expected_boundary=96+(injected_phase<24 ? 64 : 96);
                if (left!==(scan_edge==expected_boundary ? 16'sd1024 : 16'sd0) || right!==left ||
                    xleft!==left || xright!==right)
                    $fatal(1,"accumulator impulse position phase=%0d edge=%0d expected=%0d got=%0d/%0d",
                           injected_phase,scan_edge,expected_boundary,left,right);
                checks=checks+1;
                if (scan_edge==expected_boundary) begin
                    impulses=impulses+1; delay=scan_edge-(96+injected_phase);
                    if (delay<min_delay) min_delay=delay;
                    if (delay>max_delay) max_delay=delay;
                end
            end
            scan_edge<=scan_edge+1;
        end
    end
    initial begin
        for (integer p=0;p<32;p=p+1) begin
            @(negedge clk); #1; rst=1; injected_phase=p;
            repeat(16) @(negedge clk);
            #1; rst=0;
            wait(scan_edge==257);
        end
        if (checks!=192 || impulses!=32 || min_delay!=41 || max_delay!=72)
            $fatal(1,"missing accumulator impulse coverage");
        $display("jt51_acc_latency_tb: PASS checks=%0d impulses=%0d enabled-step delay=%0d..%0d",checks,impulses,min_delay,max_delay);
        $finish;
    end
    initial begin #1000000; $fatal(1,"bounded accumulator probe timeout"); end
endmodule
