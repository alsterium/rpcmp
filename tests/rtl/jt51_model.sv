module jt51 (
    input logic rst,clk,cen,cen_p1,cs_n,wr_n,a0,input logic [7:0] din,
    output logic [7:0] dout,output logic ct1,ct2,irq_n,sample,
    output logic signed [15:0] left,right,xleft,xright
);
    logic [5:0] divider;
    assign dout=0; assign ct1=0; assign ct2=0; assign irq_n=1;
    assign xleft=left; assign xright=right;
    always_ff @(posedge clk) begin
        if(rst) begin divider<=0; sample<=0; left<=0; right<=0; end
        else begin
            sample<=0;
            if(cen_p1) begin
                if(divider==31) begin
                    divider<=0; sample<=1; left<=left+16'sd101; right<=right-16'sd73;
                end else divider<=divider+1'b1;
            end
        end
    end
endmodule
