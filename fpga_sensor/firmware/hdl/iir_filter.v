module iir_filter
#(
    parameter DATA_BITS = 28,
    parameter SHIFT_BITS = 2,
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

localparam INTERNAL_BITS = DATA_BITS + SHIFT_BITS;


reg signed [INTERNAL_BITS-1:0] value_stage0;
always @(posedge CLK) begin
    if (RESET) begin
        value_stage0 <= 0;
    end else if (CE) begin
        value_stage0 <= value_stage0 + (IN_VALUE - value_stage0[INTERNAL_BITS-1:SHIFT_BITS]);
    end
end

generate

if (STAGE_COUNT == 1) begin
    assign OUT_VALUE = value_stage0;
end else begin

reg signed [INTERNAL_BITS-1:0] value_stage1;
always @(posedge CLK) begin
    if (RESET) begin
        value_stage1 <= 0;
    end else if (CE) begin
        value_stage1 <= value_stage1 + (value_stage0[INTERNAL_BITS-1:SHIFT_BITS] - value_stage1[INTERNAL_BITS-1:SHIFT_BITS]);
    end
end

if (STAGE_COUNT == 2) begin
    assign OUT_VALUE = value_stage1;
end else begin

reg signed [INTERNAL_BITS-1:0] value_stage2;
always @(posedge CLK) begin
    if (RESET) begin
        value_stage2 <= 0;
    end else if (CE) begin
        value_stage2 <= value_stage2 + (value_stage1[INTERNAL_BITS-1:SHIFT_BITS] - value_stage2[INTERNAL_BITS-1:SHIFT_BITS]);
    end
end

if (STAGE_COUNT == 3) begin
    assign OUT_VALUE = value_stage2;
end else begin

reg signed [INTERNAL_BITS-1:0] value_stage3;
always @(posedge CLK) begin
    if (RESET) begin
        value_stage3 <= 0;
    end else if (CE) begin
        value_stage3 <= value_stage3 + (value_stage2[INTERNAL_BITS-1:SHIFT_BITS] - value_stage3[INTERNAL_BITS-1:SHIFT_BITS]);
    end
end


if (STAGE_COUNT == 4) begin
    assign OUT_VALUE = value_stage3;
end else begin

reg signed [INTERNAL_BITS-1:0] value_stage4;
always @(posedge CLK) begin
    if (RESET) begin
        value_stage4 <= 0;
    end else if (CE) begin
        value_stage4 <= value_stage4 + (value_stage3[INTERNAL_BITS-1:SHIFT_BITS] - value_stage4[INTERNAL_BITS-1:SHIFT_BITS]);
    end
end
    assign OUT_VALUE = value_stage4;


end // 3

end // 3

end // 2

end // 1

endgenerate

endmodule
