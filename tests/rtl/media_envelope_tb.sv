`timescale 1ns/1ps
module media_envelope_tb;
    logic clk_audio=0, reset_n=0, quiescent=1, device_fault=0;
    logic begin_valid=0, begin_target_enabled=1;
    logic [63:0] begin_generation=0, begin_revision=1;
    logic [31:0] begin_target=2;
    logic [2:0] begin_status, progress_status;
    logic progress_valid=0, progress_ended=0;
    logic [63:0] progress_generation=0, progress_sequence=0, progress_at=0, progress_until=0, progress_loops=0;
    logic frame_tick=0, control_valid=0, control_repeat=0, control_target_enabled=1;
    logic [63:0] control_generation=0, control_revision=0;
    logic [1:0] control_action=0, control_status;
    logic [1:0] sampled_control_status;
    logic [31:0] control_target=2;
    logic signed [15:0] source_left=32767, source_right=-32768, output_left, output_right;
    logic consume, target_enabled, paused;
    logic [17:0] boundary_gain, gain, ramp_elapsed;
    logic [63:0] generation, policy_revision, media_frame, completed_loops;
    logic [31:0] target;
    logic [1:0] phase;
    logic [2:0] end_reason, failure;
    logic [8:0] queued;
    logic small_consume;
    logic [63:0] small_frame;
    logic [2:0] small_failure;
    longint unsigned epoch=0, checked=0;
    longint signed expected_gain, start_gain, i;
    integer cases=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_media_envelope dut (.*);
    rpcmp_media_envelope #(.MAX_FRAMES(64'd3)) ceiling (
        .clk_audio(clk_audio), .reset_n(reset_n), .quiescent(quiescent), .device_fault(device_fault),
        .begin_valid(begin_valid), .begin_generation(begin_generation), .begin_revision(begin_revision),
        .begin_target_enabled(begin_target_enabled), .begin_target(begin_target), .begin_status(),
        .progress_valid(progress_valid), .progress_generation(progress_generation),
        .progress_sequence(progress_sequence), .progress_at(progress_at), .progress_until(progress_until),
        .progress_loops(progress_loops), .progress_ended(progress_ended), .progress_status(),
        .frame_tick(frame_tick), .control_valid(control_valid), .control_generation(control_generation),
        .control_revision(control_revision), .control_action(control_action), .control_repeat(control_repeat),
        .control_target_enabled(control_target_enabled), .control_target(control_target),
        .source_left(source_left), .source_right(source_right), .consume(small_consume),
        .output_left(), .output_right(), .boundary_gain(), .control_status(), .generation(),
        .policy_revision(), .media_frame(small_frame), .completed_loops(), .target_enabled(), .paused(),
        .target(), .phase(), .end_reason(), .failure(small_failure), .gain(), .ramp_elapsed(), .queued()
    );
    always @(posedge clk_audio) begin
        if (gain>240000 || queued>256) $fatal(1,"gain/queue bound");
    end
    initial begin #120000000; $fatal(1,"bounded envelope test timeout"); end

    task automatic idle_inputs;
        @(negedge clk_audio);
        frame_tick=0; control_valid=0; control_repeat=0; control_action=0; progress_valid=0; begin_valid=0;
    endtask
    task automatic begin_stream(input logic enabled, input logic [31:0] count);
        idle_inputs(); epoch=epoch+1;
        begin_generation=epoch; begin_target_enabled=enabled; begin_target=count; begin_revision=1;
        begin_valid=1; #1;
        if (begin_status!==1) $fatal(1,"valid begin rejected");
        @(posedge clk_audio); #1;
        if (generation!==epoch || media_frame!==0 || gain!==240000 || queued!==0 || paused || failure || end_reason)
            $fatal(1,"begin state");
        idle_inputs(); cases=cases+1;
    endtask
    task automatic progress(input longint unsigned seq, a, u, loops, input logic ended,
                            input logic [2:0] status);
        idle_inputs(); progress_valid=1; progress_generation=epoch;
        progress_sequence=seq; progress_at=a; progress_until=u; progress_loops=loops; progress_ended=ended;
        #1; if (progress_status!==status) $fatal(1,"admission seq=%0d expected=%0d got=%0d",seq,status,progress_status);
        @(posedge clk_audio); #1; idle_inputs();
    endtask
    task automatic check_boundary(input longint signed factor, input logic should_consume);
        longint signed left_expected, right_expected;
        #1;
        left_expected=$signed(source_left); right_expected=$signed(source_right);
        left_expected=left_expected*factor/240000; right_expected=right_expected*factor/240000;
        if (!should_consume) begin left_expected=0; right_expected=0; end
        if (consume!==should_consume || boundary_gain!==(should_consume ? 18'(factor) : 18'd0) ||
            output_left!==16'(left_expected) || output_right!==16'(right_expected))
            $fatal(1,"boundary frame=%0d gain expected=%0d got=%0d consume=%0d/%0d samples=%0d/%0d",
                   media_frame,factor,boundary_gain,should_consume,consume,output_left,output_right);
        checked=checked+1;
        sampled_control_status=control_status;
        @(posedge clk_audio); #1;
    endtask
    task automatic tick(input longint signed factor, input logic should_consume);
        @(negedge clk_audio); frame_tick=1;
        check_boundary(factor,should_consume);
    endtask
    task automatic policy(input longint unsigned revision, input logic enabled,
                          input logic [31:0] count, input logic [1:0] action);
        idle_inputs(); control_valid=1; control_generation=epoch; control_revision=revision;
        control_repeat=1; control_target_enabled=enabled; control_target=count; control_action=action;
    endtask

    initial begin
        repeat(4) @(negedge clk_audio); reset_n=1;
        // Invalid Begin must not create a stream.
        begin_valid=1; begin_generation=1; quiescent=0; #1;
        if (begin_status!==3) $fatal(1,"unconfirmed quiescence accepted");
        @(posedge clk_audio); #1; if (generation!==0) $fatal(1,"invalid begin mutated state");
        idle_inputs(); quiescent=1;
        begin_stream(0,0);
        progress_valid=1; progress_generation=epoch-1; progress_sequence=1;
        progress_at=0; progress_until=1; progress_loops=0;
        #1; if (progress_status!==2) $fatal(1,"old progress generation");
        @(posedge clk_audio); #1;
        @(negedge clk_audio); progress_generation=epoch+1;
        #1; if (progress_status!==3) $fatal(1,"future progress generation");
        @(posedge clk_audio); #1;
        if (queued!==0) $fatal(1,"invalid progress mutated queue");
        progress(2,0,1,0,0,3); progress(1,0,0,0,0,3); progress(1,1,2,0,0,3);
        for (i=0;i<256;i=i+1) progress(i+1,i,i+1,i,0,1);
        progress(257,256,257,256,0,5);
        if (queued!==256) $fatal(1,"full queue changed");
        // Full admission precedes a same-edge pop; retry next edge succeeds.
        progress_valid=1; frame_tick=1; #1;
        if (progress_status!==5) $fatal(1,"full/pop priority");
        check_boundary(240000,1);
        @(negedge clk_audio); frame_tick=0; #1;
        if (progress_status!==1) $fatal(1,"full retry failed");
        @(posedge clk_audio); #1;
        idle_inputs();
        for (i=1;i<257;i=i+1) begin
            tick(240000,1);
            if (completed_loops!==i || media_frame!==i+1) $fatal(1,"RAM head/wrap ordering");
        end
        tick(0,0);
        if (failure!==3 || media_frame!==257 || queued!==0) $fatal(1,"coverage underrun");
        progress(258,257,258,257,0,4);

        // A one-entry FIFO repeatedly pops and pushes the same RAM address.
        // The bypass must deliver the new head without using read/write collision data.
        begin_stream(0,0); progress(1,0,1,0,0,1);
        for (i=1;i<=300;i=i+1) begin
            @(negedge clk_audio); frame_tick=1; progress_valid=1;
            progress_generation=epoch; progress_sequence=i+1;
            progress_at=i; progress_until=i+1; progress_loops=i;
            #1; if (progress_status!==1) $fatal(1,"single-head replacement rejected");
            check_boundary(240000,1);
            if (completed_loops!==i-1 || queued!==1 || media_frame!==i)
                $fatal(1,"single-head bypass/wrap ordering");
        end
        idle_inputs(); tick(240000,1);
        if (completed_loops!==300 || queued!==0 || media_frame!==301) $fatal(1,"last bypass head");

        // Cancel an unstarted fade exactly when its already-queued loop arrives.
        begin_stream(1,1); progress(1,0,8,0,0,1);
        progress(2,8,64'hffffffffffffffff,1,0,1);
        repeat(8) tick(240000,1);
        policy(2,0,0,0); tick(240000,1);
        if (completed_loops!==1 || phase!==0 || ramp_elapsed!==0) $fatal(1,"old policy started reserved fade");
        policy(3,1,1,0); tick(240000,1);
        if (phase!==1 || ramp_elapsed!==1) $fatal(1,"reached policy did not start fade");
        policy(4,0,0,0); tick(239999,1); idle_inputs();
        for (i=1;i<960;i=i+1) tick(239999+i/960,1);
        tick(240000,1); if (phase!==0 || gain!==240000) $fatal(1,"one-unit restoration");

        // Exact 10-second intro + two 30-second bodies: full real frame counts.
        begin_stream(1,2);
        progress(1,0,1920000,0,0,1); progress(2,1920000,3360000,1,0,1);
        progress(3,3360000,64'hffffffffffffffff,2,0,1);
        for (i=0;i<3600000;i=i+1) begin
            expected_gain=i<3360000 ? 240000 : 3600000-i;
            tick(expected_gain,1);
            if (media_frame!==i+1) $fatal(1,"70/75 second media frame");
            if (i==3359999 && phase!==0) $fatal(1,"early fade");
            if (i==3360000 && (phase!==1 || ramp_elapsed!==1)) $fatal(1,"late fade");
        end
        tick(0,0);
        if (end_reason!==4 || media_frame!==3600000 || gain!==0 || queued!==0) $fatal(1,"75 second completion");

        // Cancellation exactly at the zero-gain completion boundary restores.
        begin_stream(1,1); progress(1,0,64'hffffffffffffffff,1,0,1);
        for (i=0;i<240000;i=i+1) tick(240000-i,1);
        policy(2,0,0,0); tick(0,1);
        if (end_reason!==0 || phase!==2 || gain!==250) $fatal(1,"completion cancellation");
        idle_inputs();
        for (i=1;i<960;i=i+1) tick(250*i,1);
        tick(240000,1); if (phase!==0) $fatal(1,"restore endpoint");

        // Pause freezes gain; a changed policy reconciles only on Resume.
        begin_stream(1,1); progress(1,0,64'hffffffffffffffff,5,0,1);
        for (i=0;i<40000;i=i+1) tick(240000-i,1);
        policy(2,0,0,1); tick(0,0); idle_inputs();
        for (i=0;i<19;i=i+1) tick(0,0);
        if (!paused || gain!==200000 || media_frame!==40000 || ramp_elapsed!==40000 || phase!==1)
            $fatal(1,"paused ramp advanced or reconciled early");
        policy(2,0,0,2); tick(200000,1); idle_inputs();
        for (i=1;i<112;i=i+1) tick(200000+40000*i/960,1);
        // New reached target restarts a full fade from an analytically known partial gain.
        start_gain=200000+40000*112/960;
        policy(3,1,3,0); tick(start_gain,1); idle_inputs();
        for (i=1;i<240000;i=i+1) tick(start_gain-start_gain*i/240000,1);
        tick(0,0); if (end_reason!==4) $fatal(1,"partial-gain fade endpoint");

        // Reached-to-reached and duplicate policy keep the existing anchor.
        begin_stream(1,1); progress(1,0,64'hffffffffffffffff,5,0,1);
        for (i=0;i<1000;i=i+1) tick(240000-i,1);
        policy(2,1,3,0); tick(239000,1);
        if (ramp_elapsed!==1001) $fatal(1,"policy restarted fade");
        tick(238999,1); if (ramp_elapsed!==1002) $fatal(1,"duplicate restarted fade");
        // Old generation cannot change current policy, even with malformed content.
        control_generation=epoch-1; control_revision=0; control_action=3; tick(238998,1);
        if (sampled_control_status!==2 || policy_revision!==2) $fatal(1,"stale control");
        policy(2,1,4,0); tick(0,0);
        if (failure!==2 || sampled_control_status!==3 || media_frame!==1003)
            $fatal(1,"equal-revision conflict failure=%0d control=%0d frame=%0d",failure,sampled_control_status,media_frame);

        for (integer bad=0;bad<4;bad=bad+1) begin
            begin_stream(0,0); progress(1,0,10,0,0,1);
            policy(bad==0 ? 0 : 2,1,bad==1 ? 0 : 3,0);
            if (bad==2) control_generation=0;
            if (bad==3) control_generation=epoch+1;
            tick(0,0);
            if (failure!==2 || sampled_control_status!==3 || media_frame!==0 || queued!==0)
                $fatal(1,"malformed policy/tag not rejected: %0d",bad);
        end

        // Finite tracks end once; RepeatOne reports restart intent rather than replaying.
        for (integer mode=0;mode<2;mode=mode+1) begin
            begin_stream(mode==0,3);
            progress(1,0,4,0,0,1); progress(2,4,5,0,1,1); progress(3,5,6,0,0,3);
            repeat(4) tick(240000,1);
            tick(0,0);
            if (end_reason!==(mode==0 ? 3'd2 : 3'd3) || media_frame!==4) $fatal(1,"natural end");
        end
        // Small signed samples round toward zero, including both signs at half gain.
        begin_stream(1,1); progress(1,0,64'hffffffffffffffff,1,0,1);
        for (i=0;i<120000;i=i+1) tick(240000-i,1);
        source_left=3; source_right=-3; tick(120000,1);
        if (output_left!==1 || output_right!==-1) $fatal(1,"signed half-gain rounding");
        source_left=-32768; source_right=32767;
        policy(2,1,1,3);
        @(negedge clk_audio); frame_tick=1; device_fault=1; check_boundary(0,0);
        if (failure!==1 || end_reason!==0 || queued!==0) $fatal(1,"fault did not beat Stop");
        idle_inputs(); device_fault=0;

        // Resource ceiling and malformed progression preserve their exact failure kinds.
        begin_stream(0,0); progress(1,0,10,4,0,1); progress(2,10,11,3,0,3);
        repeat(3) tick(240000,1); tick(240000,1);
        if (small_consume!==0 || small_failure!==4 || small_frame!==3) $fatal(1,"counter ceiling wrapped");
        idle_inputs();
        force dut.last_sequence=64'hffffffffffffffff;
        progress(2,10,11,4,0,6);
        release dut.last_sequence;
        // Maximum generation is valid once and cannot wrap through a new Begin.
        idle_inputs(); begin_generation=64'hffffffffffffffff; begin_valid=1; #1;
        if (begin_status!==1) $fatal(1,"max generation refused");
        @(posedge clk_audio); #1;
        @(negedge clk_audio); begin_generation=1; #1;
        if (begin_status!==2) $fatal(1,"generation wrapped");
        @(posedge clk_audio); #1;
        if (generation!==64'hffffffffffffffff) $fatal(1,"stale begin mutated generation");
        idle_inputs(); reset_n=0; begin_valid=1; progress_valid=1; #1;
        if (begin_status!==0 || progress_status!==0) $fatal(1,"reset admission");
        $display("media_envelope_tb: PASS boundaries=%0d cases=%0d fifo=256 fade_start=3360000 fade_end=3600000 restore=960",checked,cases);
        $finish;
    end
endmodule
