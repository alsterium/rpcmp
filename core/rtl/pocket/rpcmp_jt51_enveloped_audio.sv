// Local synchronous player audio owner. See pocket-enveloped-audio-v1.
module rpcmp_jt51_enveloped_audio (
    input logic clk_audio, reset_n, stream_reset, device_fault, clear_audio_flags,
    input logic dev_valid,
    output logic dev_ready,
    input logic [7:0] dev_address, dev_value,
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
    output logic audio_mclk, audio_lrck, audio_dac,
    output logic audio_underflow, audio_overflow, audio_clipped,
    output logic [31:0] selected_count, frame_count
);
    logic [11:0] reset_remaining;
    logic terminal_seen, terminal, active, reset_trigger, shared_fault;
    logic jt_sample, consume;
    logic signed [15:0] jt_left, jt_right, source_left, source_right;
    logic signed [15:0] transformed_left, transformed_right;

    assign terminal = end_reason!=0 || failure!=0;
    assign active = generation!=0 && !terminal;
    assign shared_fault = device_fault || (stream_reset && active) ||
                          audio_underflow || audio_overflow;
    assign reset_trigger = stream_reset || shared_fault || (terminal && !terminal_seen);
    assign resetting = !reset_n || reset_trigger || reset_remaining!=0;
    assign quiescent = !resetting && !active;

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            reset_remaining<=12'd2048; terminal_seen<=0;
        end else begin
            terminal_seen<=terminal;
            if (reset_trigger) reset_remaining<=12'd2048;
            else if (reset_remaining!=0) reset_remaining<=reset_remaining-12'd1;
        end
    end

    rpcmp_media_envelope envelope (
        .clk_audio(clk_audio), .reset_n(reset_n), .quiescent(quiescent), .device_fault(shared_fault),
        .begin_valid(begin_valid), .begin_generation(begin_generation), .begin_revision(begin_revision),
        .begin_target_enabled(begin_target_enabled), .begin_target(begin_target), .begin_status(begin_status),
        .progress_valid(progress_valid), .progress_generation(progress_generation),
        .progress_sequence(progress_sequence), .progress_at(progress_at), .progress_until(progress_until),
        .progress_loops(progress_loops), .progress_ended(progress_ended), .progress_status(progress_status),
        .frame_tick(frame_boundary && !resetting), .control_valid(control_valid),
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
        .jt_sample(jt_sample), .jt_left(jt_left), .jt_right(jt_right)
    );
    rpcmp_media_output #(.INITIAL_RUNNING(0)) output_media (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .frame_consume(consume), .clear_flags(clear_audio_flags),
        .src_valid(jt_sample), .src_left({{2{jt_left[15]}},jt_left}),
        .src_right({{2{jt_right[15]}},jt_right}),
        .transformed_left(transformed_left), .transformed_right(transformed_right),
        .source_left(source_left), .source_right(source_right), .media_enable(media_enable),
        .running(), .frame_boundary(frame_boundary), .audio_mclk(audio_mclk),
        .audio_lrck(audio_lrck), .audio_dac(audio_dac), .underflow(audio_underflow),
        .overflow(audio_overflow), .clipped(audio_clipped),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
