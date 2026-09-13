`timescale 1ns/1ps
// The oracle observes actual native data pulses and counts retained clock edges.
// It does not use the source FSM or its timestamp/token registers to predict receipts.
module jt51_source_receipt_tb;
    logic clk_audio=0, reset_n=0, stream_reset=1, media_enable=0;
    logic dev_valid=0, dev_ready, marker_valid=0, marker_ready, receipt_ready=0;
    logic receipt_valid, receipt_marker, operation_exhausted;
    logic [7:0] dev_address=0, dev_value=0;
    logic [63:0] operation_token, receipt_token, receipt_at_edge, source_edge;
    logic source_edge_exhausted, device_idle, jt_sample;
    logic signed [15:0] jt_left, jt_right;
    logic pending=0, pending_marker=0, in_flight=0, exhausted=0, check_model=1;
    logic [63:0] edge_expected=0, next_expected=1, active_token=0;
    logic [63:0] pending_token=0, pending_edge=0, accepted_edge=0;
    logic [63:0] seeded_token, seeded_edge;
    logic expected_ready, data_finish;
    logic [31:0] release_phases=0;
    integer wall=0, writes=0, markers=0, delivered=0, blocked=0, replacements=0;
    integer held_deliveries=0, late_deliveries=0, reset_cases=0, busy_receipts=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_media_source dut(.*);

    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>1000000) $fatal(1,"bounded receipt test timeout");
        if (check_model) begin
            if (!reset_n || stream_reset) begin
                if (dev_ready || marker_ready || receipt_valid) $fatal(1,"reset handshake");
                pending=0; in_flight=0; exhausted=0; edge_expected=0; next_expected=1;
            end else begin
                expected_ready=media_enable && !in_flight && (!pending || receipt_ready) &&
                               !exhausted && next_expected!=64'hffffffffffffffff;
                if (dev_ready!==expected_ready || marker_ready!==(expected_ready && !dev_valid))
                    $fatal(1,"admission/reservation mismatch");
                if (receipt_valid!==pending || source_edge!==edge_expected ||
                    operation_token!==next_expected || operation_exhausted!==exhausted)
                    $fatal(1,"receipt/state mismatch");
                if (pending && (receipt_token!==pending_token || receipt_at_edge!==pending_edge ||
                                receipt_marker!==pending_marker))
                    $fatal(1,"receipt position/order mismatch got=%h/%h/%b expected=%h/%h/%b",
                           receipt_token,receipt_at_edge,receipt_marker,pending_token,pending_edge,pending_marker);
                if (pending && !receipt_ready) blocked=blocked+1;
                if (receipt_valid && receipt_ready) begin
                    delivered=delivered+1; pending=0;
                    if (!media_enable) held_deliveries=held_deliveries+1;
                    if (edge_expected>pending_edge+10) late_deliveries=late_deliveries+1;
                    if ((dev_valid && dev_ready) || (marker_valid && marker_ready)) replacements=replacements+1;
                end
                data_finish=media_enable && !dut.jt_wr_n && dut.jt_a0 && dut.cen_p1;
                if (media_enable && (dev_valid || marker_valid) && next_expected==64'hffffffffffffffff)
                    exhausted=1;
                if (dev_valid && dev_ready) begin
                    if (in_flight || pending) $fatal(1,"write without reserved slot");
                    in_flight=1; active_token=next_expected; accepted_edge=edge_expected;
                    next_expected=next_expected+1; writes=writes+1;
                end
                if (marker_valid && marker_ready) begin
                    if (in_flight || pending || dev_valid) $fatal(1,"marker overtook write/receipt");
                    pending=1; pending_marker=1; pending_token=next_expected; pending_edge=edge_expected;
                    next_expected=next_expected+1; markers=markers+1;
                end
                if (data_finish) begin
                    if (!in_flight || pending || edge_expected<=accepted_edge) $fatal(1,"unexpected native data pulse");
                    pending=1; pending_marker=0; pending_token=active_token; pending_edge=edge_expected;
                    in_flight=0; release_phases[dut.sound.cycles]=1;
                end
                if (media_enable) edge_expected=edge_expected+1;
            end
            #1;
            if (!reset_n || stream_reset) begin
                if (receipt_valid || operation_exhausted || operation_token!==1 || source_edge!==0)
                    $fatal(1,"reset retained operation state");
            end else begin
                if (receipt_valid!==pending || operation_exhausted!==exhausted ||
                    source_edge!==edge_expected || operation_token!==next_expected)
                    $fatal(1,"post-edge operation state mismatch");
                if (pending && (receipt_token!==pending_token || receipt_at_edge!==pending_edge ||
                                receipt_marker!==pending_marker)) $fatal(1,"receipt position/order mismatch after edge");
                if (data_finish) begin
                    if (!dut.jt_dout[7]) $fatal(1,"receipt did not coincide with native busy latch");
                    busy_receipts=busy_receipts+1;
                end
            end
        end
    end

    task automatic native_reset;
        @(negedge clk_audio); #1;
        dev_valid=0; marker_valid=0; receipt_ready=0; stream_reset=1; media_enable=0;
        repeat(2048) @(negedge clk_audio);
        #1; stream_reset=0; media_enable=1;
        reset_cases=reset_cases+1;
    endtask
    task automatic hold_edges(input integer count);
        logic [127:0] saved;
        @(negedge clk_audio); #1; media_enable=0;
        saved={dut.cen_accum,dut.cen,dut.cen_p1,dut.cen_phase,dut.state,
               dut.jt_wr_n,dut.jt_a0,dut.jt_din,dut.sound.cycles,source_edge};
        repeat(count) begin
            @(negedge clk_audio); #1;
            if ({dut.cen_accum,dut.cen,dut.cen_p1,dut.cen_phase,dut.state,
                 dut.jt_wr_n,dut.jt_a0,dut.jt_din,dut.sound.cycles,source_edge}!==saved)
                $fatal(1,"notification advanced held native state");
        end
    endtask
    task automatic offer_marker;
        @(negedge clk_audio); #1; marker_valid=1;
        do @(posedge clk_audio); while (!marker_ready);
        @(negedge clk_audio); #1; marker_valid=0;
    endtask
    task automatic seed_counters(input logic [63:0] token, position);
        hold_edges(2); check_model=0;
        seeded_token=token; seeded_edge=position;
        force dut.operation_token=seeded_token;
        force dut.source_edge=seeded_edge;
        @(negedge clk_audio); #1;
        release dut.operation_token; release dut.source_edge;
        next_expected=token; edge_expected=position; check_model=1;
    endtask
    initial begin
        repeat(4) @(negedge clk_audio); #1; reset_n=1;
        for (integer start_phase=0;start_phase<32;start_phase=start_phase+1) begin
            native_reset();
            do begin @(negedge clk_audio); #1; end
            while (!dut.cen_p1 || dut.sound.cycles!=start_phase);
            dev_address=8'h60+start_phase; dev_value=start_phase+1;
            dev_valid=1; marker_valid=1;
            @(posedge clk_audio);
            if (!dev_ready || marker_ready) $fatal(1,"simultaneous write priority");
            @(negedge clk_audio); #1; dev_valid=0;
            wait(receipt_valid); repeat(17) @(negedge clk_audio);
            hold_edges(19); receipt_ready=1;
            hold_edges(3);
            if (receipt_valid || marker_ready) $fatal(1,"held receipt did not drain or marker advanced");
            media_enable=1;
            @(posedge clk_audio);
            if (!marker_ready) $fatal(1,"marker did not resume");
            @(negedge clk_audio); #1; marker_valid=0;
            // Back-to-back marker receipts replace consumed receipts on each edge.
            marker_valid=1;
            repeat(4) @(negedge clk_audio);
            #1; marker_valid=0;
            repeat(2) @(negedge clk_audio);
        end
        if (release_phases!==32'hffffffff) $fatal(1,"missing data-release phases: %h",release_phases);
        // Reset every in-flight bus phase, including while physically held.
        for (integer bus_state=1;bus_state<=4;bus_state=bus_state+1) begin
            native_reset(); receipt_ready=1;
            @(negedge clk_audio); #1; dev_valid=1; dev_address=8'h08; dev_value=8'h78;
            do @(posedge clk_audio); while (!dev_ready);
            @(negedge clk_audio); #1; dev_valid=0;
            while (dut.state!=bus_state) begin @(negedge clk_audio); #1; end
            media_enable=0;
            hold_edges(7);
            native_reset();
        end
        // Reset a blocked completed notification as well as an unfinished write.
        receipt_ready=0; offer_marker();
        if (!receipt_valid) $fatal(1,"missing reset fixture receipt");
        native_reset();
        receipt_ready=1;
        seed_counters(64'hfffffffffffffffd,64'h8000000100000000);
        media_enable=1; offer_marker();
        // The last valid token traverses the in-flight write register as well
        // as the receipt slot; both paths must preserve all 64 bits.
        @(negedge clk_audio); #1; dev_valid=1; dev_address=8'h60; dev_value=8'h17;
        do @(posedge clk_audio); while (!dev_ready);
        @(negedge clk_audio); #1; dev_valid=0;
        wait(receipt_valid);
        @(negedge clk_audio); #1;
        if (receipt_token!==64'hfffffffffffffffe || receipt_marker || receipt_at_edge[63:32]!==32'h80000001)
            $fatal(1,"wide write receipt truncated");
        repeat(3) @(negedge clk_audio);
        #1;
        if (operation_token!==64'hffffffffffffffff || operation_exhausted)
            $fatal(1,"last valid tokens lost or exhausted without an offer");
        hold_edges(2); marker_valid=1;
        hold_edges(9);
        if (operation_exhausted) $fatal(1,"offered operation progressed during hold");
        media_enable=1;
        @(negedge clk_audio); #1;
        if (!operation_exhausted || marker_ready || dev_ready || operation_token!==64'hffffffffffffffff)
            $fatal(1,"token exhaustion wrapped or accepted");
        repeat(5) @(negedge clk_audio);
        native_reset(); receipt_ready=1; offer_marker();
        if (receipt_token!==1 || !receipt_marker) $fatal(1,"fresh stream reused exhausted identity");
        repeat(2) @(negedge clk_audio);
        if (held_deliveries!=32 || late_deliveries!=32 || replacements<128 || busy_receipts!=33 ||
            blocked<32*17 || reset_cases!=42 || writes!=37 || markers!=163 || delivered!=195)
            $fatal(1,"missing receipt coverage writes=%0d markers=%0d delivered=%0d blocked=%0d replacement=%0d held=%0d late=%0d reset=%0d busy=%0d",
                   writes,markers,delivered,blocked,replacements,held_deliveries,late_deliveries,reset_cases,busy_receipts);
        $display("jt51_source_receipt_tb: PASS writes=%0d markers=%0d delivered=%0d held=%0d reset=%0d phases=%h",
                 writes,markers,delivered,held_deliveries,reset_cases,release_phases);
        $finish;
    end
endmodule
