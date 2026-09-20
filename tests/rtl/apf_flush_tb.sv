`timescale 1ns/1ps
module apf_flush_tb;
    parameter integer PHASE_PS = 0;
    parameter integer FLUSH_ENABLE = 1;
    reg clk_cpu = 0, clk_74a = 0;
    always #5.5555 clk_cpu = ~clk_cpu; // 90 MHz
    initial begin
        #(PHASE_PS * 0.001);
        forever #6.734 clk_74a = ~clk_74a; // 74.25 MHz
    end
    wire clk_ram_controller = clk_cpu;
    reg bridge_wr_idle = 1;
    wire reset_n_apf;
    reg [31:0] address = 0, write_data = 0;
    reg rd = 0, wr = 0;
    wire [31:0] read_data;
    reg s_axi_arvalid = 0, s_axi_awvalid = 0, s_axi_wvalid = 0;
    reg [31:0] s_axi_araddr = 0, s_axi_awaddr = 0, s_axi_wdata = 0;
    wire s_axi_arready, s_axi_awready, s_axi_wready;
    wire s_axi_rvalid, s_axi_rlast, s_axi_bvalid;
    wire [31:0] s_axi_rdata;
    wire [1:0] s_axi_rresp, s_axi_bresp;
    wire [7:0] s_axi_arlen = 0;
    reg [7:0] s_axi_awlen = 0;
    wire [1:0] s_axi_awburst = 1;
    wire [3:0] s_axi_wstrb = 15;
    reg s_axi_wlast = 1;
    wire s_axi_rready = 1, s_axi_bready = 1;
    `include "apf_flush_bindings.svh"

    core_bridge_cmd bridge (
        .clk(clk_74a), .reset_n(reset_n_apf),
        .shutdown_pending(), .dataslot_requestread(), .dataslot_requestread_id(),
        .dataslot_requestwrite(), .dataslot_requestwrite_id(), .dataslot_requestwrite_size(),
        .dataslot_update(), .dataslot_update_id(), .dataslot_update_size(),
        .rtc_epoch_seconds(), .rtc_date_bcd(), .rtc_time_bcd(), .rtc_valid(),
        .osnotify_inmenu(), .savestate_start(), .savestate_load(), .datatable_q(),
        .target_dataslot_ack(target_dataslot_ack), .target_dataslot_done(target_dataslot_done),
        .target_dataslot_err(target_dataslot_err),
        .bridge_endian_little(1'b0), .bridge_addr(address),
        .bridge_rd(rd), .bridge_rd_data(read_data),
        .bridge_wr(wr), .bridge_wr_data(write_data),
        .shutdown_ack_s(1'b1), .status_boot_done(1'b1),
        .status_setup_done(1'b0), .status_running(reset_n_apf),
        .dataslot_requestread_ack(1'b1), .dataslot_requestread_ok(1'b1),
        .dataslot_requestwrite_ack(1'b1), .dataslot_requestwrite_ok(1'b1),
        .dataslot_allcomplete(),
        .savestate_supported(1'b0), .savestate_addr(32'd0),
        .savestate_size(32'd0), .savestate_maxloadsize(32'd0),
        .savestate_start_ack(1'b0), .savestate_start_busy(1'b0),
        .savestate_start_ok(1'b0), .savestate_start_err(1'b0),
        .savestate_load_ack(1'b0), .savestate_load_busy(1'b0),
        .savestate_load_ok(1'b0), .savestate_load_err(1'b0),
        .target_dataslot_read(target_dataslot_read),
        .target_dataslot_write(target_dataslot_write),
        .target_dataslot_getfile(target_dataslot_getfile),
        .target_dataslot_openfile(target_dataslot_openfile),
        .target_dataslot_flush(target_dataslot_flush),
        .target_dataslot_id(target_dataslot_id),
        .target_dataslot_slotoffset(target_dataslot_slotoffset),
        .target_dataslot_bridgeaddr(target_dataslot_bridgeaddr),
        .target_dataslot_length(target_dataslot_length),
        .target_buffer_param_struct(target_buffer_param_struct),
        .target_buffer_resp_struct(target_buffer_resp_struct),
        .datatable_addr(10'd0), .datatable_wren(1'b0), .datatable_data(32'd0)
    );
    task automatic apf_write(input [31:0] addr, value);
        @(negedge clk_74a); address = addr; write_data = value; wr = 1;
        @(negedge clk_74a); wr = 0;
    endtask
    task automatic apf_read(input [31:0] addr, output [31:0] value);
        @(negedge clk_74a); address = addr; rd = 1;
        @(negedge clk_74a); value = read_data; rd = 0;
    endtask
    task automatic host_command(input [15:0] cmd, result);
        reg [31:0] value;
        apf_write(32'hf8000000, {16'h434d, cmd});
        for (int i = 0; i < 100; i++) begin
            apf_read(32'hf8000000, value);
            if (value[31:16] == 16'h4f4b) begin
                if (value[15:0] !== result) $fatal(1, "Host command %h result %h", cmd, value);
                return;
            end
        end
        $fatal(1, "Host command blocked by Target");
    endtask
    task automatic cpu_write(input [31:0] addr, value);
        @(negedge clk_cpu); s_axi_awaddr = addr; s_axi_awvalid = 1;
        do @(posedge clk_cpu); while (!s_axi_awready);
        @(negedge clk_cpu); s_axi_awvalid = 0; s_axi_wdata = value; s_axi_wvalid = 1;
        do @(posedge clk_cpu); while (!s_axi_wready);
        @(negedge clk_cpu); s_axi_wvalid = 0;
        do @(posedge clk_cpu); while (!s_axi_bvalid);
        if (s_axi_bresp !== 0) $fatal(1, "CPU write response");
    endtask
    task automatic cpu_read(input [31:0] addr, output [31:0] value);
        @(negedge clk_cpu); s_axi_araddr = addr; s_axi_arvalid = 1;
        do @(posedge clk_cpu); while (!s_axi_arready);
        @(negedge clk_cpu); s_axi_arvalid = 0;
        do @(posedge clk_cpu); while (!s_axi_rvalid);
        value = s_axi_rdata;
        if (s_axi_rresp !== 0 || !s_axi_rlast) $fatal(1, "CPU read response");
    endtask
    task automatic stage_burst;
        reg [31:0] value;
        // Simultaneous AW/W first beat, then a delayed second INCR beat.
        // This independently checks both paths affected by first-beat handling.
        @(negedge clk_cpu);
        s_axi_awaddr = 32'h40000020; s_axi_awlen = 1; s_axi_awvalid = 1;
        s_axi_wdata = 6; s_axi_wvalid = 1; s_axi_wlast = 0;
        do @(posedge clk_cpu); while (!(s_axi_awready && s_axi_wready));
        @(negedge clk_cpu); s_axi_awvalid = 0; s_axi_wvalid = 0;
        repeat (4) @(negedge clk_cpu);
        s_axi_wdata = 42; s_axi_wvalid = 1; s_axi_wlast = 1;
        do @(posedge clk_cpu); while (!s_axi_wready);
        @(negedge clk_cpu); s_axi_wvalid = 0;
        do @(posedge clk_cpu); while (!s_axi_bvalid);
        if (s_axi_bresp !== 0) $fatal(1, "burst write response");
        @(negedge clk_cpu); s_axi_awlen = 0;
        cpu_read(32'h40000020, value);
        if (value !== 6) $fatal(1, "burst first address");
        cpu_read(32'h40000024, value);
        if (value !== 42) $fatal(1, "burst second address");
    endtask
    task automatic expect_target(input [15:0] cmd, id);
        reg [31:0] value;
        for (int i = 0; i < 100; i++) begin
            apf_read(32'hf8001000, value);
            if (value == {16'h636d, cmd}) begin
                apf_read(32'hf8001020, value);
                if (value !== {16'd0, id}) $fatal(1, "Target slot changed: %h", value);
                return;
            end
        end
        $fatal(1, "missing Target command %h, got %h", cmd, value);
    endtask
    task automatic complete_target(input [15:0] error_code);
        reg [31:0] value;
        reg [2:0] expected_error;
        expected_error = error_code > 7 ? 3'd7 : error_code[2:0];
        apf_write(32'hf8001000, 32'h6f6b0000 | {16'd0, error_code});
        // Drain is an independent input, not inferred from DONE or a sleep.
        bridge_wr_idle = 0;
        repeat (1500) @(negedge clk_cpu);
        cpu_read(32'h4000003c, value);
        if (value[1]) $fatal(1, "DONE escaped before memory drain");
        bridge_wr_idle = 1;
        for (int i = 0; i < 600; i++) begin
            cpu_read(32'h4000003c, value);
            if (value[1]) begin
                if (value[4:2] !== expected_error || !value[0]) $fatal(1, "wrong result %h", value);
                return;
            end
        end
        $fatal(1, "CPU completion missing");
    endtask
    task automatic flush(input [15:0] id, result);
        reg [31:0] value;
        cpu_write(32'h40000020, {16'd0,id});
        cpu_write(32'h40000038, 5);
        expect_target(16'h0188, id);
        cpu_read(32'h4000003c, value);
        if (value[1]) $fatal(1, "stale DONE completed flush");
        for (int offset = 4; offset <= 12; offset += 4) begin
            apf_read(32'hf8001020 + offset, value);
            if (value !== 0) $fatal(1, "flush reused prior parameters");
        end
        // Change staging and attempt every command before ACK. The admitted
        // slot and opcode must remain the original immutable request.
        cpu_write(32'h40000020, 16'hffff);
        for (int cmd = 1; cmd <= 5; cmd++) cpu_write(32'h40000038, cmd);
        expect_target(16'h0188, id);
        apf_write(32'hf8001000, 32'h62750000);
        repeat (12) @(negedge clk_cpu);
        for (int cmd = 1; cmd <= 5; cmd++) cpu_write(32'h40000038, cmd);
        host_command(0, 4);
        host_command(16'h00b0, 0);
        repeat (100) @(negedge clk_cpu);
        if (cpu_target_dataslot_id !== id) $fatal(1, "active payload overwritten");
        cpu_read(32'h4000003c, value);
        if (value[1]) $fatal(1, "wait reported success");
        complete_target(result);
    endtask
    initial begin : scenario
        reg [31:0] value, before_status;
        repeat (15) @(negedge clk_cpu);
        host_command(16'h0010, 0);
        host_command(16'h0011, 0);
        repeat (15) @(negedge clk_cpu);
        stage_burst();
        cpu_read(32'h40000188, value);
        if (value !== (FLUSH_ENABLE ? 32'h00010001 : 0)) $fatal(1, "capability mismatch %h", value);
        if (!FLUSH_ENABLE) begin
            $display("apf_flush_tb: PASS disabled");
            $finish;
        end
        cpu_write(32'h40000024, 32'h87654321);
        cpu_write(32'h40000028, 32'h20123456);
        cpu_write(32'h4000002c, 64);
        flush(6, 0);
        flush(7, 1);
        flush(6, 16'hfff8); // Unknown host failure must never truncate to success.
        flush(6, 0);
        // Invalid full words must not alias a valid low-three-bit opcode.
        cpu_read(32'h4000003c, before_status);
        cpu_write(32'h40000038, 0);
        cpu_write(32'h40000038, 6);
        cpu_write(32'h40000038, 13);
        cpu_write(32'h40000038, 32'hffffffff);
        cpu_read(32'h4000003c, value);
        if (value !== before_status) $fatal(1, "invalid command changed status");
        // GETFILE uses the current ID/response pointer, not the preceding flush.
        cpu_write(32'h40000020, 4);
        cpu_write(32'h40000034, 32'h20000100);
        cpu_write(32'h40000038, 4);
        expect_target(16'h0190, 4);
        cpu_write(32'h40000020, 7);
        cpu_write(32'h40000038, 5);
        expect_target(16'h0190, 4);
        apf_read(32'hf8001024, value);
        if (value !== 32'h20000100) $fatal(1, "GETFILE stale response pointer");
        apf_write(32'hf8001000, 32'h62750000);
        repeat (12) @(negedge clk_cpu);
        complete_target(0);
        flush(7, 0);
        // Reset a waiting Target operation. Local reset is not SD cancellation.
        cpu_write(32'h40000020, 6);
        cpu_write(32'h40000038, 5);
        expect_target(16'h0188, 6);
        apf_write(32'hf8001000, 32'h62750000);
        host_command(16'h0010, 0);
        if (reset_n_apf || target_dataslot_done || target_dataslot_ack) $fatal(1, "reset retained local completion");
        apf_write(32'hf8001000, 32'h6f6b0000); // a late host result
        repeat (20) @(negedge clk_cpu);
        if (target_dataslot_done) $fatal(1, "late result resurrected reset command");
        host_command(16'h0011, 0);
        flush(7, 0);
        $display("apf_flush_tb: PASS phase=%0d", PHASE_PS);
        $finish;
    end
    initial begin
        #2000000;
        $fatal(1, "test watchdog");
    end
endmodule
