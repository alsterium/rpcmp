`timescale 1ns/1ps
// Native finite-pipeline dependency proof. Retained waveform/value/counter/sign
// history is identical in both copies; multiplier intermediates differ.
module jt51_lfo_pipeline_tb;
    logic clk=0, rst=1, active=0;
    integer scan=0, age=0, checks=0, worst=0, cases=0, first_fresh=0, start_phase=0;
    integer period_checks=0;
    logic [255:0] covered=0;
    logic [12:0] schedule[256];
    logic [6:0] out1_fresh=0;
    logic [15:0] out2_fresh=0;
    logic carry_fresh=0, am_fresh=0, pm_fresh=0, sum_fresh, bit_fresh;
    logic [7:0] freq=8'hff, test_mode=0;
    logic [1:0] wave=0;
    wire [4:0] cycles=5'(scan);
    wire [7:0] am,pm,other_am,other_pm;
    wire [12:0] phase_state={cycles[3:0],dut.cnt1,dut.bitcnt,dut.bitcnt_rst};
    always #5 clk=~clk;
    jt51_lfo dut(.rst(rst),.clk(clk),.cen(1'b1),.cycles(cycles),
      .lfo_freq(freq),.lfo_amd(7'h65),.lfo_pmd(7'h73),.lfo_w(wave),
      .lfo_up(1'b0),.noise(scan[3]^scan[7]),.test(test_mode),.lfo_clk(),.am(am),.pm(pm));
    jt51_lfo other(.rst(rst),.clk(clk),.cen(1'b1),.cycles(cycles),
      .lfo_freq(freq),.lfo_amd(7'h65),.lfo_pmd(7'h73),.lfo_w(wave),
      .lfo_up(1'b0),.noise(scan[3]^scan[7]),.test(test_mode),.lfo_clk(),.am(other_am),.pm(other_pm));
    always @(posedge clk) begin
        if (rst) scan<=0; else scan<=scan+1;
        if (active) begin
            if (start_phase==0 && age>=256 && age<512) schedule[age-256]=phase_state;
            if (start_phase==0 && age>=512 && age<768) begin
                if ($isunknown(phase_state) || phase_state!==schedule[age-512])
                    $fatal(1,"serial schedule did not repeat its complete period");
                period_checks=period_checks+1;
            end
            if ((am_fresh && (am!==other_am || $isunknown(am))) ||
                (pm_fresh && (pm!==other_pm || $isunknown(pm))))
                $fatal(1,"native output disagrees after certified pipeline refresh");
            if ((dut.out1 & out1_fresh)!==(other.out1 & out1_fresh) ||
                (dut.out2 & out2_fresh)!==(other.out2 & out2_fresh) ||
                $isunknown({dut.out1 & out1_fresh,dut.out2 & out2_fresh}) ||
                (carry_fresh && (dut.integ_c!==other.integ_c || $isunknown(dut.integ_c))))
                $fatal(1,"tagged intermediate differs");
            if ($isunknown({dut.bitcnt,cycles,dut.bitsel})) $fatal(1,"unknown schedule");
            // Conservative dependencies include operands even when data masks
            // their effect. Constant bit seven, product restart and carry reset
            // remove dependencies by schedule, never by observed numeric values.
            bit_fresh=(&dut.bitcnt[2:0]) || out1_fresh[dut.bitsel];
            sum_fresh=bit_fresh && (dut.bitcnt[2:0]==0 || out2_fresh[0]) &&
                       (cycles[3:0]==15 || carry_fresh);
            if ((&dut.bitcnt[2:0]) && cycles[3:0]==15) begin
                if (dut.bitcnt[3]) pm_fresh=&out2_fresh[15:8];
                else am_fresh=&out2_fresh[15:8];
            end
            carry_fresh=sum_fresh;
            out2_fresh={sum_fresh,out2_fresh[15:1]};
            out1_fresh={out1_fresh[5:0],1'b1};
            age=age+1;
            if (am_fresh && pm_fresh) begin
                if (first_fresh==0) begin
                    first_fresh=age;
                    if(age>worst) worst=age;
                end
                checks=checks+1;
            end
            if (age>=512 && !(am_fresh && pm_fresh))
                $fatal(1,"serial pipeline still stale at age=%0d scan=%0d",age,scan);
        end
    end
    initial begin
        // bitcnt_rst is not reset by the native module. Cover both physical
        // initial values; its next assignment and later schedule are unmodified.
        for(integer cold=0;cold<2;cold=cold+1) begin
            covered=0;
            for(integer start=0;start<512;start=start+1) begin
                @(negedge clk); rst=1; active=0; start_phase=start;
                freq=8'(start); wave=2'(start>>6); test_mode=8'(start>>1);
                repeat(16) @(negedge clk);
                dut.bitcnt_rst=1'(cold); other.bitcnt_rst=1'(cold); rst=0;
                repeat(start) @(negedge clk);
                age=0; first_fresh=0; out1_fresh=0; out2_fresh=0;
                carry_fresh=0; am_fresh=0; pm_fresh=0;
                other.out1=~dut.out1; other.out2=~dut.out2; other.integ_c=~dut.integ_c;
                other.am=~dut.am; other.pm=~dut.pm;
                if (start>=256) covered[start-256]=1;
                active=1;
                repeat(768) @(negedge clk);
                active=0; cases=cases+1;
            end
            if (!(&covered)) $fatal(1,"missing steady serial start phase");
        end
        if (cases!=1024 || period_checks!=512 || worst>512 || checks==0)
            $fatal(1,"incomplete native serial proof");
        $display("jt51_lfo_pipeline_tb: PASS cases=%0d fresh_checks=%0d latest_first_fresh=%0d period_checks=%0d",
                 cases,checks,worst,period_checks);
        $finish;
    end
    initial begin #20000000; $fatal(1,"bounded LFO pipeline test timeout"); end
endmodule
