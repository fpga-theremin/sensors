/*
    Calculate SIN + COS from phase using combined lookup table + CORDIC algorithm.
    Optimized for low end devices like Lattice iCE40 Ultra 4K with LUT4 and small BRAMs.
    Step1: use looup table to get initial SIN and COS values for 2^STEP1_PHASE_BITS angles in range 0..PI/4 
    Step2: use CORDIC to rotate vector from Step1 by small angle, one of 2^STEP2_PHASE_BITS
    Latency: 2 + STEP2_PHASE_BITS + 1 CLK cycles (2 cycles for BRAM lookup, STEP2_PHASE_BITS cycles for CORDIC rotations, 1 cycle for final result rounding 
*/
module cordic_sin_cos #(
    parameter DATA_BITS = 16,
    // number of bits in input phase, higher bits used if too much bits are provided
    // really used: 3 + STEP1_PHASE_BITS + STEP2_PHASE_BITS bits 
    parameter PHASE_BITS = 19,
    // size of PI/4 sin&cos lookup table (7 is 128)
    parameter STEP1_PHASE_BITS = 7,
    // size of rotations table (9 is 512)
    parameter STEP2_PHASE_BITS = 9
) (
    /* input clock                                           */
    input wire CLK,
    /* clock enable, 1 = enable pipeline, 0 = pause pipeline */
    input wire CE,
    /* reset signal, active 1                                */
    input wire RESET,

    input wire [PHASE_BITS-1:0] PHASE,
    output wire signed [DATA_BITS-1:0] SIN,
    output wire signed [DATA_BITS-1:0] COS
    
//    , output wire debug_swap_sin_and_cos_delayed
//    , output wire debug_neg_sin_delayed
//    , output wire debug_neg_cos_delayed
//    , output wire [PHASE_BITS-1:0] debug_PHASE
//    , output wire [STEP1_PHASE_BITS-1:0] debug_step1_lookup_addr
//    , output wire [STEP2_PHASE_BITS-1:0] debug_step2_lookup_addr
);

localparam SIN_COS_LOOKUP_DATA_BITS = 16;
localparam ROTATIONS_LOOKUP_DATA_BITS = 8;
localparam ROTATIONS = ROTATIONS_LOOKUP_DATA_BITS;

// top 3 bits of phase are splitting period into PI/4 ranges.
// all calculations are being made for 0..PI/4, then transformations like swap sin and cos or sign inversion are applied

wire inv_lookup_addr = PHASE[PHASE_BITS-3];
wire swap_sin_and_cos = PHASE[PHASE_BITS-3] ^ PHASE[PHASE_BITS-2];
wire [1:0] quadrant = PHASE[PHASE_BITS-1:PHASE_BITS-2];
wire neg_cos = (quadrant == 2'b01) || (quadrant == 2'b10);
wire neg_sin = (quadrant == 2'b10) || (quadrant == 2'b11);

wire swap_sin_and_cos_delayed;
wire neg_cos_delayed;
wire neg_sin_delayed;

//assign debug_swap_sin_and_cos_delayed = PHASE[17] ^ PHASE[16]; //PHASE[PHASE_BITS-3] ^ PHASE[PHASE_BITS-2]; //swap_sin_and_cos;
//assign debug_neg_sin_delayed = PHASE[0];
//assign debug_neg_cos_delayed = PHASE[PHASE_BITS-1]; //neg_cos;
//assign debug_PHASE = PHASE;


variable_delay_shift_register #(
    // data width to store
    .DATA_BITS(3),
    .DELAY_BITS(4)
)
variable_delay_shift_register_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),
    .DELAY(ROTATIONS + 2),
    .IN_VALUE( {swap_sin_and_cos, neg_cos, neg_sin} ),
    .OUT_VALUE( {swap_sin_and_cos_delayed, neg_cos_delayed, neg_sin_delayed} )
);

localparam STEP1_MSB = PHASE_BITS-3-1;
localparam STEP1_LSB = STEP1_MSB - STEP1_PHASE_BITS + 1;
localparam STEP2_MSB = STEP1_LSB-1;
localparam STEP2_LSB = STEP2_MSB - STEP2_PHASE_BITS + 1;

wire [STEP1_PHASE_BITS-1:0] step1_lookup_addr = PHASE[STEP1_MSB:STEP1_LSB] ^ { STEP1_PHASE_BITS {inv_lookup_addr}};
wire [STEP2_PHASE_BITS-1:0] step2_lookup_addr = PHASE[STEP2_MSB:STEP2_LSB] ^ { STEP2_PHASE_BITS {inv_lookup_addr}};

//assign debug_step1_lookup_addr = step1_lookup_addr; 
//assign debug_step2_lookup_addr = step2_lookup_addr;


// optimization: COS top bit for angles 0..PI/4 is always 1, SIN is 1 if exceeds bound 
localparam SIN_HIGH_BIT_0_BOUND = 84; // for angle <= bound top bit of sin is 0, otherwise 1
wire sin_value_top_bit =  (step1_lookup_addr>SIN_HIGH_BIT_0_BOUND) ? 1'b1 : 1'b0;
// optimization: first rotation can be get from phase bit, avoiding storing of one bit of data 
wire first_rotation_sign = PHASE[STEP2_MSB] ^ 1'b1;

reg sin_value_top_bit_stage0;
reg first_rotation_sign_stage0;
reg sin_value_top_bit_stage1;
reg first_rotation_sign_stage1;
always @(posedge CLK) begin
    if (RESET) begin
        sin_value_top_bit_stage0 <= 0;
        first_rotation_sign_stage0 <= 0;
        sin_value_top_bit_stage1 <= 0;
        first_rotation_sign_stage1 <= 0;
    end else if (CE) begin
        sin_value_top_bit_stage0 <= sin_value_top_bit;
        first_rotation_sign_stage0 <= first_rotation_sign;
        sin_value_top_bit_stage1 <= sin_value_top_bit_stage0;
        first_rotation_sign_stage1 <= first_rotation_sign_stage0;
    end
end

reg [SIN_COS_LOOKUP_DATA_BITS*2-1:0] sin_cos_data_stage0 = 0;
reg [ROTATIONS_LOOKUP_DATA_BITS-1:0] rotation_data_stage0 = 0;

