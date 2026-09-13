// Shared native write/hold owner. See pocket-enveloped-audio-v1.
module rpcmp_jt51_media_source (
    input logic clk_audio, reset_n, stream_reset, media_enable,
    input logic dev_valid,
    output logic dev_ready,
    input logic [7:0] dev_address, dev_value,
    input logic marker_valid, receipt_ready,
    output logic marker_ready, receipt_valid, receipt_marker,
    output logic [63:0] operation_token, receipt_token, receipt_at_edge,
    output logic operation_exhausted,
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
    logic receipt_pending, operation_room;
    logic [63:0] command_token;

    assign cen_sum = {1'b0,cen_accum} + 26'd3579545;
    assign jt_reset = !reset_n || stream_reset;
    assign receipt_valid = receipt_pending && !jt_reset;
    assign operation_room = !jt_reset && media_enable && state == IDLE &&
                            (!receipt_pending || receipt_ready) && !operation_exhausted &&
                            !(&operation_token);
    assign dev_ready = operation_room;
    assign marker_ready = operation_room && !dev_valid;
    assign device_idle = state == IDLE;

    // One slot is reserved throughout an accepted write: no marker or later
    // write can enter while the bus owner is non-IDLE. Receipt delivery itself
    // may run during hold; it cannot advance native time or an in-flight write.
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            operation_token <= 1; operation_exhausted <= 0; command_token <= 0;
            receipt_pending <= 0; receipt_token <= 0; receipt_at_edge <= 0; receipt_marker <= 0;
        end else if (stream_reset) begin
            operation_token <= 1; operation_exhausted <= 0; command_token <= 0;
            receipt_pending <= 0; receipt_token <= 0; receipt_at_edge <= 0; receipt_marker <= 0;
        end else begin
            if (receipt_valid && receipt_ready) receipt_pending <= 0;
            if (media_enable) begin
                if ((dev_valid || marker_valid) && (&operation_token)) operation_exhausted <= 1;
                if (dev_valid && dev_ready) begin
                    command_token <= operation_token;
                    operation_token <= operation_token + 64'd1;
                end else if (marker_valid && marker_ready) begin
                    operation_token <= operation_token + 64'd1;
                    receipt_pending <= 1; receipt_token <= operation_token;
                    receipt_at_edge <= source_edge; receipt_marker <= 1;
                end
                if (state == HOLD_DATA && cen_p1) begin
                    receipt_pending <= 1; receipt_token <= command_token;
                    receipt_at_edge <= source_edge; receipt_marker <= 0;
                end
            end
        end
    end

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
