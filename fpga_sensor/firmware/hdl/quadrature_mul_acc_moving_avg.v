/*
    Dual multiply and accumulate:
        SIN_RESULT += SIN_VALUE*ADC_VALUE
        COS_RESULT += COS_VALUE*ADC_VALUE
    Output values are being accumulated indefinitely.
    Calculate difference between two output samples to get sum accumulated between these samples.
    Additionally provides ADC_VALUE zero crossing flag - marking best samples to use for difference calculations.   
*/

module quadrature_mul_acc_moving_avg
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
    output wire signed [RESULT_WIDTH-1:0] COS_RESULT
);

// MUL+ACC outputs

/* mul-acc value for ADC*SIN */
wire signed [RESULT_WIDTH-1:0] sin_acc;
/* mul-acc value for ADC*COS  */
wire signed [RESULT_WIDTH-1:0] cos_acc;
/* upper bit (SIGN) of ADC was changed */
wire adc_zero_cross;

quadrature_mul_acc_simple
#(
    // same as DCO SIN table data width
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    // number of bits from ADC
    .ADC_DATA_WIDTH(ADC_DATA_WIDTH),
    .RESULT_WIDTH(RESULT_WIDTH)
)
quadrature_mul_acc_simple_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .SIN_VALUE(SIN_VALUE),
    .COS_VALUE(COS_VALUE),
    .ADC_VALUE(ADC_VALUE),

    .SIN_RESULT(sin_acc),
    .COS_RESULT(cos_acc),
    .ADC_ZERO_CROSS(adc_zero_cross)
);

wire [DELAY_BITS-1:0] delay;

/* mul-acc value for ADC*SIN */
wire signed [RESULT_WIDTH-1:0] sin_acc_filtered;
/* mul-acc value for ADC*COS  */
wire signed [RESULT_WIDTH-1:0] cos_acc_filtered;

moving_avg_filter
#(
    .DATA_BITS(RESULT_BITS),
    .DELAY_BITS(4)
)
moving_avg_filter_sin_inst
(
    .CLK(CLK),
    .CE(adc_zero_cross & CE),
    .RESET(RESET),

    .DELAY(delay),
    .IN_VALUE(sin_acc),
    .OUT_VALUE(sin_acc_filtered)
);

moving_avg_filter
#(
    .DATA_BITS(RESULT_BITS),
    .DELAY_BITS(4)
)
moving_avg_filter_cos_inst
(
    .CLK(CLK),
    .CE(adc_zero_cross & CE),
    .RESET(RESET),

    .DELAY(delay),
    .IN_VALUE(cos_acc),
    .OUT_VALUE(cos_acc_filtered)
);



filter_autoscale_control
#(
    .TOP_DATA_BITS(4),
    .DELAY_BITS(4)
)
filter_autoscale_control_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .UPDATE(adc_zero_cross & CE),
    .TOP_SIN(sin_acc[RESULT_BITS-1:RESULT_BITS-4]),
    .TOP_COS(cos_acc[RESULT_BITS-1:RESULT_BITS-4]),

    .DELAY(delay),
    .DELAY_UPDATED()
);

assign SIN_RESULT = sin_acc_filtered;
assign COS_RESULT = cos_acc_filtered;

endmodule

