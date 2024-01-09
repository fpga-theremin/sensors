/*
     Variable delay based on shift register.
*/

module variable_delay_shift_register
#(
    // data width to store
    parameter DATA_BITS = 32,
    // shift register stores (1<<DELAY_BITS) entries
    // available delays are 1 .. (1<<DELAY_BITS)+1
    parameter DELAY_BITS = 4
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* variable delay: OUT_VALUE = IN_VALUE delayed by (DELAY+1) cycles */
    input wire [DELAY_BITS-1:0] DELAY,
    /* input value */
    input wire signed [DATA_BITS-1:0] IN_VALUE,
    /* delayed output value */
    output wire signed [DATA_BITS-1:0] OUT_VALUE
);

localparam MEM_SIZE = 1<<DELAY_BITS;
reg signed [DATA_BITS-1:0] shift_regs[MEM_SIZE-1:0];

genvar i;
generate
    for (i = MEM_SIZE-1; i >= 0; i = i - 1) begin
        always @(posedge CLK) begin
            if (CE) begin
                if (i > 0)
                    shift_regs[i] <= shift_regs[i - 1];
                else
                    shift_regs[i] <= IN_VALUE;
            end
        end
    end
endgenerate

reg signed [DATA_BITS-1:0] out;
always @(posedge CLK) begin
    out <= shift_regs[DELAY];
end

assign OUT_VALUE = out;

endmodule
