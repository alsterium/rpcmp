// HYB1 audio owner. FIFO heads are show-ahead values in this clock domain.
module rpcmp_hybrid_audio (
    input logic clk_audio, reset_n, clear, start,
    input logic pcm_empty,
    input logic [63:0] pcm_data,
    output logic pcm_pop,
    input logic fm_empty,
    input logic [48:0] fm_data, // {sample[31:0], end, address[7:0], value[7:0]}
    output logic fm_pop,
    output logic running, ended,
    output logic [3:0] faults,
    output logic [31:0] source_count, write_count, max_late_samples,
    output logic audio_mclk, audio_lrck, audio_dac
);
    localparam logic [2:0] IDLE=0, WAIT_ADDR=1, HOLD_ADDR=2, WAIT_DATA=3, HOLD_DATA=4;
    logic [2:0] bus_state;
    logic rendering, draining;
    logic [7:0] serial_phase;
    logic [1:0] startup_frames;
    logic signed [15:0] frame_left, frame_right;
    logic [23:0] cen_accum;
    logic [24:0] cen_sum;
    logic cen, cen_p1, cen_phase;
    logic jt_sample, jt_wr_n, jt_a0;
    logic [7:0] jt_din, jt_dout, command_value;
    wire signed [18:0] fm_left, fm_right;
    wire begin_stream = start && !running && !ended && faults == 0 && !clear &&
                        !pcm_empty && serial_phase == 8'hff;
    wire source_valid = rendering && !clear && faults == 0 && jt_sample && !pcm_empty;
    wire event_due = rendering && !clear && faults == 0 && !fm_empty &&
                     fm_data[48:17] <= source_count;
    wire finish_source = event_due && fm_data[16:0] == 17'h10000 && bus_state == IDLE;
    wire begin_fade = event_due && fm_data[16:0] == 17'h10001 && bus_state == IDLE;
    logic fading;
    logic [31:0] fade_at;
    wire [31:0] fade_offset = source_count - (begin_fade ? fm_data[48:17] : fade_at);
    wire [18:0] fade_remaining = !(fading || begin_fade) ? 19'd312500 :
                                fade_offset >= 312500 ? 19'd0 : 19'd312500 - fade_offset[18:0];
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin fading<=0; fade_at<=0; end
        else if (clear) begin fading<=0; fade_at<=0; end
        else if (begin_fade) begin fading<=1; fade_at<=fm_data[48:17]; end
    end
    assign pcm_pop = source_valid && !finish_source;
    assign fm_pop = event_due && bus_state == IDLE;
    assign running = rendering || draining;

    // The native clock continues during reset/stop so shift-register banks clear.
    assign cen_sum = {1'b0, cen_accum} + 25'd4000000;
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin cen_accum<=0; cen<=0; cen_p1<=0; cen_phase<=0; end
        else if (begin_stream) begin cen_accum<=0; cen<=0; cen_p1<=0; cen_phase<=0; end
        else begin
            cen<=0; cen_p1<=0;
            if (cen_sum >= 25'd12288000) begin
                cen_accum <= cen_sum[23:0] - 24'd12288000;
                cen<=1; cen_phase<=~cen_phase; cen_p1<=cen_phase;
            end else cen_accum <= cen_sum[23:0];
        end
    end

    jt51 sound (
        .rst(!reset_n || clear || !rendering), .clk(clk_audio), .cen(cen), .cen_p1(cen_p1),
        .cs_n(1'b0), .wr_n(jt_wr_n), .a0(jt_a0), .din(jt_din), .dout(jt_dout),
        .ct1(), .ct2(), .irq_n(), .sample(jt_sample), .left(), .right(), .xleft(), .xright(),
        .wide_left(fm_left), .wide_right(fm_right)
    );
    // Hardware IRQ is deliberately unused; the reference CPU timer sequences MDX.
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            bus_state<=IDLE; jt_wr_n<=1; jt_a0<=0; jt_din<=0; command_value<=0;
            write_count<=0; max_late_samples<=0;
        end else if (clear || !running) begin
            bus_state<=IDLE; jt_wr_n<=1; jt_a0<=0; jt_din<=0; command_value<=0;
            if (clear) begin write_count<=0; max_late_samples<=0; end
        end else begin
            case (bus_state)
                IDLE: if (event_due && !fm_data[16]) begin
                    jt_din<=fm_data[15:8]; command_value<=fm_data[7:0];
                    bus_state<=WAIT_ADDR;
                    if (source_count - fm_data[48:17] > max_late_samples)
                        max_late_samples <= source_count - fm_data[48:17];
                end
                WAIT_ADDR: if (!jt_dout[7]) begin jt_a0<=0; jt_wr_n<=0; bus_state<=HOLD_ADDR; end
                HOLD_ADDR: if (cen) begin jt_wr_n<=1; bus_state<=WAIT_DATA; end
                WAIT_DATA: if (!jt_dout[7]) begin
                    jt_a0<=1; jt_din<=command_value; jt_wr_n<=0; bus_state<=HOLD_DATA;
                end
                HOLD_DATA: if (cen_p1) begin
                    jt_wr_n<=1; bus_state<=IDLE; write_count<=write_count+1'b1;
                end
                default: bus_state<=IDLE;
            endcase
        end
    end

    // Select floor(n * 125 / 96): 0,1,2,3,5,... . Carry the decision through
    // the mix pipeline, retaining PCM headroom until after FM gain/limiting.
    logic [31:0] next_source;
    logic [6:0] fraction;
    logic [7:0] fraction_sum;
    logic [3:0] selected_pipe;
    wire mix_valid;
    wire signed [15:0] mix_left, mix_right;
    assign fraction_sum = {1'b0,fraction} + 8'd29;
    rpcmp_hybrid_mixer mixer (
        .clk(clk_audio), .reset_n(reset_n && !clear && faults == 0), .sample_valid(pcm_pop),
        .fm_left(fm_left), .fm_right(fm_right),
        .pcm_left(pcm_data[63:32]), .pcm_right(pcm_data[31:0]),
        .fade_remaining(fade_remaining),
        .mixed_valid(mix_valid), .mixed_left(mix_left), .mixed_right(mix_right)
    );
    logic [31:0] output_queue [0:3];
    logic [1:0] output_rd, output_wr;
    logic [2:0] output_count;
    wire push_output = mix_valid && selected_pipe[3] && running && !clear && faults == 0;
    wire frame_boundary = serial_phase == 8'hff;
    wire pop_output = frame_boundary && running && startup_frames <= 1 && output_count != 0;

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            rendering<=0; draining<=0; ended<=0; faults<=0; source_count<=0;
            next_source<=0; fraction<=0; selected_pipe<=0; startup_frames<=0;
            output_rd<=0; output_wr<=0; output_count<=0;
        end else if (clear) begin
            rendering<=0; draining<=0; ended<=0; faults<=0; source_count<=0;
            next_source<=0; fraction<=0; selected_pipe<=0; startup_frames<=0;
            output_rd<=0; output_wr<=0; output_count<=0;
        end else if (faults != 0) begin
            rendering<=0; draining<=0; selected_pipe<=0; output_count<=0;
        end else if (begin_stream) begin
            rendering<=1; startup_frames<=2;
        end else begin
            selected_pipe <= {selected_pipe[2:0],pcm_pop && source_count == next_source};
            if (pcm_pop) begin
                source_count<=source_count+1'b1;
                if (&source_count || next_source >= 32'hfffffffd) faults[3]<=1;
                if (source_count == next_source) begin
                    if (fraction_sum >= 96) begin
                        fraction<=fraction_sum[6:0]-7'd96; next_source<=next_source+2;
                    end else begin fraction<=fraction_sum[6:0]; next_source<=next_source+1'b1; end
                end
            end
            if (rendering && jt_sample && pcm_empty && !finish_source) faults[0]<=1;
            if (finish_source) begin rendering<=0; draining<=1; end
            if (bus_state == HOLD_DATA && cen_p1 && (&write_count)) faults[3]<=1;
            if (push_output && output_count == 4 && !pop_output) faults[1]<=1;
            if (push_output) begin
                output_queue[output_wr]<={mix_left,mix_right}; output_wr<=output_wr+1'b1;
            end
            if (pop_output) output_rd<=output_rd+1'b1;
            case ({push_output,pop_output})
                2'b10: output_count<=output_count+1'b1;
                2'b01: output_count<=output_count-1'b1;
                default: ;
            endcase
            if (frame_boundary && running) begin
                if (startup_frames != 0) startup_frames<=startup_frames-1'b1;
                if (startup_frames <= 1 && output_count == 0 && !push_output) begin
                    if (draining && selected_pipe == 0) begin draining<=0; ended<=1; end
                    else faults[1]<=1;
                end
            end
        end
    end

    // APF I2S requires one SCLK (four MCLKs) between LRCK and the MSB.
    // Stop never truncates a serialized word. Silence starts at the next frame.
    assign audio_mclk = clk_audio;
    always_comb begin
        audio_lrck=serial_phase[7]; audio_dac=0;
        if (serial_phase[6:2]>=1 && serial_phase[6:2]<=16) begin
            if (!serial_phase[7]) audio_dac=frame_left[16-serial_phase[6:2]];
            else audio_dac=frame_right[16-serial_phase[6:2]];
        end
    end
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin serial_phase<=0; frame_left<=0; frame_right<=0; end
        else begin
            serial_phase<=serial_phase+1'b1;
            if (frame_boundary) begin
                if (!clear && faults == 0 && pop_output)
                    {frame_left,frame_right} <= output_queue[output_rd];
                else begin frame_left<=0; frame_right<=0; end
            end
        end
    end
endmodule
