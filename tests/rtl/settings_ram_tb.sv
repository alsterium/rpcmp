`timescale 1ns/1ps
module settings_ram_tb;
    logic clk=0;
    always #5 clk=~clk;
    logic power_reset_n=0, abort_candidate=0, command_valid=0, command_ready;
    logic [4:0] command_kind=0;
    logic [1:0] command_slot=0;
    logic [31:0] command_length=0, command_word=0, command_data=0;
    logic [3:0] command_mask=0;
    logic response_valid, response_ready=0;
    logic [2:0] response_result;
    logic [31:0] response_data;
    logic bridge_read=0, bridge_write=0, bridge_write_accepted;
    logic [31:0] bridge_offset=0, bridge_data=0, bridge_read_data;
    logic [3:0] bridge_mask=0;
    rpcmp_settings_ram dut (.*);
    localparam logic [4:0] BEGIN_CPU=0, WRITE_CPU=1, PUBLISH=2, ABORT_CPU=3,
        RELEASE_CPU=4, BEGIN_LOAD=5, FINISH_LOAD=6, BEGIN_READ=7, END_READ=8,
        BEGIN_READBACK=9, FINISH_READBACK=10, RELEASE_READBACK=11,
        READ_WORD=12, STATUS=13, LOCK_SNAPSHOT=14, UNLOCK_SNAPSHOT=15;
    localparam logic [2:0] SUCCESS=1, BUSY=2, INVALID=3, INCOMPLETE=4;
    logic [31:0] value;
    int checked_commands=0;

    // Authored byte pattern is independent of the DUT's bank/mask arithmetic.
    function automatic logic [7:0] sample(input int seed, position);
        return 8'((seed*37) ^ (position*13+19));
    endfunction
    function automatic logic [31:0] expected_word(input int seed, word_index, byte_length);
        logic [31:0] result;
        result=0;
        for (int lane=0; lane<4; lane++)
            if (word_index*4+lane<byte_length) result[lane*8+:8]=sample(seed,word_index*4+lane);
        return result;
    endfunction
    task automatic cmd(input logic [4:0] kind, input logic [1:0] slot,
        input logic [31:0] word_index, data, input logic [3:0] mask,
        input logic [31:0] byte_length, input logic [2:0] result);
        @(negedge clk);
        if (!command_ready) $fatal(1,"command not ready");
        command_valid=1; command_kind=kind; command_slot=slot;
        command_word=word_index; command_data=data; command_mask=mask; command_length=byte_length;
        @(negedge clk);
        command_valid=0;
        if (!response_valid || response_result!==result)
            $fatal(1,"kind=%d slot=%d word=%h length=%h expected=%d got=%d valid=%b",
                kind,slot,word_index,byte_length,result,response_result,response_valid);
        value=response_data; checked_commands++;
        response_ready=1;
        @(negedge clk); response_ready=0;
        if (response_valid) $fatal(1,"response did not retire");
    endtask
    task automatic simple(input logic [4:0] kind, input logic [1:0] slot,
        input logic [2:0] result);
        cmd(kind,slot,0,0,0,0,result);
    endtask
    task automatic data_write(input logic [31:0] offset, data,
        input logic [3:0] mask, input bit accepted);
        @(negedge clk); bridge_offset=offset; bridge_data=data; bridge_mask=mask; bridge_write=1;
        #1;
        if (bridge_write_accepted!==accepted) $fatal(1,"wrong BRIDGE write qualification %h",offset);
        @(negedge clk); bridge_write=0;
    endtask
    task automatic byte_write(input int slot, seed, position);
        data_write(32'(slot*64+(position/4)*4),32'(sample(seed,position))<<((position%4)*8),
            4'(1<<(position%4)),1);
    endtask
    task automatic bus_read(input logic [31:0] offset, expected);
        @(negedge clk); bridge_read=1; bridge_offset=offset;
        @(negedge clk); bridge_read=0;
        if (bridge_read_data!==expected)
            $fatal(1,"BRIDGE read offset=%h expected=%h got=%h",offset,expected,bridge_read_data);
    endtask
    task automatic reset_cpu;
        @(negedge clk); abort_candidate=1;
        repeat (3) @(negedge clk);
        abort_candidate=0;
    endtask
    task automatic check_slot(input logic [1:0] slot, input int seed, byte_length);
        simple(LOCK_SNAPSHOT,slot,SUCCESS);
        simple(STATUS,slot,SUCCESS);
        if (value[6:0]!==7'(byte_length)) $fatal(1,"wrong published length");
        for (int word_index=0; word_index<16; word_index++) begin
            cmd(READ_WORD,slot,32'(word_index),0,0,0,SUCCESS);
            if (value!==expected_word(seed,word_index,byte_length))
                $fatal(1,"slot=%d word=%d expected=%h got=%h",slot,word_index,
                    expected_word(seed,word_index,byte_length),value);
        end
        simple(UNLOCK_SNAPSHOT,slot,SUCCESS);
    endtask
    task automatic candidate(input logic [1:0] slot, input int seed);
        simple(BEGIN_CPU,slot,SUCCESS);
        // Deliberately reverse word order: coverage is not a write counter.
        for (int word_index=15; word_index>=0; word_index--)
            cmd(WRITE_CPU,slot,32'(word_index),expected_word(seed,word_index,64),15,0,SUCCESS);
    endtask

    initial begin
        repeat (3) @(negedge clk); power_reset_n=1;
        check_slot(0,0,0); check_slot(1,0,0);
        bus_read(0,0); bus_read(128,0);
        for (int kind=0; kind<32; kind++) cmd(5'(kind),3,0,0,0,0,INVALID);
        for (int kind=16; kind<32; kind++) cmd(5'(kind),0,0,0,0,0,INVALID);
        cmd(BEGIN_LOAD,0,0,0,0,65,INVALID);
        cmd(BEGIN_LOAD,0,0,0,0,32'hffffffff,INVALID);
        cmd(BEGIN_READBACK,2,0,0,0,32'h80000000,INVALID);
        simple(ABORT_CPU,0,INVALID); simple(RELEASE_CPU,0,INVALID);
        simple(END_READ,0,INVALID); simple(UNLOCK_SNAPSHOT,0,INVALID);
        simple(FINISH_LOAD,0,INVALID); simple(RELEASE_READBACK,2,INVALID);
        cmd(READ_WORD,0,16,0,0,0,INVALID);
        cmd(READ_WORD,0,32'hffffffff,0,0,0,INVALID);
        data_write(0,32'hffffffff,15,0);

        // Every legal Host length, including empty and partial last words.
        for (int count=0; count<=64; count++) begin
            cmd(BEGIN_LOAD,0,0,0,0,32'(count),SUCCESS);
            simple(BEGIN_CPU,0,BUSY); simple(BEGIN_READ,0,BUSY);
            for (int position=0; position<count; position++) byte_write(0,count,position);
            data_write(32'(((count+3)/4)*4),32'hffffffff,15,0);
            reset_cpu(); // Host load must survive CPU/app reset.
            simple(FINISH_LOAD,0,SUCCESS);
            check_slot(0,count,count);
            simple(BEGIN_READ,0,SUCCESS);
            simple(BEGIN_READ,0,SUCCESS); // idempotent, one owner
            for (int word_index=0; word_index<16; word_index++)
                bus_read(32'(word_index*4),expected_word(count,word_index,count));
            simple(END_READ,0,SUCCESS);
        end
        // Every interrupted CPU prefix preserves the last complete record.
        for (int prefix=0; prefix<64; prefix++) begin
            simple(BEGIN_CPU,0,SUCCESS);
            for (int position=0; position<prefix; position++)
                cmd(WRITE_CPU,0,32'(position/4),32'(sample(91,position))<<((position%4)*8),
                    4'(1<<(position%4)),0,SUCCESS);
            simple(PUBLISH,0,INCOMPLETE);
            check_slot(0,64,64);
            if (prefix%2==0) reset_cpu(); else simple(ABORT_CPU,0,SUCCESS);
            check_slot(0,64,64);
        end
        // Every interrupted Host prefix discards only the candidate, records error.
        for (int prefix=0; prefix<64; prefix++) begin
            cmd(BEGIN_LOAD,0,0,0,0,64,SUCCESS);
            for (int position=0; position<prefix; position++) byte_write(0,92,position);
            simple(FINISH_LOAD,0,INCOMPLETE);
            reset_cpu();
            simple(STATUS,0,SUCCESS);
            if (!value[12]) $fatal(1,"lost Host load error");
            check_slot(0,64,64);
        end
        // A successful Host load clears only its error; Finish cannot pass a
        // write on the same edge even when earlier coverage was already full.
        cmd(BEGIN_LOAD,0,0,0,0,64,SUCCESS);
        for (int word_index=0; word_index<16; word_index++)
            data_write(32'(word_index*4),expected_word(64,word_index,64),15,1);
        @(negedge clk); command_valid=1; command_kind=FINISH_LOAD; command_slot=0;
        bridge_write=1; bridge_offset=60; bridge_data=expected_word(64,15,64); bridge_mask=15;
        @(negedge clk);
        if (!response_valid || response_result!==BUSY) $fatal(1,"Host Finish overtook write");
        command_valid=0; bridge_write=0; response_ready=1;
        @(negedge clk); response_ready=0;
        simple(FINISH_LOAD,0,SUCCESS); simple(STATUS,0,SUCCESS);
        if (value[12]) $fatal(1,"successful load did not clear error");
        // Rewriting one word never stands in for full byte coverage; bad indexes
        // cannot wrap into word zero or provide a missing lane.
        simple(BEGIN_CPU,0,SUCCESS);
        cmd(WRITE_CPU,0,0,0,0,0,INVALID);
        cmd(WRITE_CPU,0,16,0,15,0,INVALID);
        cmd(WRITE_CPU,0,32'hffffffff,0,15,0,INVALID);
        for (int repeat_word=0; repeat_word<64; repeat_word++)
            cmd(WRITE_CPU,0,0,expected_word(93,0,64),15,0,SUCCESS);
        simple(PUBLISH,0,INCOMPLETE);
        for (int word_index=1; word_index<16; word_index++)
            cmd(WRITE_CPU,0,32'(word_index),expected_word(93,word_index,64),7,0,SUCCESS);
        simple(PUBLISH,0,INCOMPLETE);
        for (int word_index=1; word_index<16; word_index++)
            cmd(WRITE_CPU,0,32'(word_index),expected_word(93,word_index,64),8,0,SUCCESS);
        simple(BEGIN_READ,0,SUCCESS); simple(LOCK_SNAPSHOT,0,SUCCESS);
        simple(PUBLISH,0,BUSY);
        simple(END_READ,0,SUCCESS); simple(PUBLISH,0,BUSY);
        cmd(READ_WORD,0,7,0,0,0,SUCCESS);
        if (value!==expected_word(64,7,64)) $fatal(1,"candidate visible under snapshot");
        simple(UNLOCK_SNAPSHOT,0,SUCCESS); simple(PUBLISH,0,SUCCESS);
        check_slot(0,93,64);
        simple(BEGIN_CPU,0,BUSY); cmd(BEGIN_LOAD,0,0,0,0,64,BUSY);
        reset_cpu(); check_slot(0,93,64);
        for (int word_index=0; word_index<16; word_index++)
            bus_read(32'(word_index*4),expected_word(93,word_index,64));
        // The other slot remains independently usable while A is borrowed.
        candidate(1,94); simple(PUBLISH,1,SUCCESS); check_slot(1,94,64);
        simple(BEGIN_READ,0,SUCCESS); simple(RELEASE_CPU,0,SUCCESS);
        simple(BEGIN_CPU,0,BUSY); reset_cpu();
        simple(STATUS,0,SUCCESS);
        if (!value[14]) $fatal(1,"release/reset lost changed-since-load marker");
        bus_read(28,expected_word(93,7,64));
        simple(END_READ,0,SUCCESS);
        candidate(0,95); simple(PUBLISH,0,SUCCESS); check_slot(0,95,64);
        check_slot(1,94,64);

        // Two simultaneous data writers target different owned banks/staging.
        simple(RELEASE_CPU,0,SUCCESS); simple(BEGIN_CPU,0,SUCCESS);
        cmd(BEGIN_READBACK,2,0,0,0,4,SUCCESS);
        @(negedge clk);
        command_valid=1; command_kind=WRITE_CPU; command_slot=0;
        command_word=0; command_data=expected_word(98,0,64); command_mask=15;
        bridge_write=1; bridge_offset=128; bridge_data=expected_word(99,0,64); bridge_mask=15;
        @(negedge clk);
        if (!response_valid || response_result!==SUCCESS) $fatal(1,"concurrent CPU word failed");
        command_valid=0; bridge_write=0; response_ready=1;
        @(negedge clk); response_ready=0;
        simple(FINISH_READBACK,2,SUCCESS);
        cmd(READ_WORD,2,0,0,0,0,SUCCESS);
        if (value!==expected_word(99,0,64)) $fatal(1,"concurrent readback word failed");
        simple(RELEASE_READBACK,2,SUCCESS);
        for (int word_index=1; word_index<16; word_index++)
            cmd(WRITE_CPU,0,32'(word_index),expected_word(98,word_index,64),15,0,SUCCESS);
        // Reset wins over a fully staged publish on the very same edge.
        @(negedge clk); abort_candidate=1; command_valid=1; command_kind=PUBLISH;
        @(negedge clk);
        if (!response_valid || response_result!==BUSY) $fatal(1,"publish defeated reset");
        abort_candidate=0; command_valid=0; response_ready=1;
        @(negedge clk); response_ready=0;
        check_slot(0,95,64);
        candidate(0,95); simple(PUBLISH,0,SUCCESS);

        // Finish on the last data-write edge is rejected; retry after drain.
        cmd(BEGIN_READBACK,2,0,0,0,4,SUCCESS);
        @(negedge clk); command_valid=1; command_kind=FINISH_READBACK; command_slot=2;
        bridge_write=1; bridge_offset=128; bridge_data=32'h44332211; bridge_mask=15;
        @(negedge clk);
        if (!response_valid || response_result!==BUSY) $fatal(1,"finish overtook last write");
        command_valid=0; bridge_write=0; response_ready=1;
        @(negedge clk); response_ready=0;
        simple(FINISH_READBACK,2,SUCCESS);
        cmd(READ_WORD,2,0,0,0,0,SUCCESS);
        if (value!==32'h44332211) $fatal(1,"last write was lost");
        simple(RELEASE_READBACK,2,SUCCESS);

        // Readback never reads the current mirror: supply a different media image.
        // After every incomplete prefix, reset and late writes retain the lease.
        for (int prefix=0; prefix<64; prefix++) begin
            cmd(BEGIN_READBACK,2,0,0,0,64,SUCCESS);
            for (int position=0; position<prefix; position++) byte_write(2,96,position);
            simple(FINISH_READBACK,2,INCOMPLETE);
            cmd(READ_WORD,2,0,0,0,0,BUSY);
            reset_cpu(); cmd(BEGIN_READBACK,2,0,0,0,64,BUSY);
            for (int position=prefix; position<64; position++) byte_write(2,96,position);
            simple(FINISH_READBACK,2,SUCCESS);
            data_write(128,32'hffffffff,15,0);
            for (int word_index=0; word_index<16; word_index++) begin
                cmd(READ_WORD,2,32'(word_index),0,0,0,SUCCESS);
                if (value!==expected_word(96,word_index,64)) $fatal(1,"wrong media readback");
            end
            simple(RELEASE_READBACK,2,SUCCESS);
            data_write(128,32'hffffffff,15,0);
        end
        for (int count=0; count<=64; count++) begin
            cmd(BEGIN_READBACK,2,0,0,0,32'(count),SUCCESS);
            // Full-word Host writes have padding outside the final byte length.
            for (int word_index=0; word_index<(count+3)/4; word_index++)
                data_write(32'(128+word_index*4),expected_word(97,word_index,64),15,1);
            simple(FINISH_READBACK,2,SUCCESS);
            for (int word_index=0; word_index<16; word_index++) begin
                cmd(READ_WORD,2,32'(word_index),0,0,0,SUCCESS);
                if (value!==expected_word(97,word_index,count)) $fatal(1,"wrong readback tail");
            end
            simple(RELEASE_READBACK,2,SUCCESS);
        end
        // Full-width and unaligned addresses never alias a valid bank/stage.
        cmd(BEGIN_READBACK,2,0,0,0,64,SUCCESS);
        data_write(129,32'hffffffff,15,0); data_write(192,32'hffffffff,15,0);
        data_write(32'h80000080,32'hffffffff,15,0);
        data_write(32'hfffffffc,32'hffffffff,15,0);
        data_write(128,32'hffffffff,0,0);
        simple(FINISH_READBACK,2,INCOMPLETE);
        bus_read(1,0); bus_read(192,0); bus_read(32'h80000000,0);

        // Retained response cannot be replaced, including on application reset.
        @(negedge clk); command_valid=1; command_kind=STATUS; command_slot=0;
        @(negedge clk);
        if (!response_valid || response_result!==SUCCESS) $fatal(1,"missing retained response");
        value=response_data;
        command_kind=RELEASE_CPU;
        abort_candidate=1;
        repeat (8) begin
            @(negedge clk);
            if (command_ready || !response_valid || response_data!==value || response_result!==SUCCESS)
                $fatal(1,"response changed under backpressure/reset");
        end
        command_valid=0; abort_candidate=0; response_ready=1;
        @(negedge clk); response_ready=0;
        simple(BEGIN_CPU,0,BUSY); // blocked command did not release the lease
        simple(RELEASE_READBACK,2,SUCCESS);
        check_slot(0,95,64); check_slot(1,94,64);

        // A retained RAM response survives unrelated BRIDGE writes and reset.
        simple(LOCK_SNAPSHOT,0,SUCCESS);
        cmd(BEGIN_READBACK,2,0,0,0,64,SUCCESS);
        @(negedge clk); command_valid=1; command_kind=READ_WORD; command_slot=0; command_word=3;
        @(negedge clk);
        if (!response_valid || response_result!==SUCCESS || response_data!==expected_word(95,3,64))
            $fatal(1,"missing retained RAM word");
        command_kind=WRITE_CPU; command_word=15; command_data=32'hffffffff;
        bridge_write=1; bridge_offset=128; bridge_data=32'hdeadbeef; bridge_mask=15;
        abort_candidate=1;
        repeat (8) begin
            @(negedge clk);
            if (command_ready || response_data!==expected_word(95,3,64))
                $fatal(1,"retained RAM word changed");
        end
        command_valid=0; bridge_write=0; abort_candidate=0; response_ready=1;
        @(negedge clk); response_ready=0;
        simple(UNLOCK_SNAPSHOT,0,SUCCESS); simple(RELEASE_READBACK,2,SUCCESS);
        @(negedge clk); bridge_read=1; bridge_write=1; bridge_offset=0;
        #1;
        if (bridge_write_accepted) $fatal(1,"simultaneous read/write accepted a write");
        @(negedge clk); bridge_read=0; bridge_write=0;
        if (bridge_read_data!==expected_word(95,0,64)) $fatal(1,"read did not take priority");

        // Warm software restart must not mistake a CPU mirror for SD evidence.
        simple(RELEASE_CPU,0,SUCCESS);
        cmd(BEGIN_LOAD,0,0,0,0,4,SUCCESS);
        simple(FINISH_LOAD,0,INCOMPLETE); reset_cpu();
        simple(STATUS,0,SUCCESS);
        if (!value[14] || !value[12]) $fatal(1,"failed load lost media uncertainty");
        cmd(BEGIN_LOAD,0,0,0,0,4,SUCCESS);
        data_write(0,expected_word(100,0,4),15,1);
        simple(FINISH_LOAD,0,SUCCESS); simple(STATUS,0,SUCCESS);
        if (value[14] || value[12]) $fatal(1,"successful load retained old uncertainty");
        check_slot(0,100,4);

        // Power reset removes all metadata, so physically stale RAM is unreadable.
        @(negedge clk); power_reset_n=0;
        repeat (3) @(negedge clk); power_reset_n=1;
        check_slot(0,0,0); check_slot(1,0,0);
        simple(STATUS,2,SUCCESS);
        if (value!==0) $fatal(1,"readback survived power reset");
        simple(BEGIN_CPU,0,SUCCESS); simple(PUBLISH,0,INCOMPLETE);
        reset_cpu(); simple(PUBLISH,0,BUSY);
        $display("settings_ram_tb: PASS commands=%0d",checked_commands);
        $finish;
    end
    // The Cyclone V mixed-port read-during-write value is deliberately undefined.
    // Ownership must prevent that physical collision, not rely on a RAM model's
    // choice of old/new data. Both simultaneous readers are permitted.
    always @(posedge clk) if (power_reset_n && dut.cpu_address==dut.bridge_address &&
        ((dut.cpu_write_memory && (dut.bridge_read_memory || bridge_write_accepted)) ||
         (bridge_write_accepted && dut.cpu_read_memory)))
        $fatal(1,"mixed-port RAM collision");
    initial begin #10000000; $fatal(1,"watchdog"); end
endmodule
