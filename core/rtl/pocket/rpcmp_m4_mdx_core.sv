module rpcmp_m4_mdx_core #(
    parameter longint unsigned CPU_TICKS_PER_SECOND = 74250000
) (
    input  logic clk_cpu,
    input  logic clk_audio,
    input  logic reset_n,
    output logic audio_mclk,
    output logic audio_lrck,
    output logic audio_dac,
    output logic sequence_enqueued,
    output logic sequence_complete,
    output logic queue_fault,
    output logic audio_fault
);
    localparam logic [31:0] BASE = 32'h4000_0200;
    localparam logic [2:0] WAIT_SPACE = 0, WRITE_LO = 1, WRITE_HI = 2, PUSH = 3, DONE = 4;
    logic [2:0] state;
    logic [5:0] operation_index;
    logic mmio_rd, mmio_wr;
    logic [31:0] mmio_addr, mmio_wr_data, mmio_rd_data;
    logic dev_valid, dev_ready, device_idle;
    logic [1:0] dev_kind;
    logic [7:0] dev_address, dev_value;
    logic audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;
    logic [63:0] operation_tick, operation_due;
    logic [7:0] operation_address, operation_value;

    always_comb begin
        operation_tick = 0;
        operation_address = 0;
        operation_value = 0;
        case (operation_index)
             0: begin operation_tick=0; operation_address=8'h12; operation_value=8'hc8; end
             1: begin operation_tick=0; operation_address=8'h1b; operation_value=8'h02; end
             2: begin operation_tick=0; operation_address=8'h40; operation_value=8'h00; end
             3: begin operation_tick=0; operation_address=8'h48; operation_value=8'h00; end
             4: begin operation_tick=0; operation_address=8'h50; operation_value=8'h00; end
             5: begin operation_tick=0; operation_address=8'h58; operation_value=8'h00; end
             6: begin operation_tick=0; operation_address=8'h60; operation_value=8'h00; end
             7: begin operation_tick=0; operation_address=8'h68; operation_value=8'h00; end
             8: begin operation_tick=0; operation_address=8'h70; operation_value=8'h00; end
             9: begin operation_tick=0; operation_address=8'h78; operation_value=8'h7f; end
            10: begin operation_tick=0; operation_address=8'h80; operation_value=8'h00; end
            11: begin operation_tick=0; operation_address=8'h88; operation_value=8'h00; end
            12: begin operation_tick=0; operation_address=8'h90; operation_value=8'h00; end
            13: begin operation_tick=0; operation_address=8'h98; operation_value=8'h00; end
            14: begin operation_tick=0; operation_address=8'ha0; operation_value=8'h00; end
            15: begin operation_tick=0; operation_address=8'ha8; operation_value=8'h00; end
            16: begin operation_tick=0; operation_address=8'hb0; operation_value=8'h00; end
            17: begin operation_tick=0; operation_address=8'hb8; operation_value=8'h00; end
            18: begin operation_tick=0; operation_address=8'hc0; operation_value=8'h00; end
            19: begin operation_tick=0; operation_address=8'hc8; operation_value=8'h00; end
            20: begin operation_tick=0; operation_address=8'hd0; operation_value=8'h00; end
            21: begin operation_tick=0; operation_address=8'hd8; operation_value=8'h00; end
            22: begin operation_tick=0; operation_address=8'he0; operation_value=8'h00; end
            23: begin operation_tick=0; operation_address=8'he8; operation_value=8'h00; end
            24: begin operation_tick=0; operation_address=8'hf0; operation_value=8'h00; end
            25: begin operation_tick=0; operation_address=8'hf8; operation_value=8'h00; end
            26: begin operation_tick=0; operation_address=8'h20; operation_value=8'hc0; end
            27: begin operation_tick=0; operation_address=8'h30; operation_value=8'h14; end
            28: begin operation_tick=0; operation_address=8'h28; operation_value=8'h00; end
            29: begin operation_tick=0; operation_address=8'h78; operation_value=8'h15; end
            30: begin operation_tick=0; operation_address=8'h08; operation_value=8'h78; end
            31: begin operation_tick=0; operation_address=8'h20; operation_value=8'h55; end
            32: begin operation_tick=2064; operation_address=8'h08; operation_value=8'h00; end
            default: begin operation_tick=0; operation_address=0; operation_value=0; end
        endcase
        operation_due = 0;
        if (operation_index == 32)
            operation_due = CPU_TICKS_PER_SECOND * 43 / 1000;

        mmio_rd = 0;
        mmio_wr = 0;
        mmio_addr = BASE + 8;
        mmio_wr_data = 0;
        case (state)
            WAIT_SPACE: begin mmio_rd=1; mmio_addr=BASE+8; end
            WRITE_LO: begin mmio_wr=1; mmio_addr=BASE+32'h18; mmio_wr_data=operation_due[31:0]; end
            WRITE_HI: begin mmio_wr=1; mmio_addr=BASE+32'h1c; mmio_wr_data=operation_due[63:32]; end
            PUSH: begin
                mmio_wr=1; mmio_addr=BASE+32'h20;
                mmio_wr_data=32'h8001_0000|{16'd0,operation_address,operation_value};
            end
            default: begin mmio_rd=1; mmio_addr=BASE+8; end
        endcase
        queue_fault = mmio_rd_data[10] || mmio_rd_data[9];
        sequence_complete = sequence_enqueued && mmio_rd_data[3:0] == 0 &&
                            !mmio_rd_data[11] && device_idle;
        audio_fault = audio_underflow || audio_overflow || audio_clipped;
    end

    always_ff @(posedge clk_cpu or negedge reset_n) begin
        if (!reset_n) begin
            state <= WAIT_SPACE;
            operation_index <= 0;
            sequence_enqueued <= 0;
        end else case (state)
            WAIT_SPACE: if (!mmio_rd_data[8]) state <= WRITE_LO;
            WRITE_LO: state <= WRITE_HI;
            WRITE_HI: state <= PUSH;
            PUSH: if (operation_index == 32) begin
                sequence_enqueued <= 1;
                state <= DONE;
            end else begin
                operation_index <= operation_index + 1'b1;
                state <= WAIT_SPACE;
            end
            default: state <= DONE;
        endcase
    end

    rpcmp_device_queue queue (
        .cpu_clk(clk_cpu), .dev_clk(clk_audio), .reset_n(reset_n),
        .mmio_rd(mmio_rd), .mmio_wr(mmio_wr), .mmio_addr(mmio_addr),
        .mmio_wr_data(mmio_wr_data), .mmio_rd_data(mmio_rd_data),
        .dev_valid(dev_valid), .dev_ready(dev_ready), .dev_kind(dev_kind),
        .dev_address(dev_address), .dev_value(dev_value)
    );
    rpcmp_jt51_audio device (
        .clk_audio(clk_audio), .reset_n(reset_n), .clear_audio_flags(1'b0),
        .dev_valid(dev_valid), .dev_ready(dev_ready), .dev_kind(dev_kind),
        .dev_address(dev_address), .dev_value(dev_value),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac),
        .audio_underflow(audio_underflow), .audio_overflow(audio_overflow),
        .audio_clipped(audio_clipped), .device_idle(device_idle),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
