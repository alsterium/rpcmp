`timescale 1ns/1ps
module jt51_media_audio_tb;
    logic clk_audio=0, reset_n=0, stream_reset=1;
    logic [1:0] pause_request=2'b10, dev_valid, dev_ready, media_enable, paused;
    logic [1:0] audio_mclk, audio_lrck, audio_dac, frame_boundary;
    logic [1:0] underflow, overflow, clipped;
    logic [7:0] address[2], value[2];
    logic [15:0] operations[512];
    integer operation_count=0, next_op[2]='{0,0}, ticks[2]='{0,0};
    integer wall=0, phase=0, p, channel, slot, completed_writes;
    integer read_frames=0, reference_frames=0, left_nonzero=0, right_nonzero=0;
    integer held_address=0, held_data=0, held_busy=0, held_sample=0, held_pending=0;
    logic [31:0] reference_pcm[4096];
    logic [15:0] decoded_left[2]='{0,0}, decoded_right[2]='{0,0};
    logic [1:0] active_frame=0;
    logic compare=1, send_operations=1;
    logic [1:0] enabled_before;
    logic [56:0] bus_before;
    logic [15:0] rate_buckets=0;
    always #5 clk_audio=~clk_audio;
    for (genvar n=0;n<2;n=n+1) begin: player
        rpcmp_jt51_media_audio dut (
            .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(stream_reset),
            .pause_request(pause_request[n]), .clear_audio_flags(1'b0),
            .dev_valid(dev_valid[n]), .dev_ready(dev_ready[n]),
            .dev_address(address[n]), .dev_value(value[n]), .device_idle(),
            .media_enable(media_enable[n]), .paused(paused[n]), .frame_boundary(frame_boundary[n]),
            .audio_mclk(audio_mclk[n]), .audio_lrck(audio_lrck[n]), .audio_dac(audio_dac[n]),
            .audio_underflow(underflow[n]), .audio_overflow(overflow[n]),
            .audio_clipped(clipped[n]), .selected_count(), .frame_count()
        );
    end
    // Identical timestamped authored operations, driven independently in each
    // player's media time. Player 0 is uninterrupted; player 1 inserts pauses.
    always @(negedge clk_audio) begin
        for (integer n=0;n<2;n=n+1) begin
            dev_valid[n]=0; address[n]=0; value[n]=0;
            if (send_operations && next_op[n]<operation_count) begin
                dev_valid[n]=ticks[n]>=next_op[n]*513;
                address[n]=operations[next_op[n]][15:8]; value[n]=operations[next_op[n]][7:0];
            end
        end
    end
    always @(posedge clk_audio) begin
        enabled_before=media_enable;
        bus_before={player[1].dut.cen_accum,player[1].dut.cen,player[1].dut.cen_p1,
                    player[1].dut.cen_phase,player[1].dut.state,player[1].dut.jt_wr_n,
                    player[1].dut.jt_a0,player[1].dut.jt_din,
                    player[1].dut.command_address,player[1].dut.command_value};
        if (!reset_n) begin wall=0; phase=0; end
        else begin
            wall=wall+1; phase=(phase+1)%256;
            for (integer n=0;n<2;n=n+1) begin
                if (dev_valid[n] && dev_ready[n]) next_op[n]=next_op[n]+1;
                if (media_enable[n]) ticks[n]=ticks[n]+1;
            end
            if (!stream_reset && frame_boundary[1] && pause_request[1]) begin
                if (!player[1].dut.jt_wr_n && !player[1].dut.jt_a0) held_address=held_address+1;
                if (!player[1].dut.jt_wr_n && player[1].dut.jt_a0) held_data=held_data+1;
                if (player[1].dut.jt_dout[7]) held_busy=held_busy+1;
                if (player[1].dut.jt_sample) held_sample=held_sample+1;
                if (player[1].dut.output_audio.pending) held_pending=held_pending+1;
                rate_buckets[player[1].dut.output_audio.rate_phase*16/3579545]=1;
            end
        end
        #1;
        if (audio_mclk!=={2{clk_audio}} || audio_lrck!=={2{phase>=128}})
            $fatal(1,"JT51 output clocks stopped or moved");
        if (reset_n && !stream_reset && !enabled_before[1] &&
            bus_before !== {player[1].dut.cen_accum,player[1].dut.cen,player[1].dut.cen_p1,
                            player[1].dut.cen_phase,player[1].dut.state,player[1].dut.jt_wr_n,
                            player[1].dut.jt_a0,player[1].dut.jt_din,
                            player[1].dut.command_address,player[1].dut.command_value})
            $fatal(1,"write/clock state moved during hold");
        if (reset_n && compare) begin
            if (phase==0) active_frame=~paused & {2{!stream_reset}};
            for (integer n=0;n<2;n=n+1) begin
                if (phase%4==2 && phase%128>=4 && phase%128<68) begin
                    if (phase<128) decoded_left[n]={decoded_left[n][14:0],audio_dac[n]};
                    else decoded_right[n]={decoded_right[n][14:0],audio_dac[n]};
                end
                if (phase%128<4 || phase%128>=68)
                    if (audio_dac[n]!==0) $fatal(1,"nonzero I2S padding");
            end
            if (phase==255) begin
                if (active_frame[0]) begin
                    if (reference_frames==4096) $fatal(1,"reference capacity exceeded");
                    reference_pcm[reference_frames]={decoded_left[0],decoded_right[0]};
                    reference_frames=reference_frames+1;
                end
                if (active_frame[1]) begin
                    if (read_frames>=reference_frames ||
                        {decoded_left[1],decoded_right[1]} !== reference_pcm[read_frames] ||
                        (^{decoded_left[1],decoded_right[1]})===1'bx)
                        $fatal(1,"PCM differs after removing pause frames at index=%0d",read_frames);
                    read_frames=read_frames+1;
                    if (decoded_left[1]!=0) left_nonzero=left_nonzero+1;
                    if (decoded_right[1]!=0) right_nonzero=right_nonzero+1;
                end else if ({decoded_left[1],decoded_right[1]}!==32'd0)
                    $fatal(1,"paused stereo frame is not silent");
            end
        end
        if (wall>2000000) $fatal(1,"bounded integrated pause test timeout");
    end
    task automatic append(input logic [7:0] a, input logic [7:0] v);
        if (operation_count==512) $fatal(1,"authored operation capacity");
        operations[operation_count]={a,v}; operation_count=operation_count+1;
    endtask
    task automatic at_phase(input integer target);
        do @(negedge clk_audio); while (phase!=target);
    endtask
    initial begin
        for (channel=0;channel<8;channel=channel+1) begin
            append(8'h20+channel,channel%2==0 ? 8'h47 : 8'h87);
            append(8'h28+channel,8'h38+channel); append(8'h30+channel,8'h20);
            append(8'h38+channel,8'h73);
            for (slot=0;slot<4;slot=slot+1) begin
                append(8'h40+slot*8+channel,8'h01); append(8'h60+slot*8+channel,8'h20+slot*8);
                append(8'h80+slot*8+channel,8'h1f); append(8'ha0+slot*8+channel,8'h80);
                append(8'hc0+slot*8+channel,8'h00); append(8'he0+slot*8+channel,8'h0f);
            end
            append(8'h08,8'h78+channel);
        end
        append(8'h0f,8'h9f); append(8'h18,8'hf2); append(8'h19,8'h60); append(8'h19,8'hb0);
        append(8'h10,8'hfc); append(8'h11,8'h03); append(8'h12,8'hfe); append(8'h14,8'h0f);
        for (p=0;operation_count<512;p=p+1) begin
            case (p%4)
                0: append(8'h28+p%8,8'h38+p%16);
                1: append(8'h1b,(p/4)%4);
                2: append(8'h08,8'h78+p%8);
                3: append(8'h14,8'h3f);
            endcase
        end
        repeat(4) @(negedge clk_audio); reset_n=1;
        repeat(2048) @(negedge clk_audio); stream_reset=0;
        // Empty initial hold preserves the independently known reset rate phase 0.
        repeat(513) @(negedge clk_audio); pause_request[1]=0;
        wait(!paused[1]);
        for (p=0;p<256;p=p+1) begin
            repeat(257+p%17) @(negedge clk_audio);
            at_phase(p); pause_request[1]=1;
            wait(paused[1]); @(negedge clk_audio);
            repeat(256*(1+p%3)) @(negedge clk_audio);
            at_phase(255-p); pause_request[1]=0;
            wait(!paused[1]); @(negedge clk_audio);
        end
        // Retain a native sample pulse exactly at the pause boundary, then consume
        // it once on resume. A held high pulse must not be captured every wall tick.
        do @(negedge clk_audio); while (!(frame_boundary[1] && player[1].dut.jt_sample));
        pause_request[1]=1; wait(paused[1]);
        repeat(513) @(negedge clk_audio); pause_request[1]=0; wait(!paused[1]);
        while(ticks[1]<380000) @(negedge clk_audio);
        if (read_frames<1400 || left_nonzero<100 || right_nonzero<100 ||
            next_op[1]!=512 || held_address==0 || held_data==0 || held_busy==0 ||
            held_pending==0 || held_sample==0 || rate_buckets!==16'hffff)
            $fatal(1,"missing integrated coverage frames=%0d writes=%0d addr=%0d data=%0d busy=%0d pending=%0d rates=%h",
                   read_frames,next_op[1],held_address,held_data,held_busy,held_pending,rate_buckets);
        completed_writes=next_op[1]; compare=0;
        // Present one final write just before a pause boundary, so reset must
        // cancel a retained transaction as well as clear old sound/pending data.
        at_phase(251); #2;
        operations[0]={8'h28,8'h60}; operation_count=1; next_op[1]=0;
        pause_request[1]=1; wait(paused[1]); send_operations=0;
        if (player[1].dut.state===0) $fatal(1,"reset case has no retained write");
        at_phase(73); stream_reset=1;
        repeat(2048) @(negedge clk_audio);
        if (player[1].dut.state!==0 || player[1].dut.sound.u_mmr.reg_sel!==0 || dev_ready[1])
            $fatal(1,"stream reset failed during native hold");
        stream_reset=0; repeat(512) @(negedge clk_audio);
        if (!paused[1] || player[1].dut.output_audio.pending) $fatal(1,"reset pending survived");
        pause_request[1]=0; wait(!paused[1]); repeat(2048) @(negedge clk_audio);
        if ({player[1].dut.output_audio.frame_left,player[1].dut.output_audio.frame_right}!==32'd0)
            $fatal(1,"old sound returned after reset");
        $display("jt51_media_audio_tb: PASS frames=%0d left=%0d right=%0d writes=%0d address=%0d data=%0d busy=%0d sample=%0d pending=%0d rate_buckets=16",
                 read_frames,left_nonzero,right_nonzero,completed_writes,held_address,held_data,
                 held_busy,held_sample,held_pending);
        $finish;
    end
endmodule
