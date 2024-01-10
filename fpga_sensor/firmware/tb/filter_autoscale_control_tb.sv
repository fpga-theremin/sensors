module filter_autoscale_control_tb();

localparam TOP_DATA_BITS = 4;
localparam DELAY_BITS = 4;

    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;


    /* initiate possible update of DELAY                     */
logic UPDATE;

    /* top bits of moving avg filtered out for SIN */
logic signed [TOP_DATA_BITS-1:0] TOP_SIN;
    /* top bits of moving avg filtered out for COS */
logic signed [TOP_DATA_BITS-1:0] TOP_COS;

    /* variable delay, (DELAY+1) cycles */
wire [DELAY_BITS-1:0] DELAY;
    // 1 for one CLK+CE cycle when DELAY output is changed
wire DELAY_UPDATED;

wire debug_update_stage0;
wire debug_update_stage1;
wire debug_update_stage2;
wire debug_update_stage3;
wire [TOP_DATA_BITS-2:0] debug_abs_sin_stage0;
wire [TOP_DATA_BITS-2:0] debug_abs_cos_stage0;
wire [TOP_DATA_BITS-2:0] debug_amplitude_stage1;
wire [1:0] debug_action_stage2;

filter_autoscale_control
#(
    .TOP_DATA_BITS(4),
    .DELAY_BITS(4)
)
filter_autoscale_control_inst
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
             $display("    [%d]   nextCycle  upd=%d  TOP_SIN=%d TOP_COS=%d   DELAY=%d DELAY_UPDATED=%d    upd0=%d upd1=%d upd2=%d upd3=%d    abs_sin=%d abs_cos=%d amp=%d action=%d",
             //             1                     2           3          4          5                6          7       8       9       10           11         12      13       14             
                  cycleCounter,  // 1 
                  UPDATE, TOP_SIN, TOP_COS,  // 2 3 4 
                  DELAY, DELAY_UPDATED      // 5 6
                  , debug_update_stage0 , debug_update_stage1 , debug_update_stage2 , debug_update_stage3 // 7 8 9 10 
                  , debug_abs_sin_stage0 , debug_abs_cos_stage0 , debug_amplitude_stage1 , debug_action_stage2 // 11 12 13 14
             );
         end
    endtask

    task updateInputs(input signed [TOP_DATA_BITS-1:0] sin_value, input signed [TOP_DATA_BITS-1:0] cos_value);
         begin
             TOP_SIN = sin_value; TOP_COS = cos_value; UPDATE = 1;
             $display("    [%d]   ***      New inputs: sin=%d cos=%d", cycleCounter, sin_value, cos_value);
             nextCycle();
             TOP_SIN = 0; TOP_COS = 0; UPDATE = 0;
         end
    endtask
    
    task updateFromDelay(input int divider);
        begin
            updateInputs((DELAY+1)*3/divider, (DELAY+1)*4/divider);
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

        TOP_SIN = 0;
        TOP_COS = 0;
        UPDATE = 0;
 
        $display("Starting verification");

        repeat (10)
            nextCycle();

        updateInputs(1, -2);
        repeat (3)
            nextCycle();
        updateInputs(1, -2);
        repeat (3)
            nextCycle();
        updateInputs(1, -2);
        repeat (3)
            nextCycle();
        updateInputs(1, -2);
        repeat (3)
            nextCycle();
        updateInputs(1, -2);
        repeat (3)
            nextCycle();


        updateInputs(3, 2);
        repeat (3)
            nextCycle();
        updateInputs(3, 2);
        repeat (3)
            nextCycle();
        updateInputs(1, 2);
        repeat (3)
            nextCycle();
    
        updateInputs(7, 1);
        repeat (10)
            nextCycle();
        updateInputs(7, 1);
        repeat (10)
            nextCycle();
        updateInputs(7, 1);
        repeat (10)
            nextCycle();
        updateInputs(7, 1);
        repeat (10)
            nextCycle();
        updateInputs(7, 1);
        repeat (10)
            nextCycle();
    
        updateInputs(2, -2);
        repeat (10)
            nextCycle();
        updateInputs(2, -2);
        repeat (10)
            nextCycle();
        updateInputs(2, -2);
        repeat (10)
            nextCycle();
    
        updateInputs(-8, 3);
        repeat (10)
            nextCycle();

        $display("Auto, amplitude divider = 12");

        repeat (30) begin
            updateFromDelay(12);    
            repeat (5)
                nextCycle();
        end

        $display("Auto, amplitude divider = 11");

        repeat (30) begin
            updateFromDelay(11);    
            repeat (5)
                nextCycle();
        end

        $display("Auto, amplitude divider = 9");

        repeat (30) begin
            updateFromDelay(9);    
            repeat (5)
                nextCycle();
        end

        $display("Auto, amplitude divider = 19");

        repeat (30) begin
            updateFromDelay(19);    
            repeat (5)
                nextCycle();
        end

        $display("Auto, amplitude divider = 22");

        repeat (30) begin
            updateFromDelay(22);    
            repeat (5)
                nextCycle();    
        end


        $display("Test passed");
        $finish();
    end

endmodule