always @(posedge CLK) begin
    if (RESET) begin
        //sin_cos_data_stage0 <= 0;
    end else if (CE) begin
        // SIN and COS lookup table ROM content
        case(step1_lookup_addr)
        0: sin_cos_data_stage0 <= 32'h0192fffb;
        1: sin_cos_data_stage0 <= 32'h04b6fff6;
        2: sin_cos_data_stage0 <= 32'h07daffec;
        3: sin_cos_data_stage0 <= 32'h0afeffdd;
        4: sin_cos_data_stage0 <= 32'h0e22ffc9;
        5: sin_cos_data_stage0 <= 32'h1146ffb1;
        6: sin_cos_data_stage0 <= 32'h146aff93;
        7: sin_cos_data_stage0 <= 32'h178dff70;
        8: sin_cos_data_stage0 <= 32'h1ab0ff49;
        9: sin_cos_data_stage0 <= 32'h1dd3ff1d;
        10: sin_cos_data_stage0 <= 32'h20f6feeb;
        11: sin_cos_data_stage0 <= 32'h2418feb5;
        12: sin_cos_data_stage0 <= 32'h273afe7a;
        13: sin_cos_data_stage0 <= 32'h2a5cfe3a;
        14: sin_cos_data_stage0 <= 32'h2d7dfdf5;
        15: sin_cos_data_stage0 <= 32'h309efdab;
        16: sin_cos_data_stage0 <= 32'h33befd5c;
        17: sin_cos_data_stage0 <= 32'h36defd08;
        18: sin_cos_data_stage0 <= 32'h39fefcb0;
        19: sin_cos_data_stage0 <= 32'h3d1cfc52;
        20: sin_cos_data_stage0 <= 32'h403bfbf0;
        21: sin_cos_data_stage0 <= 32'h4358fb88;
        22: sin_cos_data_stage0 <= 32'h4675fb1c;
        23: sin_cos_data_stage0 <= 32'h4991faab;
        24: sin_cos_data_stage0 <= 32'h4cadfa35;
        25: sin_cos_data_stage0 <= 32'h4fc8f9ba;
        26: sin_cos_data_stage0 <= 32'h52e2f93a;
        27: sin_cos_data_stage0 <= 32'h55fbf8b6;
        28: sin_cos_data_stage0 <= 32'h5913f82c;
        29: sin_cos_data_stage0 <= 32'h5c2bf79e;
        30: sin_cos_data_stage0 <= 32'h5f41f70b;
        31: sin_cos_data_stage0 <= 32'h6257f673;
        32: sin_cos_data_stage0 <= 32'h656cf5d6;
        33: sin_cos_data_stage0 <= 32'h6880f534;
        34: sin_cos_data_stage0 <= 32'h6b92f48d;
        35: sin_cos_data_stage0 <= 32'h6ea4f3e2;
        36: sin_cos_data_stage0 <= 32'h71b5f332;
        37: sin_cos_data_stage0 <= 32'h74c4f27d;
        38: sin_cos_data_stage0 <= 32'h77d3f1c3;
        39: sin_cos_data_stage0 <= 32'h7ae0f104;
        40: sin_cos_data_stage0 <= 32'h7decf041;
        41: sin_cos_data_stage0 <= 32'h80f7ef79;
        42: sin_cos_data_stage0 <= 32'h8401eeac;
        43: sin_cos_data_stage0 <= 32'h8709edda;
        44: sin_cos_data_stage0 <= 32'h8a10ed04;
        45: sin_cos_data_stage0 <= 32'h8d16ec28;
        46: sin_cos_data_stage0 <= 32'h901aeb48;
        47: sin_cos_data_stage0 <= 32'h931dea64;
        48: sin_cos_data_stage0 <= 32'h961fe97a;
        49: sin_cos_data_stage0 <= 32'h991fe88c;
        50: sin_cos_data_stage0 <= 32'h9c1ee799;
        51: sin_cos_data_stage0 <= 32'h9f1be6a1;
        52: sin_cos_data_stage0 <= 32'ha217e5a5;
        53: sin_cos_data_stage0 <= 32'ha511e4a4;
        54: sin_cos_data_stage0 <= 32'ha809e39f;
        55: sin_cos_data_stage0 <= 32'hab00e294;
        56: sin_cos_data_stage0 <= 32'hadf5e185;
        57: sin_cos_data_stage0 <= 32'hb0e9e072;
        58: sin_cos_data_stage0 <= 32'hb3dbdf5a;
        59: sin_cos_data_stage0 <= 32'hb6cbde3d;
        60: sin_cos_data_stage0 <= 32'hb9b9dd1b;
        61: sin_cos_data_stage0 <= 32'hbca6dbf5;
        62: sin_cos_data_stage0 <= 32'hbf90dacb;
        63: sin_cos_data_stage0 <= 32'hc279d99c;
        64: sin_cos_data_stage0 <= 32'hc560d868;
        65: sin_cos_data_stage0 <= 32'hc845d72f;
        66: sin_cos_data_stage0 <= 32'hcb28d5f3;
        67: sin_cos_data_stage0 <= 32'hce0ad4b1;
        68: sin_cos_data_stage0 <= 32'hd0e9d36b;
        69: sin_cos_data_stage0 <= 32'hd3c6d221;
        70: sin_cos_data_stage0 <= 32'hd6a1d0d2;
        71: sin_cos_data_stage0 <= 32'hd97acf7f;
        72: sin_cos_data_stage0 <= 32'hdc51ce27;
        73: sin_cos_data_stage0 <= 32'hdf26ccca;
        74: sin_cos_data_stage0 <= 32'he1f9cb6a;
        75: sin_cos_data_stage0 <= 32'he4caca05;
        76: sin_cos_data_stage0 <= 32'he798c89b;
        77: sin_cos_data_stage0 <= 32'hea64c72d;
        78: sin_cos_data_stage0 <= 32'hed2ec5bb;
        79: sin_cos_data_stage0 <= 32'heff5c444;
        80: sin_cos_data_stage0 <= 32'hf2bbc2c9;
        81: sin_cos_data_stage0 <= 32'hf57ec149;
        82: sin_cos_data_stage0 <= 32'hf83ebfc6;
        83: sin_cos_data_stage0 <= 32'hfafcbe3d;
        84: sin_cos_data_stage0 <= 32'hfdb8bcb1;
        85: sin_cos_data_stage0 <= 32'h0071bb20;
        86: sin_cos_data_stage0 <= 32'h0328b98b;
        87: sin_cos_data_stage0 <= 32'h05ddb7f2;
        88: sin_cos_data_stage0 <= 32'h088eb655;
        89: sin_cos_data_stage0 <= 32'h0b3eb4b3;
        90: sin_cos_data_stage0 <= 32'h0deab30d;
        91: sin_cos_data_stage0 <= 32'h1094b163;
        92: sin_cos_data_stage0 <= 32'h133cafb5;
        93: sin_cos_data_stage0 <= 32'h15e1ae02;
        94: sin_cos_data_stage0 <= 32'h1883ac4c;
        95: sin_cos_data_stage0 <= 32'h1b22aa91;
        96: sin_cos_data_stage0 <= 32'h1dbfa8d2;
        97: sin_cos_data_stage0 <= 32'h2059a70f;
        98: sin_cos_data_stage0 <= 32'h22f0a549;
        99: sin_cos_data_stage0 <= 32'h2584a37d;
        100: sin_cos_data_stage0 <= 32'h2816a1ae;
        101: sin_cos_data_stage0 <= 32'h2aa49fdb;
        102: sin_cos_data_stage0 <= 32'h2d309e04;
        103: sin_cos_data_stage0 <= 32'h2fb99c29;
        104: sin_cos_data_stage0 <= 32'h323f9a4a;
        105: sin_cos_data_stage0 <= 32'h34c29867;
        106: sin_cos_data_stage0 <= 32'h37429680;
        107: sin_cos_data_stage0 <= 32'h39bf9495;
        108: sin_cos_data_stage0 <= 32'h3c3992a6;
        109: sin_cos_data_stage0 <= 32'h3eb090b4;
        110: sin_cos_data_stage0 <= 32'h41248ebd;
        111: sin_cos_data_stage0 <= 32'h43958cc3;
        112: sin_cos_data_stage0 <= 32'h46028ac5;
        113: sin_cos_data_stage0 <= 32'h486d88c3;
        114: sin_cos_data_stage0 <= 32'h4ad486bd;
        115: sin_cos_data_stage0 <= 32'h4d3884b3;
        116: sin_cos_data_stage0 <= 32'h4f9982a6;
        117: sin_cos_data_stage0 <= 32'h51f78095;
        118: sin_cos_data_stage0 <= 32'h54527e80;
        119: sin_cos_data_stage0 <= 32'h56a97c68;
        120: sin_cos_data_stage0 <= 32'h58fd7a4c;
        121: sin_cos_data_stage0 <= 32'h5b4d782c;
        122: sin_cos_data_stage0 <= 32'h5d9a7609;
        123: sin_cos_data_stage0 <= 32'h5fe473e2;
        124: sin_cos_data_stage0 <= 32'h622b71b7;
        125: sin_cos_data_stage0 <= 32'h646e6f89;
        126: sin_cos_data_stage0 <= 32'h66ad6d57;
        127: sin_cos_data_stage0 <= 32'h68ea6b22;
        endcase
    end
