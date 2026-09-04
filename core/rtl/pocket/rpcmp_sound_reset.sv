module rpcmp_sound_reset #(
    parameter logic [31:0] BASE_ADDR = 32'h4000_0240,
    parameter int unsigned AUDIO_RESET_CYCLES = 256
) (
    input logic cpu_clk,
    input logic audio_clk,
    input logic reset_n,
    input logic [31:0] mmio_addr,
    input logic [31:0] mmio_wr_data,
    input logic mmio_rd,
    input logic mmio_wr,
    output logic [31:0] mmio_rd_data,
    output logic sound_reset_cpu_n,
    output logic sound_reset_audio_n
);
    localparam int COUNTER_WIDTH = $clog2(AUDIO_RESET_CYCLES + 1);
    logic request_toggle;
    logic ack_toggle;
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic ack_sync_1, ack_sync_2;
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic request_sync_1, request_sync_2;
    logic request_seen;
    logic [COUNTER_WIDTH-1:0] audio_count;
    logic [15:0] generation;
    logic busy, invalid_sticky;

    wire command_address = (mmio_addr == BASE_ADDR + 32'h0c);
    wire known_read = (mmio_addr == BASE_ADDR) ||
                      (mmio_addr == BASE_ADDR + 32'h04) ||
                      (mmio_addr == BASE_ADDR + 32'h08);
    wire reset_request = mmio_wr && command_address && mmio_wr_data[0] && !busy;

    always_ff @(posedge cpu_clk or negedge reset_n) begin
        if (!reset_n) begin
            request_toggle <= 0;
            ack_sync_1 <= 0;
            ack_sync_2 <= 0;
            generation <= 0;
            busy <= 0;
            invalid_sticky <= 0;
            sound_reset_cpu_n <= 1;
        end else begin
            ack_sync_1 <= ack_toggle;
            ack_sync_2 <= ack_sync_1;
            if (busy && (ack_sync_2 == request_toggle)) begin
                busy <= 0;
                sound_reset_cpu_n <= 1;
                generation <= generation + 1'b1;
            end
            if ((mmio_rd || mmio_wr) && (mmio_addr[1:0] != 0))
                invalid_sticky <= 1;
            if (mmio_rd && !known_read)
                invalid_sticky <= 1;
            if (mmio_wr && (!command_address || (mmio_wr_data[31:2] != 0)))
                invalid_sticky <= 1;
            if (mmio_wr && command_address && mmio_wr_data[1])
                invalid_sticky <= 0;
            if (reset_request) begin
                request_toggle <= ~request_toggle;
                busy <= 1;
                sound_reset_cpu_n <= 0;
            end
        end
    end

    always_ff @(posedge audio_clk or negedge reset_n) begin
        if (!reset_n) begin
            request_sync_1 <= 0;
            request_sync_2 <= 0;
            request_seen <= 0;
            ack_toggle <= 0;
            audio_count <= 0;
            sound_reset_audio_n <= 1;
        end else begin
            request_sync_1 <= request_toggle;
            request_sync_2 <= request_sync_1;
            if (request_sync_2 != request_seen) begin
                request_seen <= request_sync_2;
                audio_count <= AUDIO_RESET_CYCLES[COUNTER_WIDTH-1:0];
                sound_reset_audio_n <= 0;
            end else if (audio_count != 0) begin
                audio_count <= audio_count - 1'b1;
                if (audio_count == 1) begin
                    sound_reset_audio_n <= 1;
                    ack_toggle <= request_seen;
                end
            end
        end
    end

    always_comb begin
        mmio_rd_data = 0;
        if (mmio_addr == BASE_ADDR)
            mmio_rd_data = 32'h5253_4331;
        else if (mmio_addr == BASE_ADDR + 4)
            mmio_rd_data = {16'd1, 16'(AUDIO_RESET_CYCLES)};
        else if (mmio_addr == BASE_ADDR + 8)
            mmio_rd_data = {generation, 14'd0, invalid_sticky, busy};
    end
endmodule
