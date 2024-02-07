module cordic_sin_cos_tb();

localparam DATA_BITS = 16;
localparam STEP1_PHASE_BITS = 7;
localparam STEP2_PHASE_BITS = 9;
localparam PHASE_BITS = 3 + STEP1_PHASE_BITS + STEP2_PHASE_BITS; // 19
localparam LATENCY = 2 + STEP2_PHASE_BITS + 1;


localparam DELAY_BITS = 4;

logic [PHASE_BITS:0] cycleCounter = 0;
logic CLK;
logic CE;
logic RESET;

logic [PHASE_BITS-1:0] PHASE;
wire [PHASE_BITS-1:0] PHASE_DELAYED;

variable_delay_shift_register
#(
    .DATA_BITS(PHASE_BITS),
    .DELAY_BITS(DELAY_BITS)
)
variable_delay_shift_register_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),
    .DELAY(LATENCY),
    .IN_VALUE(PHASE),
    .OUT_VALUE(PHASE_DELAYED)
);

wire signed [DATA_BITS-1:0] SIN;
wire signed [DATA_BITS-1:0] COS;

wire debug_swap_sin_and_cos_delayed;
wire debug_neg_sin_delayed;
wire debug_neg_cos_delayed;
wire [PHASE_BITS-1:0] debug_PHASE;
wire [STEP1_PHASE_BITS-1:0] debug_step1_lookup_addr;
wire [STEP2_PHASE_BITS-1:0] debug_step2_lookup_addr;

cordic_sin_cos #(
    .DATA_BITS(DATA_BITS),
    .PHASE_BITS(PHASE_BITS),
    .STEP1_PHASE_BITS(STEP1_PHASE_BITS),
    .STEP2_PHASE_BITS(STEP2_PHASE_BITS)
) 
cordic_sin_cos_inst 
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),
    .*
);

task nextCycle();
     begin
         @(posedge CLK) #1 ;
         $display("    [%d]   nextCycle PHASE=%h  PHASE_DELAYED=%h  SIN=%d COS=%d", cycleCounter, PHASE, PHASE_DELAYED, SIN, COS);
         PHASE = PHASE + 1;
     end
endtask

// drive CLK
always begin
    #1.472 CLK=0;
    #1.472 CLK=1; 
    cycleCounter = cycleCounter + 1;
end

initial begin
    // reset condition
    RESET = 0;
    CE = 0;
    PHASE = 0;
    #200 @(posedge CLK) RESET = 1;
    #100 @(posedge CLK) RESET = 0;
    @(posedge CLK) #1 CE = 1;
    $display("Starting CORDIC SIN COS test");
    cycleCounter = 0;

    repeat(3000) nextCycle();
    //repeat((1<<PHASE_BITS) + 100) nextCycle();

    $display("Test passed");
    $finish();

end



endmodule
