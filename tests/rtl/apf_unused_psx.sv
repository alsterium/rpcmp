// Inactive external-controller model for the APF transport fixture. The DUT's
// default SNAC enable is zero and the scenario never accesses these registers.
// No APF, AXI, completion or CDC behavior is modeled by this module.
module analogizer_psx #(parameter MASTER_CLK_FREQ = 50_000_000) (
    input wire i_clk, i_rst, i_ena, i_stb,
    output wire [15:0] key1, key2,
    output wire [31:0] joy1, joy2,
    input wire [1:0] i_VIB_SW1, i_VIB_SW2,
    input wire [7:0] i_VIB_DAT1, i_VIB_DAT2,
    output wire PSX_CLK, PSX_CMD, PSX_ATT1, PSX_ATT2,
    input wire PSX_DAT, PSX_ACK,
    output wire [3:0] DBG_TX
);
    assign key1 = 0;
    assign key2 = 0;
    assign joy1 = 0;
    assign joy2 = 0;
    assign PSX_CLK = 1;
    assign PSX_CMD = 1;
    assign PSX_ATT1 = 1;
    assign PSX_ATT2 = 1;
    assign DBG_TX = 0;
endmodule
