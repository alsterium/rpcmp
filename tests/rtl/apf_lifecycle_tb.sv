`timescale 1ns/1ps

// Exercise the prepared upstream command handler through its APF bridge.
// The RAM model covers only the synchronous datatable port, not vendor timing.
module mf_datatable (
    input wire [9:0] address_a, address_b,
    input wire clock_a, clock_b,
    input wire [31:0] data_a, data_b,
    input wire wren_a, wren_b,
    output reg [31:0] q_a, q_b
);
    reg [31:0] memory [0:1023];
    always @(posedge clock_a) begin
        if (wren_a) memory[address_a] <= data_a;
        q_a <= memory[address_a];
    end
    always @(posedge clock_b) begin
        if (wren_b) memory[address_b] <= data_b;
        q_b <= memory[address_b];
    end
endmodule

module apf_lifecycle_tb;
    reg clk = 0;
    always #5 clk = ~clk;
    reg [31:0] address = 0, write_data = 0;
    reg rd = 0, wr = 0, boot_done = 0;
    reg target_read = 0;
    reg [15:0] slot_id = 4;
    reg [31:0] read_offset = 0, read_length = 4096;
    wire target_ack, target_done;
    wire [2:0] target_error;
    wire [31:0] read_data;
    wire reset_n, allcomplete, rtc_valid;
    wire setup_done = boot_done & allcomplete & rtc_valid;
    core_bridge_cmd dut (
        .clk(clk), .reset_n(reset_n),
        .shutdown_pending(), .dataslot_requestread(), .dataslot_requestread_id(),
        .dataslot_requestwrite(), .dataslot_requestwrite_id(), .dataslot_requestwrite_size(),
        .dataslot_update(), .dataslot_update_id(), .dataslot_update_size(),
        .rtc_epoch_seconds(), .rtc_date_bcd(), .rtc_time_bcd(),
        .osnotify_inmenu(), .savestate_start(), .savestate_load(), .datatable_q(),
        .target_dataslot_ack(target_ack), .target_dataslot_done(target_done),
        .target_dataslot_err(target_error),
        .bridge_endian_little(1'b0), .bridge_addr(address),
        .bridge_rd(rd), .bridge_rd_data(read_data),
        .bridge_wr(wr), .bridge_wr_data(write_data),
        .shutdown_ack_s(1'b1), .status_boot_done(boot_done),
        .status_setup_done(setup_done), .status_running(reset_n),
        .dataslot_requestread_ack(1'b1), .dataslot_requestread_ok(1'b1),
        .dataslot_requestwrite_ack(1'b1), .dataslot_requestwrite_ok(1'b1),
        .dataslot_allcomplete(allcomplete), .rtc_valid(rtc_valid),
        .savestate_supported(1'b0), .savestate_addr(32'd0),
        .savestate_size(32'd0), .savestate_maxloadsize(32'd0),
        .savestate_start_ack(1'b0), .savestate_start_busy(1'b0),
        .savestate_start_ok(1'b0), .savestate_start_err(1'b0),
        .savestate_load_ack(1'b0), .savestate_load_busy(1'b0),
        .savestate_load_ok(1'b0), .savestate_load_err(1'b0),
        .target_dataslot_read(target_read), .target_dataslot_write(1'b0),
        .target_dataslot_getfile(1'b0), .target_dataslot_openfile(1'b0),
`ifdef RPCMP_APF_FLUSH
        .target_dataslot_flush(1'b0),
`endif
        .target_dataslot_id(slot_id), .target_dataslot_slotoffset(read_offset),
        .target_dataslot_bridgeaddr(32'h20100000), .target_dataslot_length(read_length),
        .target_buffer_param_struct(32'd0), .target_buffer_resp_struct(32'd0),
        .datatable_addr(10'd0), .datatable_wren(1'b0), .datatable_data(32'd0)
    );
    task automatic write_reg(input [31:0] addr, value);
        @(negedge clk); address = addr; write_data = value; wr = 1;
        @(negedge clk); wr = 0;
    endtask
    task automatic read_reg(input [31:0] addr, output [31:0] value);
        @(negedge clk); address = addr; rd = 1;
        @(negedge clk); value = read_data; rd = 0;
    endtask
    task automatic command(input [15:0] cmd, result);
        reg [31:0] value;
        write_reg(32'hf8000000, {16'h434d, cmd});
        for (int i = 0; i < 100; i++) begin
            read_reg(32'hf8000000, value);
            if (value[31:16] == 16'h4f4b) begin
                if (value[15:0] !== result)
                    $fatal(1, "command %h expected %h got %h", cmd, result, value);
                return;
            end
        end
        $fatal(1, "command %h did not complete", cmd);
    endtask
    task automatic load_and_run(input bit cold);
        reg [31:0] value;
        command(16'h008f, 0);
        if (cold) begin
            command(0, 2); // RTC is sent once at cold boot
            command(16'h0090, 0);
        end
        repeat (8) @(negedge clk);
        read_reg(32'hf8001000, value);
        if (value !== 32'h636d0140) $fatal(1, "missing Ready-To-Run: %h", value);
        write_reg(32'hf8001000, 32'h6f6b0000);
        command(0, 3);
        command(16'h0011, 0);
        if (!reset_n) $fatal(1, "Reset Exit did not start core");
        command(0, 4);
    endtask
    task automatic transfer(input [15:0] id, input bit completes, input [2:0] error_code);
        reg [31:0] value;
        @(negedge clk); slot_id = id; target_read = 1;
        @(negedge clk); target_read = 0;
        repeat (8) @(negedge clk);
        read_reg(32'hf8001000, value);
        if (value !== 32'h636d0180) $fatal(1, "missing read command: %h", value);
        read_reg(32'hf8001020, value);
        if (value !== {16'd0,id}) $fatal(1, "wrong slot");
        read_reg(32'hf8001024, value);
        if (value !== read_offset) $fatal(1, "wrong offset");
        read_reg(32'hf8001028, value);
        if (value !== 32'h20100000) $fatal(1, "wrong DMA destination");
        read_reg(32'hf800102c, value);
        if (value !== read_length) $fatal(1, "wrong bounded length");
        write_reg(32'hf8001000, 32'h62750000);
        repeat (3) @(negedge clk);
        if (!target_ack || target_done) $fatal(1, "wrong busy handshake");
        write_reg(32'hf8001000, 32'h6f6b0000);
        repeat (8) @(negedge clk);
        if (target_ack || target_done !== completes || target_error !== error_code)
            $fatal(1, "wrong completion: %b %b %h", target_ack, target_done, target_error);
    endtask
    initial begin
        repeat (8) @(negedge clk);
        command(0, 1);
        boot_done = 1;
        command(0, 2);
        load_and_run(1);
        transfer(4, 1, 0);
        transfer(4, 1, 0);
        for (int mode = 1; mode <= 2; mode++) begin
            command(16'h0010, 0);
            if (reset_n || allcomplete || !rtc_valid) $fatal(1, "wrong reset/RTC lifetime");
            command(0, 3); // held Idle until replacement loading starts
            write_reg(32'hf7000020, mode);
            load_and_run(0); // no second RTC notification
            transfer(1, 1, 0); // OS/config/app transport must remain normal
            read_offset = 0; read_length = 4;
            repeat (8) transfer(4, 1, 0); // GETFILE primer/retries before app entry
            read_length = 80; // even a minimum-size, single-read library must fault
            transfer(4, mode == 1, mode == 1 ? 3 : 0);
            read_length = 4096;
            transfer(4, mode == 1, mode == 1 ? 3 : 0);
            read_offset = 4096;
            transfer(4, mode == 1, mode == 1 ? 3 : 0);
            read_length = 4;
            transfer(4, mode == 1, mode == 1 ? 3 : 0); // short tail is not a primer
            read_offset = 0; read_length = 4;
            transfer(4, 1, 0); // metadata primers remain usable even after a fault
            read_length = 4096;
            command(0, 4); // APF Host command handling survives the injected fault
        end
        command(16'h0010, 0);
        load_and_run(0); // reset clears fault configuration
        transfer(4, 1, 0);
        transfer(4, 1, 0);
        $display("apf_lifecycle_tb: PASS");
        $finish;
    end
    initial begin
        #100000;
        $fatal(1, "test watchdog");
    end
endmodule
