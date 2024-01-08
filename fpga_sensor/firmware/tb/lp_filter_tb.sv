module lp_filter_tb();

localparam IN_DATA_BITS = 32;
localparam OUT_DATA_BITS = 32;
localparam SHIFT_BITS = 5;

    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;

logic [IN_DATA_BITS-1:0] IN_VALUE;
wire [OUT_DATA_BITS-1:0] OUT_VALUES[4:0];


lp_filter #( .IN_DATA_BITS(IN_DATA_BITS), .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS), .STAGE_COUNT(0) ) lp_filter_0_stages_inst
(
    /* input clock                                           */
    .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(IN_VALUE), .OUT_VALUE(OUT_VALUES[0])
);

lp_filter #( .IN_DATA_BITS(IN_DATA_BITS), .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS), .STAGE_COUNT(1) ) lp_filter_1_stages_inst
(
    /* input clock                                           */
    .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(IN_VALUE), .OUT_VALUE(OUT_VALUES[1])
);

lp_filter #( .IN_DATA_BITS(IN_DATA_BITS), .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS), .STAGE_COUNT(2) ) lp_filter_2_stages_inst
(
    /* input clock                                           */
    .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(IN_VALUE), .OUT_VALUE(OUT_VALUES[2])
);

lp_filter #( .IN_DATA_BITS(IN_DATA_BITS), .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS), .STAGE_COUNT(3) ) lp_filter_3_stages_inst
(
    /* input clock                                           */
    .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(IN_VALUE), .OUT_VALUE(OUT_VALUES[3])
);

lp_filter #( .IN_DATA_BITS(IN_DATA_BITS), .OUT_DATA_BITS(OUT_DATA_BITS), .SHIFT_BITS(SHIFT_BITS), .STAGE_COUNT(4) ) lp_filter_4_stages_inst
(
    /* input clock                                           */
    .CLK(CLK), .CE(CE), .RESET(RESET), .IN_VALUE(IN_VALUE), .OUT_VALUE(OUT_VALUES[4])
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
             $display("    [%d]   nextCycle IN=%d OUT0=%d OUT1=%d OUT2=%d OUT3=%d OUT4=%d  in:%b out:%b", cycleCounter, IN_VALUE, OUT_VALUES[0], OUT_VALUES[1], OUT_VALUES[2], OUT_VALUES[3], OUT_VALUES[4], IN_VALUE, OUT_VALUES[4]);
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
        IN_VALUE = 109377165; // 1018654.23 Hz signal at 40000000 Hz sample rate
 
        $display("Starting verification");
    
        $display("*** Changed targed value to %d", IN_VALUE);

        repeat (2000)
            nextCycle();

        IN_VALUE = IN_VALUE / 2;
        $display("*** Changed targed value to %d", IN_VALUE);

        repeat (1000)
            nextCycle();

        IN_VALUE = IN_VALUE * 2;
        $display("*** Changed targed value to %d", IN_VALUE);

        repeat (1000)
            nextCycle();

        $display("Test passed");
        $finish();
    end

endmodule
