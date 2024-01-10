module ce_delay_after_reset_tb();

localparam DELAY_CYCLES = 16;

logic [15:0] cycleCounter = 0;
logic CLK;
logic CE;
logic RESET;

wire ce_out[33];

genvar i;
generate
    for (i = 0; i <= 32; i++)
        ce_delay_after_reset #( .DELAY_CYCLES(i) ) ce_delay_after_reset_inst2 ( .CLK(CLK), .CE(CE), .RESET(RESET), .CE_OUT(ce_out[i]) );
endgenerate


task nextCycle();
     begin
         @(posedge CLK) #1 ;
         $display("    [%d]   nextCycle CE=%b    ce0=%b ce1=%b ce2=%b ce3=%b ce4=%b ce5=%b ce6=%b ce7=%b ce8=%b ce16=%b ce32=%b", 
              cycleCounter, CE, 
              ce_out[0], ce_out[1], ce_out[2], ce_out[3], ce_out[4], ce_out[5], 
              ce_out[6], ce_out[7], ce_out[8], ce_out[16], ce_out[32] );
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
    #200 @(posedge CLK) RESET = 1;
    #100 @(posedge CLK) RESET = 0;
    $display("Starting CE delay test");
    cycleCounter = 0;
    nextCycle();
    nextCycle();
    CE = 1;

    repeat(40) nextCycle();
    $display("*** RESET !!!");
    RESET = 1;
    nextCycle();
    RESET = 0;
    repeat(40) nextCycle();

    $display("Test passed");
    $finish();

end



endmodule
