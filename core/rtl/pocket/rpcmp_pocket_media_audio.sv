// Synchronous media owner and always-running APF serializer. See pocket-media-audio-v1.
module rpcmp_pocket_media_audio #(
    parameter integer SRC_RATE_NUM = 3579545,
    parameter integer SRC_RATE_DEN = 64,
    parameter integer OUT_RATE = 48000
) (
    input logic clk_audio, reset_n, stream_reset, pause_request, clear_flags,
    input logic src_valid,
    input logic signed [17:0] src_left, src_right,
    output logic media_enable, paused, frame_boundary,
    output logic audio_mclk, audio_lrck, audio_dac,
    output logic underflow, overflow, clipped,
    output logic [31:0] selected_count, frame_count
);
    logic signed [15:0] source_left, source_right;
    logic running;
    assign paused = !running;
    rpcmp_media_output #(.SRC_RATE_NUM(SRC_RATE_NUM), .SRC_RATE_DEN(SRC_RATE_DEN),
                         .OUT_RATE(OUT_RATE), .INITIAL_RUNNING(1)) output_media (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(stream_reset),
        .frame_consume(!pause_request), .clear_flags(clear_flags),
        .src_valid(src_valid), .src_left(src_left), .src_right(src_right),
        .src_at_edge(64'd0), .source_valid(), .source_at_edge(),
        .output_valid(), .output_at_edge(),
        .src_prefix(64'd0), .source_prefix(), .output_prefix(),
        .transformed_left(source_left), .transformed_right(source_right),
        .source_left(source_left), .source_right(source_right),
        .media_enable(media_enable), .running(running), .frame_boundary(frame_boundary),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac),
        .underflow(underflow), .overflow(overflow), .clipped(clipped),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
