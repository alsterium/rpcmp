`timescale 1ns/1ps
module jt51_progress_audio_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, device_fault=0, clear_audio_flags=0;
    logic item_valid=0, item_marker=0, item_end=0;
    logic [63:0] item_at=0, item_until=0, item_loops=0;
    logic [7:0] item_address=0, item_value=0;
    logic [2:0] item_status;
    logic [6:0] source_queued;
    logic receipt_ready=0;
    logic receipt_valid, receipt_marker;
    logic [63:0] operation_token, receipt_token, receipt_at_edge;
    logic begin_valid=0;
    logic [63:0] begin_generation=0, begin_revision=0;
    logic begin_target_enabled=0;
    logic [31:0] begin_target=0;
    logic [2:0] begin_status;
    logic control_valid=0;
    logic [63:0] control_generation=0, control_revision=0;
    logic [1:0] control_action=0;
    logic control_repeat=0, control_target_enabled=0;
    logic [31:0] control_target=0;
    logic [1:0] control_status;
    logic [63:0] generation, policy_revision, media_frame, completed_loops;
    logic target_enabled, paused;
    logic [31:0] target;
    logic [1:0] phase;
    logic [2:0] end_reason, failure;
    logic [17:0] gain, ramp_elapsed;
    logic [8:0] queued;
    logic quiescent, resetting, device_idle, media_enable, frame_boundary;
    logic [63:0] source_edge, pending_source_edge, output_source_edge;
    logic [63:0] pending_prefix, output_prefix;
    logic pending_source_valid, output_source_valid;
    logic pending_checkpoint_valid, output_checkpoint_valid;
    logic pending_ended, output_ended;
    logic [63:0] pending_loops, output_loops;
    logic audio_mclk, audio_lrck, audio_dac;
    logic audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;
    function automatic logic [15:0] write_at(input integer i);
        integer group_index, op_index;
        if(i==0) return 16'h20c7;
        if(i==1) return 16'h283c;
        if(i==2) return 16'h3000;
        if(i==27) return 16'h0878;
        if(i<27) begin
            op_index=(i-3)/6; group_index=(i-3)%6;
            case(group_index)
                0:return {8'(8'h40+op_index*8),8'd1};
                1:return {8'(8'h60+op_index*8),8'(op_index*8)};
                2:return {8'(8'h80+op_index*8),8'h1f};
                3:return {8'(8'ha0+op_index*8),8'd0};
                4:return {8'(8'hc0+op_index*8),8'd0};
                5:return {8'(8'he0+op_index*8),8'h0f};
            endcase
        end
        $fatal(1,"tone write index out of bounds");
        return 0;
    endfunction

    integer wall=0, serial_phase=0, ready_after=0, scenario=0;
    integer received=0, bus_count=0, native_done=0, samples=0;
    integer checked_frames=0, checked_samples=0, nonzero=0, starts=0, fades=0, restores=0, ends=0;
    integer marker_replacement=0, merged=0;
    logic [63:0] last_selected_prefix=0;
    logic checking=1, model_started=0, model_running=0, model_paused=0, model_target_enabled;
    logic model_pending=0, model_output=0, enabled, consuming, boundary, was_reset;
    logic [63:0] retained=0, model_frame=0, model_loops=0, model_pending_edge=0, model_output_edge=0;
    logic [63:0] model_prefix=0, model_pending_prefix=0, model_output_prefix=0;
    logic [65:0] model_checkpoint=0, model_pending_data=0, model_output_data=0;
    logic [63:0] positions[64], bus_edges[64];
    logic [65:0] checkpoints[64];
    logic [7:0] bus_address=0;
    logic [31:0] model_pcm=0, output_pcm=0;
    integer model_gain=0, model_phase=0, model_elapsed=0, model_target=0, model_end=0, anchor=0;
    integer old_gain, frame_before;
    logic reached;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_progress_audio dut(.session_reset(1'b0), .*);

    function automatic logic [15:0] scaled(input logic signed [15:0] value, input integer factor);
        longint signed product;
        product=longint'(value)*factor;
        return 16'(product/240000);
    endfunction
    always @(posedge clk_audio) begin
        wall=wall+1; boundary=serial_phase==255; was_reset=!reset_n || resetting;
        if (wall>500000) $fatal(1,"progress integration timeout");
        consuming=0; frame_before=int'(model_frame); old_gain=model_gain;
        if (!reset_n) serial_phase=0;
        else serial_phase=(serial_phase+1)%256;
        if (checking) begin
            if (!was_reset && (failure || audio_underflow || audio_overflow))
                $fatal(1,"unexpected mapped stream fault frame=%0d failure=%0d",media_frame,failure);
            if (begin_status==1) begin
                model_started=1; model_frame=0; model_loops=0; model_paused=0; model_end=0;
                model_gain=240000; model_phase=0; model_elapsed=0;
                model_target_enabled=begin_target_enabled; model_target=int'(begin_target);
                ready_after=wall+2; starts=starts+1;
            end else if (boundary && model_started && !was_reset && wall>=ready_after && model_end==0) begin
                if (control_valid) begin
                    if (control_status!=1) $fatal(1,"boundary control rejected");
                    if (control_repeat) begin
                        model_target_enabled=control_target_enabled; model_target=int'(control_target);
                    end
                    if (control_action==1) model_paused=1;
                    if (control_action==2) model_paused=0;
                    if (control_action==3) model_end=1;
                end
                if (!model_paused && model_end==0) begin
                    if (model_frame>=2 && !model_pending) $fatal(1,"independent sample coverage missing");
                    model_loops=model_pending ? model_pending_data[63:0] : 64'd0;
                    if (model_pending && model_pending_data[64]) begin
                        model_end=model_target_enabled ? 2 : 3; ends=ends+1;
                    end else begin
                        reached=model_target_enabled && model_loops>=64'(model_target);
                        if (reached && model_phase!=1) begin
                            model_phase=1; model_elapsed=0; anchor=model_gain; fades=fades+1;
                        end else if (!reached && model_phase==1) begin
                            model_phase=2; model_elapsed=0; anchor=model_gain; restores=restores+1;
                        end
                        consuming=1; model_frame=model_frame+1;
                        if (model_phase!=0) begin
                            model_elapsed=model_elapsed+1;
                            if (model_phase==1) model_gain=anchor-(anchor*model_elapsed)/240000;
                            else model_gain=anchor+((240000-anchor)*model_elapsed)/960;
                        end
                    end
                end
                if (model_end!=0) begin model_gain=0; model_phase=0; model_elapsed=0; model_paused=0; end
            end
            enabled=!was_reset && (boundary ? consuming : model_running);
            if (media_enable!==enabled) $fatal(1,"native start/hold ownership wall=%0d frame=%0d",wall,model_frame);
            if (was_reset) begin
                retained=0; model_running=0; model_pending=0; model_output=0;
                model_pending_edge=0; model_output_edge=0; model_pending_prefix=0; model_output_prefix=0;
                model_checkpoint=0; model_pending_data=0; model_output_data=0;
                received=0; bus_count=0; native_done=0; samples=0; model_prefix=0; output_pcm=0;
                last_selected_prefix=0;
            end else begin
                if (source_edge!==retained) $fatal(1,"source counter differs from independently retained edges");
                if (enabled && !dut.source_media.jt_wr_n && !dut.source_media.jt_a0 && dut.source_media.cen)
                    bus_address=dut.source_media.jt_din;
                if (enabled && !dut.source_media.jt_wr_n && dut.source_media.jt_a0 && dut.source_media.cen_p1) begin
                    if (scenario!=0 || {bus_address,dut.source_media.jt_din}!==write_at(bus_count))
                        $fatal(1,"actual native bytes differ from authored tone");
                    bus_edges[bus_count]=retained; bus_count=bus_count+1;
                end
                if (dut.marker_valid && dut.marker_ready) begin
                    if (scenario==0 && bus_count!=28) $fatal(1,"checkpoint overtook authored writes");
                    bus_edges[operation_token-1]=retained;
                    if (receipt_valid && receipt_ready && receipt_marker) marker_replacement=marker_replacement+1;
                end
                if (receipt_valid && receipt_ready) begin
                    if (received>=64 || receipt_token!==64'(received+1) || receipt_at_edge!==bus_edges[received])
                        $fatal(1,"actual receipt position/order mismatch");
                    positions[received]=receipt_at_edge;
                    if (scenario==0) begin
                        if (receipt_marker!==(received>=28)) $fatal(1,"marker identity mismatch");
                        checkpoints[received]=received<28 ? 66'd0 :
                            {1'b1,(received==32),64'(received==32 ? 3 : received-28)};
                    end else if (scenario==1) begin
                        if (!receipt_marker || received!=0) $fatal(1,"zero-write receipt mismatch");
                        checkpoints[received]={2'b11,64'd0};
                    end else begin
                        if (!receipt_marker || received>=3) $fatal(1,"short checkpoint burst mismatch");
                        checkpoints[received]={1'b1,(received==2),64'(received+1)};
                    end
                    received=received+1;
                end
                if (boundary) begin
                    model_running=consuming;
                    model_output=consuming && model_pending;
                    model_output_edge=model_output ? model_pending_edge : 0;
                    model_output_prefix=model_output ? model_pending_prefix : 0;
                    model_output_data=model_output ? model_pending_data : 0;
                    output_pcm=model_output ? {scaled($signed(model_pcm[31:16]),old_gain),
                                               scaled($signed(model_pcm[15:0]),old_gain)} : 0;
                    if (consuming) begin
                        checked_frames=checked_frames+1;
                        if (frame_before<2 && model_pending) $fatal(1,"startup baseline contained a sample");
                        if (output_pcm!=0) nonzero=nonzero+1;
                        model_pending=0;
                    end
                end
                if (enabled && dut.jt_sample) begin
                    // Independent quotient selector; no DUT rate_phase or payload is an oracle.
                    if (((longint'(samples)+1)*3072000)/3579545 != (longint'(samples)*3072000)/3579545) begin
                        model_pending=1; model_pending_edge=retained;
                        if (scenario==2 && model_prefix-last_selected_prefix>1) merged=merged+1;
                        last_selected_prefix=model_prefix;
                        model_pending_prefix=model_prefix; model_pending_data=model_checkpoint;
                        model_pcm={dut.jt_left,dut.jt_right}; checked_samples=checked_samples+1;
                    end
                    samples=samples+1;
                end
                // Native samples above see the pre-edge completed checkpoint.
                // Authored operations are spaced enough to prefetch every due head.
                if (enabled && native_done<received && retained>=positions[native_done]+5273) begin
                    if (checkpoints[native_done][65]) model_checkpoint=checkpoints[native_done];
                    native_done=native_done+1; model_prefix=64'(native_done);
                end
                if (enabled) retained=retained+1;
            end
            #1;
            if (model_started && (media_frame!==model_frame || completed_loops!==model_loops ||
                gain!==18'(model_gain) || phase!==2'(model_phase) || ramp_elapsed!==18'(model_elapsed) ||
                paused!==model_paused || end_reason!==3'(model_end)))
                $fatal(1,"audible envelope mismatch frame=%0d/%0d loops=%0d/%0d gain=%0d/%0d phase=%0d/%0d end=%0d/%0d",
                    media_frame,model_frame,completed_loops,model_loops,gain,model_gain,phase,model_phase,end_reason,model_end);
            if (pending_source_valid!==model_pending || pending_source_edge!==(model_pending ? model_pending_edge : 0) ||
                pending_prefix!==(model_pending ? model_pending_prefix : 0) ||
                {pending_checkpoint_valid,pending_ended,pending_loops}!==(model_pending ? model_pending_data : 0) ||
                output_source_valid!==model_output || output_source_edge!==model_output_edge || output_prefix!==model_output_prefix ||
                {output_checkpoint_valid,output_ended,output_loops}!==model_output_data)
                $fatal(1,"selected checkpoint/position ownership differs at source=%0d",retained);
            if (audio_mclk!==clk_audio || audio_lrck!==(serial_phase>=128)) $fatal(1,"serial clock ownership");
            if (serial_phase%128<4 || serial_phase%128>=68) begin
                if(audio_dac!==0) $fatal(1,"serial padding");
            end else if(audio_dac!==(serial_phase<128 ? output_pcm[32-(serial_phase%128)/4] : output_pcm[16-(serial_phase%128)/4]))
                $fatal(1,"mapped PCM/gain serialization mismatch");
        end
    end
    task automatic edges(input integer n);
        repeat(n) @(negedge clk_audio); #1;
    endtask
    task automatic offer(input logic marker, ending, input logic [63:0] at_edge, until_edge, loops,
                         input logic [15:0] bytes, input logic [2:0] status);
        edges(1); item_valid=1; item_marker=marker; item_end=ending;
        item_at=at_edge; item_until=until_edge; item_loops=loops; {item_address,item_value}=bytes;
        #1; if(item_status!==status) $fatal(1,"item admission expected=%0d got=%0d",status,item_status);
        edges(1); item_valid=0;
    endtask
    task automatic start(input integer serial_at, input logic [63:0] epoch);
        while(!quiescent || serial_phase!=serial_at) edges(1);
        begin_generation=epoch; control_generation=epoch; begin_valid=1;
        #1; if(begin_status!=1) $fatal(1,"Begin rejected");
        edges(1); begin_valid=0;
    endtask
    task automatic control(input logic [1:0] action, input logic policy=0);
        while(serial_phase!=255) edges(1);
        control_action=action; control_repeat=policy; control_valid=1;
        edges(1); control_valid=0; control_repeat=0;
    endtask
    initial begin
        receipt_ready=1; begin_revision=1; begin_target_enabled=1; begin_target=2;
        control_revision=1; control_target_enabled=1; control_target=2;
        edges(4); reset_n=1; wait(quiescent);
        begin_generation=1; begin_valid=1; #1;
        if(begin_status!=3) $fatal(1,"empty Begin accepted");
        edges(1); begin_valid=0;
        offer(0,0,0,0,1,16'h20c7,3);
        for(integer i=0;i<28;i=i+1) offer(0,0,0,0,0,write_at(i),1);
        offer(1,0,0,12000,0,0,1);
        offer(1,0,12000,24000,1,0,1);
        offer(1,0,24000,36000,0,0,3);
        offer(1,0,24000,36000,2,0,1);
        offer(1,0,36000,48000,3,0,1);
        offer(1,1,48000,0,3,0,1);
        offer(1,1,0,0,2,0,3); // metadata validation precedes closed admission
        start(254,1);
        wait(source_edge>=11000); receipt_ready=0; edges(2048); receipt_ready=1;
        wait(pending_checkpoint_valid && pending_loops==2); control(1);
        if(completed_loops!=1 || gain!=240000) $fatal(1,"Pause did not precede loop boundary");
        edges(1024); control(2);
        wait(ramp_elapsed==4); control_target=5; control_revision=2; control(0,1);
        wait(end_reason!=0); edges(1);
        if(end_reason!=2 || completed_loops!=3 || fades!=1 || restores!=1)
            $fatal(1,"mapped fade/restore/end coverage missing");
        wait(quiescent);
        // Immediate zero-write end, held exactly on its pending boundary.
        scenario=1; begin_target_enabled=0;
        offer(1,1,0,0,0,0,1); start(255,2);
        wait(pending_ended); control(1); edges(1024);
        if(end_reason || !paused) $fatal(1,"paused natural end advanced");
        control(2); wait(quiescent);
        if(end_reason!=3) $fatal(1,"infinite natural end did not request RepeatOne");
        for(integer p=0;p<3;p=p+1) begin
            offer(1,1,0,0,0,0,1); start(p*127,64'(p+3)); wait(quiescent);
            if(end_reason!=3) $fatal(1,"serial-phase start failed");
        end
        // A blocked marker is delivered on the edge accepting its successor.
        // Several checkpoints then complete between the same selected samples.
        scenario=2; receipt_ready=0; begin_target_enabled=1; begin_target=2;
        offer(1,0,0,2,1,0,1); offer(1,0,2,4,2,0,1); offer(1,1,4,0,3,0,1);
        start(73,6); wait(source_edge>=20); edges(1); receipt_ready=1;
        wait(quiescent);
        if(end_reason!=2 || completed_loops!=3 || marker_replacement==0 || merged==0)
            $fatal(1,"missing marker replacement/merged checkpoint coverage");
        if(checked_frames<300 || checked_samples<290 || nonzero<100 || starts!=6 || ends!=6)
            $fatal(1,"missing mapped progress coverage frames=%0d samples=%0d nonzero=%0d starts=%0d ends=%0d",
                checked_frames,checked_samples,nonzero,starts,ends);
        // One write without a sealing marker must cause a real supply fault.
        checking=0; offer(0,0,0,0,0,16'h20c7,1); start(16,7);
        wait(failure!=0); if(failure!=1) $fatal(1,"source starvation not DeviceFault");
        wait(quiescent); edges(256);
        if(source_queued!=0 || receipt_valid || pending_source_valid || output_source_valid || audio_dac)
            $fatal(1,"source starvation retained old work");
        begin_target_enabled=0;
        offer(1,1,0,0,0,0,1); start(32,8); wait(quiescent);
        if(failure || end_reason!=3) $fatal(1,"fresh generation did not recover after fault");
        $display("jt51_progress_audio_tb: PASS frames=%0d samples=%0d nonzero=%0d starts=%0d fades=%0d restores=%0d ends=%0d marker_replacement=%0d merged=%0d fault_recovery=1",
            checked_frames,checked_samples,nonzero,starts,fades,restores,ends,marker_replacement,merged);
        $finish;
    end
endmodule
