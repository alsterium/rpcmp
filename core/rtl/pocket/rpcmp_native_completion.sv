// Synchronous receipt-to-native prefix. See jt51-native-completion-v1.
module rpcmp_native_completion (
    input logic clk_audio, reset_n, stream_reset, media_enable,
    input logic [63:0] source_edge,
    input logic receipt_valid,
    output logic receipt_ready,
    input logic [63:0] receipt_token, receipt_at_edge,
    output logic [63:0] native_prefix,
    output logic completion_fault
);
    localparam logic [64:0] DELAY_EDGES = 65'd5273;
    logic [127:0] entries[0:31];
    logic [4:0] read_index, write_index;
    logic [5:0] count;
    logic [127:0] head;
    logic head_valid, retire, accept, store_entry;
    logic [64:0] due_sum;

    assign due_sum = {1'b0,receipt_at_edge} + DELAY_EDGES;
    assign retire = reset_n && !stream_reset && !completion_fault && media_enable &&
                    head_valid && source_edge >= head[63:0];
    assign receipt_ready = reset_n && !stream_reset && !completion_fault &&
                           (count < 6'd32 || retire);
    assign accept = receipt_valid && receipt_ready;
    assign store_entry = accept && !due_sum[64];

    // RAM has a synchronous read and no reset. Empty-to-nonempty and retirement
    // each precede a head load: no same-address read/write value is relied on.
    always_ff @(posedge clk_audio) begin
        if (store_entry) entries[write_index] <= {receipt_token,due_sum[63:0]};
        if (reset_n && !stream_reset && !completion_fault && !head_valid && count!=0)
            head <= entries[read_index];
    end
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            read_index<=0; write_index<=0; count<=0; head_valid<=0;
            native_prefix<=0; completion_fault<=0;
        end else if (stream_reset) begin
            read_index<=0; write_index<=0; count<=0; head_valid<=0;
            native_prefix<=0; completion_fault<=0;
        end else if (!completion_fault) begin
            if (accept && due_sum[64]) completion_fault<=1;
            if (!head_valid && count!=0) head_valid<=1;
            if (retire) begin
                native_prefix<=head[127:64];
                read_index<=read_index+5'd1; head_valid<=0;
            end
            if (store_entry) write_index<=write_index+5'd1;
            case ({store_entry,retire})
                2'b10: count<=count+6'd1;
                2'b01: count<=count-6'd1;
                default: ;
            endcase
        end
    end
endmodule
