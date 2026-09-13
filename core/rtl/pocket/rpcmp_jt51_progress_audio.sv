// Scheduled checkpoint-to-output owner. See mdx-audible-progress-v1.
module rpcmp_jt51_progress_audio (
    input logic clk_audio, reset_n, stream_reset, session_reset, device_fault, clear_audio_flags,
    input logic item_valid, item_marker, item_end,
    input logic [63:0] item_at, item_until, item_loops,
    input logic [7:0] item_address, item_value,
    output logic [2:0] item_status,
    output logic [6:0] source_queued,
    input logic receipt_ready,
    output logic receipt_valid, receipt_marker,
    output logic [63:0] operation_token, receipt_token, receipt_at_edge,
    input logic begin_valid,
    input logic [63:0] begin_generation, begin_revision,
    input logic begin_target_enabled,
    input logic [31:0] begin_target,
    output logic [2:0] begin_status,
    input logic control_valid,
    input logic [63:0] control_generation, control_revision,
    input logic [1:0] control_action,
    input logic control_repeat, control_target_enabled,
    input logic [31:0] control_target,
    output logic [1:0] control_status,
    output logic [63:0] generation, policy_revision, media_frame, completed_loops,
    output logic target_enabled, paused,
    output logic [31:0] target,
    output logic [1:0] phase,
    output logic [2:0] end_reason, failure,
    output logic [17:0] gain, ramp_elapsed,
    output logic [8:0] queued,
    output logic quiescent, resetting, device_idle, media_enable, frame_boundary,
    output logic [63:0] source_edge, pending_source_edge, output_source_edge,
    output logic [63:0] pending_prefix, output_prefix,
    output logic pending_source_valid, output_source_valid,
    output logic pending_checkpoint_valid, output_checkpoint_valid,
    output logic pending_ended, output_ended,
    output logic [63:0] pending_loops, output_loops,
    output logic audio_mclk, audio_lrck, audio_dac,
    output logic audio_underflow, audio_overflow, audio_clipped,
    output logic [31:0] selected_count, frame_count
);
    logic [11:0] reset_remaining;
    logic dev_valid, dev_ready, marker_valid, marker_ready, supply_fault;
    logic [7:0] dev_address, dev_value;
    logic [2:0] queue_status;
    logic valid_metadata, armed, bootstrap_pending, progress_closed, mapping_fault;
    logic [63:0] last_admitted_loops, admitted_until;
    logic [64:0] dispatch_payload;
    logic [65:0] marker_payload, native_payload, pending_payload, output_payload;
    logic progress_valid, progress_ended;
    logic [63:0] progress_sequence, progress_at, progress_until, progress_loops;
    logic [2:0] progress_status;
    logic terminal_seen, terminal, active, reset_trigger, shared_fault;
    logic jt_sample, consume, source_edge_exhausted, operation_exhausted;
    logic native_receipt_valid, native_receipt_ready, completion_ready, completion_fault;
    logic [63:0] native_prefix;
    logic signed [15:0] jt_left, jt_right, source_left, source_right;
    logic signed [15:0] transformed_left, transformed_right;

    assign terminal = end_reason!=0 || failure!=0;
    assign active = generation!=0 && !terminal;
    assign shared_fault = device_fault || (stream_reset && active) ||
                          audio_underflow || audio_overflow || source_edge_exhausted ||
                          operation_exhausted || completion_fault || supply_fault || mapping_fault;
    assign reset_trigger = session_reset || stream_reset || shared_fault || (terminal && !terminal_seen);
    assign resetting = !reset_n || reset_trigger || reset_remaining!=0;
    assign quiescent = !resetting && !active;
    // Atomic multicast: neither the external receipt consumer nor completion
    // FIFO can consume alone. A blocked external valid cannot be withdrawn:
    // completion capacity only decreases on this shared handshake.
    assign receipt_valid = native_receipt_valid && completion_ready;
    assign native_receipt_ready = receipt_ready && completion_ready;

    assign valid_metadata = item_marker ? item_loops>=last_admitted_loops : item_loops==0;
    always_comb begin
        item_status=queue_status;
        if (item_valid) begin
            if (resetting) item_status=4;
            else if (!valid_metadata) item_status=3;
        end
    end
    rpcmp_media_source_queue #(.PAYLOAD_WIDTH(65)) source_queue (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .media_enable(media_enable), .source_edge(source_edge),
        .item_valid(item_valid && valid_metadata), .item_marker(item_marker), .item_end(item_end),
        .item_at(item_at), .item_until(item_until), .item_address(item_address), .item_value(item_value),
        .item_payload({item_end,item_loops}), .dispatch_payload(dispatch_payload),
        .item_status(queue_status), .queued(source_queued), .supply_fault(supply_fault),
        .dev_valid(dev_valid), .dev_address(dev_address), .dev_value(dev_value), .dev_ready(dev_ready),
        .marker_valid(marker_valid), .marker_ready(marker_ready)
    );

    // Marker metadata belongs to the native source's single reserved receipt.
    // An old receipt and a new marker on one edge observe the old latch first.
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin last_admitted_loops<=0; marker_payload<=0; end
        else if (resetting) begin last_admitted_loops<=0; marker_payload<=0; end
        else begin
            if (item_status==1 && item_marker) last_admitted_loops<=item_loops;
            if (marker_valid && marker_ready) marker_payload<={1'b1,dispatch_payload};
        end
    end

    rpcmp_native_completion #(.PAYLOAD_WIDTH(66)) completion (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .media_enable(media_enable), .source_edge(source_edge),
        .receipt_valid(native_receipt_valid && receipt_ready), .receipt_ready(completion_ready),
        .receipt_token(receipt_token), .receipt_at_edge(receipt_at_edge),
        .receipt_update(receipt_marker), .receipt_payload(marker_payload), .native_payload(native_payload),
        .native_prefix(native_prefix), .completion_fault(completion_fault)
    );

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            reset_remaining<=12'd2048; terminal_seen<=0;
        end else begin
            terminal_seen<=terminal;
            if (reset_trigger) reset_remaining<=12'd2048;
            else if (reset_remaining!=0) reset_remaining<=reset_remaining-12'd1;
        end
    end

    // First two frames have no selected sample. Bootstrap explicitly covers
    // that reset baseline before arming a consuming serial boundary.
    assign progress_valid = active && !resetting && !progress_closed &&
        (bootstrap_pending || (armed && pending_source_valid && media_frame==admitted_until &&
                              media_frame!=64'hffffffffffffffff));
    assign progress_sequence = bootstrap_pending ? 64'd1 : media_frame;
    assign progress_at = bootstrap_pending ? 64'd0 : media_frame;
    assign progress_until = bootstrap_pending ? 64'd2 : media_frame+64'd1;
    assign progress_loops = bootstrap_pending ? 64'd0 : pending_payload[63:0];
    assign progress_ended = !bootstrap_pending && pending_payload[64];
    assign pending_checkpoint_valid = pending_source_valid && pending_payload[65];
    assign output_checkpoint_valid = output_source_valid && output_payload[65];
    assign pending_ended = pending_payload[64];
    assign output_ended = output_payload[64];
    assign pending_loops = pending_payload[63:0];
    assign output_loops = output_payload[63:0];
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            armed<=0; bootstrap_pending<=0; progress_closed<=0; admitted_until<=0; mapping_fault<=0;
        end else if (resetting) begin
            armed<=0; bootstrap_pending<=0; progress_closed<=0; admitted_until<=0; mapping_fault<=0;
        end else if (begin_status==1) begin
            armed<=0; bootstrap_pending<=1; progress_closed<=0; admitted_until<=0;
        end else if (progress_valid) begin
            if (progress_status==1) begin
                armed<=1; bootstrap_pending<=0; admitted_until<=progress_until;
                progress_closed<=progress_ended;
            end else if (progress_status==3 || progress_status==6) mapping_fault<=1;
        end
    end

    rpcmp_media_envelope envelope (
        .clk_audio(clk_audio), .reset_n(reset_n && !session_reset),
        .quiescent(quiescent && source_queued!=0), .device_fault(shared_fault),
        .begin_valid(begin_valid), .begin_generation(begin_generation), .begin_revision(begin_revision),
        .begin_target_enabled(begin_target_enabled), .begin_target(begin_target), .begin_status(begin_status),
        .progress_valid(progress_valid), .progress_generation(generation),
        .progress_sequence(progress_sequence), .progress_at(progress_at), .progress_until(progress_until),
        .progress_loops(progress_loops), .progress_ended(progress_ended), .progress_status(progress_status),
        .frame_tick(frame_boundary && !resetting && armed), .control_valid(control_valid),
        .control_generation(control_generation), .control_revision(control_revision),
        .control_action(control_action), .control_repeat(control_repeat),
        .control_target_enabled(control_target_enabled), .control_target(control_target),
        .source_left(source_left), .source_right(source_right), .consume(consume),
        .output_left(transformed_left), .output_right(transformed_right), .boundary_gain(),
        .control_status(control_status), .generation(generation), .policy_revision(policy_revision),
        .media_frame(media_frame), .completed_loops(completed_loops), .target_enabled(target_enabled),
        .paused(paused), .target(target), .phase(phase), .end_reason(end_reason), .failure(failure),
        .gain(gain), .ramp_elapsed(ramp_elapsed), .queued(queued)
    );
    rpcmp_jt51_media_source source_media (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .media_enable(media_enable), .dev_valid(dev_valid), .dev_ready(dev_ready),
        .dev_address(dev_address), .dev_value(dev_value), .device_idle(device_idle),
        .marker_valid(marker_valid), .marker_ready(marker_ready), .operation_token(operation_token),
        .receipt_valid(native_receipt_valid), .receipt_ready(native_receipt_ready), .receipt_marker(receipt_marker),
        .receipt_token(receipt_token), .receipt_at_edge(receipt_at_edge),
        .operation_exhausted(operation_exhausted),
        .jt_sample(jt_sample), .jt_left(jt_left), .jt_right(jt_right),
        .source_edge(source_edge), .source_edge_exhausted(source_edge_exhausted)
    );
    rpcmp_media_output #(.INITIAL_RUNNING(0), .PAYLOAD_WIDTH(66)) output_media (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .frame_consume(consume), .clear_flags(clear_audio_flags),
        .src_valid(jt_sample), .src_left({{2{jt_left[15]}},jt_left}),
        .src_right({{2{jt_right[15]}},jt_right}),
        .src_at_edge(source_edge), .source_valid(pending_source_valid),
        .source_at_edge(pending_source_edge), .output_valid(output_source_valid),
        .output_at_edge(output_source_edge),
        .src_prefix(native_prefix), .source_prefix(pending_prefix), .output_prefix(output_prefix),
        .src_payload(native_payload), .source_payload(pending_payload), .output_payload(output_payload),
        .transformed_left(transformed_left), .transformed_right(transformed_right),
        .source_left(source_left), .source_right(source_right), .media_enable(media_enable),
        .running(), .frame_boundary(frame_boundary), .audio_mclk(audio_mclk),
        .audio_lrck(audio_lrck), .audio_dac(audio_dac), .underflow(audio_underflow),
        .overflow(audio_overflow), .clipped(audio_clipped),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
