// New M6 write/hold owner; legacy rpcmp_jt51_audio retains its v1 reset/queue contract.
module rpcmp_jt51_media_audio (
    input logic clk_audio, reset_n, stream_reset, pause_request, clear_audio_flags,
    input logic dev_valid,
    output logic dev_ready,
    input logic [7:0] dev_address, dev_value,
    output logic device_idle, media_enable, paused, frame_boundary,
    output logic audio_mclk, audio_lrck, audio_dac,
    output logic audio_underflow, audio_overflow, audio_clipped,
    output logic [31:0] selected_count, frame_count
);
    logic jt_sample;
    logic signed [15:0] jt_left, jt_right;
    rpcmp_jt51_media_source source_media (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(stream_reset),
        .media_enable(media_enable), .dev_valid(dev_valid), .dev_ready(dev_ready),
        .dev_address(dev_address), .dev_value(dev_value), .device_idle(device_idle),
        .marker_valid(1'b0), .marker_ready(), .operation_token(),
        .receipt_ready(1'b1), .receipt_valid(), .receipt_marker(),
        .receipt_token(), .receipt_at_edge(), .operation_exhausted(),
        .jt_sample(jt_sample), .jt_left(jt_left), .jt_right(jt_right),
        .source_edge(), .source_edge_exhausted()
    );
    rpcmp_pocket_media_audio output_audio (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(stream_reset),
        .pause_request(pause_request), .clear_flags(clear_audio_flags),
        .src_valid(jt_sample), .src_left({{2{jt_left[15]}},jt_left}),
        .src_right({{2{jt_right[15]}},jt_right}), .media_enable(media_enable),
        .paused(paused), .frame_boundary(frame_boundary), .audio_mclk(audio_mclk),
        .audio_lrck(audio_lrck), .audio_dac(audio_dac), .underflow(audio_underflow),
        .overflow(audio_overflow), .clipped(audio_clipped),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