end


always @(posedge CLK) begin
    if (RESET) begin
        //rotation_data_stage0 <= 0;
    end else if (CE) begin
        // ROTATIONS lookup table ROM content
        case(step2_lookup_addr)
        0: rotation_data_stage0 <= 8'h13;
        1: rotation_data_stage0 <= 8'he3;
        2: rotation_data_stage0 <= 8'he3;
        3: rotation_data_stage0 <= 8'h63;
        4: rotation_data_stage0 <= 8'ha3;
        5: rotation_data_stage0 <= 8'h23;
        6: rotation_data_stage0 <= 8'hc3;
        7: rotation_data_stage0 <= 8'hc3;
        8: rotation_data_stage0 <= 8'h43;
        9: rotation_data_stage0 <= 8'h83;
        10: rotation_data_stage0 <= 8'h03;
        11: rotation_data_stage0 <= 8'h03;
        12: rotation_data_stage0 <= 8'hfd;
        13: rotation_data_stage0 <= 8'h7d;
        14: rotation_data_stage0 <= 8'hbd;
        15: rotation_data_stage0 <= 8'h3d;
        16: rotation_data_stage0 <= 8'h3d;
        17: rotation_data_stage0 <= 8'hdd;
        18: rotation_data_stage0 <= 8'h5d;
        19: rotation_data_stage0 <= 8'h9d;
        20: rotation_data_stage0 <= 8'h1d;
        21: rotation_data_stage0 <= 8'h1d;
        22: rotation_data_stage0 <= 8'hed;
        23: rotation_data_stage0 <= 8'h6d;
        24: rotation_data_stage0 <= 8'had;
        25: rotation_data_stage0 <= 8'had;
        26: rotation_data_stage0 <= 8'h2d;
        27: rotation_data_stage0 <= 8'hcd;
        28: rotation_data_stage0 <= 8'h4d;
        29: rotation_data_stage0 <= 8'h8d;
        30: rotation_data_stage0 <= 8'h8d;
        31: rotation_data_stage0 <= 8'h0d;
        32: rotation_data_stage0 <= 8'hf5;
        33: rotation_data_stage0 <= 8'h75;
        34: rotation_data_stage0 <= 8'hb5;
        35: rotation_data_stage0 <= 8'hb5;
        36: rotation_data_stage0 <= 8'h35;
        37: rotation_data_stage0 <= 8'hd5;
        38: rotation_data_stage0 <= 8'h55;
        39: rotation_data_stage0 <= 8'h55;
        40: rotation_data_stage0 <= 8'h95;
        41: rotation_data_stage0 <= 8'h15;
        42: rotation_data_stage0 <= 8'he5;
        43: rotation_data_stage0 <= 8'h65;
        44: rotation_data_stage0 <= 8'h65;
        45: rotation_data_stage0 <= 8'ha5;
        46: rotation_data_stage0 <= 8'h25;
        47: rotation_data_stage0 <= 8'hc5;
        48: rotation_data_stage0 <= 8'h45;
        49: rotation_data_stage0 <= 8'h45;
        50: rotation_data_stage0 <= 8'h85;
        51: rotation_data_stage0 <= 8'h05;
        52: rotation_data_stage0 <= 8'hf9;
        53: rotation_data_stage0 <= 8'hf9;
        54: rotation_data_stage0 <= 8'h79;
        55: rotation_data_stage0 <= 8'hb9;
        56: rotation_data_stage0 <= 8'h39;
        57: rotation_data_stage0 <= 8'hd9;
        58: rotation_data_stage0 <= 8'hd9;
        59: rotation_data_stage0 <= 8'h59;
        60: rotation_data_stage0 <= 8'h99;
        61: rotation_data_stage0 <= 8'h19;
        62: rotation_data_stage0 <= 8'he9;
        63: rotation_data_stage0 <= 8'he9;
        64: rotation_data_stage0 <= 8'h69;
        65: rotation_data_stage0 <= 8'ha9;
        66: rotation_data_stage0 <= 8'h29;
        67: rotation_data_stage0 <= 8'h29;
        68: rotation_data_stage0 <= 8'hc9;
        69: rotation_data_stage0 <= 8'h49;
        70: rotation_data_stage0 <= 8'h89;
        71: rotation_data_stage0 <= 8'h09;
        72: rotation_data_stage0 <= 8'h09;
        73: rotation_data_stage0 <= 8'hf1;
        74: rotation_data_stage0 <= 8'h71;
        75: rotation_data_stage0 <= 8'hb1;
        76: rotation_data_stage0 <= 8'h31;
        77: rotation_data_stage0 <= 8'h31;
        78: rotation_data_stage0 <= 8'hd1;
        79: rotation_data_stage0 <= 8'h51;
        80: rotation_data_stage0 <= 8'h91;
        81: rotation_data_stage0 <= 8'h91;
        82: rotation_data_stage0 <= 8'h11;
        83: rotation_data_stage0 <= 8'he1;
        84: rotation_data_stage0 <= 8'h61;
        85: rotation_data_stage0 <= 8'ha1;
        86: rotation_data_stage0 <= 8'ha1;
        87: rotation_data_stage0 <= 8'h21;
        88: rotation_data_stage0 <= 8'hc1;
        89: rotation_data_stage0 <= 8'h41;
        90: rotation_data_stage0 <= 8'h81;
        91: rotation_data_stage0 <= 8'h81;
        92: rotation_data_stage0 <= 8'h01;
        93: rotation_data_stage0 <= 8'hfe;
        94: rotation_data_stage0 <= 8'h7e;
        95: rotation_data_stage0 <= 8'h7e;
        96: rotation_data_stage0 <= 8'hbe;
        97: rotation_data_stage0 <= 8'h3e;
        98: rotation_data_stage0 <= 8'hde;
        99: rotation_data_stage0 <= 8'h5e;
        100: rotation_data_stage0 <= 8'h5e;
        101: rotation_data_stage0 <= 8'h9e;
        102: rotation_data_stage0 <= 8'h1e;
        103: rotation_data_stage0 <= 8'hee;
        104: rotation_data_stage0 <= 8'h6e;
        105: rotation_data_stage0 <= 8'h6e;
        106: rotation_data_stage0 <= 8'hae;
        107: rotation_data_stage0 <= 8'h2e;
        108: rotation_data_stage0 <= 8'hce;
        109: rotation_data_stage0 <= 8'hce;
        110: rotation_data_stage0 <= 8'h4e;
        111: rotation_data_stage0 <= 8'h8e;
        112: rotation_data_stage0 <= 8'h0e;
        113: rotation_data_stage0 <= 8'hf6;
        114: rotation_data_stage0 <= 8'hf6;
        115: rotation_data_stage0 <= 8'h76;
        116: rotation_data_stage0 <= 8'hb6;
        117: rotation_data_stage0 <= 8'h36;
        118: rotation_data_stage0 <= 8'hd6;
        119: rotation_data_stage0 <= 8'hd6;
        120: rotation_data_stage0 <= 8'h56;
        121: rotation_data_stage0 <= 8'h96;
        122: rotation_data_stage0 <= 8'h16;
        123: rotation_data_stage0 <= 8'h16;
        124: rotation_data_stage0 <= 8'he6;
        125: rotation_data_stage0 <= 8'h66;
        126: rotation_data_stage0 <= 8'ha6;
        127: rotation_data_stage0 <= 8'h26;
        128: rotation_data_stage0 <= 8'h26;
        129: rotation_data_stage0 <= 8'hc6;
        130: rotation_data_stage0 <= 8'h46;
        131: rotation_data_stage0 <= 8'h86;
        132: rotation_data_stage0 <= 8'h06;
        133: rotation_data_stage0 <= 8'h06;
        134: rotation_data_stage0 <= 8'hfa;
        135: rotation_data_stage0 <= 8'h7a;
        136: rotation_data_stage0 <= 8'hba;
        137: rotation_data_stage0 <= 8'hba;
        138: rotation_data_stage0 <= 8'h3a;
        139: rotation_data_stage0 <= 8'hda;
        140: rotation_data_stage0 <= 8'h5a;
        141: rotation_data_stage0 <= 8'h9a;
        142: rotation_data_stage0 <= 8'h9a;
        143: rotation_data_stage0 <= 8'h1a;
        144: rotation_data_stage0 <= 8'hea;
        145: rotation_data_stage0 <= 8'h6a;
        146: rotation_data_stage0 <= 8'h6a;
        147: rotation_data_stage0 <= 8'haa;
        148: rotation_data_stage0 <= 8'h2a;
        149: rotation_data_stage0 <= 8'hca;
        150: rotation_data_stage0 <= 8'h4a;
        151: rotation_data_stage0 <= 8'h4a;
        152: rotation_data_stage0 <= 8'h8a;
        153: rotation_data_stage0 <= 8'h0a;
        154: rotation_data_stage0 <= 8'hf2;
        155: rotation_data_stage0 <= 8'h72;
        156: rotation_data_stage0 <= 8'h72;
        157: rotation_data_stage0 <= 8'hb2;
        158: rotation_data_stage0 <= 8'h32;
        159: rotation_data_stage0 <= 8'hd2;
        160: rotation_data_stage0 <= 8'hd2;
        161: rotation_data_stage0 <= 8'h52;
        162: rotation_data_stage0 <= 8'h92;
        163: rotation_data_stage0 <= 8'h12;
        164: rotation_data_stage0 <= 8'he2;
        165: rotation_data_stage0 <= 8'he2;
        166: rotation_data_stage0 <= 8'h62;
        167: rotation_data_stage0 <= 8'ha2;
        168: rotation_data_stage0 <= 8'h22;
        169: rotation_data_stage0 <= 8'hc2;
        170: rotation_data_stage0 <= 8'hc2;
        171: rotation_data_stage0 <= 8'h42;
        172: rotation_data_stage0 <= 8'h82;
        173: rotation_data_stage0 <= 8'h02;
        174: rotation_data_stage0 <= 8'h02;
        175: rotation_data_stage0 <= 8'hfc;
        176: rotation_data_stage0 <= 8'h7c;
        177: rotation_data_stage0 <= 8'hbc;
        178: rotation_data_stage0 <= 8'h3c;
        179: rotation_data_stage0 <= 8'h3c;
        180: rotation_data_stage0 <= 8'hdc;
        181: rotation_data_stage0 <= 8'h5c;
        182: rotation_data_stage0 <= 8'h9c;
        183: rotation_data_stage0 <= 8'h1c;
        184: rotation_data_stage0 <= 8'h1c;
        185: rotation_data_stage0 <= 8'hec;
        186: rotation_data_stage0 <= 8'h6c;
        187: rotation_data_stage0 <= 8'hac;
        188: rotation_data_stage0 <= 8'hac;
        189: rotation_data_stage0 <= 8'h2c;
        190: rotation_data_stage0 <= 8'hcc;
        191: rotation_data_stage0 <= 8'h4c;
        192: rotation_data_stage0 <= 8'h8c;
        193: rotation_data_stage0 <= 8'h8c;
        194: rotation_data_stage0 <= 8'h0c;
        195: rotation_data_stage0 <= 8'hf4;
        196: rotation_data_stage0 <= 8'h74;
        197: rotation_data_stage0 <= 8'hb4;
        198: rotation_data_stage0 <= 8'hb4;
        199: rotation_data_stage0 <= 8'h34;
        200: rotation_data_stage0 <= 8'hd4;
        201: rotation_data_stage0 <= 8'h54;
        202: rotation_data_stage0 <= 8'h54;
        203: rotation_data_stage0 <= 8'h94;
        204: rotation_data_stage0 <= 8'h14;
        205: rotation_data_stage0 <= 8'he4;
        206: rotation_data_stage0 <= 8'h64;
        207: rotation_data_stage0 <= 8'h64;
        208: rotation_data_stage0 <= 8'ha4;
        209: rotation_data_stage0 <= 8'h24;
        210: rotation_data_stage0 <= 8'hc4;
        211: rotation_data_stage0 <= 8'h44;
        212: rotation_data_stage0 <= 8'h44;
        213: rotation_data_stage0 <= 8'h84;
        214: rotation_data_stage0 <= 8'h04;
        215: rotation_data_stage0 <= 8'hf8;
        216: rotation_data_stage0 <= 8'hf8;
        217: rotation_data_stage0 <= 8'h78;
        218: rotation_data_stage0 <= 8'hb8;
        219: rotation_data_stage0 <= 8'h38;
        220: rotation_data_stage0 <= 8'hd8;
        221: rotation_data_stage0 <= 8'hd8;
        222: rotation_data_stage0 <= 8'h58;
        223: rotation_data_stage0 <= 8'h98;
        224: rotation_data_stage0 <= 8'h18;
        225: rotation_data_stage0 <= 8'he8;
        226: rotation_data_stage0 <= 8'he8;
        227: rotation_data_stage0 <= 8'h68;
        228: rotation_data_stage0 <= 8'ha8;
        229: rotation_data_stage0 <= 8'h28;
        230: rotation_data_stage0 <= 8'h28;
        231: rotation_data_stage0 <= 8'hc8;
        232: rotation_data_stage0 <= 8'h48;
        233: rotation_data_stage0 <= 8'h88;
        234: rotation_data_stage0 <= 8'h08;
        235: rotation_data_stage0 <= 8'h08;
        236: rotation_data_stage0 <= 8'hf0;
        237: rotation_data_stage0 <= 8'h70;
        238: rotation_data_stage0 <= 8'hb0;
        239: rotation_data_stage0 <= 8'h30;
        240: rotation_data_stage0 <= 8'h30;
        241: rotation_data_stage0 <= 8'hd0;
        242: rotation_data_stage0 <= 8'h50;
        243: rotation_data_stage0 <= 8'h90;
        244: rotation_data_stage0 <= 8'h90;
        245: rotation_data_stage0 <= 8'h10;
        246: rotation_data_stage0 <= 8'he0;
        247: rotation_data_stage0 <= 8'h60;
        248: rotation_data_stage0 <= 8'ha0;
        249: rotation_data_stage0 <= 8'ha0;
        250: rotation_data_stage0 <= 8'h20;
        251: rotation_data_stage0 <= 8'hc0;
        252: rotation_data_stage0 <= 8'h40;
        253: rotation_data_stage0 <= 8'h80;
        254: rotation_data_stage0 <= 8'h80;
        255: rotation_data_stage0 <= 8'h00;
        256: rotation_data_stage0 <= 8'hff;
        257: rotation_data_stage0 <= 8'h7f;
        258: rotation_data_stage0 <= 8'h7f;
        259: rotation_data_stage0 <= 8'hbf;
        260: rotation_data_stage0 <= 8'h3f;
        261: rotation_data_stage0 <= 8'hdf;
        262: rotation_data_stage0 <= 8'h5f;
        263: rotation_data_stage0 <= 8'h5f;
        264: rotation_data_stage0 <= 8'h9f;
        265: rotation_data_stage0 <= 8'h1f;
        266: rotation_data_stage0 <= 8'hef;
        267: rotation_data_stage0 <= 8'h6f;
        268: rotation_data_stage0 <= 8'h6f;
        269: rotation_data_stage0 <= 8'haf;
        270: rotation_data_stage0 <= 8'h2f;
        271: rotation_data_stage0 <= 8'hcf;
        272: rotation_data_stage0 <= 8'hcf;
        273: rotation_data_stage0 <= 8'h4f;
        274: rotation_data_stage0 <= 8'h8f;
        275: rotation_data_stage0 <= 8'h0f;
        276: rotation_data_stage0 <= 8'hf7;
        277: rotation_data_stage0 <= 8'hf7;
        278: rotation_data_stage0 <= 8'h77;
        279: rotation_data_stage0 <= 8'hb7;
        280: rotation_data_stage0 <= 8'h37;
        281: rotation_data_stage0 <= 8'hd7;
        282: rotation_data_stage0 <= 8'hd7;
        283: rotation_data_stage0 <= 8'h57;
        284: rotation_data_stage0 <= 8'h97;
        285: rotation_data_stage0 <= 8'h17;
        286: rotation_data_stage0 <= 8'h17;
        287: rotation_data_stage0 <= 8'he7;
        288: rotation_data_stage0 <= 8'h67;
        289: rotation_data_stage0 <= 8'ha7;
        290: rotation_data_stage0 <= 8'h27;
        291: rotation_data_stage0 <= 8'h27;
        292: rotation_data_stage0 <= 8'hc7;
        293: rotation_data_stage0 <= 8'h47;
        294: rotation_data_stage0 <= 8'h87;
        295: rotation_data_stage0 <= 8'h07;
        296: rotation_data_stage0 <= 8'h07;
        297: rotation_data_stage0 <= 8'hfb;
        298: rotation_data_stage0 <= 8'h7b;
        299: rotation_data_stage0 <= 8'hbb;
        300: rotation_data_stage0 <= 8'hbb;
        301: rotation_data_stage0 <= 8'h3b;
        302: rotation_data_stage0 <= 8'hdb;
        303: rotation_data_stage0 <= 8'h5b;
        304: rotation_data_stage0 <= 8'h9b;
        305: rotation_data_stage0 <= 8'h9b;
        306: rotation_data_stage0 <= 8'h1b;
        307: rotation_data_stage0 <= 8'heb;
        308: rotation_data_stage0 <= 8'h6b;
        309: rotation_data_stage0 <= 8'hab;
        310: rotation_data_stage0 <= 8'hab;
        311: rotation_data_stage0 <= 8'h2b;
        312: rotation_data_stage0 <= 8'hcb;
        313: rotation_data_stage0 <= 8'h4b;
        314: rotation_data_stage0 <= 8'h4b;
        315: rotation_data_stage0 <= 8'h8b;
        316: rotation_data_stage0 <= 8'h0b;
        317: rotation_data_stage0 <= 8'hf3;
        318: rotation_data_stage0 <= 8'h73;
        319: rotation_data_stage0 <= 8'h73;
        320: rotation_data_stage0 <= 8'hb3;
        321: rotation_data_stage0 <= 8'h33;
        322: rotation_data_stage0 <= 8'hd3;
        323: rotation_data_stage0 <= 8'h53;
        324: rotation_data_stage0 <= 8'h53;
        325: rotation_data_stage0 <= 8'h93;
        326: rotation_data_stage0 <= 8'h13;
        327: rotation_data_stage0 <= 8'he3;
        328: rotation_data_stage0 <= 8'he3;
        329: rotation_data_stage0 <= 8'h63;
        330: rotation_data_stage0 <= 8'ha3;
        331: rotation_data_stage0 <= 8'h23;
        332: rotation_data_stage0 <= 8'hc3;
        333: rotation_data_stage0 <= 8'hc3;
        334: rotation_data_stage0 <= 8'h43;
        335: rotation_data_stage0 <= 8'h83;
        336: rotation_data_stage0 <= 8'h03;
        337: rotation_data_stage0 <= 8'hfd;
        338: rotation_data_stage0 <= 8'hfd;
        339: rotation_data_stage0 <= 8'h7d;
        340: rotation_data_stage0 <= 8'hbd;
        341: rotation_data_stage0 <= 8'h3d;
        342: rotation_data_stage0 <= 8'h3d;
        343: rotation_data_stage0 <= 8'hdd;
        344: rotation_data_stage0 <= 8'h5d;
        345: rotation_data_stage0 <= 8'h9d;
        346: rotation_data_stage0 <= 8'h1d;
        347: rotation_data_stage0 <= 8'h1d;
        348: rotation_data_stage0 <= 8'hed;
        349: rotation_data_stage0 <= 8'h6d;
        350: rotation_data_stage0 <= 8'had;
        351: rotation_data_stage0 <= 8'h2d;
        352: rotation_data_stage0 <= 8'h2d;
        353: rotation_data_stage0 <= 8'hcd;
        354: rotation_data_stage0 <= 8'h4d;
        355: rotation_data_stage0 <= 8'h8d;
        356: rotation_data_stage0 <= 8'h8d;
        357: rotation_data_stage0 <= 8'h0d;
        358: rotation_data_stage0 <= 8'hf5;
        359: rotation_data_stage0 <= 8'h75;
        360: rotation_data_stage0 <= 8'hb5;
        361: rotation_data_stage0 <= 8'hb5;
        362: rotation_data_stage0 <= 8'h35;
        363: rotation_data_stage0 <= 8'hd5;
        364: rotation_data_stage0 <= 8'h55;
        365: rotation_data_stage0 <= 8'h95;
        366: rotation_data_stage0 <= 8'h95;
        367: rotation_data_stage0 <= 8'h15;
        368: rotation_data_stage0 <= 8'he5;
        369: rotation_data_stage0 <= 8'h65;
        370: rotation_data_stage0 <= 8'h65;
        371: rotation_data_stage0 <= 8'ha5;
        372: rotation_data_stage0 <= 8'h25;
        373: rotation_data_stage0 <= 8'hc5;
        374: rotation_data_stage0 <= 8'h45;
        375: rotation_data_stage0 <= 8'h45;
        376: rotation_data_stage0 <= 8'h85;
        377: rotation_data_stage0 <= 8'h05;
        378: rotation_data_stage0 <= 8'hf9;
        379: rotation_data_stage0 <= 8'hf9;
        380: rotation_data_stage0 <= 8'h79;
        381: rotation_data_stage0 <= 8'hb9;
        382: rotation_data_stage0 <= 8'h39;
        383: rotation_data_stage0 <= 8'hd9;
        384: rotation_data_stage0 <= 8'hd9;
        385: rotation_data_stage0 <= 8'h59;
        386: rotation_data_stage0 <= 8'h99;
        387: rotation_data_stage0 <= 8'h19;
        388: rotation_data_stage0 <= 8'he9;
        389: rotation_data_stage0 <= 8'he9;
        390: rotation_data_stage0 <= 8'h69;
        391: rotation_data_stage0 <= 8'ha9;
        392: rotation_data_stage0 <= 8'h29;
        393: rotation_data_stage0 <= 8'h29;
        394: rotation_data_stage0 <= 8'hc9;
        395: rotation_data_stage0 <= 8'h49;
        396: rotation_data_stage0 <= 8'h89;
        397: rotation_data_stage0 <= 8'h09;
        398: rotation_data_stage0 <= 8'h09;
        399: rotation_data_stage0 <= 8'hf1;
        400: rotation_data_stage0 <= 8'h71;
        401: rotation_data_stage0 <= 8'hb1;
        402: rotation_data_stage0 <= 8'h31;
        403: rotation_data_stage0 <= 8'h31;
        404: rotation_data_stage0 <= 8'hd1;
        405: rotation_data_stage0 <= 8'h51;
        406: rotation_data_stage0 <= 8'h91;
        407: rotation_data_stage0 <= 8'h91;
        408: rotation_data_stage0 <= 8'h11;
        409: rotation_data_stage0 <= 8'he1;
        410: rotation_data_stage0 <= 8'h61;
        411: rotation_data_stage0 <= 8'ha1;
        412: rotation_data_stage0 <= 8'ha1;
        413: rotation_data_stage0 <= 8'h21;
        414: rotation_data_stage0 <= 8'hc1;
        415: rotation_data_stage0 <= 8'h41;
        416: rotation_data_stage0 <= 8'h81;
        417: rotation_data_stage0 <= 8'h81;
        418: rotation_data_stage0 <= 8'h01;
        419: rotation_data_stage0 <= 8'hfe;
        420: rotation_data_stage0 <= 8'h7e;
        421: rotation_data_stage0 <= 8'h7e;
        422: rotation_data_stage0 <= 8'hbe;
        423: rotation_data_stage0 <= 8'h3e;
        424: rotation_data_stage0 <= 8'hde;
        425: rotation_data_stage0 <= 8'h5e;
        426: rotation_data_stage0 <= 8'h5e;
        427: rotation_data_stage0 <= 8'h9e;
        428: rotation_data_stage0 <= 8'h1e;
        429: rotation_data_stage0 <= 8'hee;
        430: rotation_data_stage0 <= 8'h6e;
        431: rotation_data_stage0 <= 8'h6e;
        432: rotation_data_stage0 <= 8'hae;
        433: rotation_data_stage0 <= 8'h2e;
        434: rotation_data_stage0 <= 8'hce;
        435: rotation_data_stage0 <= 8'hce;
        436: rotation_data_stage0 <= 8'h4e;
        437: rotation_data_stage0 <= 8'h8e;
        438: rotation_data_stage0 <= 8'h0e;
        439: rotation_data_stage0 <= 8'hf6;
        440: rotation_data_stage0 <= 8'hf6;
        441: rotation_data_stage0 <= 8'h76;
        442: rotation_data_stage0 <= 8'hb6;
        443: rotation_data_stage0 <= 8'h36;
        444: rotation_data_stage0 <= 8'hd6;
        445: rotation_data_stage0 <= 8'hd6;
        446: rotation_data_stage0 <= 8'h56;
        447: rotation_data_stage0 <= 8'h96;
        448: rotation_data_stage0 <= 8'h16;
        449: rotation_data_stage0 <= 8'h16;
        450: rotation_data_stage0 <= 8'he6;
        451: rotation_data_stage0 <= 8'h66;
        452: rotation_data_stage0 <= 8'ha6;
        453: rotation_data_stage0 <= 8'h26;
        454: rotation_data_stage0 <= 8'h26;
        455: rotation_data_stage0 <= 8'hc6;
        456: rotation_data_stage0 <= 8'h46;
        457: rotation_data_stage0 <= 8'h86;
        458: rotation_data_stage0 <= 8'h06;
        459: rotation_data_stage0 <= 8'h06;
        460: rotation_data_stage0 <= 8'hfa;
        461: rotation_data_stage0 <= 8'h7a;
        462: rotation_data_stage0 <= 8'hba;
        463: rotation_data_stage0 <= 8'hba;
        464: rotation_data_stage0 <= 8'h3a;
        465: rotation_data_stage0 <= 8'hda;
        466: rotation_data_stage0 <= 8'h5a;
        467: rotation_data_stage0 <= 8'h9a;
        468: rotation_data_stage0 <= 8'h9a;
        469: rotation_data_stage0 <= 8'h1a;
        470: rotation_data_stage0 <= 8'hea;
        471: rotation_data_stage0 <= 8'h6a;
        472: rotation_data_stage0 <= 8'haa;
        473: rotation_data_stage0 <= 8'haa;
        474: rotation_data_stage0 <= 8'h2a;
        475: rotation_data_stage0 <= 8'hca;
        476: rotation_data_stage0 <= 8'h4a;
        477: rotation_data_stage0 <= 8'h4a;
        478: rotation_data_stage0 <= 8'h8a;
        479: rotation_data_stage0 <= 8'h0a;
        480: rotation_data_stage0 <= 8'hf2;
        481: rotation_data_stage0 <= 8'h72;
        482: rotation_data_stage0 <= 8'h72;
        483: rotation_data_stage0 <= 8'hb2;
        484: rotation_data_stage0 <= 8'h32;
        485: rotation_data_stage0 <= 8'hd2;
        486: rotation_data_stage0 <= 8'h52;
        487: rotation_data_stage0 <= 8'h52;
        488: rotation_data_stage0 <= 8'h92;
        489: rotation_data_stage0 <= 8'h12;
        490: rotation_data_stage0 <= 8'he2;
        491: rotation_data_stage0 <= 8'he2;
        492: rotation_data_stage0 <= 8'h62;
        493: rotation_data_stage0 <= 8'ha2;
        494: rotation_data_stage0 <= 8'h22;
        495: rotation_data_stage0 <= 8'hc2;
        496: rotation_data_stage0 <= 8'hc2;
        497: rotation_data_stage0 <= 8'h42;
        498: rotation_data_stage0 <= 8'h82;
        499: rotation_data_stage0 <= 8'h02;
        500: rotation_data_stage0 <= 8'hfc;
        501: rotation_data_stage0 <= 8'hfc;
        502: rotation_data_stage0 <= 8'h7c;
        503: rotation_data_stage0 <= 8'hbc;
        504: rotation_data_stage0 <= 8'h3c;
        505: rotation_data_stage0 <= 8'h3c;
        506: rotation_data_stage0 <= 8'hdc;
        507: rotation_data_stage0 <= 8'h5c;
        508: rotation_data_stage0 <= 8'h9c;
        509: rotation_data_stage0 <= 8'h1c;
        510: rotation_data_stage0 <= 8'h1c;
        511: rotation_data_stage0 <= 8'hec;
        endcase
    end
