module lp_filter_stage
#(
    parameter DATA_BITS = 28,
    parameter SHIFT_BITS = 6
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* input value */
    input wire [DATA_BITS-1:0] IN_VALUE,
    /* filtered output value */
    output wire [DATA_BITS-1:0] OUT_VALUE
);

localparam INTERNAL_BITS = DATA_BITS + SHIFT_BITS;

reg signed [INTERNAL_BITS-1:0] value_stage0;
wire signed [INTERNAL_BITS:0] diff = IN_VALUE - value_stage0[INTERNAL_BITS-1:SHIFT_BITS];

always @(posedge CLK) begin
    if (RESET) begin
        value_stage0 <= 0;
    end else if (CE) begin
        value_stage0 <= value_stage0 + diff;
    end
end

assign OUT_VALUE = value_stage0[INTERNAL_BITS-1:SHIFT_BITS];

endmodule
