module rpcmp_spike_regs #(
    parameter logic [31:0] BASE_ADDR = 32'h1000_0000,
    parameter logic [63:0] STEP_TICKS = 64'd1000,
    parameter int unsigned LIVENESS_BIT = 8
) (
    input  logic        clk,
    input  logic        reset_n,
    input  logic [31:0] bridge_addr,
    input  logic        bridge_rd,
    input  logic        bridge_wr,
    input  logic [31:0] bridge_wr_data,
    output logic [31:0] bridge_rd_data,
    output logic        liveness,
    output logic        device_event_valid,
    output wire  [63:0] device_event_value
);

    localparam logic [31:0] STATUS_ADDR = BASE_ADDR + 32'h00;
    localparam logic [31:0] COMMAND_ID_ADDR = BASE_ADDR + 32'h04;
    localparam logic [31:0] COMMAND_ADDR = BASE_ADDR + 32'h08;
    localparam logic [31:0] SNAPSHOT_SEQUENCE_ADDR = BASE_ADDR + 32'h0c;
    localparam logic [31:0] COUNTER_LO_ADDR = BASE_ADDR + 32'h10;
    localparam logic [31:0] COUNTER_HI_ADDR = BASE_ADDR + 32'h14;
    localparam logic [31:0] LAST_COMMAND_ID_ADDR = BASE_ADDR + 32'h18;
    localparam logic [31:0] EVENT_SEQUENCE_ADDR = BASE_ADDR + 32'h1c;
    localparam logic [31:0] EVENT_VALUE_LO_ADDR = BASE_ADDR + 32'h20;
    localparam logic [31:0] EVENT_VALUE_HI_ADDR = BASE_ADDR + 32'h24;

    localparam logic [31:0] COMMAND_ADVANCE = 32'd1;

    logic [LIVENESS_BIT:0] liveness_counter;
    logic [31:0] staged_command_id;
    logic [31:0] last_command_id;
    logic [31:0] snapshot_sequence;
    logic [63:0] injected_tick;

    logic advance_accepted;
    logic [63:0] advanced_tick;

    assign advance_accepted = bridge_wr && (bridge_addr == COMMAND_ADDR) &&
                              (bridge_wr_data == COMMAND_ADVANCE) &&
                              (staged_command_id > last_command_id);
    assign advanced_tick = injected_tick + STEP_TICKS;
    assign liveness = liveness_counter[LIVENESS_BIT];
    assign device_event_value = injected_tick;

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            liveness_counter <= '0;
            staged_command_id <= 32'd0;
            last_command_id <= 32'd0;
            snapshot_sequence <= 32'd0;
            injected_tick <= 64'd0;
            device_event_valid <= 1'b0;
        end else begin
            liveness_counter <= liveness_counter + 1'b1;
            device_event_valid <= 1'b0;

            if (bridge_wr && (bridge_addr == COMMAND_ID_ADDR)) begin
                staged_command_id <= bridge_wr_data;
            end

            if (advance_accepted) begin
                last_command_id <= staged_command_id;
                snapshot_sequence <= snapshot_sequence + 1'b1;
                injected_tick <= advanced_tick;
                device_event_valid <= 1'b1;
            end
        end
    end

    always_comb begin
        bridge_rd_data = 32'd0;

        if (bridge_rd && (bridge_addr == STATUS_ADDR)) begin
            bridge_rd_data = {29'd0, device_event_valid, liveness, 1'b1};
        end else if (bridge_rd && (bridge_addr == COMMAND_ID_ADDR)) begin
            bridge_rd_data = staged_command_id;
        end else if (bridge_rd && (bridge_addr == SNAPSHOT_SEQUENCE_ADDR)) begin
            bridge_rd_data = snapshot_sequence;
        end else if (bridge_rd && (bridge_addr == COUNTER_LO_ADDR)) begin
            bridge_rd_data = injected_tick[31:0];
        end else if (bridge_rd && (bridge_addr == COUNTER_HI_ADDR)) begin
            bridge_rd_data = injected_tick[63:32];
        end else if (bridge_rd && (bridge_addr == LAST_COMMAND_ID_ADDR)) begin
            bridge_rd_data = last_command_id;
        end else if (bridge_rd && (bridge_addr == EVENT_SEQUENCE_ADDR)) begin
            bridge_rd_data = snapshot_sequence;
        end else if (bridge_rd && (bridge_addr == EVENT_VALUE_LO_ADDR)) begin
            bridge_rd_data = injected_tick[31:0];
        end else if (bridge_rd && (bridge_addr == EVENT_VALUE_HI_ADDR)) begin
            bridge_rd_data = injected_tick[63:32];
        end
    end

endmodule
