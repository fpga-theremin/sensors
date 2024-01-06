/*
*/

module lp_filter
#(
    parameter DATA_BITS = 28,
    parameter SHIFT_BITS = 6,
    // number of filter stages, 1 to 5
    parameter STAGE_COUNT = 4
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

genvar i;
generate
    if (STAGE_COUNT == 0) begin
        assign OUT_VALUE = IN_VALUE;
    end else if (STAGE_COUNT > 0) begin
        wire [DATA_BITS-1:0] values [STAGE_COUNT:0];
        assign values[0] = IN_VALUE;
        assign OUT_VALUE = values[STAGE_COUNT];
        for (i = 0; i < STAGE_COUNT; i = i + 1) 
            lp_filter_stage #( .DATA_BITS(DATA_BITS), .SHIFT_BITS(SHIFT_BITS) ) lp_filter_stage_inst
            (
              .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(values[i]), .OUT_VALUE(values[i+1])
            );
    end else begin
        //$display("invalid stage count %d", STAGE_COUNT);
    end
endgenerate

endmodule
