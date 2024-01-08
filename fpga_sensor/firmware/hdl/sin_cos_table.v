/*********************************************/
/* SINE table of size 4096 and 13-bit values */
/* Latency: 4 CLK cycles                     */
/*********************************************/
module sin_cos_table #(
    // precision of values in SIN table (DATA_WIDTH bits will be stored in ROM - w/o sign)
    parameter DATA_WIDTH = 13,
    // actual table size should be 1/4 of (1<<ADDR_WIDTH)
    parameter ADDR_WIDTH = 12,
    // specify init file to SIN table ROM with
    parameter INIT_FILE = "sin_table_4096_13bit.mem"
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,
    
    /* sine table address to get value from                  */
    input wire [ADDR_WIDTH-1:0] PHASE,
    /* sin sample value from ROM, delayed by 4 clock cycles */
    output wire [DATA_WIDTH-1:0] SIN_VALUE,
    /* cos sample value from ROM, delayed by 4 clock cycles */
    output wire [DATA_WIDTH-1:0] COS_VALUE
    
);
    
    
    /* ROM size: we need to store only 1/4 of table entries, the rest may be obtained by mirroring and changing the sign */
    localparam MEMSIZE = 1 << (ADDR_WIDTH - 2);
    
    /* first quarter of table always has 0 in upper bit, so we can store only DATA_WIDTH-1 bits in the table */
    (* ram_style = "block" *)  
    reg [DATA_WIDTH-2:0] memory[0:MEMSIZE-1];
           
    /* store input phase value in register */
    reg [ADDR_WIDTH-1:0] phase_stage0;
    always @(posedge CLK)
        if (RESET)
            phase_stage0 <= 0;
        else if (CE)
            phase_stage0 <= PHASE;
     
    wire [1:0] sin_phase_top = phase_stage0[ADDR_WIDTH-1:ADDR_WIDTH-2];
    wire [1:0] cos_phase_top = phase_stage0[ADDR_WIDTH-1:ADDR_WIDTH-2] + 1;
     
    /* second half of table is the same as first, but negative */
    wire sin_need_sign_change = sin_phase_top[1];
    /* second and third quarters of table are mirrored by phase */
    wire sin_need_phase_inverse = sin_phase_top[0];
     
    /* second half of table is the same as first, but negative */
    wire cos_need_sign_change = cos_phase_top[1];
    /* second and third quarters of table are mirrored by phase */
    wire cos_need_phase_inverse = cos_phase_top[0];
     
    /* SIN: invert phase to have mirrored value */
    wire [ADDR_WIDTH-3:0] sin_table_entry_index = phase_stage0[ADDR_WIDTH-3:0] ^ { (ADDR_WIDTH-2) {sin_need_phase_inverse} };
    /* COS: invert phase to have mirrored value */
    wire [ADDR_WIDTH-3:0] cos_table_entry_index = phase_stage0[ADDR_WIDTH-3:0] ^ { (ADDR_WIDTH-2) {cos_need_phase_inverse} };
     
    /* propagate sign change flag to support negative values */
    reg sin_need_sign_change_stage1;
    reg cos_need_sign_change_stage1;
    reg sin_need_sign_change_stage2;
    reg cos_need_sign_change_stage2;
     
    always @(posedge CLK)
        if (RESET) begin
            sin_need_sign_change_stage1 <= 0;
            sin_need_sign_change_stage2 <= 0;
            cos_need_sign_change_stage1 <= 0;
            cos_need_sign_change_stage2 <= 0;
        end else if (CE) begin
            sin_need_sign_change_stage1 <= sin_need_sign_change;
            sin_need_sign_change_stage2 <= sin_need_sign_change_stage1;
            cos_need_sign_change_stage1 <= cos_need_sign_change;
            cos_need_sign_change_stage2 <= cos_need_sign_change_stage1;
        end
            
    /* SIN table: block RAM based ROM - first stage of pipeline */
    reg [DATA_WIDTH-2:0] sin_rddata_stage1;
    always @(posedge CLK)
        if (RESET)
            sin_rddata_stage1 <= 0;
        else if (CE)
            sin_rddata_stage1 <= memory[sin_table_entry_index];
     
    /* COS table: block RAM based ROM - first stage of pipeline */
    reg [DATA_WIDTH-2:0] cos_rddata_stage1;
    always @(posedge CLK)
        if (RESET)
            cos_rddata_stage1 <= 0;
        else if (CE)
            cos_rddata_stage1 <= memory[cos_table_entry_index];
     
    /* SIN table: block RAM based ROM - second stage of pipeline */
    reg [DATA_WIDTH-2:0] sin_rddata_stage2;
    always @(posedge CLK)
        if (RESET)
            sin_rddata_stage2 <= 0;
        else if (CE)
            sin_rddata_stage2 <= sin_rddata_stage1;
     
    /* COS table: block RAM based ROM - second stage of pipeline */
    reg [DATA_WIDTH-2:0] cos_rddata_stage2;
    always @(posedge CLK)
        if (RESET)
            cos_rddata_stage2 <= 0; 
        else if (CE)
            cos_rddata_stage2 <= cos_rddata_stage1;
     
    /* SIN: this is output stage register, need sign bit as well */
    reg [DATA_WIDTH-1:0] sin_rddata_stage3;
    always @(posedge CLK)
        if (RESET)
            sin_rddata_stage3 <= 0; 
        else if (CE)
            sin_rddata_stage3 <= sin_need_sign_change_stage2 ? -{1'b0, sin_rddata_stage2} : {1'b0, sin_rddata_stage2}; 
     
    /* COS: this is output stage register, need sign bit as well */
    reg [DATA_WIDTH-1:0] cos_rddata_stage3;
    always @(posedge CLK)
        if (RESET)
            cos_rddata_stage3 <= 0; 
        else if (CE)
            cos_rddata_stage3 <= cos_need_sign_change_stage2 ? -{1'b0, cos_rddata_stage2} : {1'b0, cos_rddata_stage2}; 
     
    /* propagate value to output */
    assign SIN_VALUE = sin_rddata_stage3;
    assign COS_VALUE = cos_rddata_stage3;
    

initial
    $readmemh(INIT_FILE, memory, 0, MEMSIZE-1);

endmodule
