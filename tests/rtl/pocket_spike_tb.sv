`timescale 1ns/1ps

module pocket_spike_tb;

    localparam logic [31:0] BASE_ADDR = 32'h1000_0000;
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

    logic clk = 1'b0;
    logic reset_n = 1'b0;
    logic [31:0] bridge_addr = 32'd0;
    logic bridge_rd = 1'b0;
    logic bridge_wr = 1'b0;
    logic [31:0] bridge_wr_data = 32'd0;
    logic [31:0] bridge_rd_data;
    logic liveness;
    logic device_event_valid;
    logic [63:0] device_event_value;

    always #5 clk = ~clk;

    rpcmp_spike_regs #(
        .LIVENESS_BIT(2)
    ) dut (
        .clk(clk),
        .reset_n(reset_n),
        .bridge_addr(bridge_addr),
        .bridge_rd(bridge_rd),
        .bridge_wr(bridge_wr),
        .bridge_wr_data(bridge_wr_data),
        .bridge_rd_data(bridge_rd_data),
        .liveness(liveness),
        .device_event_valid(device_event_valid),
        .device_event_value(device_event_value)
    );

    task automatic expect_bit(
        input string label_text,
        input logic actual,
        input logic expected
    );
        if (actual !== expected) begin
            $fatal(1, "%s: expected %0b, got %0b", label_text, expected, actual);
        end
    endtask

    task automatic expect_word(
        input string label_text,
        input logic [31:0] actual,
        input logic [31:0] expected
    );
        if (actual !== expected) begin
            $fatal(1, "%s: expected 0x%08x, got 0x%08x", label_text, expected, actual);
        end
    endtask

    task automatic read_expect(
        input logic [31:0] address,
        input logic [31:0] expected,
        input string label_text
    );
        bridge_addr = address;
        bridge_rd = 1'b1;
        #1;
        expect_word(label_text, bridge_rd_data, expected);
        bridge_rd = 1'b0;
        bridge_addr = 32'd0;
    endtask

    task automatic write_word(
        input logic [31:0] address,
        input logic [31:0] value,
        input logic expected_event
    );
        @(negedge clk);
        bridge_addr = address;
        bridge_wr_data = value;
        bridge_wr = 1'b1;
        @(posedge clk);
        #1;
        expect_bit("device event pulse", device_event_valid, expected_event);
        @(negedge clk);
        bridge_wr = 1'b0;
        bridge_addr = 32'd0;
        bridge_wr_data = 32'd0;
    endtask

    initial begin
        repeat (2) @(negedge clk);
        expect_bit("liveness during reset", liveness, 1'b0);
        expect_bit("event valid during reset", device_event_valid, 1'b0);
        read_expect(STATUS_ADDR, 32'h0000_0001, "reset status");
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd0, "reset snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd0, "reset counter low");
        read_expect(COUNTER_HI_ADDR, 32'd0, "reset counter high");

        reset_n = 1'b1;
        repeat (4) @(posedge clk);
        #1;
        expect_bit("command-independent liveness", liveness, 1'b1);
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd0, "liveness does not advance snapshot");

        write_word(COMMAND_ID_ADDR, 32'd1, 1'b0);
        write_word(COMMAND_ADDR, 32'd1, 1'b1);
        expect_word("event output after first advance", device_event_value[31:0], 32'd1000);
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd1, "first snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd1000, "first counter low");
        read_expect(COUNTER_HI_ADDR, 32'd0, "first counter high");
        read_expect(LAST_COMMAND_ID_ADDR, 32'd1, "first command ID");
        read_expect(EVENT_SEQUENCE_ADDR, 32'd1, "first event sequence");
        read_expect(EVENT_VALUE_LO_ADDR, 32'd1000, "first event value low");
        read_expect(EVENT_VALUE_HI_ADDR, 32'd0, "first event value high");

        @(posedge clk);
        #1;
        expect_bit("event pulse clears", device_event_valid, 1'b0);

        write_word(COMMAND_ADDR, 32'd1, 1'b0);
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd1, "duplicate ID does not advance snapshot");
        read_expect(COUNTER_LO_ADDR, 32'd1000, "duplicate ID does not advance counter");
        read_expect(EVENT_SEQUENCE_ADDR, 32'd1, "duplicate ID does not emit event");

        write_word(COMMAND_ID_ADDR, 32'd2, 1'b0);
        write_word(COMMAND_ADDR, 32'hffff_ffff, 1'b0);
        read_expect(LAST_COMMAND_ID_ADDR, 32'd1, "unsupported command does not consume ID");
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd1, "unsupported command does not advance snapshot");

        write_word(COMMAND_ADDR, 32'd1, 1'b1);
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd2, "second snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd2000, "second counter low");
        read_expect(LAST_COMMAND_ID_ADDR, 32'd2, "second command ID");
        read_expect(EVENT_SEQUENCE_ADDR, 32'd2, "second event sequence");
        read_expect(EVENT_VALUE_LO_ADDR, 32'd2000, "second event value low");

        read_expect(RUN_ID_1_ADDR, 32'd0, "write-only run ID 1 read");
        read_expect(RUN_ID_2_ADDR, 32'd0, "write-only run ID 2 read");
        read_expect(BASE_ADDR + 32'h30, 32'd0, "unmapped read");
        read_expect(BASE_ADDR + 32'h01, 32'd0, "misaligned read");

        @(negedge clk);
        reset_n = 1'b0;
        #1;
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd0, "interact reset snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd0, "interact reset counter");
        read_expect(LAST_COMMAND_ID_ADDR, 32'd0, "interact reset last command ID");
        reset_n = 1'b1;
        repeat (2) @(posedge clk);

        write_word(RUN_ID_1_ADDR, 32'd1, 1'b1);
        read_expect(COMMAND_ID_ADDR, 32'd1, "run ID 1 stages command ID atomically");
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd1, "run ID 1 snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd1000, "run ID 1 counter");
        read_expect(LAST_COMMAND_ID_ADDR, 32'd1, "run ID 1 last command ID");
        read_expect(EVENT_SEQUENCE_ADDR, 32'd1, "run ID 1 event sequence");
        read_expect(EVENT_VALUE_LO_ADDR, 32'd1000, "run ID 1 event value");

        write_word(RUN_ID_1_ADDR, 32'd1, 1'b0);
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd1, "duplicate run ID 1 snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd1000, "duplicate run ID 1 counter");

        write_word(RUN_ID_2_ADDR, 32'd1, 1'b1);
        read_expect(COMMAND_ID_ADDR, 32'd2, "run ID 2 stages command ID atomically");
        read_expect(SNAPSHOT_SEQUENCE_ADDR, 32'd2, "run ID 2 snapshot sequence");
        read_expect(COUNTER_LO_ADDR, 32'd2000, "run ID 2 counter");
        read_expect(LAST_COMMAND_ID_ADDR, 32'd2, "run ID 2 last command ID");
        read_expect(EVENT_SEQUENCE_ADDR, 32'd2, "run ID 2 event sequence");
        read_expect(EVENT_VALUE_LO_ADDR, 32'd2000, "run ID 2 event value");

        $display("pocket_spike_tb: PASS");
        $finish;
    end

endmodule
