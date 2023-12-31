module lp_filter_stage_signed_tb();

localparam IN_DATA_BITS = 30;
localparam OUT_DATA_BITS = 30;
localparam SHIFT_BITS = 5;

    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;

logic signed [IN_DATA_BITS-1:0] IN_VALUE;
wire signed [OUT_DATA_BITS-1:0] OUT_VALUE;


lp_filter_stage_signed
#(
    .IN_DATA_BITS(IN_DATA_BITS),
    .OUT_DATA_BITS(OUT_DATA_BITS),
    .SHIFT_BITS(SHIFT_BITS)
)
lp_filter_stage_signed_inst
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
             $display("    [%d]   nextCycle IN=%d OUT=%d    %b    %b    diff=%b", cycleCounter, IN_VALUE, OUT_VALUE, IN_VALUE, OUT_VALUE, OUT_VALUE-IN_VALUE);
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

        IN_VALUE = -IN_VALUE * 2;
        $display("*** Changed targed value to %d", IN_VALUE);

        repeat (1000)
            nextCycle();

        $display("Test passed");
        $finish();
    end

endmodule
