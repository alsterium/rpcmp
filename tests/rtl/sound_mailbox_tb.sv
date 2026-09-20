`timescale 1ns/1ps
module sound_mailbox_tb;
    logic src_clk=0, dst_clk=0, common_reset_n=0;
    logic [1:0] src_release=0, dst_release=0;
    wire src_reset_n=src_release[1], dst_reset_n=dst_release[1];
    logic src_request_valid=0, src_request_ready, src_response_valid, src_response_ready=0;
    logic [128:0] src_request=0, dst_request, expected_request;
    logic [256:0] src_response, dst_response=0, expected_response;
    logic dst_request_valid, dst_request_ready=0, dst_response_valid=0, dst_response_ready;
    integer requests=0, responses=0;
    rpcmp_sound_mailbox #(.REQUEST_BITS(129), .RESPONSE_BITS(257)) dut(.*);
    always #5 src_clk=~src_clk;
    initial begin #17; forever #41 dst_clk=~dst_clk; end
    always_ff @(posedge src_clk or negedge common_reset_n) begin
        if(!common_reset_n) src_release<=0; else src_release<={src_release[0],1'b1};
    end
    always_ff @(posedge dst_clk or negedge common_reset_n) begin
        if(!common_reset_n) dst_release<=0; else dst_release<={dst_release[0],1'b1};
    end
    always @(posedge dst_clk) begin
        if(dst_request_valid && dst_request!==expected_request) $fatal(1,"borrowed request changed under destination backpressure");
        if(dst_request_valid && dst_request_ready) requests=requests+1;
    end
    always @(posedge src_clk) begin
        if(src_response_valid && src_response!==expected_response) $fatal(1,"borrowed response changed under source backpressure");
        if(src_response_valid && src_response_ready) responses=responses+1;
    end
    initial begin #1000000; $fatal(1,"mailbox timeout"); end
    initial begin
        repeat(3) @(negedge dst_clk); common_reset_n=1;
        repeat(3) @(negedge dst_clk);
        for(integer n=0;n<64;n=n+1) begin
            expected_request={1'b1,64'(n),~64'(n)};
            expected_response={1'b1,64'h0123456789abcdef^64'(n),64'(n+1),~64'(n),64'hfedcba9876543210};
            @(negedge src_clk);
            if(!src_request_ready || src_response_valid) $fatal(1,"mailbox not empty before new request");
            src_request=expected_request; src_request_valid=1;
            @(negedge src_clk); src_request_valid=0; src_request=~expected_request;
            wait(dst_request_valid);
            repeat(n%7+1) @(negedge dst_clk);
            if(src_request_ready || src_response_valid) $fatal(1,"destination stall was treated as completion");
            dst_request_ready=1;
            @(negedge dst_clk); dst_request_ready=0;
            repeat(n%5+1) @(negedge dst_clk);
            if(src_response_valid) $fatal(1,"acceptance was confused with response");
            dst_response=expected_response; dst_response_valid=1;
            if(!dst_response_ready) $fatal(1,"response slot missing after request acceptance");
            @(negedge dst_clk); dst_response_valid=0; dst_response=~expected_response;
            wait(src_response_valid);
            src_request_valid=1; // Busy request cannot overwrite either bundle.
            repeat(n%11+1) @(negedge src_clk);
            if(src_request_ready || requests!=n+1 || responses!=n) $fatal(1,"unread response released ownership");
            src_response_ready=1;
            @(negedge src_clk); src_response_ready=0; src_request_valid=0;
        end
        if(requests!=64 || responses!=64) $fatal(1,"mailbox request/response loss or duplication");
        $display("sound_mailbox_tb: PASS requests=64 responses=64 destination_stalls=64 source_stalls=64");
        $finish;
    end
endmodule
