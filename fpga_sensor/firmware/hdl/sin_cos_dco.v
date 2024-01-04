module sin_cos_dco
#(
    parameter PHASE_BITS = 32,
    parameter SIN_TABLE_DATA_WIDTH = 13,
    parameter SIN_TABLE_ADDR_WIDTH = 12
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* new phase increment value */
    input wire [PHASE_BITS-1:0] PHASE_INCREMENT_IN,
    /* 1 to set new value for phase increment */
    input wire PHASE_INCREMENT_IN_WE,

    /* SIN value output */
    output wire [SIN_TABLE_DATA_WIDTH-1:0] SIN_VALUE,
    /* COS value output */
    output wire [SIN_TABLE_DATA_WIDTH-1:0] COS_VALUE
);

/* Store phase increment value */
reg [PHASE_BITS-1:0] phase_increment;
always @(posedge CLK) begin
    if (RESET) begin
        phase_increment <= 0;
    end else if (CE && PHASE_INCREMENT_IN_WE) begin
        phase_increment <= PHASE_INCREMENT_IN;
    end
end

/* Update phase */
reg [PHASE_BITS-1:0] phase;
always @(posedge CLK) begin
    if (RESET) begin
        phase <= 0;
    end else if (CE && PHASE_INCREMENT_IN_WE) begin
        phase <= phase + phase_increment;
    end
end

sin_cos_table_4096_13bit #(
    .DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH)
)
sin_cos_table_4096_13bit_inst
(
    /* input clock                                           */
    .CLK(CLK),
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    .CE(CE),
    /* reset signal, active 1                                */
    .RESET(RESET),
    
    /* sine table address to get value from                  */
    .PHASE(phase[PHASE_BITS-1:(PHASE_BITS-SIN_TABLE_ADDR_WIDTH)]),
    /* sin sample value from ROM, delayed by 4 clock cycles */
    .SIN_VALUE(SIN_VALUE),
    /* cos sample value from ROM, delayed by 4 clock cycles */
    .COS_VALUE(COS_VALUE)
);


endmodule
