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
    localparam integer PHASE_STEP = OUT_RATE * SRC_RATE_DEN;
    logic [31:0] rate_phase;
    logic [7:0] serial_phase;
    logic pending, started;
    logic signed [15:0] pending_left, pending_right, frame_left, frame_right;
    logic [31:0] rate_sum;
    logic select_source;

    function automatic logic signed [15:0] saturate(input logic signed [17:0] value);
        if (value > 18'sd32767) saturate = 16'sh7fff;
        else if (value < -18'sd32768) saturate = 16'sh8000;
        else saturate = value[15:0];
    endfunction

    assign audio_mclk = clk_audio;
    assign frame_boundary = serial_phase == 8'hff;
    // The edge which enters pause is already frozen; the resume edge is retained.
    assign media_enable = reset_n && !stream_reset &&
                          !(frame_boundary ? pause_request : paused);
    assign rate_sum = rate_phase + PHASE_STEP;
    assign select_source = src_valid && rate_sum >= SRC_RATE_NUM;
    always_comb begin
        audio_lrck = serial_phase[7];
        audio_dac = 1'b0;
        if (reset_n && serial_phase[6:2] >= 1 && serial_phase[6:2] <= 16) begin
            if (!serial_phase[7]) audio_dac = frame_left[16-serial_phase[6:2]];
            else audio_dac = frame_right[16-serial_phase[6:2]];
        end
    end

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            serial_phase <= 0;
            frame_count <= 0;
            rate_phase <= 0;
            pending <= 0; started <= 0; paused <= 0;
            pending_left <= 0; pending_right <= 0;
            frame_left <= 0; frame_right <= 0;
            underflow <= 0; overflow <= 0; clipped <= 0;
            selected_count <= 0;
        end else begin
            serial_phase <= serial_phase + 1'b1;
            if (frame_boundary) frame_count <= frame_count + 1'b1;
            if (stream_reset) begin
                rate_phase <= 0;
                pending <= 0; started <= 0; paused <= 1;
                pending_left <= 0; pending_right <= 0;
                frame_left <= 0; frame_right <= 0;
                underflow <= 0; overflow <= 0; clipped <= 0;
                selected_count <= 0;
            end else begin
                if (clear_flags) begin
                    underflow <= 0; overflow <= 0; clipped <= 0;
                end
                if (frame_boundary) begin
                    paused <= pause_request;
                    if (media_enable && pending) begin
                        frame_left <= pending_left;
                        frame_right <= pending_right;
                    end else begin
                        frame_left <= 0; frame_right <= 0;
                        if (media_enable && started) underflow <= 1;
                    end
                    if (media_enable) pending <= 0;
                end
                if (media_enable) begin
                    if (src_valid) begin
                        if (rate_sum >= SRC_RATE_NUM) rate_phase <= rate_sum - SRC_RATE_NUM;
                        else rate_phase <= rate_sum;
                    end
                    if (select_source) begin
                        if (pending && !frame_boundary) overflow <= 1;
                        pending_left <= saturate(src_left);
                        pending_right <= saturate(src_right);
                        pending <= 1; started <= 1;
                        selected_count <= selected_count + 1'b1;
                        if (src_left > 18'sd32767 || src_left < -18'sd32768 ||
                            src_right > 18'sd32767 || src_right < -18'sd32768) clipped <= 1;
                    end
                end
            end
        end
    end
endmodule
