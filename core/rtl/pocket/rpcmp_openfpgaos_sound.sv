module rpcmp_openfpgaos_sound (
    input logic cpu_clk,
    input logic audio_clk,
    input logic cpu_reset_n,
    input logic audio_reset_n,
    input logic [31:0] mmio_addr,
    input logic [31:0] mmio_wr_data,
    input logic mmio_rd,
    input logic mmio_wr,
    output logic [31:0] mmio_rd_data,
    output logic audio_mclk,
    output logic audio_lrck,
    output logic audio_dac,
    output logic sound_fault
);
    localparam logic [31:0] QUEUE_BASE = 32'h4000_0200;
    localparam logic [31:0] RESET_BASE = 32'h4000_0240;
    logic [31:0] queue_rd_data, reset_rd_data;
    logic queue_select, reset_select, audio_status_select;
    logic sound_reset_cpu_n, sound_reset_audio_n;
    logic queue_cpu_reset_n, queue_audio_reset_n;
    logic dev_valid, dev_ready, device_idle;
    logic [1:0] dev_kind;
    logic [7:0] dev_address, dev_value;
    logic audio_underflow, audio_overflow, audio_clipped;
    logic [31:0] selected_count, frame_count;
    logic [3:0] audio_status_meta, audio_status_sync;

    always_comb begin
        queue_select = (mmio_addr >= QUEUE_BASE) && (mmio_addr < RESET_BASE);
        audio_status_select = (mmio_addr == RESET_BASE + 32'h10);
        reset_select = (mmio_addr >= RESET_BASE) && (mmio_addr < RESET_BASE + 32'h20) &&
                       !audio_status_select;
        mmio_rd_data = 0;
        if (queue_select)
            mmio_rd_data = queue_rd_data;
        else if (audio_status_select)
            mmio_rd_data = {28'd0, audio_status_sync};
        else if (reset_select)
            mmio_rd_data = reset_rd_data;
        sound_fault = audio_status_sync[2:0] != 0;
    end

    assign queue_cpu_reset_n = cpu_reset_n && sound_reset_cpu_n;
    assign queue_audio_reset_n = audio_reset_n && sound_reset_audio_n;

    always_ff @(posedge cpu_clk or negedge queue_cpu_reset_n) begin
        if (!queue_cpu_reset_n) begin
            audio_status_meta <= 0;
            audio_status_sync <= 0;
        end else begin
            audio_status_meta <= {device_idle, audio_clipped, audio_overflow, audio_underflow};
            audio_status_sync <= audio_status_meta;
        end
    end

    rpcmp_sound_reset reset_control (
        .cpu_clk(cpu_clk), .audio_clk(audio_clk),
        .cpu_reset_n(cpu_reset_n), .audio_reset_n(audio_reset_n),
        .mmio_addr(mmio_addr), .mmio_wr_data(mmio_wr_data),
        .mmio_rd(mmio_rd && reset_select), .mmio_wr(mmio_wr && reset_select),
        .mmio_rd_data(reset_rd_data),
        .sound_reset_cpu_n(sound_reset_cpu_n),
        .sound_reset_audio_n(sound_reset_audio_n)
    );

    rpcmp_device_queue queue (
        .cpu_clk(cpu_clk), .dev_clk(audio_clk),
        .cpu_reset_n(queue_cpu_reset_n), .dev_reset_n(queue_audio_reset_n),
        .mmio_addr(mmio_addr), .mmio_wr_data(mmio_wr_data),
        .mmio_rd(mmio_rd && queue_select), .mmio_wr(mmio_wr && queue_select),
        .mmio_rd_data(queue_rd_data),
        .dev_valid(dev_valid), .dev_ready(dev_ready), .dev_kind(dev_kind),
        .dev_address(dev_address), .dev_value(dev_value)
    );

    rpcmp_jt51_audio device (
        .clk_audio(audio_clk), .reset_n(queue_audio_reset_n), .clear_audio_flags(1'b0),
        .dev_valid(dev_valid), .dev_ready(dev_ready), .dev_kind(dev_kind),
        .dev_address(dev_address), .dev_value(dev_value),
        .audio_mclk(audio_mclk), .audio_lrck(audio_lrck), .audio_dac(audio_dac),
        .audio_underflow(audio_underflow), .audio_overflow(audio_overflow),
        .audio_clipped(audio_clipped), .device_idle(device_idle),
        .selected_count(selected_count), .frame_count(frame_count)
    );
endmodule
