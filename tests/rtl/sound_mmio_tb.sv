`timescale 1ns/1ps
module sound_mmio_tb;
    logic cpu_clk=0, audio_clk=0, cpu_reset_n=0, audio_reset_n=0, device_fault=0;
    logic [31:0] mmio_addr=0, mmio_wr_data=0, mmio_rd_data;
    logic [3:0] byte_enable=15;
    logic mmio_rd=0, mmio_wr=0, mmio_error, sound_fault, audio_mclk, audio_lrck, audio_dac;
    rpcmp_sound_mmio dut(.*);
    always #5.556 cpu_clk=~cpu_clk;
    integer phase_ps=0;
    initial begin
        if (!$value$plusargs("PHASE_PS=%d",phase_ps)) phase_ps=0;
        #(phase_ps*0.001);
        forever #40.690 audio_clk=~audio_clk;
    end
    localparam logic [31:0] BASE=32'h40000400;
    localparam logic [63:0] GEN=64'h1122334455660001, TICKET=64'hfedcba9876543210;
    logic [63:0] next_id=64'h1234567800000001, exp_id, exp_gen, exp_revision;
    logic [2:0] exp_kind;
    logic exp_enabled;
    logic [31:0] exp_target;
    logic [31:0] words[0:32], saved[0:32], value;
    logic [63:0] epoch=0, prepared=0, ack_frame=0, pause_frame=0;
    integer controls=0, feeds=0, captures=0, reset_cases=0, nonzero_bits=0;
    integer native_writes=0, emergency_deliveries=0;
    logic check_tone=0;
    logic [7:0] native_address=0;
    logic [63:0] offered_gen, offered_feed_epoch, offered_at, offered_until, offered_loops;
    logic [1:0] offered_flags;
    logic [15:0] offered_bytes, native_expected;
    integer selected_channel=0;
    wire selected_request=selected_channel==0 ? dut.control_a_valid : selected_channel==1 ? dut.feed_a_valid : dut.capture_a_valid;
    wire selected_reply=selected_channel==0 ? dut.control_r_valid : selected_channel==1 ? dut.feed_r_valid : dut.capture_r_valid;
    wire selected_visible=selected_channel==0 ? dut.control_valid : selected_channel==1 ? dut.feed_valid : dut.capture_valid;
    wire selected_ack=selected_channel==0 ? dut.control_mailbox.ack_toggle!=dut.control_mailbox.ack_sync :
        selected_channel==1 ? dut.feed_mailbox.ack_toggle!=dut.feed_mailbox.ack_sync :
        dut.capture_mailbox.ack_toggle!=dut.capture_mailbox.ack_sync;
    realtime control_at=0, feed_at=0, capture_at=0;
    real control_limit=0;
    logic was_control=0, was_feed=0, was_capture=0;
    always @(posedge cpu_clk) begin
        if (!dut.cpu_local_reset_n) begin was_control=0; was_feed=0; was_capture=0; end
        else begin
            // Record actual CPU acceptance and first visibility, independently
            // of the software polling frequency used below.
            if (mmio_wr && !mmio_error && mmio_addr==BASE+32'h44) begin
                control_at=$realtime;
                control_limit=((exp_kind==0 ? 2050 : exp_kind==1 ? 259 : 257)+5)*81.380+3*11.112;
            end
            if (mmio_wr && !mmio_error && mmio_addr==BASE+32'hb0) feed_at=$realtime;
            if (mmio_wr && !mmio_error && mmio_addr==BASE+32'h110) capture_at=$realtime;
            #0.001;
            if (dut.control_valid && !was_control && $realtime-control_at>control_limit+0.002)
                $fatal(1,"control CDC latency exceeded derived edge budget");
            if (dut.feed_valid && !was_feed && $realtime-feed_at>5*81.380+3*11.112+0.002)
                $fatal(1,"feed CDC latency exceeded derived edge budget");
            if (dut.capture_valid && !was_capture && $realtime-capture_at>5*81.380+3*11.112+0.002)
                $fatal(1,"capture CDC latency exceeded derived edge budget");
            was_control=dut.control_valid; was_feed=dut.feed_valid; was_capture=dut.capture_valid;
        end
    end
    always @(posedge audio_clk) begin
        if (dut.audio_local_reset_n && dut.emergency_pulse) emergency_deliveries=emergency_deliveries+1;
        if (dut.feed_a_valid && dut.feed_a_ready &&
            {dut.session.item_generation,dut.session.item_epoch,dut.session.item_at,dut.session.item_until,
             dut.session.item_loops,dut.session.item_end,dut.session.item_marker,dut.session.item_address,dut.session.item_value}!==
            {offered_gen,offered_feed_epoch,offered_at,offered_until,offered_loops,offered_flags,offered_bytes})
            $fatal(1,"MMIO-to-session feed payload differed from software offer");
        if (check_tone && dut.media_enable && !dut.session.audio.source_media.jt_wr_n) begin
            if (!dut.session.audio.source_media.jt_a0 && dut.session.audio.source_media.cen)
                native_address=dut.session.audio.source_media.jt_din;
            if (dut.session.audio.source_media.jt_a0 && dut.session.audio.source_media.cen_p1) begin
                if (native_writes>=28) $fatal(1,"future write dispatched early");
                native_expected=tone(native_writes);
                if ({native_address,dut.session.audio.source_media.jt_din}!==native_expected)
                    $fatal(1,"native bytes differed from independent authored tone");
                native_writes=native_writes+1;
            end
        end
        #0.001;
        if (audio_mclk!==1 || (dut.inhibited && audio_dac!==0)) $fatal(1,"audio clock/inhibit violation");
        if (audio_dac) nonzero_bits=nonzero_bits+1;
    end
    initial begin #100000000; $fatal(1,"MMIO test timeout"); end

    task automatic write_abs(input logic [31:0] address, data, input logic error=0, input logic [3:0] lanes=15);
        @(negedge cpu_clk); mmio_addr=address; mmio_wr_data=data; byte_enable=lanes; mmio_wr=1;
        #0.001; if (mmio_error!==error) $fatal(1,"write error addr=%h data=%h expected=%b actual=%b",address,data,error,mmio_error);
        @(posedge cpu_clk); #0.001;
        @(negedge cpu_clk); mmio_wr=0;
    endtask
    task automatic wr(input integer offset, input logic [31:0] data, input logic error=0);
        write_abs(BASE+32'(offset),data,error);
    endtask
    task automatic read_abs(input logic [31:0] address, output logic [31:0] data, input logic error=0);
        @(negedge cpu_clk); mmio_addr=address; mmio_rd=1;
        #0.001;
        if (mmio_error!==error) $fatal(1,"read error addr=%h expected=%b actual=%b",address,error,mmio_error);
        data=mmio_rd_data;
        if (error && data!=0) $fatal(1,"rejected read aliased another register");
        @(posedge cpu_clk); #0.001;
        @(negedge cpu_clk); mmio_rd=0;
    endtask
    task automatic rd(input integer offset, output logic [31:0] data, input logic error=0);
        read_abs(BASE+32'(offset),data,error);
    endtask
    task automatic write64(input integer offset, input logic [63:0] data);
        wr(offset,data[31:0]); wr(offset+4,data[63:32]);
    endtask
    task automatic wait_status(input integer bit_index);
        integer n;
        n=0;
        do begin rd('h0c,value); n=n+1; if(n>10000) $fatal(1,"status wait timeout bit=%0d",bit_index); end while(!value[bit_index]);
    endtask
    task automatic stage_control(input logic [2:0] kind, input logic [63:0] gen,
                                 input logic [63:0] revision=0, input logic enabled=0, input logic [31:0] count=0);
        exp_id=next_id; next_id=next_id+1; exp_gen=gen; exp_kind=kind; exp_revision=revision; exp_enabled=enabled; exp_target=count;
        write64('h20,exp_id); write64('h28,gen); wr('h30,{29'd0,kind});
        write64('h34,revision); wr('h3c,{31'd0,enabled}); wr('h40,count);
    endtask
    task automatic control_result(input logic [31:0] result, input logic release_it=1);
        wait_status(1);
        for(integer i=0;i<16;i=i+1) rd('h180+i*4,words[i]);
        if ({words[1],words[0]}!==exp_id || {words[3],words[2]}!==exp_gen ||
            {words[5],words[4]}!==exp_revision || words[6]!=={29'd0,exp_kind} ||
            words[7]!=={31'd0,exp_enabled} || words[8]!==exp_target || words[11]!==result)
            $fatal(1,"copied control identity/result mismatch kind=%0d result=%0d/%0d",exp_kind,words[11],result);
        ack_frame={words[10],words[9]}; epoch={words[13],words[12]}; prepared={words[15],words[14]};
        controls=controls+1;
        if(release_it) wr('h48,1);
    endtask
    task automatic control(input logic [2:0] kind, input logic [63:0] gen,
                           input logic [63:0] revision=0, input logic enabled=0, input logic [31:0] count=0,
                           input logic [31:0] result=1);
        stage_control(kind,gen,revision,enabled,count); wr('h44,1); control_result(result);
    endtask
    task automatic stage_feed(input logic [63:0] gen, offered_epoch, at_edge, until_edge, loops,
                              input logic [1:0] flags, input logic [15:0] bytes=0);
        offered_gen=gen; offered_feed_epoch=offered_epoch; offered_at=at_edge;
        offered_until=until_edge; offered_loops=loops; offered_flags=flags; offered_bytes=bytes;
        write64('h80,gen); write64('h88,offered_epoch); write64('h90,at_edge);
        write64('h98,until_edge); write64('ha0,loops); wr('ha8,{30'd0,flags}); wr('hac,{16'd0,bytes});
    endtask
    task automatic feed_result(input logic [31:0] result, input logic [63:0] gen, offered_epoch, input logic release_it=1);
        wait_status(3);
        for(integer i=0;i<5;i=i+1) rd('hb8+i*4,words[i]);
        if(words[0]!==result || {words[2],words[1]}!==gen || {words[4],words[3]}!==offered_epoch)
            $fatal(1,"feed receipt changed identity/result expected=%0d actual=%0d",result,words[0]);
        feeds=feeds+1;
        if(release_it) wr('hb4,1);
    endtask
    task automatic feed(input logic [63:0] at_edge=0, until_edge=0, loops=0,
                        input logic [1:0] flags=0, input logic [15:0] bytes=0, input logic [31:0] result=1);
        stage_feed(prepared,epoch,at_edge,until_edge,loops,flags,bytes); wr('hb0,1); feed_result(result,prepared,epoch);
    endtask
    task automatic stage_capture(input logic [63:0] ticket=TICKET, expected_epoch=0);
        write64('h100,ticket); write64('h108,expected_epoch);
    endtask
    task automatic read_capture(input logic [63:0] ticket, expected_epoch, input logic [31:0] result=1, input logic release_it=1);
        wait_status(5);
        for(integer i=0;i<33;i=i+1) rd('h200+i*4,words[i]);
        if ({words[1],words[0]}!==ticket || {words[3],words[2]}!==expected_epoch || words[32]!==result)
            $fatal(1,"capture ticket/epoch/result mismatch");
        if (words[17][31:8]!=0 || words[18][31:8]!=0 || words[19][31:18]!=0 || words[20][31:18]!=0 ||
            words[21][31:7]!=0 || words[26][31:3]!=0 || words[31][31:3]!=0) $fatal(1,"capture reserved bits");
        captures=captures+1;
        if(release_it) wr('h114,1);
    endtask
    task automatic capture(input logic [63:0] expected_epoch, input logic [31:0] result=1);
        stage_capture(TICKET,expected_epoch); wr('h110,1); read_capture(TICKET,expected_epoch,result);
    endtask
    function automatic logic [15:0] tone(input integer i);
        integer op, group_index;
        if(i==0) return 16'h20c7;
        if(i==1) return 16'h283c;
        if(i==2) return 16'h3000;
        if(i==27) return 16'h0878;
        op=(i-3)/6; group_index=(i-3)%6;
        case(group_index)
            0:return {8'(8'h40+op*8),8'd1};
            1:return {8'(8'h60+op*8),8'(op*8)};
            2:return {8'(8'h80+op*8),8'h1f};
            3:return {8'(8'ha0+op*8),8'd0};
            4:return {8'(8'hc0+op*8),8'd0};
            5:return {8'(8'he0+op*8),8'h0f};
        endcase
        $fatal(1,"tone index"); return 0;
    endfunction
    task automatic common_reset(input logic cpu_origin);
        #0.001;
        if(cpu_origin) cpu_reset_n=0; else audio_reset_n=0;
        repeat(4) @(negedge audio_clk);
        cpu_reset_n=1; audio_reset_n=1;
        repeat(2100) @(negedge audio_clk);
        rd('h0c,value);
        if(value!==32'h880 || sound_fault) $fatal(1,"old mailbox survived common reset status=%h",value);
        capture(0);
        if({words[5],words[4]}!=0 || {words[9],words[8]}!=0 || words[17]!=32'h94)
            $fatal(1,"pre-reset audio state replayed after either-origin common reset");
    endtask
    initial begin
        repeat(4) @(negedge audio_clk); cpu_reset_n=1; audio_reset_n=1;
        repeat(8) @(negedge audio_clk);
        rd(0,value); if(value!==32'h52534d31) $fatal(1,"ID");
        rd(4,value); if(value!==32'h00010000) $fatal(1,"version");
        rd(8,value); if(value!==7) $fatal(1,"unimplemented capabilities advertised");
        rd('h18,value); if(value!==12288000) $fatal(1,"source rate");
        rd('h1c,value); if(value!==48000) $fatal(1,"frame rate");
        for(integer o=0;o<1024;o=o+4) begin
            if ((o>='h20 && o<='h40) || (o>='h80 && o<='hac) || (o>='h100 && o<='h10c)) begin
                rd(o,value); if(value!=0) $fatal(1,"staging not reset");
            end else if (!(o==0 || o==4 || o==8 || o==12 || o==24 || o==28)) rd(o,value,1);
            if (!((o>='h20 && o<='h40) || (o>='h80 && o<='hac) || (o>='h100 && o<='h10c) ||
                  o=='h10 || o=='h14 || o=='h44 || o=='h48 || o=='hb0 || o=='hb4 || o=='h110 || o=='h114)) wr(o,0,1);
        end
        for(integer lanes=0;lanes<15;lanes=lanes+1) write_abs(BASE+'h20,32'hbadc0ffe,1,4'(lanes));
        rd('h20,value); if(value!=0) $fatal(1,"partial write changed staging");
        for(integer b=1;b<4;b=b+1) begin write_abs(BASE+32'(b),1,1); read_abs(BASE+32'(b),value,1); end
        read_abs(32'h40000000,value,1); read_abs(32'h40000200,value,1); read_abs(32'h40000240,value,1);
        read_abs(BASE-4,value,1); read_abs(BASE+1024,value,1);
        wr(0,1,1); wr('h30,8,1); wr('h3c,2,1); wr('ha8,4,1); wr('hac,32'h10000,1);
        wr('h44,0,1); wr('h48,1,1); wr('hb4,1,1); wr('h114,1,1); wr('h14,4,1);
        @(negedge cpu_clk); mmio_addr=BASE+'h20; mmio_rd=1; mmio_wr=1; mmio_wr_data=1; byte_enable=15;
        #0.001; if(!mmio_error || mmio_rd_data!=0) $fatal(1,"simultaneous read/write accepted");
        @(posedge cpu_clk); #0.001;
        @(negedge cpu_clk); mmio_rd=0; mmio_wr=0;
        rd('h20,value); if(value!=0) $fatal(1,"rejected access changed staging");
        wr('h14,3); rd('h0c,value); if(value[9:8]!=0) $fatal(1,"diagnostic clear");

        // Keep old state and old feed responses owned while Reset executes.
        stage_capture(TICKET,0); wr('h110,1); read_capture(TICKET,0,1,0);
        for(integer i=0;i<33;i=i+1) saved[i]=words[i];
        stage_control(0,GEN); wr('h44,1);
        for(integer o='h20;o<='h40;o=o+4) wr(o,0);
        repeat(20) @(negedge audio_clk);
        wr('h44,1,1); wr('h110,1,1);
        stage_feed(0,0,0,0,0,0,16'h20c7); wr('hb0,1);
        write64('h98,64'hcafebabedeadc0de); wr('hac,16'h283c);
        feed_result(5,0,0,0);
        control_result(1);
        if(epoch!=1 || prepared!=GEN || ack_frame!=0 || sound_fault) $fatal(1,"Reset identity/epoch");
        for(integer i=0;i<33;i=i+1) begin rd('h200+i*4,value); if(value!==saved[i]) $fatal(1,"unread capture changed across Reset"); end
        wr('h114,1); wr('hb4,1); capture(0,3); capture(epoch);
        if ({words[5],words[4]}!=epoch || {words[7],words[6]}!=GEN || {words[9],words[8]}!=0 || words[17]!=20)
            $fatal(1,"coherent prepared session snapshot");
        control(7,GEN,1,0,0,2);
        stage_capture(0,epoch); wr('h110,1); read_capture(0,epoch,2);

        for(integer i=0;i<28;i=i+1) feed(0,0,0,0,tone(i));
        feed(0,64'h10203040506,2,1);
        for(integer i=0;i<35;i=i+1) feed(64'h10203040506,0,0,0,16'h3000);
        stage_feed(GEN,epoch,64'h10203040506,0,0,0,16'h3000); wr('hb0,1); feed_result(2,GEN,epoch,0);
        check_tone=1;
        control(1,GEN,1,1,2); if(ack_frame!=1) $fatal(1,"Start ACK frame");
        wr('hb4,1); wr('hb0,1); feed_result(1,GEN,epoch);
        repeat(18000) @(negedge audio_clk);
        if(nonzero_bits==0 || sound_fault || native_writes!=28) $fatal(1,"real tone failed through MMIO");
        control(2,GEN,1,1,2); pause_frame=ack_frame;
        capture(epoch);
        if({words[9],words[8]}!=GEN || {words[11],words[10]}!=pause_frame || {words[13],words[12]}!=1 ||
           {words[15],words[14]}!=2 || words[16]!=2 || !words[17][1] || words[18]!=1 ||
           words[19]>=240000 || words[19]==0 || {words[23],words[22]}!=0)
            $fatal(1,"paused snapshot frame/policy/gain/checkpoint mismatch");
        // Capture is immutable across many media frames and a changed policy.
        stage_capture(TICKET,epoch); wr('h110,1); read_capture(TICKET,epoch,1,0);
        for(integer i=0;i<33;i=i+1) saved[i]=words[i];
        control(3,GEN,1,1,2); if(ack_frame!=pause_frame) $fatal(1,"Resume returned latest rather than held frame");
        control(4,GEN,2,1,5);
        repeat(1000) @(negedge audio_clk);
        for(integer i=0;i<33;i=i+1) begin rd('h200+i*4,value); if(value!==saved[i]) $fatal(1,"capture tore during policy/frame changes"); end
        wr('h114,1); capture(epoch);
        if({words[11],words[10]}<=pause_frame || {words[13],words[12]}!=2 || words[16]!=5 || words[18]!=2)
            $fatal(1,"live policy capture");

        // Every emergency write remains deliverable, including a coalesced follow-up.
        check_tone=0;
        stage_control(0,GEN+1);
        wr('h10,1); wr('h10,1); wr('h44,1,1);
        do begin rd('h0c,value); end while(value[10]);
        if(!value[7] || !value[6] || emergency_deliveries!=2) $fatal(1,"emergency was lost before Reset");
        wr('h44,1); repeat(100) @(negedge audio_clk); wr('h10,1); control_result(4);
        do begin rd('h0c,value); end while(value[10]);
        if(emergency_deliveries!=3) $fatal(1,"later emergency delivery missing");
        control(0,GEN+1); capture(epoch);
        if(words[17]!=20 || words[18]!=0 || words[19]!=0) $fatal(1,"emergency recovery snapshot");
        feed(0,0,0,3); control(1,GEN+1,1);
        repeat(10000) @(negedge audio_clk); capture(epoch);
        if(words[18]!=(3<<2) || words[19]!=0 || sound_fault) $fatal(1,"natural end across CDC");
        control(0,GEN+2); feed(0,0,0,0,16'h20c7); control(1,GEN+2,1);
        repeat(3000) @(negedge audio_clk); rd('h0c,value);
        if(!value[7] || !value[6]) $fatal(1,"real source starvation not visible to CPU");

        // Assert each reset origin at all transfer stages for each mailbox.
        common_reset(1);
        for(integer c=0;c<3;c=c+1) begin
            selected_channel=c;
            for(integer s=0;s<6;s=s+1) begin
                case(c)
                    0:begin stage_control(0,GEN); wr('h44,1); end
                    1:begin stage_feed(GEN,0,0,0,0,0,16'h20c7); wr('hb0,1); end
                    2:begin stage_capture(TICKET,0); wr('h110,1); end
                endcase
                case(s)
                    0: ;
                    1: wait(selected_request);
                    2: begin wait(selected_request); @(posedge audio_clk); #0.001; end
                    3: wait(selected_reply);
                    4: wait(selected_ack);
                    5: wait(selected_visible);
                endcase
                common_reset((s%2)==0); reset_cases=reset_cases+1;
            end
        end
        control(0,GEN); feed(0,0,0,3); control(1,GEN,1);
        repeat(10000) @(negedge audio_clk); capture(epoch);
        if(words[18]!=(3<<2) || sound_fault || reset_cases!=18) $fatal(1,"post-common-reset generation did not recover");
        $display("sound_mmio_tb: PASS phase_ps=%0d controls=%0d feeds=%0d captures=%0d reset_stages=%0d nonzero_bits=%0d",
                 phase_ps,controls,feeds,captures,reset_cases,nonzero_bits);
        $finish;
    end
endmodule
