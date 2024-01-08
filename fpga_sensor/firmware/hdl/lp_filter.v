/*
*/

module lp_filter
#(
    // input and output data width; internal data width will be OUT_DATA_BITS+SHIFT_BITS
    // in OUT_DATA_BITS > IN_DATA_BITS, additional bits are being introduced on first stage of filter 
    parameter IN_DATA_BITS = 28,
    // input and output data width; internal data width will be DATA_BITS+SHIFT_BITS 
    parameter OUT_DATA_BITS = 28,
    // shift for each stage: v_next = (IN_VALUE - v)>>SHIFT_BITS
    parameter SHIFT_BITS = 5,
    // number of filter stages, 0 to 5; if 0, filter is switched off - turned to simple wire in to out
    parameter STAGE_COUNT = 2
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* input value */
    input wire [IN_DATA_BITS-1:0] IN_VALUE,
    /* filtered output value */
    output wire [OUT_DATA_BITS-1:0] OUT_VALUE
);

genvar i;
generate
    if (STAGE_COUNT == 0) begin
        if (OUT_DATA_BITS > IN_DATA_BITS) begin
            // right pad with zeroes if output is wider than input
            assign OUT_VALUE = { IN_VALUE, {(OUT_DATA_BITS - IN_DATA_BITS){1'b0}} };
        end else begin
            // no padding needed
            assign OUT_VALUE = IN_VALUE;
        end
    end else if (STAGE_COUNT > 0) begin
        wire [OUT_DATA_BITS-1:0] values [STAGE_COUNT:0];
        assign values[0] = IN_VALUE;
        assign OUT_VALUE = values[STAGE_COUNT];
        for (i = 0; i < STAGE_COUNT; i = i + 1) begin
            // first filter stage has different input and output widths
            lp_filter_stage #( .IN_DATA_BITS((i==0)?IN_DATA_BITS:OUT_DATA_BITS), 
                    .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS) ) lp_filter_stage_inst_stage1
            (
              .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(values[i]), .OUT_VALUE(values[i+1])
            );
        end
    end else begin
        //$display("invalid stage count %d", STAGE_COUNT);
    end
endgenerate

endmodule
