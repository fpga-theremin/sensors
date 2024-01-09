module moving_avg_filter
#(
    parameter DATA_BITS = 32,
    parameter DELAY_BITS = 4
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = put new value and update output, 0 = no changes */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* variable delay, (DELAY+1) cycles */
    input wire [DELAY_BITS-1:0] DELAY,
    /* input value, running sum */
    input wire signed [DATA_BITS-1:0] IN_VALUE,
    /* filtered output value - difference between last input value and delayed value - equals to value accumulated since delayed value was stored */
    output wire signed [DATA_BITS-1:0] OUT_VALUE
);

wire signed [DATA_BITS-1:0] delayed_value;

variable_delay_shift_register
#(
    .DATA_BITS(DATA_BITS),
    .DELAY_BITS(DELAY_BITS)
)
variable_delay_shift_register_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .DELAY(DELAY),
    .IN_VALUE(IN_VALUE),
    /* delayed output value */
    .OUT_VALUE(delayed_value)
);

reg signed [DATA_BITS-1:0] diff;

always @(posedge CLK) begin
    if (RESET)
        diff = 0;
    else if (CE)
        diff = IN_VALUE - delayed_value;
end

assign OUT_VALUE = diff;

endmodule
