/*
    Dual multiply and accumulate:
        SIN_RESULT += SIN_VALUE*ADC_VALUE
        COS_RESULT += COS_VALUE*ADC_VALUE
    When ADC_VALUE crosses zero (MSB changed), restart summing from 0
        and return sums of accumulated values for last 2 zero crossings
*/

module quadrature_mul_acc
#(
    // same as DCO SIN table data width
    parameter SIN_TABLE_DATA_WIDTH = 13,
    // number of bits from ADC
    parameter ADC_DATA_WIDTH = 12,
    parameter RESULT_WIDTH = 32,
    // 1 to enable accumulation of values for one ADC period (zero crossing detection)
    //   in this case set RESULT_WIDTH=32 for iCE40 platform since it has 32-bit accumulator in DSP 
    // 0 to output multiplicatoin result values directly, without additional accumulation - to let LP filters do averaging
    //   in this case, set RESULT_WIDTH=SIN_TABLE_DATA_WIDTH+ADC_DATA_WIDTH 
    parameter PERIOD_ACC_ENABLE = 1
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

    /* 1 if SIN_RESULT and COS_RESULT updated with new value */
    output wire UPDATED_RESULT,
    /* mul-acc value for ADC*SIN */
    output wire signed [RESULT_WIDTH-1:0] SIN_RESULT,
    /* mul-acc value for ADC*COS  */
    output wire signed [RESULT_WIDTH-1:0] COS_RESULT
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

generate

if (PERIOD_ACC_ENABLE) begin

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
            if (zero_cross_stage1) begin
                sin_acc_stage2 <= sin_mul_adc_stage1;
                cos_acc_stage2 <= cos_mul_adc_stage1;
            end else begin
                sin_acc_stage2 <= sin_acc_stage2 + sin_mul_adc_stage1;
                cos_acc_stage2 <= cos_acc_stage2 + cos_mul_adc_stage1;
            end
            zero_cross_stage2 <= zero_cross_stage1;
        end
    end
    
    reg signed [RESULT_WIDTH-1:0] prev_sin_acc_stage2;
    reg signed [RESULT_WIDTH-1:0] prev_cos_acc_stage2;
    
    reg signed [RESULT_WIDTH-1:0] sum2_sin_acc_stage2;
    reg signed [RESULT_WIDTH-1:0] sum2_cos_acc_stage2;
    
    always @(posedge CLK) begin
        if (RESET) begin
            prev_sin_acc_stage2 <= 0;
            prev_cos_acc_stage2 <= 0;
            sum2_sin_acc_stage2 <= 0;
            sum2_cos_acc_stage2 <= 0;
        end else if (CE) begin
            if (zero_cross_stage1) begin
                sum2_sin_acc_stage2 <= prev_sin_acc_stage2 + sin_acc_stage2;
                sum2_cos_acc_stage2 <= prev_cos_acc_stage2 + cos_acc_stage2;
                prev_sin_acc_stage2 <= sin_acc_stage2;
                prev_cos_acc_stage2 <= cos_acc_stage2;
            end
        end
    end
    
    
    assign UPDATED_RESULT = zero_cross_stage2;
    assign SIN_RESULT = sum2_sin_acc_stage2;
    assign COS_RESULT = sum2_cos_acc_stage2;
    
end else begin
    // no period accumulation is requested

    assign UPDATED_RESULT = 1'b1;
    assign SIN_RESULT = sin_mul_adc_stage1;
    assign COS_RESULT = cos_mul_adc_stage1;

//reg signed [MUL_WIDTH-1:0] sin_mul_adc_stage1;
//reg signed [MUL_WIDTH-1:0] cos_mul_adc_stage1;

end

endgenerate 

endmodule

