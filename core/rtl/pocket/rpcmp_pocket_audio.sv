module rpcmp_pocket_audio #(
    parameter integer SRC_RATE_NUM = 3579545,
    parameter integer SRC_RATE_DEN = 64,
    parameter integer OUT_RATE = 48000
) (
    input  logic clk_audio,
    input  logic reset_n,
    input  logic clear_flags,
    input  logic src_valid,
    input  logic signed [17:0] src_left,
    input  logic signed [17:0] src_right,
    output logic audio_mclk,
    output logic audio_lrck,
    output logic audio_dac,
    output logic underflow,
    output logic overflow,
    output logic clipped,
    output logic [31:0] selected_count,
    output logic [31:0] frame_count
);
    localparam integer PHASE_STEP = OUT_RATE * SRC_RATE_DEN;
    logic [31:0] rate_phase;
    logic [7:0] serial_phase;
    logic pending, started;
    logic signed [15:0] pending_left, pending_right;
    logic signed [15:0] frame_left, frame_right;
    logic [31:0] rate_sum;
    logic select_source;

    function automatic logic signed [15:0] saturate(input logic signed [17:0] value);
        if (value > 18'sd32767) saturate = 16'sh7fff;
        else if (value < -18'sd32768) saturate = 16'sh8000;
        else saturate = value[15:0];
    endfunction

    assign audio_mclk = clk_audio;
    always_comb begin
        rate_sum = rate_phase + PHASE_STEP;
        select_source = src_valid && (rate_sum >= SRC_RATE_NUM);
        audio_lrck = serial_phase[7];
        audio_dac = 1'b0;
        if (reset_n) begin
            if (!serial_phase[7] && serial_phase[6:2] < 16)
                audio_dac = frame_left[15-serial_phase[6:2]];
            else if (serial_phase[7] && serial_phase[6:2] < 16)
                audio_dac = frame_right[15-serial_phase[6:2]];
        end
    end

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            rate_phase <= 0;
            serial_phase <= 0;
            pending <= 0;
            started <= 0;
            pending_left <= 0;
            pending_right <= 0;
            frame_left <= 0;
            frame_right <= 0;
            underflow <= 0;
            overflow <= 0;
            clipped <= 0;
            selected_count <= 0;
            frame_count <= 0;
        end else begin
            serial_phase <= serial_phase + 1'b1;
            if (clear_flags) begin
                underflow <= 0;
                overflow <= 0;
                clipped <= 0;
            end

            if (serial_phase == 8'hff) begin
                frame_count <= frame_count + 1'b1;
                if (pending) begin
                    frame_left <= pending_left;
                    frame_right <= pending_right;
                end else begin
                    frame_left <= 0;
                    frame_right <= 0;
                    if (started) underflow <= 1'b1;
                end
                pending <= 1'b0;
            end

            if (src_valid) begin
                if (rate_sum >= SRC_RATE_NUM)
                    rate_phase <= rate_sum - SRC_RATE_NUM;
                else
                    rate_phase <= rate_sum;
            end

            if (select_source) begin
                if (pending && serial_phase != 8'hff) overflow <= 1'b1;
                pending_left <= saturate(src_left);
                pending_right <= saturate(src_right);
                pending <= 1'b1;
                started <= 1'b1;
                selected_count <= selected_count + 1'b1;
                if (src_left > 18'sd32767 || src_left < -18'sd32768 ||
                    src_right > 18'sd32767 || src_right < -18'sd32768)
                    clipped <= 1'b1;
            end
        end
    end
endmodule
