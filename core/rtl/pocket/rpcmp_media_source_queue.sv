// Audio-domain scheduled batches. See media-source-queue-v1.
module rpcmp_media_source_queue #(parameter integer PAYLOAD_WIDTH = 1) (
    input logic clk_audio, reset_n, stream_reset, media_enable,
    input logic [63:0] source_edge,
    input logic item_valid, item_marker, item_end,
    input logic [63:0] item_at, item_until,
    input logic [7:0] item_address, item_value,
    input logic [PAYLOAD_WIDTH-1:0] item_payload,
    output logic [PAYLOAD_WIDTH-1:0] dispatch_payload,
    output logic [2:0] item_status,
    output logic [6:0] queued,
    output logic supply_fault,
    output logic dev_valid, marker_valid,
    output logic [7:0] dev_address, dev_value,
    input logic dev_ready, marker_ready
);
    // {marker, end, at, until, address, value}; cached head remains in occupancy.
    logic [145+PAYLOAD_WIDTH:0] entries[0:63];
    logic [145+PAYLOAD_WIDTH:0] head;
    logic [5:0] read_index, write_index;
    logic head_valid, admit_closed, sealed_end;
    logic [63:0] expected_at, sealed_until;
    logic valid_item, accept, dispatch, due, load_head;

    assign valid_item = item_at==expected_at &&
        (item_marker ? (item_address==0 && item_value==0 &&
            (item_end ? item_until==0 : item_until>item_at)) : (item_until==0 && !item_end));
    always_comb begin
        item_status=0;
        if (item_valid) begin
            if (!reset_n || stream_reset || supply_fault || admit_closed) item_status=4;
            else if (!valid_item) item_status=3;
            else if (queued==7'd64) item_status=2;
            else item_status=1;
        end
    end
    assign accept = item_status==1;
    assign due = reset_n && !stream_reset && !supply_fault && media_enable &&
                 head_valid && source_edge>=head[143:80];
    assign dev_valid = due && !head[145];
    assign marker_valid = due && head[145];
    assign dev_address = head_valid ? head[15:8] : 8'd0;
    assign dev_value = head_valid ? head[7:0] : 8'd0;
    assign dispatch_payload = head_valid ? head[146 +: PAYLOAD_WIDTH] : '0;
    assign dispatch = (dev_valid && dev_ready) || (marker_valid && marker_ready);
    assign load_head = reset_n && !stream_reset && !supply_fault && !head_valid && queued!=0;

    always_ff @(posedge clk_audio) begin
        if (accept) entries[write_index]<={item_payload,item_marker,item_end,item_at,item_until,item_address,item_value};
        if (load_head) head<=entries[read_index];
    end
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            read_index<=0; write_index<=0; queued<=0; head_valid<=0;
            expected_at<=0; admit_closed<=0; sealed_until<=0; sealed_end<=0; supply_fault<=0;
        end else if (stream_reset) begin
            read_index<=0; write_index<=0; queued<=0; head_valid<=0;
            expected_at<=0; admit_closed<=0; sealed_until<=0; sealed_end<=0; supply_fault<=0;
        end else if (!supply_fault) begin
            if (media_enable && queued==0 && dev_ready && !sealed_end && source_edge>=sealed_until)
                supply_fault<=1;
            if (load_head) head_valid<=1;
            if (dispatch) begin
                head_valid<=0; read_index<=read_index+6'd1;
                if (head[145]) begin
                    sealed_end<=head[144]; sealed_until<=head[79:16];
                end
            end
            if (accept) begin
                write_index<=write_index+6'd1;
                if (item_marker) begin expected_at<=item_until; admit_closed<=item_end; end
            end
            case ({accept,dispatch})
                2'b10: queued<=queued+7'd1;
                2'b01: queued<=queued-7'd1;
                default: ;
            endcase
        end
    end
endmodule
