`timescale 1ns/1ps
module pocket_media_audio_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, pause_request=0, clear_flags=0;
    logic src_valid=0;
    logic signed [17:0] src_left=0, src_right=0;
    logic media_enable, paused, frame_boundary, audio_mclk, audio_lrck, audio_dac;
    logic underflow, overflow, clipped;
    logic [31:0] selected_count, frame_count;
    logic reference_enable=1;
    wire reference_clk = clk_audio && reference_enable;
    logic compare=1;
    integer wall=0, media=0, phase=0, held=0, boundaries=0, p;
    integer pending_holds=0, empty_holds=0, source_holds=0, coincidences=0;
    logic [6:0] phases_seen=0;
    logic [15:0] expected_left=0, expected_right=0;
    logic advance_before;
    always #5 clk_audio=~clk_audio;
    rpcmp_pocket_media_audio #(.SRC_RATE_NUM(7),.SRC_RATE_DEN(1),.OUT_RATE(3)) dut (.*);
    // Original v1 converter is the independent oracle. Only its test clock stops.
    rpcmp_pocket_audio #(.SRC_RATE_NUM(7),.SRC_RATE_DEN(1),.OUT_RATE(3)) reference_audio (
        .clk_audio(reference_clk), .reset_n(reset_n), .clear_flags(clear_flags),
        .src_valid(src_valid), .src_left(src_left), .src_right(src_right),
        .audio_mclk(), .audio_lrck(), .audio_dac(), .underflow(), .overflow(),
        .clipped(), .selected_count(), .frame_count()
    );
    always @(negedge clk_audio) begin
        // Change a testbench clock gate only during the low clock level.
        #1; reference_enable = !reset_n || media_enable;
        if (compare) begin
            src_valid = (media/1024)%3 != 2 && (media % 19 == 0 || media % 256 == 255);
            src_left = (media * 7919) % 200003 - 100001;
            src_right = (media * 3571) % 199999 - 99999;
        end
    end
    always @(posedge clk_audio) begin
        advance_before = media_enable;
        if (!reset_n) begin wall=0; media=0; phase=0; end
        else begin
            wall=wall+1; phase=(phase+1)%256;
            if (advance_before) begin
                if (src_valid && frame_boundary) coincidences=coincidences+1;
                media=media+1;
            end else held=held+1;
            if (frame_boundary && pause_request && !paused) begin
                if (dut.output_media.pending) pending_holds=pending_holds+1;
                else empty_holds=empty_holds+1;
                if (src_valid) source_holds=source_holds+1;
                phases_seen[dut.output_media.rate_phase]=1;
            end
        end
        #1;
        if (audio_mclk !== clk_audio || audio_lrck !== (phase>=128))
            $fatal(1,"continuous clock/serial phase");
        if (reset_n && compare) begin
            if ({dut.output_media.rate_phase,dut.output_media.pending,dut.output_media.pending_left,dut.output_media.pending_right,selected_count,
                 underflow,overflow,clipped} !==
                {reference_audio.rate_phase,reference_audio.pending,reference_audio.pending_left,
                 reference_audio.pending_right,reference_audio.selected_count,
                 reference_audio.underflow,reference_audio.overflow,reference_audio.clipped})
                $fatal(1,"retained converter differs at media=%0d wall=%0d",media,wall);
            if (phase==0) begin
                boundaries=boundaries+1;
                expected_left = paused ? 16'd0 : reference_audio.frame_left;
                expected_right = paused ? 16'd0 : reference_audio.frame_right;
            end
            // Check every physical bit, including the one-bit I2S delay and padding.
            if (phase%128<4 || phase%128>=68) begin
                if (audio_dac !== 0) $fatal(1,"I2S delay/padding is not zero");
            end else if (audio_dac !== (phase<128 ?
                         expected_left[16-(phase%128)/4] : expected_right[16-(phase%128)/4]))
                $fatal(1,"decoded sample differs at media=%0d phase=%0d",media,phase);
        end
        if (wall>1000000) $fatal(1,"bounded output test timeout");
    end

    task automatic at_phase(input integer target);
        do @(negedge clk_audio); while (phase!=target);
    endtask
    initial begin
        repeat(4) @(negedge clk_audio); reset_n=1;
        for (p=0;p<256;p=p+1) begin
            // Burst/gap source windows exercise occupied and empty pause boundaries.
            repeat(257+p%17) @(negedge clk_audio);
            at_phase(p); pause_request=1;
            wait(paused); @(negedge clk_audio);
            repeat(256*(1+p%3)) @(negedge clk_audio);
            at_phase(255-p); pause_request=0;
            wait(!paused); @(negedge clk_audio);
        end
        if (pending_holds==0 || empty_holds==0 || source_holds==0 || coincidences==0 ||
            phases_seen!==7'h7f || boundaries<512 || !(underflow && overflow && clipped))
            $fatal(1,"missing output coverage pending=%0d empty=%0d source=%0d phases=%h flags=%b%b%b",
                   pending_holds,empty_holds,source_holds,phases_seen,underflow,overflow,clipped);
        // A request withdrawn before the boundary must not mute or freeze a frame.
        at_phase(10); pause_request=1;
        at_phase(20); pause_request=0;
        at_phase(0); if (paused) $fatal(1,"withdrawn pause applied");
        // Independent urgent reset/clear checks after the unmodified-oracle run.
        compare=0;
        pause_request=1; wait(paused);
        @(negedge clk_audio); clear_flags=1;
        @(negedge clk_audio); clear_flags=0;
        if (underflow || overflow || clipped) $fatal(1,"paused flag clear");
        at_phase(73); stream_reset=1;
        @(negedge clk_audio);
        if (audio_dac!==0 || selected_count!==0 || dut.output_media.pending!==0 || !paused)
            $fatal(1,"urgent stream reset failed during hold");
        repeat(513) @(negedge clk_audio);
        stream_reset=0; src_valid=0;
        at_phase(12); pause_request=0;
        wait(!paused); repeat(300) @(negedge clk_audio);
        if (dut.output_media.frame_left!==0 || dut.output_media.frame_right!==0 || underflow || selected_count!==0)
            $fatal(1,"old pending sample survived reset");
        $display("pocket_media_audio_tb: PASS requests=256 frames=%0d held=%0d pending=%0d empty=%0d source=%0d coincidence=%0d rate_phases=7",
                 boundaries,held,pending_holds,empty_holds,source_holds,coincidences);
        $finish;
    end
endmodule
