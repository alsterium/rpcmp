// Native-sample loop/fade policy. Cycle requests are one-bit handshakes;
// sample_step and loop_event stop during pause, while settings remain writable.
module rpcmp_hybrid_envelope (
    input logic clk, reset_n, clear, cycle_request, sample_step, loop_event,
    output logic cycle_ack, fading, finished,
    output wire [18:0] remaining
);
    logic [1:0] mode;
    logic [2:0] loops;
    logic [18:0] elapsed, gain;
    wire [2:0] current_loops = loops < 5 && loop_event ? loops + 3'd1 : loops;
    wire [2:0] loop_limit = mode == 0 ? 3'd2 : mode == 1 ? 3'd3 : 3'd5;
    wire want_fade = mode != 3 && current_loops >= loop_limit;
    wire [18:0] target = want_fade && fading ? 19'd312500 - elapsed : 19'd312500;
    // At most 1250 native samples (20 ms) to restore a cancelled fade.
    assign remaining = target > gain + 19'd250 ? gain + 19'd250 : target;
    assign finished = want_fade && fading && elapsed == 312500;
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin mode<=0; cycle_ack<=0; end
        else if (clear) begin mode<=0; cycle_ack<=0; end
        else if (cycle_request != cycle_ack) begin
            mode<=mode+1'b1; cycle_ack<=cycle_request;
        end
    end
    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin loops<=0; fading<=0; elapsed<=0; gain<=312500; end
        else if (clear) begin loops<=0; fading<=0; elapsed<=0; gain<=312500; end
        else begin
            if (loop_event) loops<=current_loops;
            if (sample_step && !finished) begin
                fading<=want_fade;
                elapsed<=want_fade ? (fading ? elapsed+1'b1 : 19'd1) : 19'd0;
                gain<=remaining;
            end
        end
    end
endmodule
