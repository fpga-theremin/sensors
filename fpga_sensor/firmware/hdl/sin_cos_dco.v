module sin_cos_dco
#(
    parameter PHASE_BITS = 32,
    parameter PHASE_INCREMENT_BITS = 28,
    parameter SIN_TABLE_DATA_WIDTH = 13,
    parameter SIN_TABLE_ADDR_WIDTH = 12,
    // non-zero values only for using with simulation, to emulate phase shifted ADC output
    parameter RESET_PHASE_VALUE = 0,
    // specify init file to SIN table ROM with
    parameter SIN_ROM_INIT_FILE = "sin_table_4096_13bit.mem"
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
    /* 1 to set new value for phase increment */
    input wire PHASE_INCREMENT_IN_WE,

    /* SIN value output */
    output wire [SIN_TABLE_DATA_WIDTH-1:0] SIN_VALUE,
    /* COS value output */
    output wire [SIN_TABLE_DATA_WIDTH-1:0] COS_VALUE

    //, output wire [PHASE_BITS-1:0] debug_PHASE
    //, output wire [PHASE_INCREMENT_BITS-1:0] debug_PHASE_INC
);

/* Store phase increment value */
reg [PHASE_INCREMENT_BITS-1:0] phase_increment;
always @(posedge CLK) begin
    if (RESET) begin
        phase_increment <= 0;
    end else if (CE && PHASE_INCREMENT_IN_WE) begin
        phase_increment <= PHASE_INCREMENT_IN;
    end
end

//assign debug_PHASE_INC = phase_increment;

/* Update phase */
reg [PHASE_BITS-1:0] phase;
always @(posedge CLK) begin
    if (RESET) begin
        phase <= RESET_PHASE_VALUE;
    end else if (CE) begin
        phase <= phase + phase_increment;
    end
end

//assign debug_PHASE = phase;
sin_cos_table #(
    .DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH),
    .INIT_FILE(SIN_ROM_INIT_FILE)
)
sin_cos_table_inst
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
