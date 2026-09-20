// Single copied request/response CDC used by the sound MMIO owner.
// Both resets assert together; each deassertion is synchronized by that owner.
module rpcmp_sound_mailbox #(
    parameter integer REQUEST_BITS=1, RESPONSE_BITS=1
) (
    input logic src_clk, dst_clk, src_reset_n, dst_reset_n,
    input logic src_request_valid,
    output logic src_request_ready,
    input logic [REQUEST_BITS-1:0] src_request,
    output logic src_response_valid,
    input logic src_response_ready,
    output logic [RESPONSE_BITS-1:0] src_response,
    output logic dst_request_valid,
    input logic dst_request_ready,
    output logic [REQUEST_BITS-1:0] dst_request,
    input logic dst_response_valid,
    output logic dst_response_ready,
    input logic [RESPONSE_BITS-1:0] dst_response
);
    typedef enum logic [1:0] {SRC_IDLE, SRC_WAIT, SRC_RESPONSE} src_stage_t;
    typedef enum logic [1:0] {DST_IDLE, DST_REQUEST, DST_WAIT} dst_stage_t;
    src_stage_t src_stage;
    dst_stage_t dst_stage;
    logic request_toggle, ack_toggle, request_seen;
    logic [REQUEST_BITS-1:0] request_held;
    logic [RESPONSE_BITS-1:0] response_held;
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic request_meta, request_sync, ack_meta, ack_sync;

    assign src_request_ready=src_reset_n && src_stage==SRC_IDLE;
    assign src_response_valid=src_reset_n && src_stage==SRC_RESPONSE;
    assign dst_request_valid=dst_reset_n && dst_stage==DST_REQUEST;
    assign dst_response_ready=dst_reset_n && dst_stage==DST_WAIT;

    always_ff @(posedge src_clk or negedge src_reset_n) begin
        if (!src_reset_n) begin
            src_stage<=SRC_IDLE; request_toggle<=0; request_held<=0;
            ack_meta<=0; ack_sync<=0; src_response<=0;
        end else begin
            ack_meta<=ack_toggle; ack_sync<=ack_meta;
            case (src_stage)
                SRC_IDLE: if (src_request_valid) begin
                    request_held<=src_request; request_toggle<=~request_toggle; src_stage<=SRC_WAIT;
                end
                SRC_WAIT: if (ack_sync==request_toggle) begin
                    src_response<=response_held; src_stage<=SRC_RESPONSE;
                end
                SRC_RESPONSE: if (src_response_ready) src_stage<=SRC_IDLE;
                default: src_stage<=SRC_IDLE;
            endcase
        end
    end
    always_ff @(posedge dst_clk or negedge dst_reset_n) begin
        if (!dst_reset_n) begin
            dst_stage<=DST_IDLE; request_meta<=0; request_sync<=0; request_seen<=0;
            ack_toggle<=0; dst_request<=0; response_held<=0;
        end else begin
            request_meta<=request_toggle; request_sync<=request_meta;
            case (dst_stage)
                DST_IDLE: if (request_sync!=request_seen) begin
                    dst_request<=request_held; request_seen<=request_sync; dst_stage<=DST_REQUEST;
                end
                DST_REQUEST: if (dst_request_ready) dst_stage<=DST_WAIT;
                DST_WAIT: if (dst_response_valid) begin
                    response_held<=dst_response; ack_toggle<=request_seen; dst_stage<=DST_IDLE;
                end
                default: dst_stage<=DST_IDLE;
            endcase
        end
    end
endmodule
