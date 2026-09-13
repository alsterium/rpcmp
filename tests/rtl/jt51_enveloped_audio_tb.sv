`timescale 1ns/1ps
module jt51_enveloped_audio_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, device_fault=0, clear_audio_flags=0;
    logic dev_valid, dev_ready;
    logic [7:0] dev_address, dev_value;
    logic marker_valid=0, receipt_ready=1, marker_ready, receipt_valid, receipt_marker;
    logic [63:0] operation_token, receipt_token, receipt_at_edge;
    logic begin_valid=0, begin_target_enabled=1;
    logic [63:0] begin_generation=1, begin_revision=1;
    logic [31:0] begin_target=1;
    logic [2:0] begin_status, progress_status;
    logic progress_valid=0, progress_ended=0;
    logic [63:0] progress_generation=1, progress_sequence=0, progress_at=0, progress_until=0, progress_loops=0;
    logic control_valid=0, control_repeat=0, control_target_enabled=1;
    logic [63:0] control_generation=1, control_revision=1;
    logic [1:0] control_action=0, control_status;
    logic [31:0] control_target=1;
    logic [63:0] generation, policy_revision, media_frame, completed_loops;
    logic target_enabled, paused;
    logic [31:0] target;
    logic [1:0] phase;
    logic [2:0] end_reason, failure;
    logic [17:0] gain, ramp_elapsed;
    logic [8:0] queued;
    logic quiescent, resetting, device_idle, media_enable, frame_boundary;
    logic [63:0] source_edge, pending_source_edge, output_source_edge;
    logic pending_source_valid, output_source_valid;
    logic audio_mclk, audio_lrck, audio_dac, audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;
    logic ref_valid, ref_ready, ref_enable, ref_paused, ref_mclk, ref_lrck, ref_dac;
    logic [7:0] ref_address, ref_value;
    logic ref_pause=1, send_writes=1, compare=1;
    logic [15:0] operations[64];
    integer operation_count=0, next_op[2]='{0,0}, ticks[2]='{0,0};
    integer wall=0, serial_phase=0, reference_frames=0, checked_frames=0, silent_frames=0;
    integer positive[2]='{0,0}, negative[2]='{0,0}, scaled_changes=0;
    logic [31:0] reference_pcm[8192];
    logic [63:0] reference_position[8192];
    logic reference_position_valid[8192];
    logic ref_pending_valid=0;
    logic [63:0] ref_pending_edge=0;
    integer ref_samples=0, position_checks=0;
    logic [15:0] decoded_left[2]='{0,0}, decoded_right[2]='{0,0};
    logic frame_active=0, reference_active=0;
    longint unsigned frame_index=0, before_frame;
    longint signed expected_left, expected_right, expected_factor;
    logic before_consume;
    integer channel, slot;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_enveloped_audio dut (.*);
    rpcmp_jt51_media_audio reference_audio (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .pause_request(ref_pause), .clear_audio_flags(1'b0), .dev_valid(ref_valid), .dev_ready(ref_ready),
        .dev_address(ref_address), .dev_value(ref_value), .device_idle(), .media_enable(ref_enable),
        .paused(ref_paused), .frame_boundary(), .audio_mclk(ref_mclk), .audio_lrck(ref_lrck),
        .audio_dac(ref_dac), .audio_underflow(), .audio_overflow(), .audio_clipped(),
        .selected_count(), .frame_count()
    );
    // Analytic policy timeline, independent of the DUT's phase/recurrence.
    function automatic longint signed factor_at(input longint unsigned f);
        if (f<200) return 240000;
        if (f<600) return 240000-(f-200);
        if (f<1560) return 239600+400*(f-600)/960;
        if (f<1800) return 240000;
        if (f<2200) return 240000-(f-1800);
        if (f<2300) return 239600+400*(f-2200)/960;
        return 239641-239641*(f-2300)/240000;
    endfunction
    always @(negedge clk_audio) begin
        dev_valid=0; dev_address=0; dev_value=0;
        ref_valid=0; ref_address=0; ref_value=0;
        if (send_writes && next_op[0]<operation_count) begin
            ref_valid=ticks[0]>=next_op[0]*513;
            ref_address=operations[next_op[0]][15:8]; ref_value=operations[next_op[0]][7:0];
        end
        if (send_writes && next_op[1]<operation_count) begin
            dev_valid=ticks[1]>=next_op[1]*513;
            dev_address=operations[next_op[1]][15:8]; dev_value=operations[next_op[1]][7:0];
        end
    end
    always @(posedge clk_audio) begin
        before_frame=media_frame; before_consume=frame_boundary && media_enable;
        if (compare && reset_n && !resetting) begin
            if (source_edge!==64'(ticks[1]) || reference_audio.source_media.source_edge!==64'(ticks[0]))
                $fatal(1,"native source edge advanced independently of retained time");
            if (frame_boundary && ref_enable) begin
                if (reference_frames==8192) $fatal(1,"position reference capacity");
                reference_position_valid[reference_frames]=ref_pending_valid;
                reference_position[reference_frames]=ref_pending_valid ? ref_pending_edge : 64'd0;
                ref_pending_valid=0;
            end
            // Closed-form rational selection, using the externally observed
            // native sample strobe and a test-owned retained-edge count.
            if (ref_enable && reference_audio.jt_sample) begin
                if ((64'(ref_samples+1)*3072000)/3579545 != (64'(ref_samples)*3072000)/3579545) begin
                    if (ref_pending_valid) $fatal(1,"position reference overflow");
                    ref_pending_valid=1; ref_pending_edge=64'(ticks[0]);
                end
                ref_samples=ref_samples+1;
            end
        end
        if (!reset_n) begin wall=0; serial_phase=0; end
        else begin
            wall=wall+1; serial_phase=(serial_phase+1)%256;
            if (ref_valid && ref_ready) next_op[0]=next_op[0]+1;
            if (dev_valid && dev_ready) next_op[1]=next_op[1]+1;
            if (ref_enable) ticks[0]=ticks[0]+1;
            if (media_enable) ticks[1]=ticks[1]+1;
        end
        #1;
        if (audio_mclk!==clk_audio || audio_lrck!==(serial_phase>=128) ||
            ref_mclk!==clk_audio || ref_lrck!==audio_lrck) $fatal(1,"serial clock continuity");
        if (reset_n && compare) begin
            if (failure || audio_underflow || audio_overflow || audio_clipped)
                $fatal(1,"unexpected audio fault frame=%0d failure=%0d",media_frame,failure);
            if (serial_phase==0) begin
                frame_active=before_consume; frame_index=before_frame;
                reference_active=!ref_paused && !resetting;
                if (media_frame!==(before_frame+(before_consume ? 64'd1 : 64'd0)))
                    $fatal(1,"sample and media position do not commit together");
            end else if (media_frame!==before_frame) $fatal(1,"media position advanced off boundary");
        end
        for (integer p=0;p<2;p=p+1) begin
            if (serial_phase%4==2 && serial_phase%128>=4 && serial_phase%128<68) begin
                if (serial_phase<128) decoded_left[p]={decoded_left[p][14:0],p==0 ? ref_dac : audio_dac};
                else decoded_right[p]={decoded_right[p][14:0],p==0 ? ref_dac : audio_dac};
            end
        end
        if (serial_phase%128<4 || serial_phase%128>=68)
            if (audio_dac!==0 || ref_dac!==0) $fatal(1,"I2S padding");
        if (reset_n && compare && serial_phase==255) begin
            if (reference_active) begin
                if (reference_frames==8192) $fatal(1,"reference capacity");
                reference_pcm[reference_frames]={decoded_left[0],decoded_right[0]};
                reference_frames=reference_frames+1;
            end
            if (frame_active) begin
                if (frame_index>=reference_frames || frame_index!==checked_frames) $fatal(1,"reference ordering");
                if (output_source_valid!==reference_position_valid[frame_index] ||
                    output_source_edge!==reference_position[frame_index])
                    $fatal(1,"native sample position does not match the serialized frame");
                // From reset: native scan starts at 0 with zero deasserted. Its
                // second strobe follows two 32-slot scans (~448 audio edges).
                // 3072000/3579545 selection drops strobe 1 and keeps strobe 2.
                // Frame 0 starts source time; frame 1 is at edge 256; frame 2
                // at edge 512 is the first with a native position.
                if (output_source_valid!==(frame_index>=2)) $fatal(1,"native startup position boundary");
                if (output_source_valid) position_checks=position_checks+1;
                expected_factor=factor_at(frame_index);
                expected_left=$signed(reference_pcm[frame_index][31:16]);
                expected_right=$signed(reference_pcm[frame_index][15:0]);
                expected_left=expected_left*expected_factor/240000;
                expected_right=expected_right*expected_factor/240000;
                if ({decoded_left[1],decoded_right[1]} !== {16'(expected_left),16'(expected_right)})
                    $fatal(1,"scaled PCM mismatch frame=%0d gain=%0d expected=%0d/%0d got=%0d/%0d",
                           frame_index,expected_factor,expected_left,expected_right,
                           $signed(decoded_left[1]),$signed(decoded_right[1]));
                checked_frames=checked_frames+1;
                if ({decoded_left[1],decoded_right[1]}!==reference_pcm[frame_index]) scaled_changes=scaled_changes+1;
                for (integer p=0;p<2;p=p+1) begin
                    if ($signed(p==0 ? decoded_left[1] : decoded_right[1])>0) positive[p]=positive[p]+1;
                    if ($signed(p==0 ? decoded_left[1] : decoded_right[1])<0) negative[p]=negative[p]+1;
                end
            end else begin
                if ({decoded_left[1],decoded_right[1]}!==32'd0) $fatal(1,"nonconsuming frame not silent");
                if (output_source_valid || output_source_edge!=0) $fatal(1,"pause invented a source position");
                silent_frames=silent_frames+1;
            end
        end
        if (wall>2000000) $fatal(1,"bounded enveloped output timeout");
    end
    task automatic append(input logic [7:0] a, v);
        if (operation_count==64) $fatal(1,"authored operation capacity");
        operations[operation_count]={a,v}; operation_count=operation_count+1;
    endtask
    task automatic at_phase(input integer p);
        do @(negedge clk_audio); while (serial_phase!=p);
    endtask
    task automatic begin_stream(input longint unsigned gen);
        wait(quiescent); @(negedge clk_audio);
        begin_generation=gen; begin_valid=1;
        #1; if (begin_status!==1) $fatal(1,"begin rejected after native reset");
        @(negedge clk_audio); begin_valid=0;
        if (generation!==gen || media_frame!==0 || failure || end_reason) $fatal(1,"begin state");
    endtask
    task automatic progress(input longint unsigned seq, a, u, loops, input logic ended);
        @(negedge clk_audio); progress_generation=generation; progress_sequence=seq;
        progress_at=a; progress_until=u; progress_loops=loops; progress_ended=ended; progress_valid=1;
        #1; if (progress_status!==1) $fatal(1,"mapped progress rejected");
        @(negedge clk_audio); progress_valid=0;
    endtask
    task automatic control(input logic [1:0] action, input logic update_repeat,
                           input longint unsigned revision, input logic enabled,
                           input logic [31:0] count, input longint unsigned gen, input logic [1:0] result);
        at_phase(255); control_valid=1; control_action=action; control_repeat=update_repeat;
        control_revision=revision; control_target_enabled=enabled; control_target=count; control_generation=gen;
        #1; if (control_status!==result) $fatal(1,"control disposition got=%0d expected=%0d",control_status,result);
        @(negedge clk_audio); control_valid=0;
    endtask
    task automatic reject_begin;
        @(negedge clk_audio); begin_generation=generation+1; begin_valid=1;
        #1; if (begin_status!==3) $fatal(1,"begin accepted without physical quiescence");
        @(negedge clk_audio); begin_valid=0;
    endtask
    task automatic check_reset_hold;
        integer held;
        held=0;
        while (resetting) begin
            // End can wake this task just after the terminal edge. The reset
            // is synchronous: inspect storage after its next receiving edge,
            // not the intervening falling edge before reset has been sampled.
            @(posedge clk_audio); #1; held=held+1;
            if (media_enable || dev_ready || audio_dac!==0 || source_edge!==0 ||
                pending_source_valid || output_source_valid || pending_source_edge!==0 || output_source_edge!==0 ||
                marker_ready || receipt_valid || operation_token!==1 || dut.operation_exhausted)
                $fatal(1,"reset did not inhibit sound/write or discard positions gen=%0d held=%0d enable=%b ready=%b dac=%b edge=%h pending=%b/%h output=%b/%h",
                       generation,held,media_enable,dev_ready,audio_dac,source_edge,
                       pending_source_valid,pending_source_edge,output_source_valid,output_source_edge);
            if (held>2060) $fatal(1,"native reset failed to complete");
        end
        if (held<2048 || !quiescent) $fatal(1,"native reset warmup too short: %0d",held);
    endtask
    initial begin
        // Two distinct authored tones, one panned to each side. No music fixture.
        for (channel=0;channel<2;channel=channel+1) begin
            append(8'h20+channel,channel==0 ? 8'h47 : 8'h87);
            append(8'h28+channel,channel==0 ? 8'h3c : 8'h48); append(8'h30+channel,0);
            for (slot=0;slot<4;slot=slot+1) begin
                append(8'h40+slot*8+channel,1); append(8'h60+slot*8+channel,slot*8);
                append(8'h80+slot*8+channel,8'h1f); append(8'ha0+slot*8+channel,0);
                append(8'hc0+slot*8+channel,0); append(8'he0+slot*8+channel,8'h0f);
            end
            append(8'h08,8'h78+channel);
        end
        repeat(4) @(negedge clk_audio); reset_n=1;
        reject_begin(); wait(quiescent);
        // Align Begin early in the wall frame so initial coverage is preloaded.
        at_phase(16); begin_stream(2);
        progress(1,0,200,0,0); progress(2,200,100000,1,0); ref_pause=0;
        wait(media_frame==100); reject_begin();
        wait(media_frame==600); control(1,0,1,1,1,2,1);
        repeat(7*256) @(negedge clk_audio);
        control(0,1,2,0,0,2,1);
        repeat(5*256) @(negedge clk_audio);
        if (media_frame!==600 || gain!==239600 || ramp_elapsed!==400 || phase!==1)
            $fatal(1,"pause did not preserve fade/source position");
        control(2,0,2,0,0,2,1);
        wait(media_frame==1800); control(0,1,3,1,1,2,1);
        wait(media_frame==2200); control(0,1,4,1,2,2,1);
        wait(media_frame==2300); control(0,1,5,1,1,2,1);
        wait(media_frame==2400); control(0,1,6,1,1,2,1);
        wait(media_frame==2500); control(3,1,0,1,0,1,2);
        wait(media_frame==2600); control(1,0,6,1,1,2,1);
        repeat(13*256) @(negedge clk_audio);
        if (media_frame!==2600 || gain!==factor_at(2600)) $fatal(1,"second held fade");
        control(2,0,6,1,1,2,1);
        wait(media_frame==3200); control(3,0,6,1,1,2,1);
        if (end_reason!==1 || failure || media_frame!==3200 || queued!==0) $fatal(1,"Stop state");
        if (checked_frames!=3200 || scaled_changes<1000 || silent_frames<20 ||
            positive[0]<100 || positive[1]<100 || negative[0]<100 || negative[1]<100 ||
            next_op[0]!=operation_count || next_op[1]!=operation_count) $fatal(1,"missing PCM coverage");
        compare=0; send_writes=0; ref_pause=1;
        check_reset_hold();
        repeat(512) begin
            @(negedge clk_audio);
            if (dev_ready || media_enable || audio_dac!==0 || media_frame!==3200)
                $fatal(1,"old stream restarted after Stop reset");
        end
        // A fresh silent generation cannot replay old sound. Its finite end is exact.
        at_phase(16); begin_stream(3); progress(1,0,16,0,0); progress(2,16,17,0,1);
        while (end_reason==0) begin
            @(negedge clk_audio);
            if (audio_dac!==0) $fatal(1,"old tone survived new generation");
        end
        if (end_reason!==2 || media_frame!==16 || failure) $fatal(1,"finite end boundary");
        check_reset_hold();
        // Coverage failure also initiates physical reset; it must not become End.
        at_phase(16); begin_stream(4);
        wait(failure!=0);
        if (failure!==3 || end_reason || media_frame!==0) $fatal(1,"coverage failure kind");
        check_reset_hold();
        // Explicit reset during hold invalidates the old generation and position.
        at_phase(16); begin_stream(5); progress(1,0,100,0,0);
        wait(media_frame==4); control(1,0,1,1,1,5,1);
        at_phase(73); stream_reset=1;
        @(negedge clk_audio); stream_reset=0;
        if (failure!==1 || end_reason || media_frame!==4 || queued!==0) $fatal(1,"active stream reset failure");
        check_reset_hold();
        // A fault offered on the Stop boundary wins and may truncate urgent output.
        at_phase(16); begin_stream(6); progress(1,0,100,0,0);
        wait(media_frame==4); at_phase(255);
        control_valid=1; control_generation=6; control_action=3; control_repeat=0; device_fault=1;
        @(negedge clk_audio); control_valid=0; device_fault=0;
        if (failure!==1 || end_reason || media_frame!==4) $fatal(1,"fault/Stop priority");
        check_reset_hold();
        // Inject converter sticky faults to check the owner's fault/reset wiring.
        // The converter's actual under/overflow causes are exercised separately.
        for (integer fault_kind=0;fault_kind<2;fault_kind=fault_kind+1) begin
            at_phase(16); begin_stream(7+fault_kind); progress(1,0,100,0,0);
            wait(media_frame==4); at_phase(73);
            if (fault_kind==0) force dut.output_media.underflow=1'b1;
            else force dut.output_media.overflow=1'b1;
            @(negedge clk_audio);
            if (fault_kind==0) release dut.output_media.underflow;
            else release dut.output_media.overflow;
            if (failure!==1 || end_reason || media_frame!==4) $fatal(1,"converter fault did not close stream");
            check_reset_hold();
            if (audio_underflow || audio_overflow || failure!==1) $fatal(1,"physical reset lost latched failure");
        end
        begin_target_enabled=0;
        at_phase(16); begin_stream(9); progress(1,0,8,0,0); progress(2,8,9,0,1);
        wait(end_reason!=0);
        if (end_reason!==3 || media_frame!==8 || failure) $fatal(1,"RepeatOne end intent");
        check_reset_hold();
        at_phase(16); begin_stream(10); progress(1,0,100,0,0);
        wait(media_frame==4); control(0,1,2,1,1,11,3);
        if (failure!==2 || end_reason || media_frame!==4) $fatal(1,"protocol reset reason");
        check_reset_hold();
        // Exhaustion is exercised at its real increment, not by forcing a fault.
        at_phase(16); begin_stream(11); progress(1,0,100,0,0);
        wait(media_frame==4); control(1,0,1,0,0,11,1);
        force dut.source_media.source_edge=64'hfffffffffffffffe;
        @(negedge clk_audio); release dut.source_media.source_edge;
        repeat(512) @(negedge clk_audio);
        if (source_edge!==64'hfffffffffffffffe || failure || dut.source_edge_exhausted)
            $fatal(1,"held source edge exhausted early");
        control(2,0,1,0,0,11,1);
        if (source_edge!==64'hffffffffffffffff || dut.source_edge_exhausted) $fatal(1,"last indexed edge lost");
        @(negedge clk_audio);
        if (source_edge!==64'hffffffffffffffff || !dut.source_edge_exhausted) $fatal(1,"source edge wrapped");
        @(negedge clk_audio);
        if (failure!==1 || end_reason || media_frame!==5) $fatal(1,"source exhaustion did not close stream");
        check_reset_hold();
        if (position_checks!=3198) $fatal(1,"missing native position coverage");
        // The final valid operation is delivered through the public wrapper.
        // A further offer faults at the real counter boundary, after Resume.
        at_phase(16); begin_stream(12); progress(1,0,100,0,0);
        wait(media_frame==4); control(1,0,1,0,0,12,1);
        force dut.source_media.operation_token=64'hfffffffffffffffe;
        @(negedge clk_audio); release dut.source_media.operation_token;
        receipt_ready=0; marker_valid=1;
        repeat(512) @(negedge clk_audio);
        if (operation_token!==64'hfffffffffffffffe || marker_ready || receipt_valid || failure)
            $fatal(1,"held operation accepted");
        control(2,0,1,0,0,12,1);
        marker_valid=0;
        if (operation_token!==64'hffffffffffffffff || !receipt_valid || !receipt_marker ||
            receipt_token!==64'hfffffffffffffffe || receipt_at_edge!==source_edge-1 || dut.operation_exhausted)
            $fatal(1,"final operation receipt missing through wrapper");
        @(negedge clk_audio); marker_valid=1;
        @(negedge clk_audio); marker_valid=0;
        @(negedge clk_audio);
        if (failure!==1 || end_reason || media_frame!==5 || receipt_valid)
            $fatal(1,"operation exhaustion did not close stream");
        check_reset_hold();
        receipt_ready=1;
        $display("jt51_enveloped_audio_tb: PASS frames=%0d scaled=%0d silent=%0d positive=%0d/%0d negative=%0d/%0d writes=%0d positions=%0d restore=960 resets=11",
                 checked_frames,scaled_changes,silent_frames,positive[0],positive[1],negative[0],negative[1],operation_count,position_checks);
        $finish;
    end
endmodule
