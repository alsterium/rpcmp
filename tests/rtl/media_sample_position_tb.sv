`timescale 1ns/1ps
// Integer quotient selection and authored payloads are independent of rate_phase.
module media_sample_position_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, frame_consume=1, clear_flags=0;
    logic src_valid=0, source_valid, output_valid, media_enable, running, frame_boundary;
    logic signed [17:0] src_left=0, src_right=0;
    logic signed [15:0] source_left, source_right;
    logic [63:0] src_at_edge=0, source_at_edge, output_at_edge;
    logic audio_mclk, audio_lrck, audio_dac, underflow, overflow, clipped;
    logic [31:0] selected_count, frame_count;
    integer wall=0, media=0, phase=0, samples=0, selected=0, checked=0;
    integer dropped=0, replaced=0, coincident=0, held=0, empty=0, zero_positions=0;
    logic pending_valid=0, expected_valid=0, expected_running=1, enabled, boundary;
    logic [63:0] pending_edge=0, expected_edge=0;
    logic [31:0] pending_pcm=0, expected_pcm=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_media_output #(.SRC_RATE_NUM(7),.SRC_RATE_DEN(1),.OUT_RATE(3)) dut (
        .transformed_left(source_left), .transformed_right(source_right), .*
    );
    function automatic logic [15:0] clamp(input integer value);
        if (value>32767) return 16'h7fff;
        if (value< -32768) return 16'h8000;
        return 16'(value);
    endfunction
    always @(negedge clk_audio) begin
        #1;
        src_valid=(zero_positions!=0 || samples<3) &&
                  (media/1024)%3!=2 && (media%19==0 || media%256==255);
        // Include a valid zero and nonzero upper 32 bits. These are opaque
        // authored positions, not an oracle copied from a production counter.
        src_at_edge=samples==2 ? 64'd0 : (64'ha589c00000000000 | (64'(samples)*7907));
        src_left=(media*7919)%200003-100001;
        src_right=(media*3571)%199999-99999;
    end
    always @(posedge clk_audio) begin
        boundary=phase==255;
        enabled=reset_n && !stream_reset && (boundary ? frame_consume : expected_running);
        if (media_enable!==enabled) $fatal(1,"position clock ownership");
        if (!reset_n) begin wall=0; media=0; phase=0; end
        else begin wall=wall+1; phase=(phase+1)%256; end
        if (!reset_n || stream_reset) begin
            samples=0; pending_valid=0; pending_edge=0; pending_pcm=0;
            expected_valid=0; expected_edge=0; expected_pcm=0;
            expected_running=!reset_n;
        end else begin
            if (boundary) begin
                expected_running=frame_consume;
                expected_valid=enabled && pending_valid;
                expected_edge=expected_valid ? pending_edge : 64'd0;
                expected_pcm=expected_valid ? pending_pcm : 32'd0;
                if (enabled) begin
                    if (pending_valid) begin
                        checked=checked+1;
                        if (pending_edge==0) zero_positions=zero_positions+1;
                    end else empty=empty+1;
                    pending_valid=0;
                end else if (pending_valid) held=held+1;
            end
            if (enabled) begin
                media=media+1;
                if (src_valid) begin
                    if (((samples+1)*3)/7 != (samples*3)/7) begin
                        if (pending_valid) replaced=replaced+1;
                        if (boundary) coincident=coincident+1;
                        pending_valid=1; pending_edge=src_at_edge;
                        pending_pcm={clamp(int'(src_left)),clamp(int'(src_right))};
                        selected=selected+1;
                    end else dropped=dropped+1;
                    samples=samples+1;
                end
            end
        end
        #1;
        if (source_valid!==pending_valid || source_at_edge!==(pending_valid ? pending_edge : 64'd0) ||
            output_valid!==expected_valid || output_at_edge!==expected_edge)
            $fatal(1,"sample position mismatch media=%0d phase=%0d pending=%h/%h output=%h/%h",
                   media,phase,source_at_edge,pending_edge,output_at_edge,expected_edge);
        if (audio_mclk!==clk_audio || audio_lrck!==(phase>=128)) $fatal(1,"continuous serial clock");
        if (phase%128<4 || phase%128>=68) begin
            if (audio_dac!==0) $fatal(1,"serial padding");
        end else if (audio_dac!==(phase<128 ? expected_pcm[32-(phase%128)/4] : expected_pcm[16-(phase%128)/4]))
            $fatal(1,"serialized PCM does not belong to the reported position");
        if (wall>1000000) $fatal(1,"bounded position test timeout");
    end
    task automatic at_phase(input integer target);
        do @(negedge clk_audio); while (phase!=target);
    endtask
    initial begin
        repeat(4) @(negedge clk_audio); reset_n=1;
        // Preserve the first selected sample's valid zero position until output.
        wait(samples==3); @(negedge clk_audio); frame_consume=0;
        at_phase(0); frame_consume=1;
        for(integer p=0;p<256;p=p+1) begin
            repeat(257+p%17) @(negedge clk_audio);
            at_phase(p); frame_consume=0;
            wait(!running); repeat(256*(1+p%3)) @(negedge clk_audio);
            at_phase(255-p); frame_consume=1; wait(running);
        end
        if (checked<100 || dropped==0 || replaced==0 || coincident==0 || held==0 ||
            empty==0 || zero_positions==0) $fatal(1,"missing position coverage zero=%0d",zero_positions);
        // Reset invalidates an active partial frame and a held pending sample.
        wait(output_valid); at_phase(37); stream_reset=1;
        repeat(3) @(negedge clk_audio); stream_reset=0;
        wait(source_valid); frame_consume=0; wait(!running);
        at_phase(73); stream_reset=1; clear_flags=1;
        repeat(3) @(negedge clk_audio); stream_reset=0; clear_flags=0;
        repeat(300) @(negedge clk_audio);
        if (source_valid || output_valid) $fatal(1,"old sample position survived reset");
        $display("media_sample_position_tb: PASS output=%0d selected=%0d dropped=%0d replaced=%0d coincidence=%0d held=%0d empty=%0d zero=%0d",
                 checked,selected,dropped,replaced,coincident,held,empty,zero_positions);
        $finish;
    end
endmodule