end

reg [SIN_COS_LOOKUP_DATA_BITS*2-1:0] sin_cos_data_stage1 = 0;
reg [ROTATIONS_LOOKUP_DATA_BITS-1:0] rotation_data_stage1 = 0;

always @(posedge CLK) begin
    if (RESET) begin
        //sin_cos_data_stage1 <= 0;
        //rotation_data_stage1 <= 0;
    end else if (CE) begin
        sin_cos_data_stage1 <= sin_cos_data_stage0;
        rotation_data_stage1 <= rotation_data_stage0;
    end
end

localparam EXTRA_DATA_BITS = 4;
localparam INTERNAL_DATA_BITS = SIN_COS_LOOKUP_DATA_BITS + 2 + EXTRA_DATA_BITS;
localparam FIRST_ROTATION_SHIFT = 9;  
wire signed [INTERNAL_DATA_BITS-1:0] x_stage_1 = {1'b0, 1'b1, sin_cos_data_stage1[SIN_COS_LOOKUP_DATA_BITS-1:0], {EXTRA_DATA_BITS{1'b0}} }; 
wire signed [INTERNAL_DATA_BITS-1:0] y_stage_1 = {1'b0, sin_value_top_bit_stage1, sin_cos_data_stage1[SIN_COS_LOOKUP_DATA_BITS*2-1:SIN_COS_LOOKUP_DATA_BITS], {EXTRA_DATA_BITS{1'b0}} }; 
wire [ROTATIONS_LOOKUP_DATA_BITS:0] rotation_stage_1 = {rotation_data_stage1, first_rotation_sign_stage1};

reg signed [INTERNAL_DATA_BITS-1:0] x_buf[ROTATIONS-1:0];
reg signed [INTERNAL_DATA_BITS-1:0] y_buf[ROTATIONS-1:0];
reg [ROTATIONS_LOOKUP_DATA_BITS-1:0] rotation_buf[ROTATIONS-1:0];
genvar i;
generate
        for (i = 0; i < ROTATIONS; i = i + 1) begin
    always @(posedge CLK) begin
            if (RESET) begin
                x_buf[i] <= 0;
                y_buf[i] <= 0;
                rotation_buf[i] <= 0;
            end else if (CE) begin
                if (i == 0) begin
                    x_buf[i] <= (rotation_stage_1[0]) ? x_stage_1 + (y_stage_1 >> (FIRST_ROTATION_SHIFT+i)) 
                                                      : x_stage_1 - (y_stage_1 >> (FIRST_ROTATION_SHIFT+i));
                    y_buf[i] <= (rotation_stage_1[0]) ? y_stage_1 - (x_stage_1 >> (FIRST_ROTATION_SHIFT+i))
                                                      : y_stage_1 + (x_stage_1 >> (FIRST_ROTATION_SHIFT+i));
                    rotation_buf[i] <= (rotation_stage_1 >> 1);
                end else begin
                    x_buf[i] <= (rotation_buf[i-1][0]) ? x_buf[i-1] + (y_buf[i-1] >> (FIRST_ROTATION_SHIFT+i)) 
                                                       : x_buf[i-1] - (y_buf[i-1] >> (FIRST_ROTATION_SHIFT+i));
                    y_buf[i] <= (rotation_buf[i-1][0]) ? y_buf[i-1] - (x_buf[i-1] >> (FIRST_ROTATION_SHIFT+i))
                                                       : y_buf[i-1] + (x_buf[i-1] >> (FIRST_ROTATION_SHIFT+i));
                    rotation_buf[i] <= (rotation_buf[i-1] >> 1);
                end
            end
        end 
    end
endgenerate

wire signed [INTERNAL_DATA_BITS-1:0] x_swapped = swap_sin_and_cos_delayed ? y_buf[ROTATIONS-1] : x_buf[ROTATIONS-1];
wire signed [INTERNAL_DATA_BITS-1:0] y_swapped = swap_sin_and_cos_delayed ? x_buf[ROTATIONS-1] : y_buf[ROTATIONS-1];
wire signed [INTERNAL_DATA_BITS-1:0] x_inverted = x_swapped ^ { INTERNAL_DATA_BITS {neg_cos_delayed} };
wire signed [INTERNAL_DATA_BITS-1:0] y_inverted = y_swapped ^ { INTERNAL_DATA_BITS {neg_sin_delayed} };

localparam ROUNDING = 1 << (EXTRA_DATA_BITS+1);
reg signed [INTERNAL_DATA_BITS-1:0] x_out;
reg signed [INTERNAL_DATA_BITS-1:0] y_out;
always @(posedge CLK) begin
    if (RESET) begin
        x_out <= 0;
        y_out <= 0;
    end else if (CE) begin
        x_out <= x_inverted + ROUNDING + neg_cos_delayed;
        y_out <= y_inverted + ROUNDING + neg_sin_delayed;
    end
end

//assign SIN = sin_cos_data_stage1[31:16];
//assign COS = sin_cos_data_stage1[15:0];
assign SIN = x_out[INTERNAL_DATA_BITS-1:INTERNAL_DATA_BITS-DATA_BITS];
assign COS = y_out[INTERNAL_DATA_BITS-1:INTERNAL_DATA_BITS-DATA_BITS];

endmodule
