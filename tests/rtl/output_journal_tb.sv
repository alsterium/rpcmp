`timescale 1ns/1ps
module output_journal_tb;
    logic clk_audio=0, reset_n=0, clear=0, record_valid=0, record_natural_end=0, pop_valid=0;
    logic [63:0] record_generation=0, record_frame=0, record_prefix=0, pop_sequence=0;
    logic [1:0] pop_status;
    logic head_valid, latest_valid, exhausted;
    logic [5:0] queued;
    logic [63:0] next_sequence, lost;
    logic [256:0] head_record, latest_record;
    rpcmp_output_journal dut(.*);
    always #5 clk_audio=~clk_audio;
    logic [256:0] expected[0:31], latest=0;
    integer first=0, count=0, checked=0;
    logic [63:0] expected_next=1, missing=0;
    logic have_latest=0;

    task automatic step(input logic put, get, input logic [63:0] token=0, input logic flush=0);
        logic remove_head, insert;
        logic [1:0] status;
        logic [256:0] item;
        @(negedge clk_audio);
        record_valid=put; pop_valid=get; pop_sequence=token; clear=flush;
        record_generation=64'h1234567890abcdef;
        record_frame=64'hfedcba9876500000+64'(checked);
        record_prefix=64'h1020304000000000+64'(checked)*17;
        record_natural_end=(checked%7)==0;
        status=!get || flush ? 0 : count==0 ? 2 : token!=expected[first][63:0] ? 3 : 1;
        #1; if(pop_status!==status) $fatal(1,"pop validation step=%0d",checked);
        remove_head=status==1;
        insert=put && !flush && expected_next!=64'hffffffffffffffff && (count<32 || remove_head);
        item={record_natural_end,record_prefix,record_frame,record_generation,
              expected_next==64'hffffffffffffffff ? 64'd0 : expected_next};
        if(flush) begin
            first=0; count=0; expected_next=1; missing=0; latest=0; have_latest=0;
        end else begin
            if(remove_head) begin first=(first+1)%32; count=count-1; end
            if(insert) begin expected[(first+count)%32]=item; count=count+1; end
            if(put) begin
                latest=item; have_latest=1;
                if(expected_next!=64'hffffffffffffffff) expected_next=expected_next+1;
                if(!insert && missing!=64'hffffffffffffffff) missing=missing+1;
            end
        end
        @(posedge clk_audio); #1;
        if(queued!==6'(count) || lost!==missing || next_sequence!==expected_next || latest_valid!==have_latest ||
           latest_record!==latest || exhausted!==(expected_next==64'hffffffffffffffff))
            $fatal(1,"journal accounting step=%0d count=%0d/%0d lost=%0d/%0d",checked,queued,count,lost,missing);
        if(!flush && (head_valid!==(count!=0) || head_record!==(count!=0 ? expected[first] : 257'd0)))
            $fatal(1,"retained head mismatch step=%0d",checked);
        checked=checked+1;
    endtask
    initial begin
        repeat(3) @(negedge clk_audio); reset_n=1;
        step(0,1); step(1,0); step(0,1,999); step(1,1,1); step(0,1,2);
        // Full, loss and full simultaneous replacement across many RAM wraps.
        for(integer i=0;i<32;i=i+1) step(1,0);
        for(integer i=0;i<12;i=i+1) step(1,0);
        step(1,1,999);
        for(integer i=0;i<100;i=i+1) step(1,1,expected[first][63:0]);
        for(integer i=0;i<32;i=i+1) step(0,1,expected[first][63:0]);
        step(0,1); step(1,1,123,1); step(0,0);
        // Reach real u64 terminal arithmetic without a billions-of-years run.
        @(negedge clk_audio); record_valid=0; pop_valid=0;
        force dut.next_sequence=64'hfffffffffffffffe; #1; release dut.next_sequence;
        expected_next=64'hfffffffffffffffe;
        step(1,0); step(1,0); step(0,1,64'hfffffffffffffffe);
        @(negedge clk_audio); record_valid=0; pop_valid=0;
        force dut.lost=64'hfffffffffffffffe; #1; release dut.lost;
        missing=64'hfffffffffffffffe;
        step(1,0); step(1,0); step(1,0); step(1,1,0,1); step(1,0); step(0,1,1);
        $display("output_journal_tb: PASS checked=%0d full_loss_wrap_reset_exhaustion=1",checked);
        $finish;
    end
    initial begin #100000; $fatal(1,"journal timeout"); end
endmodule
