`timescale 1ns/1ps
module hybrid_envelope_tb;
    logic clk=0, reset_n=0, clear=0, cycle_request=0, sample_step=0, loop_event=0;
    wire cycle_ack, fading, finished;
    wire [18:0] remaining;
    integer samples=0;
    rpcmp_hybrid_envelope dut(.*);
    always #5 clk=~clk;
    task automatic reset_policy;
        @(negedge clk); clear=1; cycle_request=0; sample_step=0; loop_event=0; reset_n=1;
        @(negedge clk); clear=0;
        if (remaining!=312500 || finished || fading || cycle_ack) $fatal(1,"policy reset");
    endtask
    task automatic cycle;
        @(negedge clk); cycle_request=~cycle_request;
        @(negedge clk);
        if(cycle_ack!=cycle_request) $fatal(1,"cycle not acknowledged");
        repeat(4) @(negedge clk); // Holding the toggle must not repeat it.
    endtask
    task automatic loop_done;
        @(negedge clk); loop_event=1;
        @(negedge clk); loop_event=0;
    endtask
    task automatic sample(input integer expected);
        @(negedge clk); sample_step=1;
        #1;
        if(finished || remaining!=expected)
            $fatal(1,"sample %0d gain=%0d expected=%0d ended=%b",samples,remaining,expected,finished);
        @(negedge clk); sample_step=0; samples=samples+1;
    endtask
    task automatic restore_gain(input integer previous);
        integer gain;
        gain=previous;
        while(gain<312500) begin
            gain= gain+250>312500 ? 312500 : gain+250;
            sample(gain);
        end
        if(fading || finished) $fatal(1,"cancelled fade did not restore");
    endtask
    initial begin
        integer count, held_gain;
        // Literal 2/3/5 counts and all 312500 gain positions per mode.
        for(integer mode=0;mode<3;mode=mode+1) begin
            reset_policy();
            repeat(mode) cycle();
            count= mode==0 ? 2 : mode==1 ? 3 : 5;
            for(integer n=1;n<count;n=n+1) begin
                loop_done(); sample(312500);
                if(fading) $fatal(1,"fade before requested complete loop");
            end
            loop_done();
            for(integer n=0;n<312500;n=n+1) begin
                sample(312500-n);
                if(n==12345) begin
                    held_gain=remaining;
                    repeat(71) @(negedge clk);
                    if(remaining!=held_gain || finished) $fatal(1,"paused fade advanced");
                end
            end
            if(!finished || remaining!=0) $fatal(1,"five-second end missing");
            repeat(10) @(negedge clk);
            if(!finished) $fatal(1,"completion unstable");
        end
        reset_policy(); repeat(3) cycle();
        repeat(32) begin loop_done(); sample(312500); end
        if(fading || finished) $fatal(1,"repeat one faded");
        cycle(); // Count has already passed two: fade now, do not restart count.
        for(integer n=0;n<200;n=n+1) sample(312500-n);
        cycle(); // Still past three: preserve the existing fade.
        sample(312300);
        cycle(); // Still past five: preserve it again.
        sample(312299);
        cycle(); // Repeat one cancels it.
        restore_gain(312299);

        reset_policy(); loop_done(); loop_done();
        for(integer n=0;n<156250;n=n+1) sample(312500-n);
        cycle(); // Increase to three while held halfway through a fade.
        repeat(100) @(negedge clk);
        restore_gain(156251);
        loop_done(); sample(312500); sample(312499);
        cycle(); // Five now requires more loops.
        restore_gain(312499);
        loop_done(); sample(312500);
        if(fading) $fatal(1,"fifth loop counted early");
        loop_done(); sample(312500); sample(312499);
        cycle(); restore_gain(312499);
        reset_policy();
        $display("hybrid_envelope_tb: PASS counts=2,3,5,one fade_samples=937500 live_pause_restore=1 total=%0d",samples);
        $finish;
    end
    initial begin #100000000; $fatal(1,"envelope watchdog"); end
endmodule
