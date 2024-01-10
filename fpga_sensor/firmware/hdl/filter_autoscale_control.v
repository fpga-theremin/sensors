/*
    Auto-scale control for moving_avg_filter
*/
module filter_autoscale_control
#(
    parameter TOP_DATA_BITS = 4,
    parameter DELAY_BITS = 4
)
(
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = put new value and update output, 0 = no changes */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    /* initiate possible update of DELAY                     */
    input wire UPDATE,

    /* top bits of moving avg filtered out for SIN */
    input wire signed [TOP_DATA_BITS-1:0] TOP_SIN,
    /* top bits of moving avg filtered out for COS */
    input wire signed [TOP_DATA_BITS-1:0] TOP_COS,

    /* variable delay, (DELAY+1) cycles */
    output wire [DELAY_BITS-1:0] DELAY,
    // 1 for one CLK+CE cycle when DELAY output is changed
    output wire DELAY_UPDATED
    

    , output wire debug_update_stage0
    , output wire debug_update_stage1
    , output wire debug_update_stage2
    , output wire debug_update_stage3
    , output wire [TOP_DATA_BITS-2:0] debug_abs_sin_stage0
    , output wire [TOP_DATA_BITS-2:0] debug_abs_cos_stage0
    , output wire [TOP_DATA_BITS-2:0] debug_amplitude_stage1
    , output wire [1:0] debug_action_stage2
);

// 1 when CE and UPDATE requested, skipped for first 4 cases after reset
// after update_stage0 set to 1, disable update start for next 4 cycles
wire update_protected;
reg update_stage0;
reg update_stage1;
reg update_stage2;
reg update_stage3;


//assign update_stage0 = CE & UPDATE;
ce_delay_after_reset #( .DELAY_CYCLES(4) ) ce_protector ( .CLK(CLK), .CE(CE & UPDATE), .RESET(RESET|update_stage1), .CE_OUT(update_protected) );
always @(posedge CLK) begin
    if (RESET) begin
        update_stage0 <= 0;
        update_stage1 <= 0;
        update_stage2 <= 0;
        update_stage3 <= 0;
    end else if (CE) begin
        update_stage0 <= update_protected;
        update_stage1 <= update_stage0;
        update_stage2 <= update_stage1;
        update_stage3 <= update_stage2;
    end
end

// stage0 : calculate absolute values of sin and cos
reg [TOP_DATA_BITS-2:0] abs_sin_stage0;
reg [TOP_DATA_BITS-2:0] abs_cos_stage0;

