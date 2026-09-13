`timescale 1ns/1ps
// The unmodified oracle sees only retained media clock edges. Its gated clock
// exists in this testbench only; the candidate always receives the fabric clock.
module jt51_hold_tb;
    logic clk=0, rst=1, hold=0, automatic_holds=0, check_samples=0;
    logic cen=0, cen_p1=0, cen_phase=0;
    logic [24:0] phase=0;
    logic [25:0] phase_sum;
    logic wr_n=1, a0=0;
    logic [7:0] din=0, reference_status, candidate_status;
    logic reference_sample, candidate_sample;
    logic signed [15:0] reference_left, reference_right, candidate_left, candidate_right;
    logic signed [15:0] reference_xleft, reference_xright, candidate_xleft, candidate_xright;
    logic reference_irq, candidate_irq, reference_ct1, reference_ct2, candidate_ct1, candidate_ct2;
    wire reference_clk = clk && (!hold || rst);
    integer media_cycles=0, wall_cycles=0, held_cycles=0, hold_remaining=0;
    integer samples=0, nonzero_left=0, nonzero_right=0, writes=0, pauses=0;
    integer address_held=0, data_held=0, busy_held=0;
    integer channel, slot, wave, wait_until;
    logic [255:0] paused_phases=0;

    always #5 clk=~clk;
    assign phase_sum = {1'b0, phase} + 26'd3579545;
    always @(posedge clk) begin
        wall_cycles <= wall_cycles + 1;
        if (!hold || rst) begin
            media_cycles <= media_cycles + 1;
            cen <= phase_sum >= 26'd12288000;
            cen_p1 <= 0;
            if (phase_sum >= 26'd12288000) begin
                phase <= phase_sum - 26'd12288000;
                cen_phase <= ~cen_phase;
                cen_p1 <= cen_phase;
            end else phase <= phase_sum[24:0];
        end else begin
            held_cycles <= held_cycles + 1;
            if (!wr_n && !a0) address_held <= address_held + 1;
            if (!wr_n && a0) data_held <= data_held + 1;
            if (reference_status[7]) busy_held <= busy_held + 1;
        end
    end
    always @(negedge clk) if (automatic_holds) begin
        if (hold_remaining != 0) begin
            hold_remaining = hold_remaining - 1;
            hold = hold_remaining != 0;
        end else if (media_cycles % 257 == 0) begin
            hold_remaining = 1 + (media_cycles / 257) % 19;
            hold = 1;
            paused_phases[media_cycles % 256] = 1;
            pauses = pauses + 1;
        end
    end

    jt51 reference_native(
        .rst(rst), .clk(reference_clk), .cen(cen), .cen_p1(cen_p1),
        .cs_n(1'b0), .wr_n(wr_n), .a0(a0), .din(din), .dout(reference_status),
        .ct1(reference_ct1), .ct2(reference_ct2), .irq_n(reference_irq),
        .sample(reference_sample), .left(reference_left), .right(reference_right),
        .xleft(reference_xleft), .xright(reference_xright)
    );
`ifdef RPCMP_HOLD_CEN_ONLY
    jt51 candidate_native(
        .rst(rst), .clk(clk), .cen(cen && !hold), .cen_p1(cen_p1 && !hold),
`else
    rpcmp_hold_jt51 candidate_native(
        .rpcmp_hold(hold), .rst(rst), .clk(clk), .cen(cen), .cen_p1(cen_p1),
`endif
        .cs_n(1'b0), .wr_n(wr_n), .a0(a0), .din(din), .dout(candidate_status),
        .ct1(candidate_ct1), .ct2(candidate_ct2), .irq_n(candidate_irq),
        .sample(candidate_sample), .left(candidate_left), .right(candidate_right),
        .xleft(candidate_xleft), .xright(candidate_xright)
    );

    always @(posedge clk) begin
        #1;
        if (candidate_native.u_mmr.reg_sel !== reference_native.u_mmr.reg_sel)
            $fatal(1,"MMR changed while held at media=%0d hold=%0d",media_cycles,hold);
        if ({candidate_status,candidate_irq,candidate_ct1,candidate_ct2} !==
            {reference_status,reference_irq,reference_ct1,reference_ct2})
            $fatal(1,"status mismatch at media=%0d",media_cycles);
        if ({candidate_sample,candidate_left,candidate_right,candidate_xleft,candidate_xright} !==
            {reference_sample,reference_left,reference_right,reference_xleft,reference_xright})
            $fatal(1,"native sample mismatch at media=%0d hold=%0d",media_cycles,hold);
        if (check_samples && !hold && reference_sample) begin
            if ((^{reference_left,reference_right}) === 1'bx)
                $fatal(1,"unknown native sample");
            samples = samples + 1;
            if (reference_left != 0) nonzero_left = nonzero_left + 1;
            if (reference_right != 0) nonzero_right = nonzero_right + 1;
        end
        if (wall_cycles > 2000000) $fatal(1,"bounded test timeout");
    end

    task automatic write_half(input logic select_data, input logic [7:0] value);
        begin
            while (reference_status[7] !== 1'b0) @(negedge clk);
            @(negedge clk); a0=select_data; din=value; wr_n=0;
            do @(posedge clk); while (!cen_p1 || hold);
            @(negedge clk); wr_n=1;
            @(negedge clk);
        end
    endtask
    task automatic write_register(input logic [7:0] address, input logic [7:0] value);
        begin
            write_half(0,address); write_half(1,value); writes=writes+1;
        end
    endtask

    initial begin
        // Warm every reset shift stage; the original source defines their reset cadence.
        repeat(2048) @(negedge clk); rst=0;
        // Pause with a newly presented address before the native register edge.
        // Merely disabling cen cannot hold the memory-mapped register block.
        @(negedge clk); a0=0; din=8'h20; wr_n=0; hold=1;
        repeat(13) @(negedge clk);
        hold=0;
        do @(posedge clk); while (!cen_p1);
        @(negedge clk); wr_n=1;
        automatic_holds=1;
        for (channel=0; channel<8; channel=channel+1) begin
            write_register(8'h20+channel, channel%2==0 ? 8'h47 : 8'h87);
            write_register(8'h28+channel, 8'h38+channel);
            write_register(8'h30+channel, 8'h20);
            write_register(8'h38+channel, 8'h73);
            for (slot=0; slot<4; slot=slot+1) begin
                write_register(8'h40+slot*8+channel, 8'h01);
                write_register(8'h60+slot*8+channel, 8'h20+slot*8);
                write_register(8'h80+slot*8+channel, 8'h1f);
                write_register(8'ha0+slot*8+channel, 8'h80);
                write_register(8'hc0+slot*8+channel, 8'h00);
                write_register(8'he0+slot*8+channel, 8'h0f);
            end
            write_register(8'h08,8'h78+channel);
        end
        write_register(8'h0f,8'h9f);
        write_register(8'h18,8'hf2);
        write_register(8'h19,8'h60);
        write_register(8'h19,8'hb0);
        write_register(8'h10,8'hfc);
        write_register(8'h11,8'h03);
        write_register(8'h12,8'hfe);
        write_register(8'h14,8'h0f);
        check_samples=1;
        for (wave=0; wave<4; wave=wave+1) begin
            write_register(8'h1b,wave);
            wait_until=media_cycles+40000;
            while(media_cycles<wait_until) @(negedge clk);
            write_register(8'h14,8'h3f);
            write_register(8'h28,8'h40+wave);
            write_register(8'h08,8'h00);
            write_register(8'h08,8'h78);
        end
        while(hold) @(negedge clk);
        automatic_holds=0;
        @(negedge clk); hold=1; rst=1; check_samples=0;
        repeat(2048) @(negedge clk);
        if (candidate_native.u_mmr.reg_sel !== 0 || candidate_status[7] !== 0)
            $fatal(1,"reset did not override hold");
        hold=0; rst=0;
        repeat(2048) @(negedge clk);
        if (samples<600 || nonzero_left<100 || nonzero_right<100 || pauses<500 ||
            address_held==0 || data_held==0 || busy_held==0 ||
            paused_phases !== {256{1'b1}})
            $fatal(1,"insufficient coverage samples=%0d left=%0d right=%0d pauses=%0d phases=%h",
                   samples,nonzero_left,nonzero_right,pauses,paused_phases);
        $display("jt51_hold_tb: PASS samples=%0d left=%0d right=%0d writes=%0d pauses=%0d held_cycles=%0d address=%0d data=%0d busy=%0d phases=256",
                 samples,nonzero_left,nonzero_right,writes,pauses,held_cycles,address_held,data_held,busy_held);
        $finish;
    end
endmodule
