`timescale 1ns / 1ps
module adc_dac_frontend_tb();


// 1 to enable accumulation of values for one ADC period (zero crossing detection)
//   in this case set RESULT_WIDTH=32 for iCE40 platform since it has 32-bit accumulator in DSP 
// 0 to output multiplicatoin result values directly, without additional accumulation - to let LP filters do averaging
//   in this case, set RESULT_WIDTH=SIN_TABLE_DATA_WIDTH+ADC_DATA_WIDTH 
localparam PERIOD_ACC_ENABLE = 1;

localparam PHASE_BITS = 32;
localparam PHASE_INCREMENT_BITS = 28;
localparam PHASE_INCREMENT_FILTER_SHIFT_BITS = 5;
localparam PHASE_INCREMENT_FILTER_STAGE_COUNT = 2; //2;
localparam PHASE_INCREMENT_FEEDBACK_FILTER_SHIFT_BITS = 4;
localparam PHASE_INCREMENT_FEEDBACK_FILTER_STAGE_COUNT = 2;
localparam SIN_TABLE_DATA_WIDTH = 13;
localparam SIN_TABLE_ADDR_WIDTH = 12;
localparam ADC_DATA_WIDTH = 12;
localparam DAC_DATA_WIDTH = 12;

localparam MUL_WIDTH = SIN_TABLE_DATA_WIDTH + ADC_DATA_WIDTH;
localparam MUL_ACC_WIDTH = PERIOD_ACC_ENABLE ? 32 : MUL_WIDTH;

localparam RESULT_FILTER_SHIFT_BITS = 6;
localparam RESULT_MUL_ACC_WIDTH = MUL_ACC_WIDTH + RESULT_FILTER_SHIFT_BITS;
localparam RESULT_FILTER_STAGE_COUNT = 4;

    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;


    logic [PHASE_INCREMENT_BITS-1:0] PHASE_INCREMENT_IN;
    wire signed [ADC_DATA_WIDTH-1:0] ADC_VALUE;

    wire signed [DAC_DATA_WIDTH-1:0] DAC_VALUE;

    wire signed [RESULT_MUL_ACC_WIDTH-1:0] SIN_MUL_ACC;
    wire signed [RESULT_MUL_ACC_WIDTH-1:0] COS_MUL_ACC;

    wire [PHASE_INCREMENT_BITS-1:0] CURRENT_PHASE_INCREMENT;

//wire signed [MUL_ACC_WIDTH-1:0] debug_sin_mul_acc_for_last_period;
//wire signed [MUL_ACC_WIDTH-1:0] debug_cos_mul_acc_for_last_period;

adc_dac_frontend
#(
    .PHASE_BITS(PHASE_BITS),
    .PHASE_INCREMENT_BITS(PHASE_INCREMENT_BITS),
    .PHASE_INCREMENT_FILTER_SHIFT_BITS(PHASE_INCREMENT_FILTER_SHIFT_BITS),
    .PHASE_INCREMENT_FILTER_STAGE_COUNT(PHASE_INCREMENT_FILTER_STAGE_COUNT),
    .PHASE_INCREMENT_FEEDBACK_FILTER_SHIFT_BITS(PHASE_INCREMENT_FEEDBACK_FILTER_SHIFT_BITS),
    .PHASE_INCREMENT_FEEDBACK_FILTER_STAGE_COUNT(PHASE_INCREMENT_FEEDBACK_FILTER_STAGE_COUNT),
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .SIN_TABLE_ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH),
    .ADC_DATA_WIDTH(ADC_DATA_WIDTH),
    .DAC_DATA_WIDTH(DAC_DATA_WIDTH),
    .PERIOD_ACC_ENABLE(PERIOD_ACC_ENABLE),
    .MUL_ACC_WIDTH(MUL_ACC_WIDTH),
    .RESULT_MUL_ACC_WIDTH(RESULT_MUL_ACC_WIDTH),
    .RESULT_FILTER_SHIFT_BITS(RESULT_FILTER_SHIFT_BITS),
    .RESULT_FILTER_STAGE_COUNT(RESULT_FILTER_STAGE_COUNT)
)
adc_dac_frontend_inst
(
    .*
);

wire dummy_bit;
sin_cos_dco
#(
    .PHASE_BITS(PHASE_BITS),
    .PHASE_INCREMENT_BITS(PHASE_INCREMENT_BITS),
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .SIN_TABLE_ADDR_WIDTH(SIN_TABLE_ADDR_WIDTH)
)
sin_cos_dco_adc_sim_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),

    .PHASE_INCREMENT_IN(PHASE_INCREMENT_IN),
    .PHASE_INCREMENT_IN_WE(CE),

    /* SIN value output */
    .SIN_VALUE( {ADC_VALUE, dummy_bit} ),
    .COS_VALUE()
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
             $display("    [%d]   nextCycle DAC=%d ADC=%d SIN_ACC=%d COS_ACC=%d  phase_inc=%h  atan2/2pi=%f", cycleCounter, DAC_VALUE, ADC_VALUE, SIN_MUL_ACC, COS_MUL_ACC, CURRENT_PHASE_INCREMENT, $atan2(COS_MUL_ACC, SIN_MUL_ACC)/2/3.1415926525);
             //$display("    [%d]   nextCycle DAC=%d ADC=%d SIN_ACC=%d COS_ACC=%d  phase_inc=%h    dbg_sin=%d dbg_cos=%d", cycleCounter, DAC_VALUE, ADC_VALUE, SIN_MUL_ACC, COS_MUL_ACC, CURRENT_PHASE_INCREMENT, debug_sin_mul_acc_for_last_period, debug_cos_mul_acc_for_last_period);
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
 
        @(posedge CLK) #1 //PHASE_INCREMENT_IN_WE = 0;

     
        $display("Starting verification");
    

        /* Outputs are delayed by 4 clock cycles */
        repeat (1000) begin
            nextCycle();
        end

        $display("*** Changing frequency");
        PHASE_INCREMENT_IN = 100377165; // 1018654.23 Hz signal at 40000000 Hz sample rate

        /* Outputs are delayed by 4 clock cycles */
        repeat (1000) begin
            nextCycle();
        end

        $display("*** Changing frequency to 100KHz");
        PHASE_INCREMENT_IN = 10035786;

        /* Outputs are delayed by 4 clock cycles */
        repeat (2000) begin
            nextCycle();
        end

        $display("*** Changing frequency to 101KHz");
        PHASE_INCREMENT_IN = 10135786;

        /* Outputs are delayed by 4 clock cycles */
        repeat (2000) begin
            nextCycle();
        end

        $display("Test passed");
        $finish();
    end


endmodule
