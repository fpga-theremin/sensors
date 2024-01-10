/*
    Dual multiply and accumulate:
        SIN_RESULT += SIN_VALUE*ADC_VALUE
        COS_RESULT += COS_VALUE*ADC_VALUE
    Output values are being accumulated indefinitely.
    Calculate difference between two output samples to get sum accumulated between these samples.
    Additionally provides ADC_VALUE zero crossing flag - marking best samples to use for difference calculations.   
*/

module quadrature_mul_acc_simple
#(
    // same as DCO SIN table data width
    parameter SIN_TABLE_DATA_WIDTH = 13,
    // number of bits from ADC
    parameter ADC_DATA_WIDTH = 12,
    parameter RESULT_WIDTH = 32
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* SIN table lookup result */
    input wire signed [SIN_TABLE_DATA_WIDTH-1:0] SIN_VALUE,
    /* COS table lookup result */
    input wire signed [SIN_TABLE_DATA_WIDTH-1:0] COS_VALUE,
    /* value from ADC */
    input wire signed [ADC_DATA_WIDTH-1:0] ADC_VALUE,

    /* mul-acc value for ADC*SIN */
    output wire signed [RESULT_WIDTH-1:0] SIN_RESULT,
    /* mul-acc value for ADC*COS  */
    output wire signed [RESULT_WIDTH-1:0] COS_RESULT,
    /* upper bit (SIGN) of ADC was changed */
    output wire ADC_ZERO_CROSS
);

// stage0
reg signed [SIN_TABLE_DATA_WIDTH-1:0] sin_value_stage0;
reg signed [SIN_TABLE_DATA_WIDTH-1:0] cos_value_stage0;
reg signed [ADC_DATA_WIDTH-1:0] adc_value_stage0;

reg zero_cross_stage0;

always @(posedge CLK) begin
    if (RESET) begin
        sin_value_stage0 <= 0;
        cos_value_stage0 <= 0;
        adc_value_stage0 <= 0;
        
        zero_cross_stage0 <= 0;
    end else if (CE) begin
        sin_value_stage0 <= SIN_VALUE;
        cos_value_stage0 <= COS_VALUE;
        adc_value_stage0 <= ADC_VALUE;

        zero_cross_stage0 <= ADC_VALUE[ADC_DATA_WIDTH-1] ^ adc_value_stage0[ADC_DATA_WIDTH-1];
    end
end



localparam MUL_WIDTH = SIN_TABLE_DATA_WIDTH + ADC_DATA_WIDTH;

reg signed [MUL_WIDTH-1:0] sin_mul_adc_stage1;
reg signed [MUL_WIDTH-1:0] cos_mul_adc_stage1;

reg zero_cross_stage1;

always @(posedge CLK) begin
    if (RESET) begin
        sin_mul_adc_stage1 <= 0;
        cos_mul_adc_stage1 <= 0;
        zero_cross_stage1 <= 0;
    end else if (CE) begin
        sin_mul_adc_stage1 <= sin_value_stage0 * adc_value_stage0;
        cos_mul_adc_stage1 <= cos_value_stage0 * adc_value_stage0;
        zero_cross_stage1 <= zero_cross_stage0;
    end
end

reg signed [RESULT_WIDTH-1:0] sin_acc_stage2;
reg signed [RESULT_WIDTH-1:0] cos_acc_stage2;

reg zero_cross_stage2;

always @(posedge CLK) begin
    if (RESET) begin
        sin_acc_stage2 <= 0;
        cos_acc_stage2 <= 0;
        zero_cross_stage2 <= 0;
    end else if (CE) begin
        sin_acc_stage2 <= sin_acc_stage2 + sin_mul_adc_stage1;
        cos_acc_stage2 <= cos_acc_stage2 + cos_mul_adc_stage1;
        zero_cross_stage2 <= zero_cross_stage1;
    end
end

assign SIN_RESULT = sin_acc_stage2;
assign COS_RESULT = cos_acc_stage2;
assign ADC_ZERO_CROSS = zero_cross_stage2;

endmodule

