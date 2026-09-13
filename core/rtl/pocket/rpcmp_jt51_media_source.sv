// Shared native write/hold owner. See pocket-enveloped-audio-v1.
module rpcmp_jt51_media_source (
    input logic clk_audio, reset_n, stream_reset, media_enable,
    input logic dev_valid,
    output logic dev_ready,
    input logic [7:0] dev_address, dev_value,
    output logic device_idle, jt_sample,
    output logic [63:0] source_edge,
    output logic source_edge_exhausted,
    output logic signed [15:0] jt_left, jt_right
);
    localparam logic [2:0] IDLE=0, WAIT_ADDR=1, HOLD_ADDR=2, WAIT_DATA=3, HOLD_DATA=4;
    logic [2:0] state;
    logic [7:0] command_address, command_value;
    logic [24:0] cen_accum;
    logic cen, cen_p1, cen_phase;
    logic [25:0] cen_sum;
    logic jt_wr_n, jt_a0;
    logic [7:0] jt_din, jt_dout;
    logic jt_reset;

    assign cen_sum = {1'b0,cen_accum} + 26'd3579545;
    assign jt_reset = !reset_n || stream_reset;
    assign dev_ready = media_enable && state == IDLE;
    assign device_idle = state == IDLE;

    // The pre-edge value identifies this retained source edge, including samples
    // observed by the converter on the same edge. It is not a write-commit tag.
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin source_edge <= 0; source_edge_exhausted <= 0; end
        else if (stream_reset) begin source_edge <= 0; source_edge_exhausted <= 0; end
        else if (media_enable && !source_edge_exhausted) begin
            if (&source_edge) source_edge_exhausted <= 1;
            else source_edge <= source_edge + 64'd1;
        end
    end

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            cen_accum <= 0; cen <= 0; cen_p1 <= 0; cen_phase <= 0;
        end else if (stream_reset || media_enable) begin
            cen <= 0; cen_p1 <= 0;
            if (cen_sum >= 26'd12288000) begin
                // The invariant accumulator < 12,288,000 bounds the sum below 2^24.
                cen_accum <= cen_sum[24:0] - 25'd12288000;
                cen <= 1; cen_phase <= ~cen_phase;
                if (cen_phase) cen_p1 <= 1;
            end else cen_accum <= cen_sum[24:0];
        end
    end

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            state <= IDLE; command_address <= 0; command_value <= 0;
            jt_wr_n <= 1; jt_a0 <= 0; jt_din <= 0;
        end else if (stream_reset) begin
            state <= IDLE; command_address <= 0; command_value <= 0;
            jt_wr_n <= 1; jt_a0 <= 0; jt_din <= 0;
        end else if (media_enable) begin
            case (state)
                IDLE: if (dev_valid && dev_ready) begin
                    command_address <= dev_address; command_value <= dev_value;
                    state <= WAIT_ADDR;
                end
                WAIT_ADDR: if (!jt_dout[7]) begin
                    jt_a0 <= 0; jt_din <= command_address; jt_wr_n <= 0; state <= HOLD_ADDR;
                end
                HOLD_ADDR: if (cen) begin jt_wr_n <= 1; state <= WAIT_DATA; end
                WAIT_DATA: if (!jt_dout[7]) begin
                    jt_a0 <= 1; jt_din <= command_value; jt_wr_n <= 0; state <= HOLD_DATA;
                end
                // Busy and the native register scan use cen_p1. A cen-only
                // pulse can miss busy and let the next write overwrite data
                // before its operator slot is visited.
                HOLD_DATA: if (cen_p1) begin jt_wr_n <= 1; state <= IDLE; end
                default: state <= IDLE;
            endcase
        end
    end

    rpcmp_hold_jt51 sound (
        .rpcmp_hold(!media_enable && !stream_reset), .rst(jt_reset), .clk(clk_audio),
        .cen(cen), .cen_p1(cen_p1), .cs_n(1'b0), .wr_n(jt_wr_n), .a0(jt_a0),
        .din(jt_din), .dout(jt_dout), .ct1(), .ct2(), .irq_n(),
        .sample(jt_sample), .left(jt_left), .right(jt_right), .xleft(), .xright()
    );
endmodule
