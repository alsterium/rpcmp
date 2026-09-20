// Lossy display retention. No output from this module controls audio.
module rpcmp_output_journal (
    input logic clk_audio, reset_n, clear,
    input logic record_valid, record_natural_end,
    input logic [63:0] record_generation, record_frame, record_prefix,
    input logic pop_valid,
    input logic [63:0] pop_sequence,
    output logic [1:0] pop_status,
    output logic head_valid, latest_valid, exhausted,
    output logic [5:0] queued,
    output logic [63:0] next_sequence, lost,
    output logic [256:0] head_record, latest_record
);
    logic [256:0] memory[0:31], ram_head, bypass_value, offered;
    logic [4:0] head, tail, read_address;
    logic bypass_head, pop, push, arrival;
    assign head_valid=reset_n && !clear && queued!=0;
    assign head_record=head_valid ? (bypass_head ? bypass_value : ram_head) : 257'd0;
    assign exhausted=next_sequence==64'hffffffffffffffff;
    // Low-to-high: sequence, generation, frame, prefix, natural-end flag.
    assign offered={record_natural_end,record_prefix,record_frame,record_generation,
                    exhausted ? 64'd0 : next_sequence};
    assign arrival=reset_n && !clear && record_valid;
    always_comb begin
        pop_status=0;
        if (reset_n && !clear && pop_valid) begin
            if (!head_valid) pop_status=2;
            else if (pop_sequence!=head_record[63:0]) pop_status=3;
            else pop_status=1;
        end
    end
    assign pop=pop_status==1;
    assign push=arrival && !exhausted && (queued<32 || pop);
    assign read_address=pop ? head+5'd1 : head;
    // Occupancy and bypass own validity; no RAM reset or collision dependency.
    always_ff @(posedge clk_audio) begin
        if (push) memory[tail]<=offered;
        ram_head<=memory[read_address];
    end
    always_ff @(posedge clk_audio or negedge reset_n) begin
        if (!reset_n) begin
            head<=0; tail<=0; queued<=0; bypass_head<=0; bypass_value<=0;
            next_sequence<=1; lost<=0; latest_valid<=0; latest_record<=0;
        end else if (clear) begin
            head<=0; tail<=0; queued<=0; bypass_head<=0;
            next_sequence<=1; lost<=0; latest_valid<=0; latest_record<=0;
        end else begin
            bypass_head<=push && (queued==0 || (queued==1 && pop));
            if (push && (queued==0 || (queued==1 && pop))) bypass_value<=offered;
            if (arrival) begin
                latest_valid<=1; latest_record<=offered;
                if (!exhausted) next_sequence<=next_sequence+64'd1;
                if (!push && lost!=64'hffffffffffffffff) lost<=lost+64'd1;
            end
            if (push) tail<=tail+5'd1;
            if (pop) head<=head+5'd1;
            case ({push,pop})
                2'b10: queued<=queued+6'd1;
                2'b01: queued<=queued-6'd1;
                default: queued<=queued;
            endcase
        end
    end
endmodule
