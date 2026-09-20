// HYB1 local, single-word bus. The AXI adapter owns burst/response handling.
module rpcmp_hybrid_mmio (
    input logic clk_cpu, clk_audio, reset_n,
    input logic read, write,
    input logic [9:0] address,
    input logic [3:0] byte_enable,
    input logic [31:0] write_data,
    output logic [31:0] read_data,
    output logic error,
    output wire audio_mclk, audio_lrck, audio_dac
);
    (* preserve, altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic [1:0] cpu_reset_pipe, audio_reset_pipe;
    always_ff @(posedge clk_cpu or negedge reset_n)
        if (!reset_n) cpu_reset_pipe<=0; else cpu_reset_pipe<={cpu_reset_pipe[0],1'b1};
    always_ff @(posedge clk_audio or negedge reset_n)
        if (!reset_n) audio_reset_pipe<=0; else audio_reset_pipe<={audio_reset_pipe[0],1'b1};
    wire cpu_reset_n=cpu_reset_pipe[1], audio_reset_n=audio_reset_pipe[1];
    localparam logic [1:0] CLEAR_WAIT=0, RELEASE_WAIT=1, READY=2;
    logic [1:0] control_state;
    logic clear_request, start_request, started;
    (* preserve, altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic [1:0] clear_sync, start_sync, ack_sync;
    (* preserve, altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic [5:0] status_meta, status_sync;
    logic clear_ack;
    logic [12:0] reset_cycles;
    logic [4:0] release_cycles;
    wire audio_clear=clear_sync[1];
    wire running, ended;
    wire [3:0] faults;
    always_ff @(posedge clk_audio or negedge audio_reset_n) begin
        if (!audio_reset_n) begin
            clear_sync<=3; start_sync<=0; clear_ack<=0; reset_cycles<=0; release_cycles<=0;
        end else begin
            clear_sync<={clear_sync[0],clear_request}; start_sync<={start_sync[0],start_request};
            if (audio_clear) begin
                release_cycles<=0;
                if (reset_cycles == 4095) clear_ack<=1;
                else reset_cycles<=reset_cycles+1'b1;
            end else begin
                reset_cycles<=0;
                // Both vendor FIFO reset synchronizers and pointer pipelines
                // settle before the CPU can publish a fresh stream.
                if (release_cycles == 16) clear_ack<=0;
                else release_cycles<=release_cycles+1'b1;
            end
        end
    end
    always_ff @(posedge clk_cpu or negedge cpu_reset_n) begin
        if (!cpu_reset_n) begin ack_sync<=0; status_meta<=0; status_sync<=0; end
        else begin
            ack_sync<={ack_sync[0],clear_ack};
            status_meta<={faults,ended,running}; status_sync<=status_meta;
        end
    end

    logic [31:0] staged_left, staged_at, last_at;
    logic left_valid, at_valid, end_queued;
    wire pcm_full, fm_full, pcm_empty, fm_empty, pcm_pop, fm_pop;
    wire [11:0] pcm_used;
    wire [9:0] fm_used;
    wire [63:0] pcm_head;
    wire [48:0] fm_head;
    wire available=control_state == READY && status_sync[5:1] == 0;
    wire pcm_commit=write && !error && address == 8'h14;
    wire fm_commit=write && !error && address == 8'h24;
    wire fifo_clear=!reset_n || audio_clear;

    dcfifo_mixed_widths #(.intended_device_family("Cyclone V"), .lpm_numwords(4096),
        .lpm_showahead("ON"), .lpm_type("dcfifo_mixed_widths"), .lpm_width(64), .lpm_widthu(12),
        .lpm_width_r(64), .lpm_widthu_r(12),
        .overflow_checking("ON"), .underflow_checking("ON"),
        .rdsync_delaypipe(5), .wrsync_delaypipe(5), .use_eab("ON"),
        .read_aclr_synch("ON"), .write_aclr_synch("ON")) pcm_fifo (
        .wrclk(clk_cpu), .rdclk(clk_audio), .aclr(fifo_clear),
        .data({staged_left,write_data}), .wrreq(pcm_commit), .wrfull(pcm_full), .wrusedw(pcm_used),
        .q(pcm_head), .rdreq(pcm_pop), .rdempty(pcm_empty), .rdfull(), .rdusedw(), .wrempty(), .eccstatus()
    );
    dcfifo_mixed_widths #(.intended_device_family("Cyclone V"), .lpm_numwords(1024),
        .lpm_showahead("ON"), .lpm_type("dcfifo_mixed_widths"), .lpm_width(49), .lpm_widthu(10),
        .lpm_width_r(49), .lpm_widthu_r(10),
        .overflow_checking("ON"), .underflow_checking("ON"),
        .rdsync_delaypipe(5), .wrsync_delaypipe(5), .use_eab("ON"),
        .read_aclr_synch("ON"), .write_aclr_synch("ON")) fm_fifo (
        .wrclk(clk_cpu), .rdclk(clk_audio), .aclr(fifo_clear),
        .data({staged_at,write_data[16:0]}), .wrreq(fm_commit), .wrfull(fm_full), .wrusedw(fm_used),
        .q(fm_head), .rdreq(fm_pop), .rdempty(fm_empty), .rdfull(), .rdusedw(), .wrempty(), .eccstatus()
    );
    rpcmp_hybrid_audio audio (
        .clk_audio(clk_audio), .reset_n(audio_reset_n), .clear(audio_clear), .start(start_sync[1]),
        .pcm_empty(pcm_empty), .pcm_data(pcm_head), .pcm_pop(pcm_pop),
        .fm_empty(fm_empty), .fm_data(fm_head), .fm_pop(fm_pop),
        .running(running), .ended(ended), .faults(faults),
        .source_count(), .write_count(), .max_late_samples(),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac)
    );

    always_comb begin
        read_data=0; error=0;
        if (read && write) error=1;
        else if (read || write) begin
            if (address[9:8] != 0 || address[1:0] != 0 || byte_enable != 4'hf || !cpu_reset_n) error=1;
            else if (read) case (address)
                8'h00: read_data=32'h48594231;
                8'h04: read_data={24'd0,status_sync[5:2],status_sync[1],status_sync[0],started,
                                  control_state == READY};
                8'h18: read_data=pcm_full ? 0 : 32'd4096-{20'd0,pcm_used};
                8'h28: read_data=fm_full ? 0 : 32'd1024-{22'd0,fm_used};
                default: error=1;
            endcase
            else case (address)
                8'h08: begin
                    if (write_data == 1) error=control_state != READY;
                    else if (write_data == 2)
                        error=!available || started || (!pcm_full && pcm_used == 0);
                    else error=1;
                end
                8'h10: error=!available;
                8'h14: error=!available || !left_valid || pcm_full;
                8'h20: error=!available || end_queued;
                8'h24: error=!available || end_queued || !at_valid || fm_full ||
                             staged_at < last_at || write_data[31:17] != 0 ||
                             (write_data[16] && write_data[15:0] != 0);
                default: error=1;
            endcase
        end
    end
    always_ff @(posedge clk_cpu or negedge cpu_reset_n) begin
        if (!cpu_reset_n) begin
            control_state<=CLEAR_WAIT; clear_request<=1; start_request<=0; started<=0;
            staged_left<=0; staged_at<=0; last_at<=0; left_valid<=0; at_valid<=0; end_queued<=0;
        end else begin
            if (control_state == CLEAR_WAIT && ack_sync[1]) begin
                clear_request<=0; control_state<=RELEASE_WAIT;
            end
            if (control_state == RELEASE_WAIT && !ack_sync[1]) control_state<=READY;
            if (write && !error) case (address)
                8'h08: if (write_data == 1) begin
                    clear_request<=1; start_request<=0; started<=0; control_state<=CLEAR_WAIT;
                    left_valid<=0; at_valid<=0; last_at<=0; end_queued<=0;
                end else begin start_request<=1; started<=1; end
                8'h10: begin staged_left<=write_data; left_valid<=1; end
                8'h14: left_valid<=0;
                8'h20: begin staged_at<=write_data; at_valid<=1; end
                8'h24: begin last_at<=staged_at; at_valid<=0; end_queued<=write_data[16]; end
                default: ;
            endcase
        end
    end
endmodule
