// Synchronous, already-mapped progress and gain owner. See media-envelope-rtl-v1.
module rpcmp_media_envelope #(
    parameter logic [63:0] MAX_FRAMES = 64'hffffffffffffffff
) (
    input logic clk_audio, reset_n, quiescent, device_fault,
    input logic begin_valid,
    input logic [63:0] begin_generation, begin_revision,
    input logic begin_target_enabled,
    input logic [31:0] begin_target,
    output logic [2:0] begin_status,
    input logic progress_valid,
    input logic [63:0] progress_generation, progress_sequence,
    input logic [63:0] progress_at, progress_until, progress_loops,
    input logic progress_ended,
    output logic [2:0] progress_status,
    input logic frame_tick, control_valid,
    input logic [63:0] control_generation, control_revision,
    input logic [1:0] control_action,
    input logic control_repeat, control_target_enabled,
    input logic [31:0] control_target,
    input logic signed [15:0] source_left, source_right,
    output logic consume,
    output logic signed [15:0] output_left, output_right,
    output logic [17:0] boundary_gain,
    output logic [1:0] control_status,
    output logic [63:0] generation, policy_revision, media_frame, completed_loops,
    output logic target_enabled, paused,
    output logic [31:0] target,
    output logic [1:0] phase,
    output logic [2:0] end_reason, failure,
    output logic [17:0] gain, ramp_elapsed,
    output logic [8:0] queued
);
    localparam logic [17:0] UNITY=18'd240000, RESTORE=18'd960;
    localparam logic [2:0] NONE=0, ACCEPTED=1, STALE=2, INVALID=3, CLOSED=4, FULL=5, EXHAUSTED=6;
    localparam logic [1:0] STEADY=0, FADING=1, RESTORING=2;
    typedef struct packed {
        logic [63:0] generation, revision, frame, loops, covered_until;
        logic target_enabled;
        logic [31:0] target;
        logic paused;
        logic [1:0] phase;
        logic [2:0] end_reason, failure;
        logic [17:0] gain, elapsed, step, remainder_step, remainder;
    } state_t;
    typedef struct packed {
        logic [63:0] at_frame, until_frame, loops;
        logic ended;
    } progress_t;
    state_t state, next_state;
    progress_t memory[0:255], ram_head, bypass_value, head_value, offered;
    logic [7:0] head, tail, read_address;
    logic [8:0] count;
    logic bypass_head, push, pop, flush_queue, begin_accepted;
    logic [63:0] last_sequence, last_until, last_loops;
    logic end_enqueued, reached, valid_control;
    logic [18:0] remainder_sum, step_amount;
    logic [17:0] duration;

    function automatic state_t finish(input state_t value, input logic [2:0] reason);
        state_t result;
        result=value;
        result.end_reason=reason; result.phase=STEADY; result.paused=0;
        result.gain=0; result.elapsed=0; result.step=0;
        result.remainder=0; result.remainder_step=0;
        return result;
    endfunction
    function automatic state_t fail(input state_t value, input logic [2:0] reason);
        state_t result;
        result=finish(value,3'd0);
        if (result.failure==0) result.failure=reason;
        return result;
    endfunction
    function automatic state_t start_ramp(input state_t value, input logic [1:0] new_phase);
        state_t result;
        logic [17:0] difference;
        result=value;
        result.phase=new_phase; result.elapsed=0; result.remainder=0;
        if (new_phase==FADING) begin
            result.step=value.gain==UNITY ? 18'd1 : 18'd0;
            result.remainder_step=value.gain==UNITY ? 18'd0 : value.gain;
        end else begin
            difference=UNITY-value.gain;
            result.step=difference/RESTORE;
            result.remainder_step=difference%RESTORE;
        end
        return result;
    endfunction
    function automatic logic signed [15:0] scale(input logic signed [15:0] sample,
                                                input logic [17:0] factor);
        logic signed [63:0] product, quotient;
        product=$signed(sample)*$signed({1'b0,factor});
        quotient=product/64'sd240000;
        return quotient[15:0];
    endfunction

    assign generation=state.generation;
    assign policy_revision=state.revision;
    assign media_frame=state.frame;
    assign completed_loops=state.loops;
    assign target_enabled=state.target_enabled;
    assign target=state.target;
    assign paused=state.paused;
    assign phase=state.phase;
    assign end_reason=state.end_reason;
    assign failure=state.failure;
    assign gain=state.generation==0 || state.end_reason!=0 || state.failure!=0 ? 18'd0 : state.gain;
    assign ramp_elapsed=state.elapsed;
    assign queued=count;
    assign offered={progress_at,progress_until,progress_loops,progress_ended};
    assign head_value=bypass_head ? bypass_value : ram_head;
    assign begin_accepted=begin_status==ACCEPTED;
    assign push=progress_status==ACCEPTED;
    assign read_address=pop ? head+8'd1 : head;

    // No reset on the RAM or its read register: occupancy/bypass own validity.
    // The read address anticipates a pop; an empty/single-entry push bypasses
    // the read-during-write case, so no vendor-specific collision value is used.
    always_ff @(posedge clk_audio) begin
        if (push) memory[tail]<=offered;
        ram_head<=memory[read_address];
    end

    always_comb begin
        begin_status=NONE;
        if (begin_valid && reset_n) begin
            if (device_fault) begin_status=CLOSED;
            else if (!quiescent || begin_generation==0 || begin_revision==0 ||
                     (begin_target_enabled && begin_target==0) || MAX_FRAMES==0) begin_status=INVALID;
            else if (begin_generation<=state.generation) begin_status=STALE;
            else begin_status=ACCEPTED;
        end
        progress_status=NONE;
        if (progress_valid && reset_n) begin
            if (begin_accepted || device_fault) progress_status=CLOSED;
            else if (progress_generation<state.generation) progress_status=STALE;
            else if (state.generation==0 || progress_generation!=state.generation) progress_status=INVALID;
            else if (state.failure!=0 || state.end_reason!=0) progress_status=CLOSED;
            else if (last_sequence==64'hffffffffffffffff) progress_status=EXHAUSTED;
            else if (progress_sequence!=last_sequence+64'd1 || progress_at!=last_until ||
                     progress_at<state.frame || progress_until<=progress_at ||
                     progress_loops<last_loops || end_enqueued) progress_status=INVALID;
            else if (count==9'd256) progress_status=FULL;
            else progress_status=ACCEPTED;
        end

        next_state=state;
        pop=0; flush_queue=0; consume=0;
        output_left=0; output_right=0; boundary_gain=0; control_status=0;
        reached=0; valid_control=0; remainder_sum=0; step_amount=0; duration=0;
        if (device_fault) begin
            next_state=fail(state,3'd1); flush_queue=1;
        end else if (begin_accepted) begin
            next_state='0;
            next_state.generation=begin_generation; next_state.revision=begin_revision;
            next_state.target_enabled=begin_target_enabled;
            next_state.target=begin_target_enabled ? begin_target : 32'd0;
            next_state.gain=UNITY;
            flush_queue=1;
        end else if (frame_tick && state.failure==0) begin
            if (control_valid) begin
                if (control_generation!=0 && control_generation<state.generation) control_status=2;
                else if (control_generation==0 || control_generation!=state.generation) begin
                    control_status=3; next_state=fail(next_state,3'd2);
                end else begin
                    control_status=1; valid_control=1;
                end
            end
            if (valid_control && control_repeat) begin
                if (control_revision==0 || (control_target_enabled && control_target==0) ||
                    control_revision<state.revision ||
                    (control_revision==state.revision &&
                     (control_target_enabled!=state.target_enabled ||
                      (control_target_enabled && control_target!=state.target)))) begin
                    control_status=3; next_state=fail(next_state,3'd2);
                end else begin
                    next_state.revision=control_revision;
                    next_state.target_enabled=control_target_enabled;
                    next_state.target=control_target_enabled ? control_target : 32'd0;
                end
            end
            if (valid_control && next_state.failure==0 && control_action==3)
                next_state=finish(next_state,3'd1);
            if (next_state.generation!=0 && next_state.failure==0 && next_state.end_reason==0) begin
                if (valid_control && control_action==1) next_state.paused=1;
                else if (valid_control && control_action==2) next_state.paused=0;
                if (!next_state.paused) begin
                    if (count!=0 && head_value.at_frame==state.frame) begin
                        pop=1;
                        next_state.loops=head_value.loops;
                        next_state.covered_until=head_value.until_frame;
                        if (head_value.ended)
                            next_state=finish(next_state,next_state.target_enabled ? 3'd2 : 3'd3);
                    end
                    if (next_state.end_reason==0) begin
                        reached=next_state.target_enabled && next_state.loops>=next_state.target;
                        if (next_state.phase==FADING && !reached) begin
                            if (next_state.gain==UNITY) begin
                                next_state.phase=STEADY; next_state.elapsed=0;
                            end else next_state=start_ramp(next_state,RESTORING);
                        end else if (next_state.phase!=FADING && reached)
                            next_state=start_ramp(next_state,FADING);
                        if (next_state.phase==FADING && next_state.elapsed==UNITY)
                            next_state=finish(next_state,3'd4);
                        else begin
                            if (next_state.phase==RESTORING && next_state.elapsed==RESTORE) begin
                                next_state.phase=STEADY; next_state.elapsed=0;
                            end
                            if (state.frame==MAX_FRAMES) next_state=fail(next_state,3'd4);
                            else if (state.frame>=next_state.covered_until) next_state=fail(next_state,3'd3);
                            else begin
                                // Reconciliation anchors new ramps at the existing gain.
                                // Scale that registered value independently of control
                                // validation; a rejected/ending boundary selects zero.
                                consume=1; boundary_gain=state.gain;
                                output_left=scale(source_left,state.gain);
                                output_right=scale(source_right,state.gain);
                                next_state.frame=state.frame+64'd1;
                                if (next_state.phase!=STEADY) begin
                                    duration=next_state.phase==FADING ? UNITY : RESTORE;
                                    remainder_sum={1'b0,next_state.remainder}+{1'b0,next_state.remainder_step};
                                    step_amount={1'b0,next_state.step};
                                    if (remainder_sum>={1'b0,duration}) begin
                                        remainder_sum=remainder_sum-{1'b0,duration};
                                        step_amount=step_amount+19'd1;
                                    end
                                    next_state.remainder=remainder_sum[17:0];
                                    next_state.elapsed=next_state.elapsed+18'd1;
                                    if (next_state.phase==FADING) next_state.gain=next_state.gain-step_amount[17:0];
                                    else next_state.gain=next_state.gain+step_amount[17:0];
                                end
                            end
                        end
                    end
                end
            end
            if (next_state.failure!=0 || next_state.end_reason!=0) flush_queue=1;
        end
    end

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            state<='0; head<=0; tail<=0; count<=0;
            bypass_head<=0; bypass_value<='0;
            last_sequence<=0; last_until<=0; last_loops<=0; end_enqueued<=0;
        end else begin
            state<=next_state;
            if (begin_accepted) begin
                last_sequence<=0; last_until<=0; last_loops<=0; end_enqueued<=0;
            end else if (push) begin
                last_sequence<=progress_sequence; last_until<=progress_until;
                last_loops<=progress_loops; end_enqueued<=progress_ended;
            end
            if (flush_queue) begin
                head<=0; tail<=0; count<=0; bypass_head<=0;
            end else begin
                bypass_head<=push && (count==0 || (count==1 && pop));
                if (push && (count==0 || (count==1 && pop))) bypass_value<=offered;
                if (push) tail<=tail+8'd1;
                if (pop) head<=head+8'd1;
                case ({push,pop})
                    2'b10: count<=count+9'd1;
                    2'b01: count<=count-9'd1;
                    default: count<=count;
                endcase
            end
        end
    end
endmodule
