`timescale 1ns/1ps
module jt51_source_queue_tb;
    logic clk_audio=0, reset_n=0, stream_reset=0, device_fault, clear_audio_flags=0;
    logic dev_valid, dev_ready;
    logic [7:0] dev_address, dev_value;
    logic marker_valid, receipt_ready=1, marker_ready, receipt_valid, receipt_marker;
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
    logic [63:0] pending_prefix, output_prefix;
    logic pending_source_valid, output_source_valid;
    logic audio_mclk, audio_lrck, audio_dac, audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;

    logic item_valid=0, item_marker=0, item_end=0;
    logic [63:0] item_at=0, item_until=0;
    logic [7:0] item_address=0, item_value=0;
    logic [2:0] item_status;
    logic [6:0] source_queued;
    logic feed=0, checking=1;
    integer feed_limit=8195;
    logic ref_ready, ref_sample;
    logic signed [15:0] ref_left, ref_right;
    logic [15:0] ref_write;
    integer sent=0, issued=0, bus_writes=0, receipts=0, ref_index=0, wall=0, full=0;
    integer pcm_checks=0, nonzero=0, pause_admits=0, final_frames=0, late_writes=0;
    integer serial_phase=0;
    logic [63:0] bus_edges[8195];
    logic [63:0] held_edge;
    logic [15:0] expected_write;
    logic [7:0] bus_address=0;
    integer address_pulses=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_enveloped_audio dut(.*);
    rpcmp_media_source_queue source_queue (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting),
        .media_enable(media_enable), .source_edge(source_edge),
        .item_valid(item_valid), .item_marker(item_marker), .item_end(item_end),
        .item_at(item_at), .item_until(item_until), .item_address(item_address), .item_value(item_value),
        .item_payload(1'b0), .dispatch_payload(),
        .item_status(item_status), .queued(source_queued), .supply_fault(device_fault),
        .dev_valid(dev_valid), .dev_address(dev_address), .dev_value(dev_value), .dev_ready(dev_ready),
        .marker_valid(marker_valid), .marker_ready(marker_ready)
    );
    // Independent authored bytes bypass the queue, at the same accepted edges.
    // This compares native execution, not a guessed first audible write effect.
    assign ref_write=write_at(ref_index);
    rpcmp_jt51_media_source reference_source (
        .clk_audio(clk_audio), .reset_n(reset_n), .stream_reset(resetting), .media_enable(media_enable),
        .dev_valid(dev_valid && dev_ready), .dev_ready(ref_ready),
        .dev_address(ref_write[15:8]), .dev_value(ref_write[7:0]), .device_idle(),
        .marker_valid(1'b0), .marker_ready(), .receipt_ready(1'b1), .receipt_valid(), .receipt_marker(),
        .receipt_token(), .receipt_at_edge(), .operation_token(), .operation_exhausted(),
        .jt_sample(ref_sample), .jt_left(ref_left), .jt_right(ref_right),
        .source_edge(), .source_edge_exhausted()
    );
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
        return {8'(8'h60+((i-28)%4)*8),8'(((i-28)/4)%64)};
    endfunction
    always @(negedge clk_audio) begin
        item_valid=feed && sent<feed_limit && (!media_enable || wall%1024<64);
        item_marker=sent>=8192; item_end=sent==8194;
        item_at=sent<=8192 ? 0 : (sent==8193 ? 1000000 : 2000000);
        item_until=sent==8192 ? 1000000 : (sent==8193 ? 2000000 : 0);
        {item_address,item_value}=sent<8192 ? write_at(sent) : 16'd0;
    end
    always @(posedge clk_audio) begin
        wall=wall+1;
        if (!reset_n) serial_phase=0;
        else serial_phase=(serial_phase+1)%256;
        if (wall>2500000) $fatal(1,"native source queue timeout");
        if (item_status==1) sent=sent+1;
        if (checking) begin
            if (item_status==1) begin
                if (paused) pause_admits=pause_admits+1;
            end
            if (item_status==2) full=full+1;
            if (item_status==3 || (item_status==4 && feed && !resetting && sent<8195))
                $fatal(1,"authored input rejected");
            if (reset_n && !resetting) begin
                if (device_fault || failure || audio_underflow || audio_overflow)
                    $fatal(1,"streaming maximum batch faulted");
                if (dev_valid && dev_ready) begin
                    if (!ref_ready || operation_token!==64'(issued+1)) $fatal(1,"native acceptance order");
                    issued=issued+1;
                    ref_index<=ref_index+1;
                end
                if (media_enable && !dut.source_media.jt_wr_n && !dut.source_media.jt_a0 && dut.source_media.cen) begin
                    bus_address=dut.source_media.jt_din; address_pulses=address_pulses+1;
                end
                if (media_enable && !dut.source_media.jt_wr_n && dut.source_media.jt_a0 && dut.source_media.cen_p1) begin
                    expected_write=write_at(bus_writes);
                    if (address_pulses!=bus_writes+1 || {bus_address,dut.source_media.jt_din}!==expected_write)
                        $fatal(1,"native bus write differs from authored batch at %0d",bus_writes);
                    bus_edges[bus_writes]=source_edge; bus_writes=bus_writes+1;
                    if (source_edge>1000000) late_writes=late_writes+1;
                end
                if (marker_valid && marker_ready) begin
                    if (issued!=8192 || operation_token<8193 || operation_token>8195)
                        $fatal(1,"marker overtook batch");
                    bus_edges[operation_token-1]=source_edge;
                    if (operation_token==8194 && source_edge<1000000) $fatal(1,"early second marker");
                    if (operation_token==8195 && source_edge<2000000) $fatal(1,"early final marker");
                end
                if (receipt_valid && receipt_ready) begin
                    if (receipt_token!==64'(receipts+1) || receipt_marker!==(receipts>=8192) ||
                        receipt_at_edge!==bus_edges[receipts]) $fatal(1,"receipt lost order/actual edge");
                    receipts=receipts+1;
                end
                if (media_enable && dut.jt_sample) begin
                    if (!ref_sample || {dut.jt_left,dut.jt_right}!=={ref_left,ref_right})
                        $fatal(1,"queued native PCM differs from authored unqueued source");
                    pcm_checks=pcm_checks+1;
                    if (ref_left!=0 || ref_right!=0) nonzero=nonzero+1;
                end
                if (frame_boundary && output_source_valid && output_prefix==8195) final_frames=final_frames+1;
            end
        end
    end
    task automatic edges(input integer n);
        repeat(n) @(negedge clk_audio); #1;
    endtask
    task automatic start(input logic [63:0] epoch);
        while(!quiescent || serial_phase!=16) edges(1);
        begin_generation=epoch; begin_valid=1;
        #1; if(begin_status!=1) $fatal(1,"Begin rejected");
        edges(1); begin_valid=0;
        // Explicit fixture coverage: this test does not map MDX loop/end.
        progress_generation=epoch; progress_sequence=1; progress_at=0; progress_until=20000;
        progress_valid=1;
        #1; if(progress_status!=1) $fatal(1,"fixture coverage rejected");
        edges(1); progress_valid=0;
    endtask
    task automatic control(input logic [1:0] action);
        while(serial_phase!=255) edges(1);
        control_valid=1; control_action=action;
        #1; if(!frame_boundary || control_status!=1) $fatal(1,"control not offered on its frame boundary");
        edges(1); control_valid=0;
    endtask
    initial begin
        edges(4); reset_n=1; wait(quiescent); feed=1;
        wait(source_queued==64); start(1);
        wait(issued>=2000); control(1); held_edge=source_edge;
        edges(2048);
        if (!paused || source_edge!==held_edge) $fatal(1,"paused source advanced");
        control(2);
        // Exercise external receipt backpressure without starving queued input.
        wait(issued>=4000); receipt_ready=0; edges(4096); receipt_ready=1;
        wait(final_frames>=2); feed=0;
        if(sent!=8195 || issued!=8192 || bus_writes!=8192 || receipts!=8195 ||
           full==0 || pause_admits==0 || nonzero<1000 || late_writes==0)
            $fatal(1,"missing native queue coverage");
        checking=0; control(3); wait(quiescent);
        if(end_reason!=1 || failure || source_queued!=0) $fatal(1,"Stop did not discard queue");
        // Reset an actual data pulse with more queued work, then prove the
        // new generation cannot receive its retained head or delayed receipt.
        sent=0; feed_limit=64; feed=1;
        wait(source_queued==64); feed=0; start(2);
        while(dut.source_media.jt_wr_n || !dut.source_media.jt_a0) edges(1);
        if(source_queued==0 || device_idle) $fatal(1,"missing queued/in-flight reset fixture");
        stream_reset=1; edges(1); stream_reset=0;
        wait(quiescent);
        if(failure!=1 || source_queued!=0 || receipt_valid) $fatal(1,"reset preserved queued/in-flight work");
        // New generation intentionally has no supplied operation/coverage.
        // Registered supply fault enters the real shared reset path.
        start(3); wait(failure!=0);
        if(failure!=1 || end_reason!=0) $fatal(1,"supply fault did not become DeviceFault");
        wait(quiescent); edges(256);
        if(audio_dac!==0 || source_queued!=0 || dev_valid || marker_valid)
            $fatal(1,"old stream survived supply failure");
        $display("jt51_source_queue_tb: PASS writes=%0d receipts=%0d pcm=%0d nonzero=%0d full=%0d pause_admits=%0d late=%0d final_frames=%0d reset_fault=2",
                 bus_writes,receipts,pcm_checks,nonzero,full,pause_admits,late_writes,final_frames);
        $finish;
    end
endmodule