always @(posedge CLK) begin
    if (RESET) begin
        abs_sin_stage0 <= 0;
        abs_cos_stage0 <= 0;
    end else if (update_protected) begin
        abs_sin_stage0 <= (TOP_SIN == {TOP_DATA_BITS{1'b1}}) ? {TOP_DATA_BITS-1{1'b1}}      // min negative - use max positive
                 : TOP_SIN[DELAY_BITS-1]              ? -TOP_SIN                     // change sign if negative is in range
                 :                                       TOP_SIN[TOP_DATA_BITS-2:0]; // positive -- just remove sign bit
        abs_cos_stage0 <= (TOP_COS == {TOP_DATA_BITS{1'b1}}) ? {TOP_DATA_BITS-1{1'b1}}      // min negative - use max positive
                 : TOP_COS[DELAY_BITS-1]              ? -TOP_COS                     // change sign if negative is in range
                 :                                       TOP_COS[TOP_DATA_BITS-2:0]; // positive -- just remove sign bit
    end
end

// stage1: calculate max possible sqrt(sin*sin + cos*cos) for given abs upper bits
reg [TOP_DATA_BITS-2:0] amplitude_stage1;
always @(posedge CLK) begin
    if (RESET) begin
        amplitude_stage1 <= 0;
    end else if (update_stage0 & CE) begin
        case ({abs_sin_stage0, abs_cos_stage0})
        6'b000_000: amplitude_stage1 <= 0;
        6'b000_001: amplitude_stage1 <= 1;
        6'b000_010: amplitude_stage1 <= 2;
        6'b000_011: amplitude_stage1 <= 3;
        6'b000_100: amplitude_stage1 <= 4;
        6'b000_101: amplitude_stage1 <= 5;
        6'b000_110: amplitude_stage1 <= 6;
        6'b000_111: amplitude_stage1 <= 7;
        6'b001_000: amplitude_stage1 <= 1;
        6'b001_001: amplitude_stage1 <= 1;
        6'b001_010: amplitude_stage1 <= 2;
        6'b001_011: amplitude_stage1 <= 3;
        6'b001_100: amplitude_stage1 <= 4;
        6'b001_101: amplitude_stage1 <= 5;
        6'b001_110: amplitude_stage1 <= 6;
        6'b001_111: amplitude_stage1 <= 7;
        6'b010_000: amplitude_stage1 <= 2;
        6'b010_001: amplitude_stage1 <= 2;
        6'b010_010: amplitude_stage1 <= 2;
        6'b010_011: amplitude_stage1 <= 3;
        6'b010_100: amplitude_stage1 <= 4;
        6'b010_101: amplitude_stage1 <= 5;
        6'b010_110: amplitude_stage1 <= 6;
        6'b010_111: amplitude_stage1 <= 7;
        6'b011_000: amplitude_stage1 <= 3;
        6'b011_001: amplitude_stage1 <= 3;
        6'b011_010: amplitude_stage1 <= 3;
        6'b011_011: amplitude_stage1 <= 4;
        6'b011_100: amplitude_stage1 <= 5;
        6'b011_101: amplitude_stage1 <= 5;
        6'b011_110: amplitude_stage1 <= 6;
        6'b011_111: amplitude_stage1 <= 7;
        6'b100_000: amplitude_stage1 <= 4;
        6'b100_001: amplitude_stage1 <= 4;
        6'b100_010: amplitude_stage1 <= 4;
        6'b100_011: amplitude_stage1 <= 5;
        6'b100_100: amplitude_stage1 <= 5;
        6'b100_101: amplitude_stage1 <= 6;
        6'b100_110: amplitude_stage1 <= 7;
        6'b100_111: amplitude_stage1 <= 7;
        6'b101_000: amplitude_stage1 <= 5;
        6'b101_001: amplitude_stage1 <= 5;
        6'b101_010: amplitude_stage1 <= 5;
        6'b101_011: amplitude_stage1 <= 5;
        6'b101_100: amplitude_stage1 <= 6;
        6'b101_101: amplitude_stage1 <= 7;
        6'b101_110: amplitude_stage1 <= 7;
        6'b101_111: amplitude_stage1 <= 7;
        6'b110_000: amplitude_stage1 <= 6;
        6'b110_001: amplitude_stage1 <= 6;
        6'b110_010: amplitude_stage1 <= 6;
        6'b110_011: amplitude_stage1 <= 6;
        6'b110_100: amplitude_stage1 <= 7;
        6'b110_101: amplitude_stage1 <= 7;
        6'b110_110: amplitude_stage1 <= 7;
        6'b110_111: amplitude_stage1 <= 7;
        6'b111_000: amplitude_stage1 <= 7;
        6'b111_001: amplitude_stage1 <= 7;
        6'b111_010: amplitude_stage1 <= 7;
        6'b111_011: amplitude_stage1 <= 7;
        6'b111_100: amplitude_stage1 <= 7;
        6'b111_101: amplitude_stage1 <= 7;
        6'b111_110: amplitude_stage1 <= 7;
        6'b111_111: amplitude_stage1 <= 7;        
        endcase
    end
end

reg [DELAY_BITS-2:0] scale;
// 
assign DELAY = {scale, 1'b1};


// stage1: calculate max possible sqrt(sin*sin + cos*cos) for given abs upper bits
reg [1:0] action_stage2;
always @(posedge CLK) begin
    if (RESET) begin
        action_stage2 <= 0;
    end else if (update_stage1 & CE) begin
        case ({scale, amplitude_stage1})
        6'b000_000: action_stage2 <= 2;
        6'b000_001: action_stage2 <= 2;
        6'b000_010: action_stage2 <= 2;
        6'b000_011: action_stage2 <= 0;
        6'b000_100: action_stage2 <= 0;
        6'b000_101: action_stage2 <= 0;
        6'b000_110: action_stage2 <= 0;
        6'b000_111: action_stage2 <= 0;
        6'b001_000: action_stage2 <= 2;
        6'b001_001: action_stage2 <= 2;
        6'b001_010: action_stage2 <= 2;
        6'b001_011: action_stage2 <= 2;
        6'b001_100: action_stage2 <= 0;
        6'b001_101: action_stage2 <= 0;
        6'b001_110: action_stage2 <= 1;
        6'b001_111: action_stage2 <= 1;
        6'b010_000: action_stage2 <= 2;
        6'b010_001: action_stage2 <= 2;
        6'b010_010: action_stage2 <= 2;
        6'b010_011: action_stage2 <= 2;
        6'b010_100: action_stage2 <= 0;
        6'b010_101: action_stage2 <= 0;
        6'b010_110: action_stage2 <= 1;
        6'b010_111: action_stage2 <= 1;
        6'b011_000: action_stage2 <= 2;
        6'b011_001: action_stage2 <= 2;
        6'b011_010: action_stage2 <= 2;
        6'b011_011: action_stage2 <= 2;
        6'b011_100: action_stage2 <= 0;
        6'b011_101: action_stage2 <= 0;
        6'b011_110: action_stage2 <= 1;
        6'b011_111: action_stage2 <= 1;
        6'b100_000: action_stage2 <= 2;
        6'b100_001: action_stage2 <= 2;
        6'b100_010: action_stage2 <= 2;
        6'b100_011: action_stage2 <= 2;
        6'b100_100: action_stage2 <= 2;
        6'b100_101: action_stage2 <= 0;
        6'b100_110: action_stage2 <= 1;
        6'b100_111: action_stage2 <= 1;
        6'b101_000: action_stage2 <= 2;
        6'b101_001: action_stage2 <= 2;
        6'b101_010: action_stage2 <= 2;
        6'b101_011: action_stage2 <= 2;
        6'b101_100: action_stage2 <= 2;
        6'b101_101: action_stage2 <= 0;
        6'b101_110: action_stage2 <= 1;
        6'b101_111: action_stage2 <= 1;
        6'b110_000: action_stage2 <= 2;
        6'b110_001: action_stage2 <= 2;
        6'b110_010: action_stage2 <= 2;
        6'b110_011: action_stage2 <= 2;
        6'b110_100: action_stage2 <= 2;
        6'b110_101: action_stage2 <= 0;
        6'b110_110: action_stage2 <= 1;
        6'b110_111: action_stage2 <= 1;
        6'b111_000: action_stage2 <= 0;
        6'b111_001: action_stage2 <= 0;
        6'b111_010: action_stage2 <= 0;
        6'b111_011: action_stage2 <= 0;
        6'b111_100: action_stage2 <= 0;
        6'b111_101: action_stage2 <= 0;
        6'b111_110: action_stage2 <= 1;
        6'b111_111: action_stage2 <= 1;
        endcase
    end
end

always @(posedge CLK) begin
    if (RESET) begin
        scale <= 0;
    end else if (update_stage2 & CE) begin
        if (action_stage2 == 1)
            scale <= scale - 1;
        else if (action_stage2 == 2)
            scale <= scale + 1;
    end
end

assign DELAY_UPDATED = update_stage3;

assign debug_update_stage0 = update_stage0;
assign debug_update_stage1 = update_stage1;
assign debug_update_stage2 = update_stage2;
assign debug_update_stage3 = update_stage3;
assign debug_abs_sin_stage0 = abs_sin_stage0;
assign debug_abs_cos_stage0 = abs_cos_stage0;
assign debug_amplitude_stage1 = amplitude_stage1;
assign debug_action_stage2 = action_stage2;

endmodule
