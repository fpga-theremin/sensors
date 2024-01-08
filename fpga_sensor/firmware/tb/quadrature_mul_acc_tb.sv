module quadrature_mul_acc_tb();

localparam SIN_TABLE_DATA_WIDTH = 13;
localparam ADC_DATA_WIDTH = 12;
localparam RESULT_WIDTH = 32;

    logic [15:0] cycleCounter = 0;
    logic CLK;
    logic CE;
    logic RESET;

reg signed [SIN_TABLE_DATA_WIDTH-1:0] SIN_VALUE;
reg signed [SIN_TABLE_DATA_WIDTH-1:0] COS_VALUE;
reg signed [ADC_DATA_WIDTH-1:0] ADC_VALUE;

wire UPDATED_RESULT;
wire signed [RESULT_WIDTH-1:0] SIN_RESULT;
wire signed [RESULT_WIDTH-1:0] COS_RESULT;

quadrature_mul_acc
#(
    .SIN_TABLE_DATA_WIDTH(SIN_TABLE_DATA_WIDTH),
    .ADC_DATA_WIDTH(ADC_DATA_WIDTH),
    .RESULT_WIDTH(RESULT_WIDTH)
)
quadrature_mul_acc_inst
(
    .*
);


    task nextCycle(input signed [SIN_TABLE_DATA_WIDTH-1:0] sin_value, input signed [SIN_TABLE_DATA_WIDTH-1:0] cos_value, input signed [ADC_DATA_WIDTH-1:0] adc_value);
         begin
             @(posedge CLK) #1 ;
             SIN_VALUE = sin_value;
             COS_VALUE = cos_value;
             ADC_VALUE = adc_value;
             $display("    [%d]   nextCycle SIN=%d COS=%d ADC=%d UPDATED=%b SIN_RESULT=%d COS_RESULT=%d ", cycleCounter, SIN_VALUE, COS_VALUE, ADC_VALUE, UPDATED_RESULT, SIN_RESULT, COS_RESULT);
             if (UPDATED_RESULT) begin
                 $display("    [%d]   *********   SIN_RESULT=%d COS_RESULT=%d ", cycleCounter, SIN_RESULT, COS_RESULT);
             end
         end
    endtask

    task checkResult(input signed [RESULT_WIDTH-1:0] expected_sin_value, input signed [RESULT_WIDTH-1:0] expected_cos_value);
         begin
                 $display("    [%d]        checking expected result for 2 recent z-crossing sin=%d cos=%d", cycleCounter, expected_sin_value, expected_cos_value);
             if (!UPDATED_RESULT) begin
                 $display("    [%d] ERROR: UPDATED_RESULT is expected to be 1 but actual 0", cycleCounter);
                 $finish();
             end
             //$display("    [%d]             SIN=%d COS=%d", cycleCounter, SIN_VALUE, COS_VALUE);
             if (SIN_RESULT != expected_sin_value) begin
                 $display("    [%d] ERROR: SIN_RESULT does not match with expected result, expected %x actual %x", cycleCounter, expected_sin_value, SIN_RESULT);
                 $finish();
             end
             if (COS_RESULT != expected_cos_value) begin
                 $display("    [%d] ERROR: COS_RESULT does not match with expected result, expected %x actual %x", cycleCounter, expected_cos_value, COS_RESULT);
                 $finish();
             end
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
        CE = 0; SIN_VALUE=0; COS_VALUE=0; ADC_VALUE=0;
        #200 @(posedge CLK) RESET = 1;
        #100 @(posedge CLK) RESET = 0;
        @(posedge CLK) #1 CE = 1;
        cycleCounter = 0;
 
        $display("Starting verification");
    
        nextCycle(1, 2, 1); // 1*1 2*1
        nextCycle(2, 3, 2); // 2*2 3*2
        nextCycle(1, 0, 1); // 1*1 0*1           

        $display("        zero crossing, acc: %d %d ", (1*1)+(2*2)+(1*1),   (2*1)+(3*2)+(0*1));
        
        nextCycle(2, 3, -1); // 2*-1 3*-1
        nextCycle(3, 4, -2); // 3*-2 4*-2
        nextCycle(2, -5, -3); // 2*-3 -5*-3
        nextCycle(1, -7, -1); // 1*-1 -7*-1      

                           checkResult((1*1)+(2*2)+(1*1),   (2*1)+(3*2)+(0*1) );

        $display("        zero crossing, acc: %d %d ", (2*-1)+(3*-2)+(2*-3)+(1*-1),   (3*-1)+(4*-2)+(-5*-3)+(-7*-1)  );

        nextCycle(-1, -3, 2); // -1*2 -3*2
        nextCycle(-2, -1, 5); // -2*5 -1*5
        nextCycle(-3, 2, 3);  // -3*3 2*3

        $display("        zero crossing, acc: %d %d ", (-1*2)+(-2*5)+(-3*3),   (-3*2)+(-1*5)+(2*3)  );

        nextCycle(200, 300, -100); // 200*-100 300*-100
                           checkResult((1*1)+(2*2)+(1*1) + (2*-1)+(3*-2)+(2*-3)+(1*-1),   (2*1)+(3*2)+(0*1) + (3*-1)+(4*-2)+(-5*-3)+(-7*-1) );
        nextCycle(900, -100, -300); // 900*-300 -100*-300
        nextCycle(800, -600, -100); // 800*-100 -600*-100

        $display("        zero crossing, acc: %d %d ", (200*-100)+(900*-300)+(800*-100),   (300*-100)+(-100*-300)+(-600*-100)  );
        
        nextCycle(-1, -1, 0); // -1*-1 -1*-1
                           checkResult((2*-1)+(3*-2)+(2*-3)+(1*-1) + (-1*2)+(-2*5)+(-3*3),   (3*-1)+(4*-2)+(-5*-3)+(-7*-1) + (-3*2)+(-1*5)+(2*3) );
        nextCycle(-1, -1, 1); // -1*-1 -1*-1
        nextCycle(-1, -1, 3); // -1*-1 -1*-1
        nextCycle(-1, -1, 1); // -1*-1 -1*-1
                           checkResult( (-1*2)+(-2*5)+(-3*3) + (200*-100)+(900*-300)+(800*-100),   (-3*2)+(-1*5)+(2*3) + (300*-100)+(-100*-300)+(-600*-100) );

        $display("Test passed");
        $finish();
    end

endmodule
