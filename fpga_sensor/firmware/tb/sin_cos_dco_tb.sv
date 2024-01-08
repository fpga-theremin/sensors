/*********************************************/
/* SINE table of size 4096 and 13-bit values */
/* Latency: 4 CLK cycles                     */
/* TESTBENCH                                 */
/*********************************************/
`timescale 1ns / 1ps
module sin_cos_dco_tb ();

    localparam PHASE_BITS = 32;
    localparam PHASE_INCREMENT_BITS = 28;
    localparam SIN_TABLE_DATA_WIDTH = 13;
    localparam SIN_TABLE_ADDR_WIDTH = 12;


    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;

    /* new phase increment value */
    logic [PHASE_INCREMENT_BITS-1:0] PHASE_INCREMENT_IN;
    /* 1 to set new value for phase increment */
    logic PHASE_INCREMENT_IN_WE;

    /* SIN value output */
    wire signed [SIN_TABLE_DATA_WIDTH-1:0] SIN_VALUE;
    /* COS value output */
    wire signed [SIN_TABLE_DATA_WIDTH-1:0] COS_VALUE;

//    wire [PHASE_BITS-1:0] debug_PHASE;
//    wire [PHASE_INCREMENT_BITS-1:0] debug_PHASE_INC;

sin_cos_dco
#(
    .PHASE_BITS(PHASE_BITS),
    .PHASE_INCREMENT_BITS(PHASE_INCREMENT_BITS),
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .SIN_TABLE_ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH)
)
sin_cos_dco_inst
(
    .*
);
    
    // drive CLK
    always begin
        #1.472 CLK=0;
        #1.472 CLK=1; 
        cycleCounter = cycleCounter + 1;
    end
    
    task nextCycle();
         begin
             @(posedge CLK) #1 ;
             //$display("    [%d]   nextCycle SIN=%d COS=%d  phase=%h increment=%h", cycleCounter, SIN_VALUE, COS_VALUE, debug_PHASE, debug_PHASE_INC);
             $display("    [%d]   nextCycle SIN=%d COS=%d", cycleCounter, SIN_VALUE, COS_VALUE);
         end
    endtask

    task checkResult(input signed [SIN_TABLE_DATA_WIDTH-1:0] expected_sin_value, input signed [SIN_TABLE_DATA_WIDTH-1:0] expected_cos_value);
         begin
             //$display("    [%d]             SIN=%d COS=%d", cycleCounter, SIN_VALUE, COS_VALUE);
             if (SIN_VALUE != expected_sin_value) begin
                 $display("    [%d] ERROR: SIN_VALUE does not match with expected result, expected %x actual %x", cycleCounter, expected_sin_value, SIN_VALUE);
                 $finish();
             end
             if (COS_VALUE != expected_cos_value) begin
                 $display("    [%d] ERROR: COS_VALUE does not match with expected result, expected %x actual %x", cycleCounter, expected_cos_value, COS_VALUE);
                 $finish();
             end
         end
    endtask

    initial begin
        // reset condition
        RESET = 0;
        CE = 0;
        #200 @(posedge CLK) RESET = 1;
        #100 @(posedge CLK) RESET = 0;
        @(posedge CLK) #1 CE = 1;
        cycleCounter = 0;
        // set phase increment
        PHASE_INCREMENT_IN = 109377165; // 1018654.23 Hz signal at 40000000 Hz sample rate
        PHASE_INCREMENT_IN_WE = 1;
 
        @(posedge CLK) #1 PHASE_INCREMENT_IN_WE = 0;

     
        $display("Starting verification");
    

        /* Outputs are delayed by 4 clock cycles */
        nextCycle();
        nextCycle();
        nextCycle();
        $display("    Values are propagated");
        nextCycle(); checkResult(3, 4095);
        nextCycle(); checkResult(654, 4042);
        nextCycle(); checkResult(1288, 3887);
        nextCycle(); checkResult(1889, 3633);
        nextCycle();
        nextCycle();
        nextCycle();
        nextCycle();
        nextCycle();
        nextCycle();
        nextCycle();
        //nextCycle(7); checkResult(22, 4095);
        $display("Test passed");
        $finish();
    end
    
    endmodule
    
