// Reset-retained, synchronous settings storage. See pocket-settings-ram-v1.
// Connect power_reset_n only to configuration/power reset, never CPU/audio reset.
module rpcmp_settings_ram (
    input logic clk, power_reset_n, abort_candidate,
    input logic command_valid,
    output logic command_ready,
    input logic [4:0] command_kind,
    input logic [1:0] command_slot,
    input logic [31:0] command_length, command_word, command_data,
    input logic [3:0] command_mask,
    output logic response_valid,
    input logic response_ready,
    output logic [2:0] response_result,
    output logic [31:0] response_data,
    input logic bridge_read, bridge_write,
    input logic [31:0] bridge_offset, bridge_data,
    input logic [3:0] bridge_mask,
    output logic bridge_write_accepted,
    output logic [31:0] bridge_read_data
);
    localparam logic [4:0] BEGIN_CPU=0, WRITE_CPU=1, PUBLISH=2, ABORT_CPU=3,
        RELEASE_CPU=4, BEGIN_LOAD=5, FINISH_LOAD=6, BEGIN_READ=7, END_READ=8,
        BEGIN_READBACK=9, FINISH_READBACK=10, RELEASE_READBACK=11,
        READ_WORD=12, STATUS=13, LOCK_SNAPSHOT=14, UNLOCK_SNAPSHOT=15;
    localparam logic [2:0] SUCCESS=1, BUSY=2, INVALID=3, INCOMPLETE=4;
    typedef enum logic [1:0] {EMPTY, CPU, HOST} fill_t;
    typedef enum logic [1:0] {RB_EMPTY, RB_RECEIVING, RB_SEALED} readback_t;
    fill_t fill [0:1];
    readback_t readback_state;
    // One true dual-port memory: 64 bank words, then 16 readback words.
    // Metadata/validity has reset; the memory itself deliberately has none.
    logic [31:0] cpu_word_q, bridge_word_q, response_meta;
    logic [6:0] cpu_address, bridge_address, response_length, bridge_result_length;
    logic [3:0] response_word, bridge_result_word;
    logic cpu_read_memory, cpu_write_memory, bridge_read_memory;
    logic response_is_word, bridge_result_valid;
    logic active_bank [0:1], cpu_lease [0:1], host_lock [0:1], snapshot_lock [0:1];
    logic load_error [0:1], changed_since_load [0:1];
    logic [6:0] length [0:1], load_length [0:1], readback_length;
    logic [63:0] coverage [0:1], readback_coverage;
    logic slot_valid, slot_busy, slot_select;
    logic bridge_valid, bridge_slot, bridge_is_readback;
    logic [3:0] bridge_word, accepted_mask;
    logic [6:0] write_length;

    function automatic logic [63:0] required_bytes(input logic [6:0] count);
        return count==0 ? 64'd0 : 64'hffffffffffffffff >> (7'd64-count);
    endfunction

    function automatic logic [31:0] bounded_word(
        input logic [31:0] value, input logic [3:0] word_index,
        input logic [6:0] byte_length
    );
        logic [31:0] result;
        result=0;
        for (int lane=0; lane<4; lane++)
            if (({1'b0,word_index,2'b00}+lane)<byte_length)
                result[lane*8+:8]=value[lane*8+:8];
        return result;
    endfunction

    assign command_ready=power_reset_n && !response_valid;
    assign slot_valid=command_slot<2;
    assign slot_select=command_slot[0];
    assign slot_busy=fill[slot_select]!=EMPTY || cpu_lease[slot_select] ||
        host_lock[slot_select] || snapshot_lock[slot_select];
    assign bridge_valid=bridge_offset<192 && bridge_offset[1:0]==0;
    assign bridge_slot=bridge_offset[6];
    assign bridge_is_readback=bridge_offset[7];
    assign bridge_word=bridge_offset[5:2];
    assign cpu_read_memory=command_valid && command_ready && command_kind==READ_WORD &&
        command_slot<3 && command_word<16 &&
        (command_slot==2 ? readback_state==RB_SEALED : snapshot_lock[slot_select]);
    assign cpu_write_memory=command_valid && command_ready && command_kind==WRITE_CPU &&
        slot_valid && command_word<16 && command_mask!=0 &&
        !abort_candidate && fill[slot_select]==CPU;
    assign cpu_address=command_slot==2 ? {3'b100,command_word[3:0]} :
        {1'b0,slot_select,active_bank[slot_select]^cpu_write_memory,command_word[3:0]};
    assign bridge_read_memory=power_reset_n && bridge_read && bridge_valid &&
        !bridge_is_readback && (host_lock[bridge_slot] || cpu_lease[bridge_slot]);
    assign bridge_address=bridge_is_readback ? {3'b100,bridge_word} :
        {1'b0,bridge_slot,active_bank[bridge_slot]^bridge_write_accepted,bridge_word};
    assign response_data=response_is_word ? bounded_word(cpu_word_q,response_word,response_length) : response_meta;
    assign bridge_read_data=bridge_result_valid ?
        bounded_word(bridge_word_q,bridge_result_word,bridge_result_length) : 32'd0;

    // Use the same Cyclone V primitive as the APF shell. Its vendor model is
    // also used by the testbench. Leases exclude mixed-port write collisions.
    altsyncram #(
        .operation_mode("BIDIR_DUAL_PORT"), .intended_device_family("Cyclone V"),
        .width_a(32), .width_b(32), .widthad_a(7), .widthad_b(7),
        .numwords_a(128), .numwords_b(128), .width_byteena_a(4), .width_byteena_b(4),
        .address_reg_b("CLOCK1"), .indata_reg_b("CLOCK1"),
        .wrcontrol_wraddress_reg_b("CLOCK1"), .byteena_reg_b("CLOCK1"),
        .outdata_reg_a("UNREGISTERED"), .outdata_reg_b("UNREGISTERED"),
        .clock_enable_input_a("NORMAL"), .clock_enable_input_b("NORMAL"),
        .clock_enable_output_a("BYPASS"), .clock_enable_output_b("BYPASS"),
        .power_up_uninitialized("TRUE"), .read_during_write_mode_mixed_ports("DONT_CARE"),
        .read_during_write_mode_port_a("NEW_DATA_NO_NBE_READ"),
        .read_during_write_mode_port_b("NEW_DATA_NO_NBE_READ")
    ) storage (
        .clock0(clk), .clock1(clk), .clocken0(cpu_read_memory || cpu_write_memory),
        .clocken1(bridge_read_memory || bridge_write_accepted), .clocken2(1'b1), .clocken3(1'b1),
        .address_a(cpu_address), .address_b(bridge_address),
        .data_a(command_data), .data_b(bridge_data),
        .byteena_a(command_mask), .byteena_b(accepted_mask),
        .wren_a(cpu_write_memory), .wren_b(bridge_write_accepted),
        .rden_a(cpu_read_memory), .rden_b(bridge_read_memory),
        .q_a(cpu_word_q), .q_b(bridge_word_q), .eccstatus(),
        .aclr0(1'b0), .aclr1(1'b0), .addressstall_a(1'b0), .addressstall_b(1'b0)
    );

    always_comb begin
        write_length=bridge_is_readback ? readback_length : load_length[bridge_slot];
        accepted_mask=0;
        if (power_reset_n && bridge_write && !bridge_read && bridge_valid &&
            (bridge_is_readback ? readback_state==RB_RECEIVING : fill[bridge_slot]==HOST))
            for (int lane=0; lane<4; lane++)
                if (({1'b0,bridge_word,2'b00}+lane)<write_length)
                    accepted_mask[lane]=bridge_mask[lane];
        bridge_write_accepted=|accepted_mask;
    end

    always_ff @(posedge clk or negedge power_reset_n) begin
        if (!power_reset_n) begin
            response_valid<=0; response_result<=0; response_meta<=0;
            response_is_word<=0; response_word<=0; response_length<=0;
            bridge_result_valid<=0; bridge_result_word<=0; bridge_result_length<=0;
            readback_state<=RB_EMPTY; readback_length<=0; readback_coverage<=0;
            for (int slot=0; slot<2; slot++) begin
                active_bank[slot]<=0; cpu_lease[slot]<=0;
                host_lock[slot]<=0; snapshot_lock[slot]<=0; load_error[slot]<=0;
                changed_since_load[slot]<=0;
                fill[slot]<=EMPTY; length[slot]<=0; load_length[slot]<=0;
                coverage[slot]<=0;
            end
        end else begin
            if (response_valid && response_ready) response_valid<=0;
            if (abort_candidate)
                for (int slot=0; slot<2; slot++)
                    if (fill[slot]==CPU) begin fill[slot]<=EMPTY; coverage[slot]<=0; end

            if (bridge_read) begin
                bridge_result_valid<=bridge_read_memory;
                bridge_result_word<=bridge_word; bridge_result_length<=length[bridge_slot];
            end
            if (bridge_write && !bridge_read) bridge_result_valid<=0;
            if (bridge_write_accepted)
                for (int lane=0; lane<4; lane++)
                    if (accepted_mask[lane]) begin
                        if (bridge_is_readback) begin
                            readback_coverage[{bridge_word,2'b00}+lane]<=1;
                        end else begin
                            coverage[bridge_slot][{bridge_word,2'b00}+lane]<=1;
                        end
                    end

            if (command_valid && command_ready) begin
                response_valid<=1; response_result<=INVALID; response_meta<=0;
                response_is_word<=cpu_read_memory; response_word<=command_word[3:0];
                response_length<=command_slot==2 ? readback_length : length[slot_select];
                case (command_kind)
                    BEGIN_CPU, BEGIN_LOAD: if (slot_valid &&
                        (command_kind==BEGIN_CPU || command_length<=64)) begin
                        response_result<=BUSY;
                        if (!slot_busy && !(abort_candidate && command_kind==BEGIN_CPU)) begin
                            fill[slot_select]<=command_kind==BEGIN_CPU ? CPU : HOST;
                            coverage[slot_select]<=0;
                            load_length[slot_select]<=command_kind==BEGIN_CPU ? 7'd64 : command_length[6:0];
                            response_result<=SUCCESS;
                        end
                    end
                    WRITE_CPU: if (slot_valid && command_word<16 && command_mask!=0) begin
                        response_result<=BUSY;
                        if (!abort_candidate && fill[slot_select]==CPU) begin
                            for (int lane=0; lane<4; lane++)
                                if (command_mask[lane]) begin
                                    coverage[slot_select][{command_word[3:0],2'b00}+lane]<=1;
                                end
                            response_result<=SUCCESS;
                        end
                    end
                    PUBLISH: if (slot_valid) begin
                        response_result<=BUSY;
                        if (!abort_candidate && fill[slot_select]==CPU &&
                            !host_lock[slot_select] && !snapshot_lock[slot_select]) begin
                            response_result<=INCOMPLETE;
                            if (&coverage[slot_select]) begin
                                active_bank[slot_select]<=~active_bank[slot_select];
                                length[slot_select]<=64; fill[slot_select]<=EMPTY;
                                cpu_lease[slot_select]<=1;
                                changed_since_load[slot_select]<=1;
                                response_result<=SUCCESS;
                            end
                        end
                    end
                    ABORT_CPU: if (slot_valid && fill[slot_select]==CPU) begin
                        fill[slot_select]<=EMPTY; coverage[slot_select]<=0;
                        response_result<=SUCCESS;
                    end
                    RELEASE_CPU: if (slot_valid && cpu_lease[slot_select]) begin
                        cpu_lease[slot_select]<=0; response_result<=SUCCESS;
                    end
                    FINISH_LOAD: if (slot_valid && fill[slot_select]==HOST) begin
                        response_result<=BUSY;
                        if (!(bridge_write_accepted && !bridge_is_readback && bridge_slot==slot_select)) begin
                            fill[slot_select]<=EMPTY; load_error[slot_select]<=1;
                            response_result<=INCOMPLETE;
                            if ((coverage[slot_select] & required_bytes(load_length[slot_select]))==
                                required_bytes(load_length[slot_select])) begin
                                active_bank[slot_select]<=~active_bank[slot_select];
                                length[slot_select]<=load_length[slot_select];
                                load_error[slot_select]<=0; changed_since_load[slot_select]<=0;
                                response_result<=SUCCESS;
                            end
                        end
                    end
                    BEGIN_READ, LOCK_SNAPSHOT: if (slot_valid) begin
                        response_result<=BUSY;
                        if (fill[slot_select]!=HOST) begin
                            if (command_kind==BEGIN_READ) host_lock[slot_select]<=1;
                            else snapshot_lock[slot_select]<=1;
                            response_result<=SUCCESS;
                        end
                    end
                    END_READ: if (slot_valid && host_lock[slot_select]) begin
                        host_lock[slot_select]<=0; response_result<=SUCCESS;
                    end
                    UNLOCK_SNAPSHOT: if (slot_valid && snapshot_lock[slot_select]) begin
                        snapshot_lock[slot_select]<=0; response_result<=SUCCESS;
                    end
                    BEGIN_READBACK: if (command_slot==2 && command_length<=64) begin
                        response_result<=BUSY;
                        if (readback_state==RB_EMPTY) begin
                            readback_state<=RB_RECEIVING; readback_length<=command_length[6:0];
                            readback_coverage<=0; response_result<=SUCCESS;
                        end
                    end
                    FINISH_READBACK: if (command_slot==2 && readback_state==RB_RECEIVING) begin
                        response_result<=BUSY;
                        if (!(bridge_write_accepted && bridge_is_readback)) begin
                            response_result<=INCOMPLETE;
                            if ((readback_coverage & required_bytes(readback_length))==required_bytes(readback_length)) begin
                                readback_state<=RB_SEALED; response_result<=SUCCESS;
                            end
                        end
                    end
                    RELEASE_READBACK: if (command_slot==2 && readback_state!=RB_EMPTY) begin
                        readback_state<=RB_EMPTY; readback_length<=0;
                        readback_coverage<=0; response_result<=SUCCESS;
                    end
                    READ_WORD: if (command_slot<3 && command_word<16) begin
                        response_result<=BUSY;
                        if (cpu_read_memory) response_result<=SUCCESS;
                    end
                    STATUS: if (command_slot<3) begin
                        response_result<=SUCCESS;
                        if (command_slot==2)
                            response_meta<={22'd0,readback_state==RB_SEALED,
                                readback_state==RB_RECEIVING,1'b0,readback_length};
                        else response_meta<={17'd0,changed_since_load[slot_select],snapshot_lock[slot_select],load_error[slot_select],
                            host_lock[slot_select],fill[slot_select]==HOST,cpu_lease[slot_select],
                            fill[slot_select]==CPU,1'b0,length[slot_select]};
                    end
                    default: begin end
                endcase
            end
        end
    end
endmodule
