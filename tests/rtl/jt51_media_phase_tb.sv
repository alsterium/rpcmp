`timescale 1ns/1ps
// Independent closed-form strobe equation, checked against the pinned native RTL.
module jt51_media_phase_tb;
    logic clk_audio=0, reset_n=0, stream_reset=1, pause_request=1, clear_audio_flags=0;
    logic dev_valid=0, dev_ready, device_idle, media_enable, paused, frame_boundary;
    logic [7:0] dev_address=8'h60, dev_value=8'h17;
    logic audio_mclk, audio_lrck, audio_dac, audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;
    logic started=0;
    longint unsigned edge_index=0, samples=0, selected=0, frames=0;
    longint unsigned initial_accum, initial_phase, initial_pulse, expected_edge, k, lead;
    longint unsigned min_lead=256, max_lead=0;
    integer total_samples=0, total_selected=0, total_frames=0, held_edges=0, cases=0;
    integer wall=0, writes=0;
    logic [2:0] initial_states=0;
    always #5 clk_audio=~clk_audio;
    rpcmp_jt51_media_audio dut(.*);

    always @(posedge clk_audio) begin
        wall=wall+1;
        if (wall>3000000) $fatal(1,"bounded phase probe timeout");
        if (!reset_n || stream_reset) begin
            started=0; edge_index=0; samples=0; selected=0; frames=0;
        end else begin
            if (dut.source_media.source_edge!==edge_index) $fatal(1,"retained clock mismatch");
            if (media_enable) begin
                if (!started) begin
                    started=1;
                    initial_accum=dut.source_media.cen_accum;
                    initial_phase=dut.source_media.cen_phase;
                    initial_pulse=dut.source_media.cen_p1;
                    if ($isunknown({initial_accum,initial_phase,initial_pulse}) || initial_accum>=12288000 ||
                        (initial_pulse && (initial_phase || initial_accum>=3579545)))
                        $fatal(1,"invalid initial native clock state");
                    initial_states[initial_pulse ? 2 : initial_phase]=1;
                end
                if (frame_boundary) begin
                    if (edge_index!=256*frames) $fatal(1,"media-frame/source-edge alignment");
                    frames=frames+1; total_frames=total_frames+1;
                end
                if (dut.jt_sample) begin
                    samples=samples+1; total_samples=total_samples+1;
                    expected_edge=(((64*samples+2-2*initial_pulse-initial_phase)*12288000-
                                    initial_accum)+3579544)/3579545;
                    if (edge_index!==expected_edge)
                        $fatal(1,"native strobe equation case=%0d j=%0d actual=%0d expected=%0d A=%0d C=%0d p=%0d",
                               cases,samples,edge_index,expected_edge,initial_accum,initial_phase,initial_pulse);
                    k=samples*3072000/3579545;
                    if (k!=selected) begin
                        if (k!=selected+1 || edge_index<256*k || edge_index>256*(k+1)-29)
                            $fatal(1,"selected-sample lead bound");
                        lead=256*(k+1)-edge_index;
                        if (lead<min_lead) min_lead=lead;
                        if (lead>max_lead) max_lead=lead;
                        selected=k; total_selected=total_selected+1;
                    end
                end
                edge_index=edge_index+1;
            end else if (started) held_edges=held_edges+1;
            if (dev_valid && dev_ready) writes=writes+1;
            if (audio_underflow || audio_overflow) $fatal(1,"native converter scheduling fault");
        end
        #1;
        if (reset_n && !stream_reset && selected_count!==32'(selected))
            $fatal(1,"actual converter selection differs from the closed-form ordinal");
    end

    initial begin
        repeat(4) @(negedge clk_audio); #1; reset_n=1;
        for (integer trial=0;trial<32;trial=trial+1) begin
            @(negedge clk_audio); #1;
            cases=trial; stream_reset=1; pause_request=1; dev_valid=0;
            repeat(2048+trial*7) @(negedge clk_audio);
            #1; stream_reset=0;
            repeat(trial*3+1) @(negedge clk_audio);
            #1; pause_request=0; dev_valid=1;
            wait(samples==128);
            @(negedge clk_audio); #1; pause_request=1;
            wait(paused);
            repeat(3*256+trial) @(negedge clk_audio);
            #1; pause_request=0;
            wait(samples==256);
            @(negedge clk_audio); #1; pause_request=1; dev_valid=0;
            wait(paused);
        end
        if (initial_states!==3'b111 || total_samples!=8192 || total_selected!=32*219 ||
            held_edges<32*3*256 || writes<1000 || min_lead<29 || max_lead>256)
            $fatal(1,"phase probe coverage states=%b samples=%0d selected=%0d held=%0d writes=%0d",
                   initial_states,total_samples,total_selected,held_edges,writes);
        $display("jt51_media_phase_tb: PASS samples=%0d selected=%0d frames=%0d held=%0d writes=%0d states=%b lead=%0d..%0d",
                 total_samples,total_selected,total_frames,held_edges,writes,initial_states,min_lead,max_lead);
        $finish;
    end
endmodule
