module rpcmp_jt51_audio (
    input  logic clk_audio,
    input  logic reset_n,
    input  logic clear_audio_flags,
    input  logic dev_valid,
    output logic dev_ready,
    input  logic [1:0] dev_kind,
    input  logic [7:0] dev_address,
    input  logic [7:0] dev_value,
    output logic audio_mclk,
    output logic audio_lrck,
    output logic audio_dac,
    output logic audio_underflow,
    output logic audio_overflow,
    output logic audio_clipped,
    output logic [31:0] selected_count,
    output logic [31:0] frame_count
);
    localparam logic [2:0] IDLE=0, RESET_HOLD=1, WAIT_ADDR=2,
                           HOLD_ADDR=3, WAIT_DATA=4, HOLD_DATA=5;
    logic [2:0] state;
    logic [7:0] reset_count;
    logic [7:0] command_address, command_value;
    logic [24:0] cen_accum;
    logic cen, cen_p1, cen_phase;
    logic [25:0] cen_sum;
    logic jt_wr_n, jt_a0;
    logic [7:0] jt_din, jt_dout;
    logic jt_sample;
    logic signed [15:0] jt_left, jt_right;
    logic jt_reset;

    assign cen_sum = {1'b0,cen_accum}+26'd3579545;
    assign jt_reset = !reset_n || state==RESET_HOLD;
    assign dev_ready = state==IDLE && (dev_kind==0 || dev_kind==1);

    always_ff @(posedge clk_audio or negedge reset_n) begin
        if(!reset_n) begin
            state<=IDLE; reset_count<=0; command_address<=0; command_value<=0;
            cen_accum<=0; cen<=0; cen_p1<=0; cen_phase<=0;
            jt_wr_n<=1; jt_a0<=0; jt_din<=0;
        end else begin
            cen<=0; cen_p1<=0;
            if(cen_sum>=26'd12288000) begin
                cen_accum<=cen_sum-26'd12288000; cen<=1; cen_phase<=~cen_phase;
                if(cen_phase) cen_p1<=1;
            end else cen_accum<=cen_sum[24:0];

            case(state)
                IDLE: if(dev_valid && dev_ready) begin
                    if(dev_kind==0) begin
                        reset_count<=8'hff; state<=RESET_HOLD;
                    end else begin
                        command_address<=dev_address;
                        command_value<=dev_value;
                        state<=WAIT_ADDR;
                    end
                end
                RESET_HOLD: begin
                    if(reset_count==0) state<=IDLE;
                    else reset_count<=reset_count-1'b1;
                end
                WAIT_ADDR: if(!jt_dout[7]) begin
                    jt_a0<=0; jt_din<=command_address; jt_wr_n<=0; state<=HOLD_ADDR;
                end
                HOLD_ADDR: if(cen) begin jt_wr_n<=1; state<=WAIT_DATA; end
                WAIT_DATA: if(!jt_dout[7]) begin
                    jt_a0<=1; jt_din<=command_value; jt_wr_n<=0; state<=HOLD_DATA;
                end
                HOLD_DATA: if(cen) begin jt_wr_n<=1; state<=IDLE; end
                default: state<=IDLE;
            endcase
        end
    end

    jt51 sound (
        .rst(jt_reset),.clk(clk_audio),.cen(cen),.cen_p1(cen_p1),
        .cs_n(1'b0),.wr_n(jt_wr_n),.a0(jt_a0),.din(jt_din),.dout(jt_dout),
        .ct1(),.ct2(),.irq_n(),.sample(jt_sample),.left(jt_left),
        .right(jt_right),.xleft(),.xright()
    );

    rpcmp_pocket_audio pocket_audio (
        .clk_audio(clk_audio),.reset_n(reset_n && state!=RESET_HOLD),
        .clear_flags(clear_audio_flags),.src_valid(jt_sample),
        .src_left({{2{jt_left[15]}},jt_left}),
        .src_right({{2{jt_right[15]}},jt_right}),
        .audio_mclk(audio_mclk),.audio_lrck(audio_lrck),.audio_dac(audio_dac),
        .underflow(audio_underflow),.overflow(audio_overflow),
        .clipped(audio_clipped),.selected_count(selected_count),
        .frame_count(frame_count)
    );
endmodule
