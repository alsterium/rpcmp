`timescale 1ps/1ps
module hybrid_stream_tb;
    logic clk_cpu=0, clk_audio=0, reset_n=0;
    logic read=0, write=0;
    logic [9:0] address=0;
    logic [3:0] byte_enable=15;
    logic [31:0] write_data=0;
    wire [31:0] read_data;
    wire error, audio_mclk, audio_lrck, audio_dac;
    integer phase=0, checked=0, expected_frames=0, observed=0, native_writes=0;
    integer bit_cycle=0;
    logic channel=0, synchronized=0, compare_audio=0, compare_fm=0;
    logic voice_mode=0;
    integer voice_left[0:999], voice_right[0:999], wide_peak=0;
    logic [15:0] serial_left=0, serial_right=0;
    rpcmp_hybrid_mmio dut(.*);
    function automatic integer reference_mix(input integer f, p);
        longint value;
        begin
            value=f;
            if (value>65535) value=65535;
            if (value< -65536) value=-65536;
            value=(value*8211) >>> 14;
            if (value>32767) value=32767;
            if (value< -32768) value=-32768;
            value=value+p;
            if (value>32767) value=32767;
            if (value< -32768) value=-32768;
            reference_mix=value;
        end
    endfunction
    always @(posedge clk_audio) if (voice_mode && dut.audio.pcm_pop) begin
        if ($isunknown({dut.audio.fm_left,dut.audio.fm_right,dut.pcm_head})) $fatal(1,"unknown source");
        voice_left[dut.audio.source_count]=reference_mix(dut.audio.fm_left,40000);
        voice_right[dut.audio.source_count]=reference_mix(dut.audio.fm_right,-40000);
        if ($signed(dut.audio.fm_left)>wide_peak) wide_peak=$signed(dut.audio.fm_left);
        if (-$signed(dut.audio.fm_left)>wide_peak) wide_peak=-$signed(dut.audio.fm_left);
    end
    always @(posedge clk_audio) if (compare_fm && dut.audio.bus_state == 4 && dut.audio.cen_p1) begin
        if (dut.audio.jt_din !== (native_writes & 255)) $fatal(1,"FM order/value");
        if (native_writes >= 100 && dut.audio.source_count < 400) $fatal(1,"early FM write");
        native_writes=native_writes+1;
    end
    always #5556 clk_cpu=~clk_cpu;
    initial begin
        if (!$value$plusargs("PHASE_PS=%d",phase)) phase=0;
        #(phase);
        forever #40690 clk_audio=~clk_audio;
    end
    // Decode external pins, including all padding bits and channel order.
    always @(negedge clk_audio) if (reset_n) begin
        if (audio_lrck !== channel) begin
            if (synchronized && bit_cycle != 128) $fatal(1,"noncontinuous LRCK");
            channel=audio_lrck; bit_cycle=0;
            if (!channel) synchronized=1;
        end
        if (synchronized) begin
            if (bit_cycle < 64 && bit_cycle % 4 == 1) begin
                if (!channel) serial_left={serial_left[14:0],audio_dac};
                else serial_right={serial_right[14:0],audio_dac};
            end
            if (bit_cycle >= 64 && audio_dac !== 0) $fatal(1,"nonzero padding");
            if (channel && bit_cycle == 127) begin
                observed=observed+1;
                if (compare_audio && (checked != 0 || serial_left !== 0 || serial_right !== 0)) begin
                    if (checked < expected_frames) begin
                        if ($signed(serial_left) !== (voice_mode ? voice_left[checked*125/96] : 1000 + checked*125/96) ||
                            $signed(serial_right) !== (voice_mode ? voice_right[checked*125/96] : -1000 - checked*125/96))
                            $fatal(1,"frame %0d got %0d,%0d expected source %0d",checked,
                                   $signed(serial_left),$signed(serial_right),checked*125/96);
                        checked=checked+1;
                    end else if (serial_left !== 0 || serial_right !== 0)
                        $fatal(1,"audio beyond EOF");
                end
            end
            bit_cycle=bit_cycle+1;
        end
    end
    task automatic wr(input logic [9:0] a, input logic [31:0] v, input logic reject=0,
                      input logic [3:0] lanes=15);
        @(negedge clk_cpu); address=a; write_data=v; write=1; byte_enable=lanes;
        #1; if (error !== reject) $fatal(1,"write %h=%h reject %b got %b",a,v,reject,error);
        @(negedge clk_cpu); write=0; byte_enable=15;
    endtask
    task automatic rd(input logic [9:0] a, output logic [31:0] v);
        @(negedge clk_cpu); address=a; read=1;
        #1; if (error) $fatal(1,"read rejected"); v=read_data;
        @(negedge clk_cpu); read=0;
    endtask
    task automatic ready;
        logic [31:0] status;
        do begin repeat(64) @(negedge clk_cpu); rd(4,status); end while (!status[0]);
        if (status != 1) $fatal(1,"clear left status %h",status);
    endtask
    task automatic fm(input integer at_sample, value);
        wr('h20,at_sample); wr('h24,value);
    endtask
    task automatic pcm(input integer value);
        wr('h10,value); wr('h14,-value);
    endtask
    task automatic normal(input integer frames, event_count);
        logic [31:0] status;
        checked=0; expected_frames=(frames*96+124)/125; compare_audio=1;
        native_writes=0; compare_fm=1;
        for (int i=0;i<event_count;i=i+1) fm(i<100 ? 0 : 400,'h1b00 | (i & 255));
        fm(frames,'h10000);
        for (int i=0;i<frames;i=i+1) pcm(1000+i);
        wr(8,2);
        do begin repeat(128) @(negedge clk_cpu); rd(4,status); end while (!status[3] && status[7:4]==0);
        if (status[7:4] != 0) $fatal(1,"normal playback fault %h source=%0d",status,dut.audio.source_count);
        repeat(768) @(negedge clk_audio);
        if (checked != expected_frames || dut.audio.source_count != frames ||
            dut.audio.write_count != event_count || native_writes != event_count)
            $fatal(1,"lost frames/events: frames=%0d source=%0d writes=%0d",checked,
                   dut.audio.source_count,dut.audio.write_count);
        if (event_count>0 && dut.audio.max_late_samples == 0) $fatal(1,"dense delay unobserved");
        compare_audio=0; compare_fm=0;
    endtask
    initial begin
        logic [31:0] value;
        logic [15:0] stop_left, stop_right;
        integer stopped_frame;
        repeat(10) @(negedge clk_cpu); reset_n=1;
        ready(); rd(0,value); if (value != 'h48594231) $fatal(1,"wrong ID");
        if ($test$plusargs("VOICE")) begin
            // Real eight-channel native synthesis plus PCM outside int16.
            voice_mode=1; compare_audio=1; expected_frames=768;
            for (int ch=0;ch<8;ch=ch+1) begin
                fm(0,((32+ch)<<8) | 'hc7); fm(0,((40+ch)<<8) | (48+ch));
                for (int op=0;op<4;op=op+1) begin
                    fm(0,((64+op*8+ch)<<8) | (1+op)); fm(0,(96+op*8+ch)<<8);
                    fm(0,((128+op*8+ch)<<8) | 31); fm(0,(160+op*8+ch)<<8);
                    fm(0,(192+op*8+ch)<<8); fm(0,((224+op*8+ch)<<8) | 15);
                end
            end
            for (int ch=0;ch<8;ch=ch+1) fm(0,'h0878 | ch);
            fm(1000,'h10000);
            for (int i=0;i<1000;i=i+1) pcm(40000);
            wr(8,2);
            do begin repeat(128) @(negedge clk_cpu); rd(4,value); end while (!value[3] && value[7:4]==0);
            repeat(512) @(negedge clk_audio);
            if (value[7:4] != 0 || checked!=768 || wide_peak<=65535 || dut.audio.write_count!=216)
                $fatal(1,"voice status=%h frames=%0d peak=%0d writes=%0d",value,checked,wide_peak,dut.audio.write_count);
            $display("hybrid_stream_tb: VOICE PASS phase=%0d frames=768 writes=216 peak=%0d",phase,wide_peak);
            $finish;
        end
        wr(8,2,1); wr('h14,7,1); wr('h24,7,1);
        wr('h10,123,1,3); wr('h11,123,1); wr('h30,1,1); wr('h110,123,1);
        wr('h20,10); wr('h24,'h10001,1); wr('h24,'h20000,1); wr('h24,0);
        wr('h20,9); wr('h24,0,1); // nonmonotonic, staged value retained
        wr(8,1); ready(); wr('h14,7,1); wr('h24,7,1);
        normal(1000,200);
        wr(8,2,1); wr(8,1); ready();

        // Both FIFOs at exact capacity, failed commits must retain their stages.
        for (int i=0;i<1024;i=i+1) fm(0,'h1b00);
        wr('h20,0); wr('h24,'h1b01,1); rd('h28,value);
        if (value != 0) $fatal(1,"FM full wraps capacity");
        for (int i=0;i<4096;i=i+1) pcm(1000+i);
        wr('h10,9999); wr('h14,-9999,1); rd('h18,value);
        if (value != 0) $fatal(1,"PCM full wraps capacity");
        wr(8,2);
        repeat(600) @(negedge clk_audio);
        wr('h24,'h1b01); wr('h14,-9999);
        if (dut.left_valid || dut.at_valid) $fatal(1,"retry did not consume stage");
        // Clear during native traffic and partway through a serialized word.
        wait(dut.audio.bus_state == 4);
        wait(dut.audio.serial_phase == 31);
        stop_left=dut.audio.frame_left; stop_right=dut.audio.frame_right; stopped_frame=observed;
        wr(8,1);
        wait(observed>stopped_frame);
        if (serial_left !== stop_left || serial_right !== stop_right) $fatal(1,"stop tore serial word");
        ready();
        repeat(512) @(negedge clk_audio);
        if (serial_left !== 0 || serial_right !== 0) $fatal(1,"stop not silent");
        normal(126,0);
        wr(8,1); ready();

        // Unmarked exhaustion is a sticky failure, not EOF or implicit retry.
        pcm(1000); pcm(1001); wr(8,2);
        repeat(1800) @(negedge clk_audio); rd(4,value);
        if (value[7:4] != 1 || value[3:2] != 0) $fatal(1,"starvation status %h",value);
        if (serial_left !== 0 || serial_right !== 0) $fatal(1,"fault not silent");
        wr(8,2,1); wr('h10,1,1); wr(8,1); ready(); normal(125,0);
        wr(8,1); ready();
        // Stream beyond FIFO capacity with deliberately irregular CPU stalls.
        checked=0; expected_frames=4608; compare_audio=1;
        fm(6000,'h10000);
        for (int i=0;i<2000;i=i+1) pcm(1000+i);
        wr(8,2);
        for (int i=2000;i<6000;i=i+1) begin
            do begin rd('h18,value); if (value==0) repeat(75) @(negedge clk_cpu); end while (value==0);
            pcm(1000+i);
            if (i % 499 == 0) repeat(3200) @(negedge clk_cpu);
        end
        do begin repeat(128) @(negedge clk_cpu); rd(4,value); end while (!value[3] && value[7:4]==0);
        if (value[7:4] != 0) $fatal(1,"refill fault %h",value);
        repeat(768) @(negedge clk_audio);
        if (checked != expected_frames) $fatal(1,"refill audio lost");
        compare_audio=0;
        $display("hybrid_stream_tb: PASS phase=%0d normal_frames=5569 fifo_full=2 starvation=1",phase);
        $finish;
    end
    initial begin #(64'd200000000000); $fatal(1,"timeout"); end
endmodule
