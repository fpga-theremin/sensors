/*
     Variable delay based on shift register.
*/

module fixed_delay_shift_register
#(
    // data width to store
    parameter DATA_BITS = 32,
    // delay (0 means simple wire, 1 = single FF stage, etc...)
    parameter DELAY_CYCLES = 16
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* input value */
    input wire signed [DATA_BITS-1:0] IN_VALUE,
    /* delayed output value */
    output wire signed [DATA_BITS-1:0] OUT_VALUE
);

genvar i;
generate
    if (DELAY_CYCLES == 0) begin

        assign OUT_VALUE = IN_VALUE;

    end else if (DELAY_CYCLES == 1) begin

        reg signed [DATA_BITS-1:0] out;
        always @(posedge CLK) begin
            out <= IN_VALUE;
        end
        assign OUT_VALUE = out;

    end else begin

        reg signed [DATA_BITS-1:0] shift_regs[DELAY_CYCLES-1:0];

        for (i = DELAY_CYCLES-1; i >= 0; i = i - 1) begin
            always @(posedge CLK) begin
                if (CE) begin
                    if (i > 0)
                        shift_regs[i] <= shift_regs[i - 1];
                    else
                        shift_regs[i] <= IN_VALUE;
                end
            end
        end

        assign OUT_VALUE = shift_regs[DELAY_CYCLES-1];

    end

endgenerate


endmodule
