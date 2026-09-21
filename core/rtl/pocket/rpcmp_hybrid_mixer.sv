// HYB1: reference FM limiter/gain, then wide PCM addition and final saturation.
module rpcmp_hybrid_mixer (
    input logic clk, reset_n, sample_valid, hold,
    input logic signed [18:0] fm_left, fm_right,
    input logic signed [31:0] pcm_left, pcm_right,
    input logic [18:0] fade_remaining,
    output logic mixed_valid,
    output logic signed [15:0] mixed_left, mixed_right
);
    logic gain_valid, sum_valid, fade_valid;
    logic [18:0] fade_delay1, fade_delay2;
    logic signed [35:0] faded_left, faded_right;
    logic signed [15:0] scaled_left, scaled_right;
    logic signed [31:0] delayed_pcm_left, delayed_pcm_right;
    logic signed [32:0] sum_left, sum_right;

    function automatic logic signed [15:0] scale_fm(input logic signed [18:0] value);
        logic signed [31:0] limited, product, scaled;
        begin
            if (value > 19'sd65535) limited = 32'sd65535;
            else if (value < -19'sd65536) limited = -32'sd65536;
            else limited = {{13{value[18]}}, value};
            product = limited * 32'sd8211;
            scaled = product >>> 14;
            if (scaled > 32'sd32767) scale_fm = 16'sh7fff;
            else if (scaled < -32'sd32768) scale_fm = 16'sh8000;
            else scale_fm = scaled[15:0];
        end
    endfunction

    function automatic logic signed [15:0] saturate(input logic signed [32:0] value);
        if (value > 33'sd32767) saturate = 16'sh7fff;
        else if (value < -33'sd32768) saturate = 16'sh8000;
        else saturate = value[15:0];
    endfunction

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            gain_valid <= 0; sum_valid <= 0; fade_valid <= 0; mixed_valid <= 0;
            fade_delay1 <= 0; fade_delay2 <= 0; faded_left <= 0; faded_right <= 0;
            scaled_left <= 0; scaled_right <= 0;
            delayed_pcm_left <= 0; delayed_pcm_right <= 0;
            sum_left <= 0; sum_right <= 0; mixed_left <= 0; mixed_right <= 0;
        end else if (!hold) begin
            gain_valid <= sample_valid;
            fade_delay1 <= fade_remaining;
            fade_delay2 <= fade_delay1;
            scaled_left <= scale_fm(fm_left);
            scaled_right <= scale_fm(fm_right);
            delayed_pcm_left <= pcm_left;
            delayed_pcm_right <= pcm_right;
            sum_valid <= gain_valid;
            sum_left <= {{17{scaled_left[15]}}, scaled_left} +
                        {delayed_pcm_left[31], delayed_pcm_left};
            sum_right <= {{17{scaled_right[15]}}, scaled_right} +
                         {delayed_pcm_right[31], delayed_pcm_right};
            fade_valid <= sum_valid;
            faded_left <= saturate(sum_left) * $signed({1'b0,fade_delay2});
            faded_right <= saturate(sum_right) * $signed({1'b0,fade_delay2});
            mixed_valid <= fade_valid;
            mixed_left <= 16'(faded_left / 36'sd312500);
            mixed_right <= 16'(faded_right / 36'sd312500);
        end
    end
endmodule
