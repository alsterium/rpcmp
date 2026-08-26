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
    localparam logic [31:0] RUN_ID_1_ADDR = BASE_ADDR + 32'h28;
    localparam logic [31:0] RUN_ID_2_ADDR = BASE_ADDR + 32'h2c;
    localparam logic [31:0] BUILD_SIGNATURE_ADDR = BASE_ADDR + 32'h30;
    localparam logic [31:0] WRITE_SEQUENCE_ADDR = BASE_ADDR + 32'h34;
    localparam logic [31:0] LAST_WRITE_ADDR_ADDR = BASE_ADDR + 32'h38;
    localparam logic [31:0] LAST_WRITE_DATA_ADDR = BASE_ADDR + 32'h3c;

    // Exact action addresses used by Analogue's core-example-interact.
    localparam logic [31:0] INTERACT_RUN_ID_1_ADDR = 32'h00f0_0010;
    localparam logic [31:0] INTERACT_RUN_ID_2_ADDR = 32'h00f0_0018;

    localparam logic [31:0] COMMAND_ADVANCE = 32'd1;
    localparam logic [31:0] BUILD_SIGNATURE = 32'h4d30_3034;

    logic [LIVENESS_BIT:0] liveness_counter;
    logic [31:0] staged_command_id;
    logic [31:0] last_command_id;
    logic [31:0] snapshot_sequence;
    logic [63:0] injected_tick;
    logic [31:0] write_sequence;
    logic [31:0] last_write_addr;
    logic [31:0] last_write_data;

    logic advance_accepted;
    logic staged_advance_accepted;
    logic run_id_1_accepted;
    logic run_id_2_accepted;
    logic [31:0] accepted_command_id;
    logic [63:0] advanced_tick;
    logic observed_rpcmp_write;

    assign staged_advance_accepted = bridge_wr && (bridge_addr == COMMAND_ADDR) &&
                                     (bridge_wr_data == COMMAND_ADVANCE) &&
                                     (staged_command_id > last_command_id);
    assign run_id_1_accepted = bridge_wr &&
                               (((bridge_addr == RUN_ID_1_ADDR) &&
                                 (bridge_wr_data == COMMAND_ADVANCE)) ||
                                (bridge_addr == INTERACT_RUN_ID_1_ADDR)) &&
                               (32'd1 > last_command_id);
    assign run_id_2_accepted = bridge_wr &&
                               (((bridge_addr == RUN_ID_2_ADDR) &&
                                 (bridge_wr_data == COMMAND_ADVANCE)) ||
                                (bridge_addr == INTERACT_RUN_ID_2_ADDR)) &&
                               (32'd2 > last_command_id);
    assign advance_accepted = staged_advance_accepted || run_id_1_accepted ||
                              run_id_2_accepted;
    assign accepted_command_id = run_id_2_accepted ? 32'd2 :
                                 run_id_1_accepted ? 32'd1 : staged_command_id;
    assign advanced_tick = injected_tick + STEP_TICKS;
    assign observed_rpcmp_write = bridge_wr &&
                                  (((bridge_addr >= BASE_ADDR) &&
                                    (bridge_addr <= LAST_WRITE_DATA_ADDR)) ||
                                   (bridge_addr == INTERACT_RUN_ID_1_ADDR) ||
                                   (bridge_addr == INTERACT_RUN_ID_2_ADDR));
    assign liveness = liveness_counter[LIVENESS_BIT];
    assign device_event_value = injected_tick;

    always_ff @(posedge clk or negedge reset_n) begin
        if (!reset_n) begin
            liveness_counter <= '0;
            staged_command_id <= 32'd0;
            last_command_id <= 32'd0;
            snapshot_sequence <= 32'd0;
            injected_tick <= 64'd0;
            write_sequence <= 32'd0;
            last_write_addr <= 32'd0;
            last_write_data <= 32'd0;
            device_event_valid <= 1'b0;
        end else begin
            liveness_counter <= liveness_counter + 1'b1;
            device_event_valid <= 1'b0;

            if (observed_rpcmp_write) begin
                write_sequence <= write_sequence + 1'b1;
                last_write_addr <= bridge_addr;
                last_write_data <= bridge_wr_data;
            end

            if (bridge_wr && (bridge_addr == COMMAND_ID_ADDR)) begin
                staged_command_id <= bridge_wr_data;
            end

            if (advance_accepted) begin
                staged_command_id <= accepted_command_id;
                last_command_id <= accepted_command_id;
                snapshot_sequence <= snapshot_sequence + 1'b1;
                injected_tick <= advanced_tick;
                device_event_valid <= 1'b1;
            end
        end
    end

    always_comb begin
        bridge_rd_data = 32'd0;

        // APF buffers the current bus value before pulsing bridge_rd. Keep read
        // data decoded from the address continuously, as in the official core.
        if (bridge_addr == STATUS_ADDR) begin
            bridge_rd_data = {29'd0, device_event_valid, liveness, 1'b1};
        end else if (bridge_addr == COMMAND_ID_ADDR) begin
            bridge_rd_data = staged_command_id;
        end else if (bridge_addr == SNAPSHOT_SEQUENCE_ADDR) begin
            bridge_rd_data = snapshot_sequence;
        end else if (bridge_addr == COUNTER_LO_ADDR) begin
            bridge_rd_data = injected_tick[31:0];
        end else if (bridge_addr == COUNTER_HI_ADDR) begin
            bridge_rd_data = injected_tick[63:32];
        end else if (bridge_addr == LAST_COMMAND_ID_ADDR) begin
            bridge_rd_data = last_command_id;
        end else if (bridge_addr == EVENT_SEQUENCE_ADDR) begin
            bridge_rd_data = snapshot_sequence;
        end else if (bridge_addr == EVENT_VALUE_LO_ADDR) begin
            bridge_rd_data = injected_tick[31:0];
        end else if (bridge_addr == EVENT_VALUE_HI_ADDR) begin
            bridge_rd_data = injected_tick[63:32];
        end else if (bridge_addr == BUILD_SIGNATURE_ADDR) begin
            bridge_rd_data = BUILD_SIGNATURE;
        end else if (bridge_addr == WRITE_SEQUENCE_ADDR) begin
            bridge_rd_data = write_sequence;
        end else if (bridge_addr == LAST_WRITE_ADDR_ADDR) begin
            bridge_rd_data = last_write_addr;
        end else if (bridge_addr == LAST_WRITE_DATA_ADDR) begin
            bridge_rd_data = last_write_data;
        end
    end

endmodule
