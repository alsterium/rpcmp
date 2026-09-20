// CPU-local sound protocol. Word layout and ownership: pocket-sound-mmio-v1.
module rpcmp_sound_mmio (
    input logic cpu_clk, audio_clk, cpu_reset_n, audio_reset_n, device_fault,
    input logic [31:0] mmio_addr, mmio_wr_data,
    input logic [3:0] byte_enable,
    input logic mmio_rd, mmio_wr,
    output logic [31:0] mmio_rd_data,
    output logic mmio_error, sound_fault,
    output logic audio_mclk, audio_lrck, audio_dac
);
    wire common_reset_n=cpu_reset_n && audio_reset_n;
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic [1:0] cpu_release, audio_release;
    wire cpu_local_reset_n=cpu_release[1], audio_local_reset_n=audio_release[1];
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic fault_meta, fault_sync, inhibited_meta, inhibited_sync, online_meta, online_sync;
    (* altera_attribute = "-name SYNCHRONIZER_IDENTIFICATION FORCED_IF_ASYNCHRONOUS" *)
    logic inhibit_meta, inhibit_sync, inhibit_ack_meta, inhibit_ack_sync;
    logic inhibit_toggle, inhibit_seen, inhibit_again;
    wire emergency_pulse=audio_local_reset_n && inhibit_sync!=inhibit_seen;
    wire emergency_pending=inhibit_again || inhibit_toggle!=inhibit_ack_sync;

    logic [287:0] control_stage, control_audio_request;
    logic [511:0] control_cpu_response, control_audio_response;
    logic [383:0] feed_stage, feed_audio_request;
    logic [159:0] feed_cpu_response, feed_audio_response;
    logic [127:0] capture_stage, capture_audio_request;
    logic [1055:0] capture_cpu_response, capture_audio_response;
    logic control_submit, control_release, control_ready, control_valid;
    logic control_a_valid, control_a_ready, control_r_valid, control_r_ready;
    logic feed_submit, feed_release, feed_ready, feed_valid;
    logic feed_a_valid, feed_a_ready, feed_r_valid, feed_r_ready;
    logic capture_submit, capture_release, capture_ready, capture_valid;
    logic capture_a_valid, capture_a_ready, capture_r_valid, capture_r_ready;
    logic invalid_access, busy_access, invalid_sticky, busy_sticky, clear_flags, inhibit_submit;
    logic control_write, feed_write, capture_write;
    wire [9:0] offset=mmio_addr[9:0];
    wire [7:0] word_index=mmio_addr[9:2];
    wire control_staging=offset>=10'h020 && offset<=10'h040;
    wire feed_staging=offset>=10'h080 && offset<=10'h0ac;
    wire capture_staging=offset>=10'h100 && offset<=10'h10c;

    logic [63:0] response_id, response_generation, response_revision, response_frame;
    logic [2:0] response_kind, response_result;
    logic response_target_enabled;
    logic [31:0] response_target;
    logic [2:0] item_status;
    logic [6:0] source_queued;
    logic [63:0] feed_epoch, prepared_generation, generation, policy_revision, media_frame, completed_loops;
    logic fault, inhibited, target_enabled, paused;
    logic [31:0] target;
    logic [1:0] phase;
    logic [2:0] end_reason, failure;
    logic [17:0] gain, ramp_elapsed;
    logic quiescent, resetting, device_idle, media_enable;
    logic [63:0] pending_prefix, output_prefix, pending_loops, output_loops;
    logic pending_source_valid, output_source_valid, pending_checkpoint_valid, output_checkpoint_valid;
    logic pending_ended, output_ended;

    always_ff @(posedge cpu_clk or negedge common_reset_n) begin
        if (!common_reset_n) cpu_release<=0;
        else cpu_release<={cpu_release[0],1'b1};
    end
    always_ff @(posedge audio_clk or negedge common_reset_n) begin
        if (!common_reset_n) audio_release<=0;
        else audio_release<={audio_release[0],1'b1};
    end
    assign sound_fault=fault_sync;
    always_ff @(posedge cpu_clk or negedge cpu_local_reset_n) begin
        if (!cpu_local_reset_n) begin
            fault_meta<=0; fault_sync<=0; inhibited_meta<=0; inhibited_sync<=0; online_meta<=0; online_sync<=0;
            inhibit_ack_meta<=0; inhibit_ack_sync<=0; inhibit_toggle<=0; inhibit_again<=0;
        end else begin
            fault_meta<=fault; fault_sync<=fault_meta;
            inhibited_meta<=inhibited; inhibited_sync<=inhibited_meta;
            online_meta<=audio_local_reset_n; online_sync<=online_meta;
            inhibit_ack_meta<=inhibit_seen; inhibit_ack_sync<=inhibit_ack_meta;
            if (inhibit_ack_sync==inhibit_toggle) begin
                if (inhibit_again || inhibit_submit) begin inhibit_toggle<=~inhibit_toggle; inhibit_again<=0; end
            end else if (inhibit_submit) inhibit_again<=1;
        end
    end
    always_ff @(posedge audio_clk or negedge audio_local_reset_n) begin
        if (!audio_local_reset_n) begin inhibit_meta<=0; inhibit_sync<=0; inhibit_seen<=0; end
        else begin
            inhibit_meta<=inhibit_toggle; inhibit_sync<=inhibit_meta;
            if (emergency_pulse) inhibit_seen<=inhibit_sync;
        end
    end

    always_comb begin
        mmio_rd_data=0; invalid_access=0; busy_access=0;
        control_write=0; feed_write=0; capture_write=0; clear_flags=0; inhibit_submit=0;
        control_submit=0; control_release=0; feed_submit=0; feed_release=0;
        capture_submit=0; capture_release=0;
        if (mmio_rd || mmio_wr) begin
            if (!cpu_local_reset_n || mmio_addr[31:10]!=22'h100001 || offset[1:0]!=0 ||
                (mmio_rd && mmio_wr) || (mmio_wr && byte_enable!=4'hf)) invalid_access=1;
            else if (mmio_wr) begin
                if (control_staging) begin
                    if ((offset==10'h030 && mmio_wr_data[31:3]!=0) ||
                        (offset==10'h03c && mmio_wr_data[31:1]!=0)) invalid_access=1;
                    else control_write=1;
                end else if (feed_staging) begin
                    if ((offset==10'h0a8 && mmio_wr_data[31:2]!=0) ||
                        (offset==10'h0ac && mmio_wr_data[31:16]!=0)) invalid_access=1;
                    else feed_write=1;
                end else if (capture_staging) capture_write=1;
                else case (offset)
                    10'h010: if (mmio_wr_data==1) inhibit_submit=1; else invalid_access=1;
                    10'h014: if (mmio_wr_data[31:2]==0) clear_flags=1; else invalid_access=1;
                    10'h044: if (mmio_wr_data!=1) invalid_access=1;
                        else if (!control_ready || !online_sync || (control_stage[130:128]==0 && emergency_pending)) busy_access=1;
                        else control_submit=1;
                    10'h048: if (mmio_wr_data==1 && control_valid) control_release=1; else invalid_access=1;
                    10'h0b0: if (mmio_wr_data!=1) invalid_access=1;
                        else if (!feed_ready || !online_sync) busy_access=1;
                        else feed_submit=1;
                    10'h0b4: if (mmio_wr_data==1 && feed_valid) feed_release=1; else invalid_access=1;
                    10'h110: if (mmio_wr_data!=1) invalid_access=1;
                        else if (!capture_ready || !online_sync) busy_access=1;
                        else capture_submit=1;
                    10'h114: if (mmio_wr_data==1 && capture_valid) capture_release=1; else invalid_access=1;
                    default: invalid_access=1;
                endcase
            end else begin
                if (control_staging) mmio_rd_data=control_stage[(word_index-8'd8)*32 +: 32];
                else if (feed_staging) mmio_rd_data=feed_stage[(word_index-8'd32)*32 +: 32];
                else if (capture_staging) mmio_rd_data=capture_stage[(word_index-8'd64)*32 +: 32];
                else if (offset>=10'h180 && offset<=10'h1bc) begin
                    if (control_valid) mmio_rd_data=control_cpu_response[(word_index-8'd96)*32 +: 32];
                    else invalid_access=1;
                end else if (offset>=10'h0b8 && offset<=10'h0c8) begin
                    if (feed_valid) mmio_rd_data=feed_cpu_response[(word_index-8'd46)*32 +: 32];
                    else invalid_access=1;
                end else if (offset>=10'h200 && offset<=10'h280) begin
                    if (capture_valid) mmio_rd_data=capture_cpu_response[(word_index-8'd128)*32 +: 32];
                    else invalid_access=1;
                end else case (offset)
                    10'h000: mmio_rd_data=32'h52534d31;
                    10'h004: mmio_rd_data=32'h00010000;
                    10'h008: mmio_rd_data=32'h00000007;
                    10'h00c: mmio_rd_data={20'd0,online_sync,emergency_pending,busy_sticky,invalid_sticky,
                        inhibited_sync,fault_sync,capture_valid,!capture_ready,feed_valid,!feed_ready,
                        control_valid,!control_ready};
                    10'h018: mmio_rd_data=32'd12288000;
                    10'h01c: mmio_rd_data=32'd48000;
                    default: invalid_access=1;
                endcase
            end
        end
        mmio_error=invalid_access || busy_access;
    end
    always_ff @(posedge cpu_clk or negedge cpu_local_reset_n) begin
        if (!cpu_local_reset_n) begin
            control_stage<=0; feed_stage<=0; capture_stage<=0; invalid_sticky<=0; busy_sticky<=0;
        end else begin
            if (control_write) control_stage[(word_index-8'd8)*32 +: 32]<=mmio_wr_data;
            if (feed_write) feed_stage[(word_index-8'd32)*32 +: 32]<=mmio_wr_data;
            if (capture_write) capture_stage[(word_index-8'd64)*32 +: 32]<=mmio_wr_data;
            if (invalid_access) invalid_sticky<=1;
            else if (clear_flags && mmio_wr_data[0]) invalid_sticky<=0;
            if (busy_access) busy_sticky<=1;
            else if (clear_flags && mmio_wr_data[1]) busy_sticky<=0;
        end
    end

    rpcmp_sound_mailbox #(.REQUEST_BITS(288), .RESPONSE_BITS(512)) control_mailbox (
        .src_clk(cpu_clk), .dst_clk(audio_clk), .src_reset_n(cpu_local_reset_n), .dst_reset_n(audio_local_reset_n),
        .src_request_valid(control_submit), .src_request_ready(control_ready), .src_request(control_stage),
        .src_response_valid(control_valid), .src_response_ready(control_release), .src_response(control_cpu_response),
        .dst_request_valid(control_a_valid), .dst_request_ready(control_a_ready), .dst_request(control_audio_request),
        .dst_response_valid(control_r_valid), .dst_response_ready(control_r_ready), .dst_response(control_audio_response)
    );
    rpcmp_sound_mailbox #(.REQUEST_BITS(384), .RESPONSE_BITS(160)) feed_mailbox (
        .src_clk(cpu_clk), .dst_clk(audio_clk), .src_reset_n(cpu_local_reset_n), .dst_reset_n(audio_local_reset_n),
        .src_request_valid(feed_submit), .src_request_ready(feed_ready), .src_request(feed_stage),
        .src_response_valid(feed_valid), .src_response_ready(feed_release), .src_response(feed_cpu_response),
        .dst_request_valid(feed_a_valid), .dst_request_ready(feed_a_ready), .dst_request(feed_audio_request),
        .dst_response_valid(feed_r_valid), .dst_response_ready(feed_r_ready), .dst_response(feed_audio_response)
    );
    rpcmp_sound_mailbox #(.REQUEST_BITS(128), .RESPONSE_BITS(1056)) capture_mailbox (
        .src_clk(cpu_clk), .dst_clk(audio_clk), .src_reset_n(cpu_local_reset_n), .dst_reset_n(audio_local_reset_n),
        .src_request_valid(capture_submit), .src_request_ready(capture_ready), .src_request(capture_stage),
        .src_response_valid(capture_valid), .src_response_ready(capture_release), .src_response(capture_cpu_response),
        .dst_request_valid(capture_a_valid), .dst_request_ready(capture_a_ready), .dst_request(capture_audio_request),
        .dst_response_valid(capture_r_valid), .dst_response_ready(capture_r_ready), .dst_response(capture_audio_response)
    );

    always_comb begin
        control_audio_response=0;
        control_audio_response[0 +: 64]=response_id;
        control_audio_response[64 +: 64]=response_generation;
        control_audio_response[128 +: 64]=response_revision;
        control_audio_response[192 +: 32]={29'd0,response_kind};
        control_audio_response[224 +: 32]={31'd0,response_target_enabled};
        control_audio_response[256 +: 32]=response_target;
        control_audio_response[288 +: 64]=response_frame;
        control_audio_response[352 +: 32]={29'd0,response_result};
        control_audio_response[384 +: 64]=feed_epoch;
        control_audio_response[448 +: 64]=prepared_generation;
    end
    assign feed_a_ready=!feed_r_valid;
    assign capture_a_ready=!capture_r_valid;
    always_ff @(posedge audio_clk or negedge audio_local_reset_n) begin
        if (!audio_local_reset_n) begin
            feed_audio_response<=0; capture_audio_response<=0; feed_r_valid<=0; capture_r_valid<=0;
        end else begin
            if (feed_r_valid && feed_r_ready) feed_r_valid<=0;
            if (feed_a_valid && feed_a_ready) begin
                feed_audio_response<={feed_audio_request[127:0],29'd0,item_status};
                feed_r_valid<=1;
            end
            if (capture_r_valid && capture_r_ready) capture_r_valid<=0;
            if (capture_a_valid && capture_a_ready) begin
                capture_audio_response[0 +: 128]<=capture_audio_request;
                capture_audio_response[128 +: 64]<=feed_epoch;
                capture_audio_response[192 +: 64]<=prepared_generation;
                capture_audio_response[256 +: 64]<=generation;
                capture_audio_response[320 +: 64]<=media_frame;
                capture_audio_response[384 +: 64]<=policy_revision;
                capture_audio_response[448 +: 64]<=completed_loops;
                capture_audio_response[512 +: 32]<=target;
                capture_audio_response[544 +: 32]<={24'd0,inhibited,fault,media_enable,device_idle,resetting,quiescent,paused,target_enabled};
                capture_audio_response[576 +: 32]<={24'd0,failure,end_reason,phase};
                capture_audio_response[608 +: 32]<={14'd0,gain};
                capture_audio_response[640 +: 32]<={14'd0,ramp_elapsed};
                capture_audio_response[672 +: 32]<={25'd0,source_queued};
                capture_audio_response[704 +: 64]<=output_prefix;
                capture_audio_response[768 +: 64]<=output_loops;
                capture_audio_response[832 +: 32]<={29'd0,output_ended,output_checkpoint_valid,output_source_valid};
                capture_audio_response[864 +: 64]<=pending_prefix;
                capture_audio_response[928 +: 64]<=pending_loops;
                capture_audio_response[992 +: 32]<={29'd0,pending_ended,pending_checkpoint_valid,pending_source_valid};
                capture_audio_response[1024 +: 32]<=capture_audio_request[63:0]==0 ? 32'd2 :
                    capture_audio_request[127:64]!=feed_epoch ? 32'd3 : 32'd1;
                capture_r_valid<=1;
            end
        end
    end

    rpcmp_sound_session session (
        .clk_audio(audio_clk), .reset_n(audio_local_reset_n), .device_fault(device_fault), .emergency_silence(emergency_pulse),
        .request_valid(control_a_valid), .request_ready(control_a_ready),
        .request_id(control_audio_request[63:0]), .request_generation(control_audio_request[127:64]),
        .request_kind(control_audio_request[130:128]), .request_revision(control_audio_request[223:160]),
        .request_target_enabled(control_audio_request[224]), .request_target(control_audio_request[287:256]),
        .response_valid(control_r_valid), .response_ready(control_r_ready),
        .response_id(response_id), .response_generation(response_generation), .response_revision(response_revision),
        .response_frame(response_frame), .response_kind(response_kind), .response_result(response_result),
        .response_target_enabled(response_target_enabled), .response_target(response_target),
        .item_valid(feed_a_valid && feed_a_ready), .item_marker(feed_audio_request[320]), .item_end(feed_audio_request[321]),
        .item_generation(feed_audio_request[63:0]), .item_epoch(feed_audio_request[127:64]),
        .item_at(feed_audio_request[191:128]), .item_until(feed_audio_request[255:192]), .item_loops(feed_audio_request[319:256]),
        .item_address(feed_audio_request[367:360]), .item_value(feed_audio_request[359:352]),
        .item_status(item_status), .source_queued(source_queued), .feed_epoch(feed_epoch), .prepared_generation(prepared_generation),
        .fault(fault), .inhibited(inhibited), .generation(generation), .policy_revision(policy_revision), .media_frame(media_frame),
        .completed_loops(completed_loops), .target_enabled(target_enabled), .paused(paused), .target(target),
        .phase(phase), .end_reason(end_reason), .failure(failure), .gain(gain), .ramp_elapsed(ramp_elapsed),
        .quiescent(quiescent), .resetting(resetting), .device_idle(device_idle), .media_enable(media_enable), .frame_boundary(),
        .pending_prefix(pending_prefix), .output_prefix(output_prefix), .pending_source_valid(pending_source_valid),
        .output_source_valid(output_source_valid), .pending_checkpoint_valid(pending_checkpoint_valid),
        .output_checkpoint_valid(output_checkpoint_valid), .pending_ended(pending_ended), .output_ended(output_ended),
        .pending_loops(pending_loops), .output_loops(output_loops),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac)
    );
endmodule
