`timescale 1ns/1ps
module sound_session_tb;
    logic clk_audio=0, reset_n=0, device_fault=0, emergency_silence=0;
    logic request_valid=0, request_ready, response_valid, response_ready=0;
    logic [63:0] request_id=0, request_generation=0, request_revision=0;
    logic [2:0] request_kind=0;
    logic request_target_enabled=0;
    logic [31:0] request_target=0;
    logic [63:0] response_id, response_generation, response_revision, response_frame;
    logic [2:0] response_kind, response_result;
    logic response_target_enabled;
    logic [31:0] response_target;
    logic item_valid=0, item_marker=0, item_end=0;
    logic [63:0] item_generation=0, item_epoch=0, item_at=0, item_until=0, item_loops=0;
    logic [7:0] item_address=0, item_value=0;
    logic [2:0] item_status;
    logic [6:0] source_queued;
    logic [63:0] feed_epoch, prepared_generation, generation, policy_revision, media_frame, completed_loops;
    logic fault, inhibited, target_enabled, paused;
    logic [31:0] target;
    logic [1:0] phase;
    logic [2:0] end_reason, failure;
    logic [17:0] gain, ramp_elapsed;
    logic quiescent, resetting, device_idle, media_enable, frame_boundary;
    logic [63:0] pending_prefix, output_prefix, pending_loops, output_loops;
    logic pending_source_valid, output_source_valid, pending_checkpoint_valid, output_checkpoint_valid;
    logic pending_ended, output_ended, audio_mclk, audio_lrck, audio_dac;
    rpcmp_sound_session dut(.*);
    always #5 clk_audio=~clk_audio;

    integer wall=0, serial_phase=0, accepted_at=0, accepted_phase=0, latency=0;
    integer reset_max=0, start_max=0, control_max=0, phase_cases=0, nonzero_bits=0;
    integer responses=0, emergency_cases=0, tone_bits_before=0;
    logic held=0;
    logic [294:0] saved_response;
    wire [294:0] response_bundle={response_id,response_generation,response_revision,response_frame,
                                 response_kind,response_result,response_target_enabled,response_target};
    logic [63:0] next_id=1, next_generation=1, frozen_frame, old_epoch;
    logic [63:0] sent_id, sent_generation, sent_revision;
    logic [2:0] sent_kind;
    logic sent_enabled;
    logic [31:0] sent_target;
    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>2000000) $fatal(1,"session test timeout");
        if (held && (!response_valid || response_bundle!==saved_response))
            $fatal(1,"response changed before release");
        held=response_valid && !response_ready;
        saved_response=response_bundle;
        if (!reset_n) begin serial_phase=0; held=0; end
        else serial_phase=(serial_phase+1)%256;
        #1;
        if (audio_mclk!==1'b1 || audio_lrck!==(serial_phase>=128))
            $fatal(1,"normal reset/inhibit disturbed continuous serial clocks");
        if (inhibited && audio_dac!==0) $fatal(1,"inhibit did not silence serial data");
        if ((paused || quiescent) && audio_dac!==0) $fatal(1,"paused/quiescent session was not silent");
        if (response_valid && request_ready) $fatal(1,"unread response admitted another request");
        if (audio_dac) nonzero_bits=nonzero_bits+1;
    end
    always @(negedge clk_audio) begin
        #1; if (audio_mclk!==1'b0) $fatal(1,"MCLK stopped");
    end

    task automatic send(input logic [2:0] kind, input logic [63:0] gen,
                        input logic [63:0] revision=0, input logic enabled=0,
                        input logic [31:0] count=0, input integer at_phase=-1,
                        input logic [63:0] identity=0, input logic fault_at_accept=0);
        @(negedge clk_audio);
        while (at_phase>=0 && serial_phase!=at_phase) @(negedge clk_audio);
        if (!request_ready) $fatal(1,"request unexpectedly busy");
        request_valid=1; request_id=identity!=0 ? identity : next_id;
        if (identity==0) next_id=next_id+1;
        request_generation=gen; request_revision=revision; request_kind=kind;
        request_target_enabled=enabled; request_target=count;
        sent_id=request_id; sent_generation=gen; sent_revision=revision;
        sent_kind=kind; sent_enabled=enabled; sent_target=count;
        accepted_at=wall+1; accepted_phase=serial_phase;
        if (fault_at_accept) device_fault=1;
        @(posedge clk_audio); #1;
        @(negedge clk_audio); request_valid=0;
        if (fault_at_accept) device_fault=0;
        // The owner must use its copy, not a caller's reused staging words.
        request_id=0; request_generation=0; request_revision=0; request_kind=7;
        request_target_enabled=0; request_target=0;
    endtask
    task automatic result(input logic [2:0] expected, input integer limit);
        while (!response_valid && wall-accepted_at<=limit) begin @(posedge clk_audio); #1; end
        latency=wall-accepted_at;
        if (!response_valid || latency>limit || response_result!==expected)
            $fatal(1,"control result kind=%0d expected=%0d actual=%0d latency=%0d limit=%0d phase=%0d failure=%0d",
                   sent_kind,expected,response_result,latency,limit,accepted_phase,failure);
        if ({response_id,response_generation,response_revision,response_kind,response_target_enabled,response_target}!==
            {sent_id,sent_generation,sent_revision,sent_kind,sent_enabled,sent_target})
            $fatal(1,"response did not echo immutable request");
        responses=responses+1;
        if (expected==1) begin
            if (sent_kind==0 && latency>reset_max) reset_max=latency;
            if (sent_kind==1 && latency>start_max) start_max=latency;
            if (sent_kind>=2 && latency>control_max) control_max=latency;
        end
    endtask
    task automatic release_response;
        @(negedge clk_audio); response_ready=1;
        if (request_ready) $fatal(1,"response pop bypassed request slot ownership");
        @(posedge clk_audio); #1;
        @(negedge clk_audio); response_ready=0;
    endtask
    task automatic reset_session;
        old_epoch=feed_epoch;
        send(0,next_generation); result(1,2050);
        if (latency!=2050 || !quiescent || resetting || !device_idle || media_frame!=0 ||
            fault || inhibited || generation!=0 || source_queued!=0 || pending_source_valid || output_source_valid ||
            feed_epoch!==old_epoch+1 || prepared_generation!==next_generation)
            $fatal(1,"Reset success did not establish clean physical session latency=%0d",latency);
        item_generation=next_generation; item_epoch=feed_epoch;
        next_generation=next_generation+1;
        release_response();
    endtask
    task automatic item(input logic marker=1, input logic ended=0, input logic [63:0] until_edge=64'hffffffffffff,
                        input logic [63:0] loops=0, input logic [15:0] bytes=0, input logic [2:0] expected=1);
        @(negedge clk_audio);
        item_valid=1; item_marker=marker; item_end=ended; item_at=0;
        item_until=marker ? until_edge : 64'd0; item_loops=loops;
        item_address=bytes[15:8]; item_value=bytes[7:0];
        #1; if(item_status!==expected) $fatal(1,"item result expected=%0d actual=%0d",expected,item_status);
        @(posedge clk_audio); #1;
        @(negedge clk_audio); item_valid=0;
    endtask
    function automatic logic [15:0] tone(input integer i);
        integer group_index, op_index;
        if(i==0) return 16'h20c7;
        if(i==1) return 16'h283c;
        if(i==2) return 16'h3000;
        if(i==27) return 16'h0878;
        op_index=(i-3)/6; group_index=(i-3)%6;
        case(group_index)
            0:return {8'(8'h40+op_index*8),8'd1};
            1:return {8'(8'h60+op_index*8),8'(op_index*8)};
            2:return {8'(8'h80+op_index*8),8'h1f};
            3:return {8'(8'ha0+op_index*8),8'd0};
            4:return {8'(8'hc0+op_index*8),8'd0};
            5:return {8'(8'he0+op_index*8),8'h0f};
        endcase
        $fatal(1,"tone index"); return 0;
    endfunction
    task automatic start_session(input integer at_phase=-1);
        integer boundary_delay;
        send(1,prepared_generation,1,0,0,at_phase); result(1,259);
        boundary_delay=(255-accepted_phase+256)%256;
        if (boundary_delay<3) boundary_delay=boundary_delay+256;
        if (latency!=boundary_delay+1 || response_frame!=1 || media_frame!=1 || fault || inhibited)
            $fatal(1,"Start boundary/bootstrap bound differs from serial-clock oracle");
        release_response();
    endtask
    task automatic boundary_control(input logic [2:0] kind, input integer at_phase,
                                    input logic [63:0] revision=1, input logic enabled=0,
                                    input logic [31:0] count=0);
        integer boundary_delay;
        send(kind,generation,revision,enabled,count,at_phase); result(1,257);
        boundary_delay=(255-accepted_phase+256)%256;
        if (boundary_delay==0) boundary_delay=256;
        if (latency!=boundary_delay+1) $fatal(1,"boundary ACK timing differs from independent serial phase");
        if (kind==2) begin
            if (!paused || response_frame!=media_frame) $fatal(1,"Pause ACK position");
            frozen_frame=media_frame;
        end else if (kind==3) begin
            if (paused || response_frame!=frozen_frame || media_frame!=frozen_frame+1)
                $fatal(1,"Resume ACK confused held position with latest consumption");
        end else if (response_frame!=(paused ? media_frame : media_frame-1) ||
                     policy_revision!=revision || target_enabled!=enabled || target!=count)
            $fatal(1,"policy boundary result");
        release_response();
    endtask
    initial begin
        repeat(4) @(negedge clk_audio);
        reset_n=1;
        // Common reset is inhibited and cannot be mistaken for an explicit Reset ACK.
        send(1,1,1); result(4,0); release_response();
        // A fault sampled on the Reset acceptance edge cannot be lost when
        // the caller removes it before the following clock.
        send(0,1,0,0,0,-1,0,1); result(4,1); release_response();
        for (integer p=0;p<256;p=p+1) begin
            reset_session(); item(); start_session(p);
            boundary_control(2,p);
            repeat(259) @(negedge clk_audio);
            if (media_frame!=frozen_frame || media_enable) $fatal(1,"pause position advanced");
            boundary_control(4,p,2,1,3);
            boundary_control(3,p,2,1,3);
            boundary_control(4,p,3,0,0);
            phase_cases=phase_cases+1;
        end
        // Retrying Reset with the already-started generation is permitted,
        // but must not make that generation eligible for another Begin.
        send(0,generation); result(1,2050); release_response();
        item_generation=prepared_generation; item_epoch=feed_epoch; item();
        send(1,prepared_generation,1); result(3,0); release_response();
        reset_session();
        for (integer i=0;i<28;i=i+1) item(0,0,0,0,tone(i));
        item(); start_session();
        if (device_idle || source_queued==0) $fatal(1,"in-flight Reset fixture missed native/queued work");
        reset_session();
        repeat(600) @(negedge clk_audio);
        if (source_queued!=0 || pending_source_valid || output_source_valid || media_frame!=0)
            $fatal(1,"old work escaped successful Reset");
        reset_session();
        send(1,prepared_generation,1); result(2,1); release_response(); // Empty Begin can be retried.
        for (integer i=0;i<28;i=i+1) item(0,0,0,0,tone(i));
        item(1,0,64'hffffffffffff,2); start_session();
        tone_bits_before=nonzero_bits;
        repeat(30000) @(negedge clk_audio);
        if (nonzero_bits==tone_bits_before || completed_loops!=2 || fault) $fatal(1,"real JT51 tone did not reach serial output");
        // Unread completion is stable while audio continues and input staging changes.
        send(4,generation,2,1,2); result(1,257);
        frozen_frame=media_frame;
        request_valid=1; request_id=next_id; request_generation=generation;
        request_kind=0; request_revision=0;
        repeat(800) @(negedge clk_audio);
        if (media_frame<=frozen_frame || phase!=1 || gain>=240000) $fatal(1,"unread policy ACK held audio/fade");
        request_valid=0;
        release_response();
        boundary_control(4,31,3,0,0);
        if (phase!=2) $fatal(1,"live policy did not restore");
        // Replayed IDs, invalid payload, stale generation and conflicting revisions are inert.
        send(2,generation,3,0,0,-1,next_id-1); result(3,0); release_response();
        send(2,generation,0); result(2,0); release_response();
        send(2,generation-1,3); result(3,0); release_response();
        send(4,generation,3,1,5); result(2,0); release_response();
        if (paused || fault || policy_revision!=3) $fatal(1,"rejected controls affected playback");
        boundary_control(2,55,3);
        reset_session(); // Explicit Reset from paused.
        item_epoch=item_epoch-1; item(1,0,100,0,0,5); item_epoch=feed_epoch;
        item_generation=item_generation-1; item(1,0,100,0,0,5); item_generation=prepared_generation;
        // Reset wins over a valid same-edge item and makes its old epoch unusable.
        @(negedge clk_audio); item_valid=1; item_marker=1; item_end=0; item_until=10000;
        request_valid=1; request_id=next_id; next_id=next_id+1;
        request_generation=next_generation; next_generation=next_generation+1; request_kind=0; request_revision=0;
        sent_id=request_id; sent_generation=request_generation; sent_revision=0; sent_kind=0; sent_enabled=0; sent_target=0;
        accepted_at=wall+1;
        #1; if (item_status!=4) $fatal(1,"Reset did not win over item");
        @(posedge clk_audio); #1;
        @(negedge clk_audio); item_valid=0; request_valid=0;
        result(1,2050); release_response();
        item_generation=prepared_generation; item_epoch=feed_epoch;
        // Natural end and real source starvation close supply independently of response reads.
        item(1,1,0); start_session();
        wait(pending_ended);
        // The end boundary wins before a request accepted on that same edge
        // can enter the envelope on a later boundary.
        send(2,generation,1,0,0,255); result(2,1); release_response();
        repeat(10000) @(negedge clk_audio);
        if (end_reason==0 || fault || !quiescent) $fatal(1,"natural end session did not quiesce");
        send(2,generation,1); result(2,0); release_response();
        item(1,0,100,0,0,4);
        reset_session(); item(1,0,512); start_session();
        repeat(2000) @(negedge clk_audio);
        if (!fault || !inhibited || failure==0) $fatal(1,"actual source starvation was hidden");
        reset_session();
        // Emergency is independent at Begin, Start wait, boundary wait, reset, unread ACK.
        for (integer e=0;e<5;e=e+1) begin
            if (e!=0) reset_session();
            item();
            if (e<2) begin
                send(1,prepared_generation,1,0,0,20);
                if (e==1) repeat(2) @(negedge clk_audio);
            end else if (e==2) begin
                start_session(); send(2,generation,1,0,0,20);
            end else if (e==3) send(0,prepared_generation);
            else begin start_session(); send(4,generation,2,1,3); result(1,257); end
            emergency_silence=1;
            repeat(2) @(negedge clk_audio);
            if (!fault || !inhibited || audio_dac!=0) $fatal(1,"emergency not immediate");
            if (e==4) begin
                if (response_result!=1) $fatal(1,"later fault rewrote completed response");
            end else result(4,2050);
            release_response();
            send(0,prepared_generation); result(4,1); release_response();
            emergency_silence=0;
            repeat(2100) @(negedge clk_audio);
            if (!inhibited || !fault) $fatal(1,"fault cleared without successful Reset");
            emergency_cases=emergency_cases+1;
        end
        reset_session(); item(); start_session();
        device_fault=1; repeat(2) @(negedge clk_audio);
        if (!fault || !inhibited) $fatal(1,"device fault not latched");
        device_fault=0; reset_session();
        send(1,prepared_generation-1,1); result(3,0); release_response();
        // Boundary fixtures enter otherwise unreachable u64 limits without simulating 2^64 requests.
        @(negedge clk_audio); force dut.feed_epoch=64'hffffffffffffffff;
        send(0,prepared_generation); result(4,0); release_response();
        send(0,prepared_generation); result(4,0); release_response();
        if (!fault || !inhibited || feed_epoch!=64'hffffffffffffffff) $fatal(1,"epoch wrapped/recovered without common reset");
        @(negedge clk_audio); release dut.feed_epoch; reset_n=0;
        repeat(4) @(negedge clk_audio);
        reset_n=1;
        send(0,1,0,0,0,-1,64'hffffffffffffffff); result(1,2050); release_response();
        send(0,2,0,0,0,-1,64'hffffffffffffffff); result(3,0); release_response();
        if (phase_cases!=256 || emergency_cases!=5 || start_max!=259 || control_max!=257 || reset_max!=2050)
            $fatal(1,"phase/latency coverage incomplete");
        $display("sound_session_tb: PASS phases=%0d reset_max=%0d start_max=%0d control_max=%0d responses=%0d emergencies=%0d nonzero_bits=%0d",
                 phase_cases,reset_max,start_max,control_max,responses,emergency_cases,nonzero_bits);
        $finish;
    end
endmodule
