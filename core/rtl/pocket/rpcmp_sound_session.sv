// Copied, single-owner audio-domain controls. See pocket-sound-session-v1.
module rpcmp_sound_session (
    input logic clk_audio, reset_n, device_fault, emergency_silence,
    input logic request_valid,
    output logic request_ready,
    input logic [63:0] request_id, request_generation, request_revision,
    input logic [2:0] request_kind,
    input logic request_target_enabled,
    input logic [31:0] request_target,
    output logic response_valid,
    input logic response_ready,
    output logic [63:0] response_id, response_generation, response_revision, response_frame,
    output logic [2:0] response_kind, response_result,
    output logic response_target_enabled,
    output logic [31:0] response_target,
    input logic item_valid, item_marker, item_end,
    input logic [63:0] item_generation, item_epoch, item_at, item_until, item_loops,
    input logic [7:0] item_address, item_value,
    output logic [2:0] item_status,
    output logic [6:0] source_queued,
    output logic [63:0] feed_epoch, prepared_generation,
    output logic fault, inhibited,
    output logic [63:0] generation, policy_revision, media_frame, completed_loops,
    output logic target_enabled, paused,
    output logic [31:0] target,
    output logic [1:0] phase,
    output logic [2:0] end_reason, failure,
    output logic [17:0] gain, ramp_elapsed,
    output logic quiescent, resetting, device_idle, media_enable, frame_boundary,
    output logic [63:0] pending_prefix, output_prefix,
    output logic pending_source_valid, output_source_valid,
    output logic pending_checkpoint_valid, output_checkpoint_valid,
    output logic pending_ended, output_ended,
    output logic [63:0] pending_loops, output_loops,
    output logic journal_clear, commit_valid, commit_natural_end,
    output logic [63:0] commit_generation, commit_frame, commit_prefix,
    output logic audio_mclk, audio_lrck, audio_dac
);
    localparam logic [2:0] RESET=0, START=1, PAUSE=2, RESUME=3, SET_POLICY=4;
    localparam logic [2:0] SUCCESS=1, INVALID=2, STALE=3, FAILED=4;
    typedef enum logic [2:0] {IDLE, BEGIN_REQUEST, START_WAIT, BOUNDARY_WAIT,
                             BOUNDARY_RESULT, RESET_WAIT, RESPONSE} stage_t;
    stage_t stage;
    logic [63:0] last_id, last_reset_generation, last_started_generation;
    logic feed_open, session_reset, external_fault, well_formed, reset_accept;
    logic begin_valid, control_valid, raw_dac;
    logic [2:0] begin_status, feed_status;
    logic [1:0] control_status, control_action;

    assign external_fault=device_fault || emergency_silence;
    assign request_ready=reset_n && stage==IDLE;
    assign response_valid=reset_n && stage==RESPONSE;
    assign well_formed=request_id!=0 && request_generation!=0 && request_kind<=SET_POLICY &&
        (request_kind==RESET ? request_revision==0 && !request_target_enabled && request_target==0 :
         request_revision!=0 && (request_target_enabled ? request_target!=0 : request_target==0));
    assign reset_accept=request_valid && request_ready && well_formed && request_kind==RESET &&
        request_id>last_id && request_generation>=last_reset_generation;
    assign begin_valid=stage==BEGIN_REQUEST && !fault && !inhibited && !external_fault;
    assign control_valid=stage==BOUNDARY_WAIT && !fault && !inhibited && !external_fault &&
        end_reason==0 && failure==0;
    assign control_action=response_kind==PAUSE ? 2'd1 : response_kind==RESUME ? 2'd2 : 2'd0;
    assign audio_dac=inhibited ? 1'b0 : raw_dac;
    assign journal_clear=reset_accept || session_reset;

    always_comb begin
        item_status=0;
        if (item_valid) begin
            if (!reset_n) item_status=4;
            else if (item_generation!=prepared_generation || item_epoch!=feed_epoch) item_status=5;
            else if (!feed_open || reset_accept || fault || inhibited || external_fault ||
                     end_reason!=0 || failure!=0) item_status=4;
            else item_status=feed_status;
        end
    end

    rpcmp_jt51_progress_audio audio (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(1'b0), .session_reset(session_reset),
        .device_fault(external_fault), .clear_audio_flags(1'b0),
        .item_valid(item_valid && reset_n && feed_open && !reset_accept && !fault && !inhibited &&
                    !external_fault && end_reason==0 && failure==0 &&
                    item_generation==prepared_generation && item_epoch==feed_epoch),
        .item_marker(item_marker), .item_end(item_end), .item_at(item_at), .item_until(item_until),
        .item_loops(item_loops), .item_address(item_address), .item_value(item_value),
        .item_status(feed_status), .source_queued(source_queued), .receipt_ready(1'b1),
        .receipt_valid(), .receipt_marker(), .operation_token(), .receipt_token(), .receipt_at_edge(),
        .begin_valid(begin_valid), .begin_generation(response_generation), .begin_revision(response_revision),
        .begin_target_enabled(response_target_enabled), .begin_target(response_target), .begin_status(begin_status),
        .control_valid(control_valid), .control_generation(response_generation), .control_revision(response_revision),
        .control_action(control_action), .control_repeat(1'b1),
        .control_target_enabled(response_target_enabled), .control_target(response_target), .control_status(control_status),
        .generation(generation), .policy_revision(policy_revision), .media_frame(media_frame),
        .completed_loops(completed_loops), .target_enabled(target_enabled), .paused(paused), .target(target),
        .phase(phase), .end_reason(end_reason), .failure(failure), .gain(gain), .ramp_elapsed(ramp_elapsed), .queued(),
        .quiescent(quiescent), .resetting(resetting), .device_idle(device_idle),
        .media_enable(media_enable), .frame_boundary(frame_boundary),
        .source_edge(), .pending_source_edge(), .output_source_edge(),
        .pending_prefix(pending_prefix), .output_prefix(output_prefix),
        .pending_source_valid(pending_source_valid), .output_source_valid(output_source_valid),
        .pending_checkpoint_valid(pending_checkpoint_valid), .output_checkpoint_valid(output_checkpoint_valid),
        .pending_ended(pending_ended), .output_ended(output_ended), .pending_loops(pending_loops), .output_loops(output_loops),
        .commit_valid(commit_valid), .commit_natural_end(commit_natural_end),
        .commit_generation(commit_generation), .commit_frame(commit_frame), .commit_prefix(commit_prefix),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(raw_dac),
        .audio_underflow(), .audio_overflow(), .audio_clipped(), .selected_count(), .frame_count()
    );

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            stage<=IDLE; last_id<=0; last_reset_generation<=0; last_started_generation<=0;
            feed_epoch<=0; prepared_generation<=0; feed_open<=0; session_reset<=0;
            fault<=0; inhibited<=1;
            response_id<=0; response_generation<=0; response_revision<=0; response_frame<=0;
            response_kind<=0; response_result<=0; response_target_enabled<=0; response_target<=0;
        end else begin
            session_reset<=0;
            if (external_fault || failure!=0) begin fault<=1; inhibited<=1; feed_open<=0; end
            if (end_reason!=0) feed_open<=0;
            case (stage)
                IDLE: if (request_valid) begin
                    response_id<=request_id; response_generation<=request_generation;
                    response_revision<=request_revision; response_kind<=request_kind;
                    response_target_enabled<=request_target_enabled; response_target<=request_target;
                    response_frame<=media_frame; response_result<=INVALID; stage<=RESPONSE;
                    if (well_formed) begin
                        if (request_id<=last_id) response_result<=STALE;
                        else begin
                            last_id<=request_id;
                            if (request_kind==RESET) begin
                                if (request_generation<last_reset_generation) response_result<=STALE;
                                else begin
                                    last_reset_generation<=request_generation; feed_open<=0;
                                    if (feed_epoch==64'hffffffffffffffff) begin
                                        response_result<=FAILED; fault<=1; inhibited<=1;
                                    end else begin
                                        feed_epoch<=feed_epoch+64'd1; session_reset<=1;
                                        // The acceptance edge is part of Reset ownership;
                                        // a one-edge fault must survive its deassertion.
                                        if (external_fault) response_result<=FAILED;
                                        else stage<=RESET_WAIT;
                                    end
                                end
                            end else if (fault || inhibited || external_fault || failure!=0) response_result<=FAILED;
                            else if (request_kind==START) begin
                                if (request_generation!=prepared_generation || request_generation<=last_started_generation)
                                    response_result<=STALE;
                                else if (feed_open) stage<=BEGIN_REQUEST;
                            end else if (request_generation!=generation || generation==0) response_result<=STALE;
                            else if (end_reason==0 &&
                                     request_revision>=policy_revision &&
                                     (request_revision!=policy_revision ||
                                      (request_target_enabled==target_enabled && request_target==target)) &&
                                     (request_kind!=PAUSE || !paused) && (request_kind!=RESUME || paused))
                                stage<=BOUNDARY_WAIT;
                        end
                    end
                end
                RESET_WAIT: begin
                    if (external_fault) begin
                        response_result<=FAILED; response_frame<=media_frame; stage<=RESPONSE;
                    end else if (!session_reset && quiescent && media_frame==0 && failure==0) begin
                        prepared_generation<=response_generation; feed_open<=1; fault<=0; inhibited<=0;
                        response_result<=SUCCESS; response_frame<=0; stage<=RESPONSE;
                    end
                end
                BEGIN_REQUEST: begin
                    if (external_fault || fault || failure!=0) begin response_result<=FAILED; stage<=RESPONSE; end
                    else if (begin_status==1) begin
                        last_started_generation<=response_generation; stage<=START_WAIT;
                    end else begin response_result<=INVALID; stage<=RESPONSE; end
                end
                START_WAIT: begin
                    if (external_fault || fault || failure!=0) begin response_result<=FAILED; stage<=RESPONSE; end
                    else if (media_frame!=0) begin
                        response_result<=SUCCESS; response_frame<=media_frame; stage<=RESPONSE;
                    end
                end
                BOUNDARY_WAIT: begin
                    if (external_fault || fault || failure!=0) begin response_result<=FAILED; stage<=RESPONSE; end
                    else if (end_reason!=0) begin response_result<=INVALID; stage<=RESPONSE; end
                    else if (control_status!=0) begin
                        response_frame<=media_frame; response_result<=control_status==1 ? SUCCESS : INVALID;
                        stage<=BOUNDARY_RESULT;
                    end
                end
                BOUNDARY_RESULT: begin
                    if (external_fault || fault || failure!=0) response_result<=FAILED;
                    stage<=RESPONSE;
                end
                RESPONSE: if (response_ready) stage<=IDLE;
                default: begin stage<=IDLE; fault<=1; inhibited<=1; feed_open<=0; end
            endcase
        end
    end
endmodule
