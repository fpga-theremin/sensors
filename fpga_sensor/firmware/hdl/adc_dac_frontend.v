
module adc_dac_frontend
#(
    parameter PHASE_BITS = 32,
    parameter PHASE_INCREMENT_BITS = 28,
    parameter PHASE_INCREMENT_FILTER_SHIFT_BITS = 3,
    parameter PHASE_INCREMENT_FILTER_STAGE_COUNT = 2,
    parameter SIN_TABLE_DATA_WIDTH = 13,
    parameter SIN_TABLE_ADDR_WIDTH = 12,
    parameter ADC_DATA_WIDTH = 12,
    parameter DAC_DATA_WIDTH = 12,
    parameter MUL_ACC_WIDTH = 32,
    parameter RESULT_FILTER_SHIFT_BITS = 3,
    parameter RESULT_FILTER_STAGE_COUNT = 2
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* new phase increment value */
    input wire [PHASE_INCREMENT_BITS-1:0] PHASE_INCREMENT_IN,

    /* input sample from ADC */
    input wire signed [ADC_DATA_WIDTH-1:0] ADC_VALUE,
    /* output sample for DAC */
    output wire signed [DAC_DATA_WIDTH-1:0] DAC_VALUE,

    /* filtered smoothly changing mul-acc value for ADC*SIN */
    output wire signed [MUL_ACC_WIDTH-1:0] SIN_MUL_ACC,
    /* filtered smoothly changing mul-acc value for ADC*COS */
    output wire signed [MUL_ACC_WIDTH-1:0] COS_MUL_ACC
);


// filter PHASE_INCREMENT_IN to provide smooth frequency changes
wire [PHASE_INCREMENT_BITS-1:0] phase_inc_filtered;

iir_filter
#(
    .DATA_BITS(PHASE_INCREMENT_BITS),
    .SHIFT_BITS(PHASE_INCREMENT_FILTER_SHIFT_BITS),
    .STAGE_COUNT(PHASE_INCREMENT_FILTER_STAGE_COUNT)
)
iir_filter_phase_inc_input_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .IN_VALUE(PHASE_INCREMENT_IN),
    .OUT_VALUE(phase_inc_filtered)
);


wire [SIN_TABLE_DATA_WIDTH-1:0] sin_table_value;
wire [SIN_TABLE_DATA_WIDTH-1:0] cos_table_value;

sin_cos_dco
#(
    .PHASE_BITS(PHASE_BITS),
    .PHASE_INCREMENT_BITS(PHASE_INCREMENT_BITS),
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .SIN_TABLE_ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH)
)
dco_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    /* new phase increment value */
    .PHASE_INCREMENT_IN(phase_inc_filtered),
    /* 1 to set new value for phase increment */
    .PHASE_INCREMENT_IN_WE(CE),

    /* SIN value output */
    .SIN_VALUE(sin_table_value),
    /* COS value output */
    .COS_VALUE(cos_table_value)
);

assign DAC_VALUE = sin_table_value[SIN_TABLE_DATA_WIDTH-1:(SIN_TABLE_DATA_WIDTH-DAC_DATA_WIDTH)];

/* mul-acc value for ADC*SIN */
wire signed [MUL_ACC_WIDTH-1:0] sin_mul_acc_for_last_period;
/* mul-acc value for ADC*COS  */
wire signed [MUL_ACC_WIDTH-1:0] cos_mul_acc_for_last_period;

quadrant_mul_acc
#(
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .ADC_DATA_WIDTH(ADC_DATA_WIDTH),
    .RESULT_WIDTH(MUL_ACC_WIDTH)
)
quadrant_mul_acc_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .SIN_VALUE(sin_table_value),
    .COS_VALUE(cos_table_value),
    /* value from ADC */
    .ADC_VALUE(ADC_VALUE),

    /* 1 if SIN_RESULT and COS_RESULT updated with new value */
    .UPDATED_RESULT(),
    /* mul-acc value for ADC*SIN */
    .SIN_RESULT(sin_mul_acc_for_last_period),
    /* mul-acc value for ADC*COS  */
    .COS_RESULT(cos_mul_acc_for_last_period)
);


/* mul-acc value for ADC*SIN */
wire signed [MUL_ACC_WIDTH-1:0] sin_mul_acc_filtered;
/* mul-acc value for ADC*COS  */
wire signed [MUL_ACC_WIDTH-1:0] cos_mul_acc_filtered;

assign SIN_MUL_ACC = sin_mul_acc_filtered;
assign COS_MUL_ACC = cos_mul_acc_filtered;

iir_filter
#(
    .DATA_BITS(MUL_ACC_WIDTH),
    .SHIFT_BITS(RESULT_FILTER_SHIFT_BITS),
    .STAGE_COUNT(RESULT_FILTER_STAGE_COUNT)
)
iir_filter_sin_mul_acc_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .IN_VALUE(cos_mul_acc_for_last_period),
    .OUT_VALUE(cos_mul_acc_filtered)
);


endmodule