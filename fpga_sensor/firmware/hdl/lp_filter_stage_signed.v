module lp_filter_stage_signed
#(
    parameter IN_DATA_BITS = 28,
    parameter SHIFT_BITS = 6,
    // should be between IN_DATA_BITS and IN_DATA_BITS+SHIFT_BITS
    parameter OUT_DATA_BITS = 28
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* input value */
    input wire signed [IN_DATA_BITS-1:0] IN_VALUE,
    /* filtered output value */
    output wire signed [OUT_DATA_BITS-1:0] OUT_VALUE
);

localparam INTERNAL_BITS = IN_DATA_BITS + SHIFT_BITS;

reg signed [INTERNAL_BITS-1:0] value_stage0;
wire signed [IN_DATA_BITS-1:0] value_stage0_upper_bits = value_stage0[INTERNAL_BITS-1:SHIFT_BITS];
wire signed [IN_DATA_BITS:0] diff = IN_VALUE - value_stage0_upper_bits;

always @(posedge CLK) begin
    if (RESET) begin
        value_stage0 <= 0;
    end else if (CE) begin
        value_stage0 <= value_stage0 + diff;
    end
end

assign OUT_VALUE = value_stage0[INTERNAL_BITS-1:INTERNAL_BITS-OUT_DATA_BITS];

endmodule
