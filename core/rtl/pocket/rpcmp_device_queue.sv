module rpcmp_device_queue #(
    parameter logic [31:0] BASE_ADDR = 32'h4000_0200,
    parameter int unsigned DEPTH = 8
) (
    input logic cpu_clk, dev_clk, reset_n,
    input logic [31:0] mmio_addr, mmio_wr_data,
    input logic mmio_rd, mmio_wr,
    output logic [31:0] mmio_rd_data,
    output logic dev_valid,
    input logic dev_ready,
    output logic [1:0] dev_kind,
    output logic [7:0] dev_address, dev_value
);
    localparam int PTR_WIDTH = $clog2(DEPTH);
    localparam int COUNT_WIDTH = $clog2(DEPTH + 1);
    logic [63:0] due_mem [0:DEPTH-1];
    logic [1:0] kind_mem [0:DEPTH-1];
    logic [7:0] address_mem [0:DEPTH-1], value_mem [0:DEPTH-1];
    logic [PTR_WIDTH-1:0] write_ptr, read_ptr;
    logic [COUNT_WIDTH-1:0] count;
    logic [63:0] now_tick, staged_due;
    logic overflow_sticky, invalid_sticky, inflight;
    logic [1:0] transfer_kind;
    logic [7:0] transfer_address, transfer_value;
    logic request_toggle, ack_toggle, ack_sync_1, ack_sync_2, ack_seen;
    logic request_sync_1, request_sync_2, request_seen;

    wire queue_full = (count == DEPTH);
    wire push_address = (mmio_addr == BASE_ADDR + 32'h20);
    wire push_requested = mmio_wr && push_address && mmio_wr_data[31];
    wire push_valid = push_requested && (mmio_wr_data[30:18] == 0) &&
                      (mmio_wr_data[17:16] <= 1) &&
                      ((mmio_wr_data[17:16] != 0) || (mmio_wr_data[15:0] == 0));
    wire known_write = (mmio_addr == BASE_ADDR + 32'h0c) ||
                       (mmio_addr == BASE_ADDR + 32'h18) ||
                       (mmio_addr == BASE_ADDR + 32'h1c) || push_address;
    wire enqueue = push_valid && !queue_full;
    wire ack_arrived = inflight && (ack_sync_2 != ack_seen);
    wire dispatch_ready = !inflight && (count != 0) && (now_tick >= due_mem[read_ptr]);

    always_ff @(posedge cpu_clk or negedge reset_n) begin
        if (!reset_n) begin
            write_ptr <= '0; read_ptr <= '0; count <= '0; now_tick <= '0;
            staged_due <= '0; overflow_sticky <= 0; invalid_sticky <= 0;
            transfer_kind <= 0; transfer_address <= 0; transfer_value <= 0;
            request_toggle <= 0; inflight <= 0; ack_sync_1 <= 0;
            ack_sync_2 <= 0; ack_seen <= 0;
        end else begin
            now_tick <= now_tick + 1'b1;
            ack_sync_1 <= ack_toggle; ack_sync_2 <= ack_sync_1;
            if (mmio_wr && ((mmio_addr[1:0] != 0) || !known_write)) invalid_sticky <= 1;
            if (mmio_wr && push_address && !push_valid) invalid_sticky <= 1;
            if (push_valid && queue_full) overflow_sticky <= 1;
            if (mmio_wr && (mmio_addr == BASE_ADDR + 32'h0c)) begin
                if (mmio_wr_data[0]) overflow_sticky <= 0;
                if (mmio_wr_data[1]) invalid_sticky <= 0;
                if (mmio_wr_data[31:2] != 0) invalid_sticky <= 1;
            end
            if (mmio_wr && (mmio_addr == BASE_ADDR + 32'h18)) staged_due[31:0] <= mmio_wr_data;
            if (mmio_wr && (mmio_addr == BASE_ADDR + 32'h1c)) staged_due[63:32] <= mmio_wr_data;
            if (enqueue) begin
                due_mem[write_ptr] <= staged_due;
                kind_mem[write_ptr] <= mmio_wr_data[17:16];
                address_mem[write_ptr] <= mmio_wr_data[15:8];
                value_mem[write_ptr] <= mmio_wr_data[7:0];
                write_ptr <= write_ptr + 1'b1;
            end
            if (dispatch_ready) begin
                transfer_kind <= kind_mem[read_ptr];
                transfer_address <= address_mem[read_ptr];
                transfer_value <= value_mem[read_ptr];
                request_toggle <= ~request_toggle; inflight <= 1;
            end
            if (ack_arrived) begin
                ack_seen <= ack_sync_2; inflight <= 0; read_ptr <= read_ptr + 1'b1;
            end
            case ({enqueue, ack_arrived})
                2'b10: count <= count + 1'b1;
                2'b01: count <= count - 1'b1;
                default: count <= count;
            endcase
        end
    end

    always_ff @(posedge dev_clk or negedge reset_n) begin
        if (!reset_n) begin
            request_sync_1 <= 0; request_sync_2 <= 0; request_seen <= 0;
            ack_toggle <= 0; dev_valid <= 0; dev_kind <= 0;
            dev_address <= 0; dev_value <= 0;
        end else begin
            request_sync_1 <= request_toggle; request_sync_2 <= request_sync_1;
            if (!dev_valid && (request_sync_2 != request_seen)) begin
                request_seen <= request_sync_2; dev_kind <= transfer_kind;
                dev_address <= transfer_address; dev_value <= transfer_value; dev_valid <= 1;
            end
            if (dev_valid && dev_ready) begin
                dev_valid <= 0; ack_toggle <= request_seen;
            end
        end
    end

    always_comb begin
        mmio_rd_data = 0;
        if (mmio_addr == BASE_ADDR) mmio_rd_data = 32'h5251_4d31;
        else if (mmio_addr == BASE_ADDR + 4) mmio_rd_data = {16'd1, 8'd0, 8'(DEPTH)};
        else if (mmio_addr == BASE_ADDR + 8)
            mmio_rd_data = {20'd0, inflight, invalid_sticky, overflow_sticky,
                            queue_full, 4'd0, 4'(count)};
        else if (mmio_addr == BASE_ADDR + 32'h10) mmio_rd_data = now_tick[31:0];
        else if (mmio_addr == BASE_ADDR + 32'h14) mmio_rd_data = now_tick[63:32];
        else if (mmio_addr == BASE_ADDR + 32'h18) mmio_rd_data = staged_due[31:0];
        else if (mmio_addr == BASE_ADDR + 32'h1c) mmio_rd_data = staged_due[63:32];
    end
    wire unused_mmio_rd = mmio_rd;
endmodule
