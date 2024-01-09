/*
     Skips specified number of CLK cycles with CE=1 before allowing to propagate CE to output.
*/

module ce_delay_after_reset
#(
     // how many CLK cycles with CE=1 (2..256)
     parameter DELAY_CYCLES = 16
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,
    /* CE propagated to out, but only if there were at least DELAY_CYCLES of CLK with CE=1 after last RESET */
    output wire CE_OUT
);

localparam COUNTER_BITS = DELAY_CYCLES <= 2   ? 1
                        : DELAY_CYCLES <= 4   ? 2
                        : DELAY_CYCLES <= 8   ? 3
                        : DELAY_CYCLES <= 16  ? 4
                        : DELAY_CYCLES <= 32  ? 5
                        : DELAY_CYCLES <= 64  ? 6
                        : DELAY_CYCLES <= 128 ? 7
                        :                       8;

generate

if (DELAY_CYCLES <= 0) begin

    //  no delay at all
    assign CE_OUT = CE;
    
end else if (DELAY_CYCLES == 1) begin

    // skip first CE
    reg done;
    always @(posedge CLK) begin
        if (RESET)
            done <= 0;
        else if (CE)
            done <= 1'b1;
    end
    assign CE_OUT = done & CE;
    
end else begin

    // skip first DELAY_CYCLES CE
    reg [COUNTER_BITS-1:0] counter;
    reg done;
    
    assign CE_OUT = CE & done;
    
    always @(posedge CLK) begin
        if (RESET) begin
            counter <= DELAY_CYCLES-1;
            done <= 0;
        end else if (CE) begin
            if (|counter) begin
                // counter is non zero
                counter <= counter - 1;
            end else begin
                // counter == 0
                done <= 1'b1;
            end
        end
    end
    
end
    
endgenerate

endmodule
