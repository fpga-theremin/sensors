module variable_delay_shift_register_tb();

localparam DATA_BITS = 32;
localparam DELAY_BITS = 4;

logic [DATA_BITS-1:0] cycleCounter = 0;
logic CLK;
logic CE;
logic RESET;

logic [DELAY_BITS-1:0] DELAY;
logic [DATA_BITS-1:0] IN_VALUE;
wire [DATA_BITS-1:0] OUT_VALUE;

assign IN_VALUE = cycleCounter;

variable_delay_shift_register
#(
    .DATA_BITS(DATA_BITS),
    .DELAY_BITS(DELAY_BITS)
)
variable_delay_shift_register_inst
(
    .*
);

task nextCycle();
     begin
         @(posedge CLK) #1 ;
         $display("    [%d]   nextCycle IN=%d OUT=%d  IN-OUT=%d", cycleCounter, IN_VALUE, OUT_VALUE, IN_VALUE-OUT_VALUE);
     end
endtask

task setDelay(input signed [DELAY_BITS-1:0] new_delay);
     begin
         @(posedge CLK) #1 ;
         DELAY = new_delay;
         $display("    [%d]           new delay is set: %d", cycleCounter, DELAY);
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
    DELAY = 0;
    #200 @(posedge CLK) RESET = 1;
    #100 @(posedge CLK) RESET = 0;
    @(posedge CLK) #1 CE = 1;
    $display("Starting shift register test");
    cycleCounter = 0;

    for (int i = 0; i < (1<<DELAY_BITS); i++) begin
        setDelay(i);
        repeat(10) nextCycle();
    end

    setDelay(0);
    repeat(10) nextCycle();

    $display("Test passed");
    $finish();

end



endmodule
