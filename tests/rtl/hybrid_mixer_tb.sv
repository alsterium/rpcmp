`timescale 1ns/1ps
module hybrid_mixer_tb;
    logic clk=0, reset_n=0, sample_valid=0;
    logic signed [18:0] fm_left=0, fm_right=0;
    logic signed [31:0] pcm_left=0, pcm_right=0;
    logic [18:0] fade_remaining=312500;
    wire mixed_valid;
    wire signed [15:0] mixed_left, mixed_right;
    integer checked=0;
    integer fade_checked=0;
    logic fade_test=0;
    longint fade_expected;
    logic signed [15:0] expected_left [0:7];
    logic signed [15:0] expected_right [0:7];
    rpcmp_hybrid_mixer dut(.*);
    always #5 clk=~clk;

    task automatic send(input integer fl, fr, pl, pr);
        @(negedge clk);
        sample_valid=1; fm_left=fl; fm_right=fr; pcm_left=pl; pcm_right=pr;
    endtask
    always @(negedge clk) if (reset_n && mixed_valid) begin
        if (fade_test) begin
            // 8211 FM + 16384 PCM = 24595. Exactly 312500 native frames = 5 seconds.
            fade_expected=64'd24595 * (312500-fade_checked) / 312500;
            if (fade_checked>312500 || mixed_left !== 16'(fade_expected) ||
                mixed_right !== 16'(-fade_expected)) $fatal(1,"fade frame %0d",fade_checked);
            fade_checked=fade_checked+1;
        end else begin
        if (checked >= 8) $fatal(1,"unexpected output");
        if (mixed_left !== expected_left[checked] || mixed_right !== expected_right[checked])
            $fatal(1,"mix %0d: got %0d,%0d expected %0d,%0d",checked,mixed_left,mixed_right,
                   expected_left[checked],expected_right[checked]);
        checked=checked+1;
        end
    end
    initial begin
        // Literal mathematical results, independent of the RTL helper functions.
        expected_left[0]=0; expected_right[0]=0;
        expected_left[1]=8211; expected_right[1]=-8211;
        expected_left[2]=22767; expected_right[2]=-22768;
        expected_left[3]=23578; expected_right[3]=-23578;
        expected_left[4]=32767; expected_right[4]=-32768;
        expected_left[5]=32767; expected_right[5]=-32768;
        expected_left[6]=32767; expected_right[6]=-32768;
        expected_left[7]=-1; expected_right[7]=-1;
        repeat(5) @(negedge clk);
        reset_n=1;
        send(0,0,0,0);
        send(16384,-16384,0,0);
        send(200000,-200000,-10000,10000);
        send(-32768,32768,40000,-40000);
        @(negedge clk); sample_valid=0;
        repeat(3) @(negedge clk);
        send(0,0,2147483647,-2147483648);
        send(200000,-200000,2147483647,-2147483648);
        send(0,0,32768,-32769);
        send(-1,0,0,-1);
        @(negedge clk); sample_valid=0;
        repeat(5) @(negedge clk);
        if (checked != 8) $fatal(1,"lost samples");
        // Reset in flight must discard the outstanding sample.
        send(16384,16384,0,0);
        @(negedge clk); reset_n=0; sample_valid=0;
        repeat(3) @(negedge clk);
        reset_n=1;
        repeat(5) @(negedge clk);
        if (checked != 8) $fatal(1,"reset leaked a sample");
        fade_test=1;
        for (int n=0;n<=312500;n=n+1) begin
            send(16384,-16384,16384,-16384); fade_remaining=19'(312500-n);
            if (n%49999==0) begin
                @(negedge clk); sample_valid=0;
                repeat(7) @(negedge clk);
            end
        end
        @(negedge clk); sample_valid=0;
        repeat(6) @(negedge clk);
        if (fade_checked!=312501) $fatal(1,"fade samples lost");
        $display("hybrid_mixer_tb: PASS samples=%0d fade_frames=%0d",checked,fade_checked);
        $finish;
    end
    initial begin #10000000; $fatal(1,"timeout"); end
endmodule
