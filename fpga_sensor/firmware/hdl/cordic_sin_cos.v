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
    parameter PHASE_BITS = 20,
    // size of PI/4 sin&cos lookup table (7 is 128)
    parameter STEP1_PHASE_BITS = 8,
    // size of rotations table (9 is 512)
    parameter STEP2_PHASE_BITS = 9,
    // additional internal data bits to increase precision, recommended: 4
    parameter EXTRA_DATA_BITS = 4
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
localparam ROTATIONS = ROTATIONS_LOOKUP_DATA_BITS + 1;

// for angle <= bound top bit of sin is 0, otherwise 1
localparam SIN_HIGH_BIT_0_BOUND = 170;

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


fixed_delay_shift_register #(
    // data width to store
    .DATA_BITS(3),
    .DELAY_CYCLES(ROTATIONS + 2)
)
fixed_delay_shift_register_inst
(
    .CLK(CLK),
    .CE(CE),
    .RESET(RESET),
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
wire sin_value_top_bit =  (step1_lookup_addr>SIN_HIGH_BIT_0_BOUND) ? 1'b1 : 1'b0;
// optimization: first rotation can be get from phase bit, avoiding storing of one bit of data 
wire first_rotation_sign = step2_lookup_addr[STEP2_PHASE_BITS-1] ^ 1'b1;

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

localparam FIRST_ROTATION_SHIFT = 10;

// SIN&COS lookup table ROM internal register
reg [SIN_COS_LOOKUP_DATA_BITS*2-1:0] sin_cos_data_stage0 = 0;
// ROTATIONS lookup table ROM internal register
reg [ROTATIONS_LOOKUP_DATA_BITS-1:0] rotation_data_stage0 = 0;

(* ram_style = "block" *)  
always @(posedge CLK) begin
    if (CE) begin
        // SIN and COS lookup table ROM content
        case(step1_lookup_addr)
        0: sin_cos_data_stage0 <= 32'h00c9fffb;
        1: sin_cos_data_stage0 <= 32'h025bfffa;
        2: sin_cos_data_stage0 <= 32'h03edfff8;
        3: sin_cos_data_stage0 <= 32'h057ffff4;
        4: sin_cos_data_stage0 <= 32'h0711ffef;
        5: sin_cos_data_stage0 <= 32'h08a3ffe9;
        6: sin_cos_data_stage0 <= 32'h0a35ffe1;
        7: sin_cos_data_stage0 <= 32'h0bc7ffd9;
        8: sin_cos_data_stage0 <= 32'h0d59ffcf;
        9: sin_cos_data_stage0 <= 32'h0eebffc4;
        10: sin_cos_data_stage0 <= 32'h107dffb7;
        11: sin_cos_data_stage0 <= 32'h120fffaa;
        12: sin_cos_data_stage0 <= 32'h13a1ff9b;
        13: sin_cos_data_stage0 <= 32'h1532ff8b;
        14: sin_cos_data_stage0 <= 32'h16c4ff7a;
        15: sin_cos_data_stage0 <= 32'h1856ff67;
        16: sin_cos_data_stage0 <= 32'h19e8ff54;
        17: sin_cos_data_stage0 <= 32'h1b79ff3f;
        18: sin_cos_data_stage0 <= 32'h1d0bff28;
        19: sin_cos_data_stage0 <= 32'h1e9cff11;
        20: sin_cos_data_stage0 <= 32'h202dfef8;
        21: sin_cos_data_stage0 <= 32'h21bffede;
        22: sin_cos_data_stage0 <= 32'h2350fec3;
        23: sin_cos_data_stage0 <= 32'h24e1fea7;
        24: sin_cos_data_stage0 <= 32'h2672fe89;
        25: sin_cos_data_stage0 <= 32'h2803fe6b;
        26: sin_cos_data_stage0 <= 32'h2994fe4a;
        27: sin_cos_data_stage0 <= 32'h2b24fe29;
        28: sin_cos_data_stage0 <= 32'h2cb5fe07;
        29: sin_cos_data_stage0 <= 32'h2e46fde3;
        30: sin_cos_data_stage0 <= 32'h2fd6fdbe;
        31: sin_cos_data_stage0 <= 32'h3166fd98;
        32: sin_cos_data_stage0 <= 32'h32f6fd70;
        33: sin_cos_data_stage0 <= 32'h3487fd48;
        34: sin_cos_data_stage0 <= 32'h3616fd1e;
        35: sin_cos_data_stage0 <= 32'h37a6fcf3;
        36: sin_cos_data_stage0 <= 32'h3936fcc7;
        37: sin_cos_data_stage0 <= 32'h3ac5fc99;
        38: sin_cos_data_stage0 <= 32'h3c55fc6a;
        39: sin_cos_data_stage0 <= 32'h3de4fc3a;
        40: sin_cos_data_stage0 <= 32'h3f73fc09;
        41: sin_cos_data_stage0 <= 32'h4102fbd7;
        42: sin_cos_data_stage0 <= 32'h4291fba3;
        43: sin_cos_data_stage0 <= 32'h441ffb6e;
        44: sin_cos_data_stage0 <= 32'h45aefb38;
        45: sin_cos_data_stage0 <= 32'h473cfb00;
        46: sin_cos_data_stage0 <= 32'h48cafac8;
        47: sin_cos_data_stage0 <= 32'h4a58fa8e;
        48: sin_cos_data_stage0 <= 32'h4be6fa53;
        49: sin_cos_data_stage0 <= 32'h4d74fa17;
        50: sin_cos_data_stage0 <= 32'h4f01f9d9;
        51: sin_cos_data_stage0 <= 32'h508ef99b;
        52: sin_cos_data_stage0 <= 32'h521bf95b;
        53: sin_cos_data_stage0 <= 32'h53a8f91a;
        54: sin_cos_data_stage0 <= 32'h5535f8d8;
        55: sin_cos_data_stage0 <= 32'h56c1f894;
        56: sin_cos_data_stage0 <= 32'h584df84f;
        57: sin_cos_data_stage0 <= 32'h59d9f809;
        58: sin_cos_data_stage0 <= 32'h5b65f7c2;
        59: sin_cos_data_stage0 <= 32'h5cf0f77a;
        60: sin_cos_data_stage0 <= 32'h5e7cf730;
        61: sin_cos_data_stage0 <= 32'h6007f6e5;
        62: sin_cos_data_stage0 <= 32'h6192f699;
        63: sin_cos_data_stage0 <= 32'h631cf64c;
        64: sin_cos_data_stage0 <= 32'h64a7f5fe;
        65: sin_cos_data_stage0 <= 32'h6631f5ae;
        66: sin_cos_data_stage0 <= 32'h67bbf55d;
        67: sin_cos_data_stage0 <= 32'h6944f50b;
        68: sin_cos_data_stage0 <= 32'h6acef4b8;
        69: sin_cos_data_stage0 <= 32'h6c57f463;
        70: sin_cos_data_stage0 <= 32'h6de0f40e;
        71: sin_cos_data_stage0 <= 32'h6f68f3b7;
        72: sin_cos_data_stage0 <= 32'h70f1f35f;
        73: sin_cos_data_stage0 <= 32'h7279f305;
        74: sin_cos_data_stage0 <= 32'h7401f2ab;
        75: sin_cos_data_stage0 <= 32'h7588f24f;
        76: sin_cos_data_stage0 <= 32'h770ff1f2;
        77: sin_cos_data_stage0 <= 32'h7896f194;
        78: sin_cos_data_stage0 <= 32'h7a1df135;
        79: sin_cos_data_stage0 <= 32'h7ba3f0d4;
        80: sin_cos_data_stage0 <= 32'h7d29f072;
        81: sin_cos_data_stage0 <= 32'h7eaff010;
        82: sin_cos_data_stage0 <= 32'h8035efab;
        83: sin_cos_data_stage0 <= 32'h81baef46;
        84: sin_cos_data_stage0 <= 32'h833feee0;
        85: sin_cos_data_stage0 <= 32'h84c3ee78;
        86: sin_cos_data_stage0 <= 32'h8647ee0f;
        87: sin_cos_data_stage0 <= 32'h87cbeda5;
        88: sin_cos_data_stage0 <= 32'h894fed3a;
        89: sin_cos_data_stage0 <= 32'h8ad2eccd;
        90: sin_cos_data_stage0 <= 32'h8c55ec60;
        91: sin_cos_data_stage0 <= 32'h8dd7ebf1;
        92: sin_cos_data_stage0 <= 32'h8f5aeb81;
        93: sin_cos_data_stage0 <= 32'h90dbeb10;
        94: sin_cos_data_stage0 <= 32'h925dea9d;
        95: sin_cos_data_stage0 <= 32'h93deea2a;
        96: sin_cos_data_stage0 <= 32'h955fe9b5;
        97: sin_cos_data_stage0 <= 32'h96dfe93f;
        98: sin_cos_data_stage0 <= 32'h985fe8c8;
        99: sin_cos_data_stage0 <= 32'h99dfe850;
        100: sin_cos_data_stage0 <= 32'h9b5ee7d6;
        101: sin_cos_data_stage0 <= 32'h9cdde75c;
        102: sin_cos_data_stage0 <= 32'h9e5ce6e0;
        103: sin_cos_data_stage0 <= 32'h9fdae663;
        104: sin_cos_data_stage0 <= 32'ha158e5e5;
        105: sin_cos_data_stage0 <= 32'ha2d5e566;
        106: sin_cos_data_stage0 <= 32'ha452e4e5;
        107: sin_cos_data_stage0 <= 32'ha5cfe464;
        108: sin_cos_data_stage0 <= 32'ha74be3e1;
        109: sin_cos_data_stage0 <= 32'ha8c7e35d;
        110: sin_cos_data_stage0 <= 32'haa43e2d8;
        111: sin_cos_data_stage0 <= 32'habbee251;
        112: sin_cos_data_stage0 <= 32'had38e1ca;
        113: sin_cos_data_stage0 <= 32'haeb2e141;
        114: sin_cos_data_stage0 <= 32'hb02ce0b7;
        115: sin_cos_data_stage0 <= 32'hb1a5e02c;
        116: sin_cos_data_stage0 <= 32'hb31edfa0;
        117: sin_cos_data_stage0 <= 32'hb497df13;
        118: sin_cos_data_stage0 <= 32'hb60fde85;
        119: sin_cos_data_stage0 <= 32'hb787ddf5;
        120: sin_cos_data_stage0 <= 32'hb8fedd64;
        121: sin_cos_data_stage0 <= 32'hba74dcd3;
        122: sin_cos_data_stage0 <= 32'hbbebdc40;
        123: sin_cos_data_stage0 <= 32'hbd61dbab;
        124: sin_cos_data_stage0 <= 32'hbed6db16;
        125: sin_cos_data_stage0 <= 32'hc04bda80;
        126: sin_cos_data_stage0 <= 32'hc1bfd9e8;
        127: sin_cos_data_stage0 <= 32'hc333d94f;
        128: sin_cos_data_stage0 <= 32'hc4a7d8b5;
        129: sin_cos_data_stage0 <= 32'hc61ad81a;
        130: sin_cos_data_stage0 <= 32'hc78cd77e;
        131: sin_cos_data_stage0 <= 32'hc8fed6e1;
        132: sin_cos_data_stage0 <= 32'hca70d642;
        133: sin_cos_data_stage0 <= 32'hcbe1d5a3;
        134: sin_cos_data_stage0 <= 32'hcd52d502;
        135: sin_cos_data_stage0 <= 32'hcec2d460;
        136: sin_cos_data_stage0 <= 32'hd031d3bd;
        137: sin_cos_data_stage0 <= 32'hd1a0d319;
        138: sin_cos_data_stage0 <= 32'hd30fd274;
        139: sin_cos_data_stage0 <= 32'hd47dd1ce;
        140: sin_cos_data_stage0 <= 32'hd5ebd126;
        141: sin_cos_data_stage0 <= 32'hd758d07e;
        142: sin_cos_data_stage0 <= 32'hd8c4cfd4;
        143: sin_cos_data_stage0 <= 32'hda30cf29;
        144: sin_cos_data_stage0 <= 32'hdb9cce7d;
        145: sin_cos_data_stage0 <= 32'hdd07cdd0;
        146: sin_cos_data_stage0 <= 32'hde71cd22;
        147: sin_cos_data_stage0 <= 32'hdfdbcc73;
        148: sin_cos_data_stage0 <= 32'he145cbc3;
        149: sin_cos_data_stage0 <= 32'he2adcb11;
        150: sin_cos_data_stage0 <= 32'he416ca5e;
        151: sin_cos_data_stage0 <= 32'he57dc9ab;
        152: sin_cos_data_stage0 <= 32'he6e5c8f6;
        153: sin_cos_data_stage0 <= 32'he84bc840;
        154: sin_cos_data_stage0 <= 32'he9b1c789;
        155: sin_cos_data_stage0 <= 32'heb17c6d1;
        156: sin_cos_data_stage0 <= 32'hec7cc618;
        157: sin_cos_data_stage0 <= 32'hede0c55e;
        158: sin_cos_data_stage0 <= 32'hef44c4a2;
        159: sin_cos_data_stage0 <= 32'hf0a7c3e6;
        160: sin_cos_data_stage0 <= 32'hf20ac328;
        161: sin_cos_data_stage0 <= 32'hf36cc26a;
        162: sin_cos_data_stage0 <= 32'hf4cdc1aa;
        163: sin_cos_data_stage0 <= 32'hf62ec0e9;
        164: sin_cos_data_stage0 <= 32'hf78ec027;
        165: sin_cos_data_stage0 <= 32'hf8eebf64;
        166: sin_cos_data_stage0 <= 32'hfa4dbea0;
        167: sin_cos_data_stage0 <= 32'hfbacbddb;
        168: sin_cos_data_stage0 <= 32'hfd09bd15;
        169: sin_cos_data_stage0 <= 32'hfe67bc4d;
        170: sin_cos_data_stage0 <= 32'hffc3bb85;
        171: sin_cos_data_stage0 <= 32'h011fbabc;
        172: sin_cos_data_stage0 <= 32'h027bb9f1;
        173: sin_cos_data_stage0 <= 32'h03d6b926;
        174: sin_cos_data_stage0 <= 32'h0530b859;
        175: sin_cos_data_stage0 <= 32'h0689b78b;
        176: sin_cos_data_stage0 <= 32'h07e2b6bd;
        177: sin_cos_data_stage0 <= 32'h093bb5ed;
        178: sin_cos_data_stage0 <= 32'h0a92b51c;
        179: sin_cos_data_stage0 <= 32'h0be9b44a;
        180: sin_cos_data_stage0 <= 32'h0d3fb377;
        181: sin_cos_data_stage0 <= 32'h0e95b2a3;
        182: sin_cos_data_stage0 <= 32'h0feab1ce;
        183: sin_cos_data_stage0 <= 32'h113fb0f8;
        184: sin_cos_data_stage0 <= 32'h1292b021;
        185: sin_cos_data_stage0 <= 32'h13e5af49;
        186: sin_cos_data_stage0 <= 32'h1538ae70;
        187: sin_cos_data_stage0 <= 32'h1689ad95;
        188: sin_cos_data_stage0 <= 32'h17dbacba;
        189: sin_cos_data_stage0 <= 32'h192babde;
        190: sin_cos_data_stage0 <= 32'h1a7bab00;
        191: sin_cos_data_stage0 <= 32'h1bcaaa22;
        192: sin_cos_data_stage0 <= 32'h1d18a943;
        193: sin_cos_data_stage0 <= 32'h1e66a862;
        194: sin_cos_data_stage0 <= 32'h1fb3a781;
        195: sin_cos_data_stage0 <= 32'h20ffa69e;
        196: sin_cos_data_stage0 <= 32'h224aa5bb;
        197: sin_cos_data_stage0 <= 32'h2395a4d6;
        198: sin_cos_data_stage0 <= 32'h24e0a3f1;
        199: sin_cos_data_stage0 <= 32'h2629a30a;
        200: sin_cos_data_stage0 <= 32'h2772a223;
        201: sin_cos_data_stage0 <= 32'h28baa13a;
        202: sin_cos_data_stage0 <= 32'h2a01a051;
        203: sin_cos_data_stage0 <= 32'h2b489f66;
        204: sin_cos_data_stage0 <= 32'h2c8e9e7b;
        205: sin_cos_data_stage0 <= 32'h2dd39d8e;
        206: sin_cos_data_stage0 <= 32'h2f179ca0;
        207: sin_cos_data_stage0 <= 32'h305b9bb2;
        208: sin_cos_data_stage0 <= 32'h319e9ac2;
        209: sin_cos_data_stage0 <= 32'h32e099d2;
        210: sin_cos_data_stage0 <= 32'h342298e0;
        211: sin_cos_data_stage0 <= 32'h356297ee;
        212: sin_cos_data_stage0 <= 32'h36a296fa;
        213: sin_cos_data_stage0 <= 32'h37e29606;
        214: sin_cos_data_stage0 <= 32'h39209510;
        215: sin_cos_data_stage0 <= 32'h3a5e941a;
        216: sin_cos_data_stage0 <= 32'h3b9b9323;
        217: sin_cos_data_stage0 <= 32'h3cd7922a;
        218: sin_cos_data_stage0 <= 32'h3e139131;
        219: sin_cos_data_stage0 <= 32'h3f4d9037;
        220: sin_cos_data_stage0 <= 32'h40878f3b;
        221: sin_cos_data_stage0 <= 32'h41c18e3f;
        222: sin_cos_data_stage0 <= 32'h42f98d42;
        223: sin_cos_data_stage0 <= 32'h44318c44;
        224: sin_cos_data_stage0 <= 32'h45678b45;
        225: sin_cos_data_stage0 <= 32'h469d8a45;
        226: sin_cos_data_stage0 <= 32'h47d38944;
        227: sin_cos_data_stage0 <= 32'h49078842;
        228: sin_cos_data_stage0 <= 32'h4a3b873f;
        229: sin_cos_data_stage0 <= 32'h4b6e863b;
        230: sin_cos_data_stage0 <= 32'h4ca08536;
        231: sin_cos_data_stage0 <= 32'h4dd18430;
        232: sin_cos_data_stage0 <= 32'h4f02832a;
        233: sin_cos_data_stage0 <= 32'h50318222;
        234: sin_cos_data_stage0 <= 32'h5160811a;
        235: sin_cos_data_stage0 <= 32'h528e8010;
        236: sin_cos_data_stage0 <= 32'h53bb7f06;
        237: sin_cos_data_stage0 <= 32'h54e87dfb;
        238: sin_cos_data_stage0 <= 32'h56137cee;
        239: sin_cos_data_stage0 <= 32'h573e7be1;
        240: sin_cos_data_stage0 <= 32'h58687ad3;
        241: sin_cos_data_stage0 <= 32'h599179c4;
        242: sin_cos_data_stage0 <= 32'h5aba78b4;
        243: sin_cos_data_stage0 <= 32'h5be177a4;
        244: sin_cos_data_stage0 <= 32'h5d087692;
        245: sin_cos_data_stage0 <= 32'h5e2d757f;
        246: sin_cos_data_stage0 <= 32'h5f52746c;
        247: sin_cos_data_stage0 <= 32'h60767358;
        248: sin_cos_data_stage0 <= 32'h619a7242;
        249: sin_cos_data_stage0 <= 32'h62bc712c;
        250: sin_cos_data_stage0 <= 32'h63de7015;
        251: sin_cos_data_stage0 <= 32'h64fe6efd;
        252: sin_cos_data_stage0 <= 32'h661e6de4;
        253: sin_cos_data_stage0 <= 32'h673d6ccb;
        254: sin_cos_data_stage0 <= 32'h685b6bb0;
        255: sin_cos_data_stage0 <= 32'h69786a95;
        endcase
    end
end


    (* ram_style = "block" *)  
always @(posedge CLK) begin
    if (CE) begin
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

localparam INTERNAL_DATA_BITS = SIN_COS_LOOKUP_DATA_BITS + 2 + EXTRA_DATA_BITS;
wire signed [INTERNAL_DATA_BITS-1:0] x_stage_1 = {1'b0, 1'b1, sin_cos_data_stage1[SIN_COS_LOOKUP_DATA_BITS-1:0], {EXTRA_DATA_BITS{1'b0}} }; 
wire signed [INTERNAL_DATA_BITS-1:0] y_stage_1 = {1'b0, sin_value_top_bit_stage1, sin_cos_data_stage1[SIN_COS_LOOKUP_DATA_BITS*2-1:SIN_COS_LOOKUP_DATA_BITS], {EXTRA_DATA_BITS{1'b0}} }; 
wire [ROTATIONS_LOOKUP_DATA_BITS:0] rotation_stage_1 = {rotation_data_stage1, first_rotation_sign_stage1};

reg signed [INTERNAL_DATA_BITS-1:0] x_buf[ROTATIONS-1:0];
reg signed [INTERNAL_DATA_BITS-1:0] y_buf[ROTATIONS-1:0];
//reg [ROTATIONS_LOOKUP_DATA_BITS-1:0] rotation_buf[ROTATIONS-1:0];
genvar i;
generate
    for (i = 0; i < ROTATIONS; i = i + 1) begin
        wire rot_angle;
        
        fixed_delay_shift_register #(
            // data width to store
            .DATA_BITS(1),
            .DELAY_CYCLES(i)
        )
        rot_angle_delay_inst
        (
            .CLK(CLK),
            .CE(CE),
            .RESET(RESET),
            .IN_VALUE( rotation_stage_1[i] ),
            .OUT_VALUE( rot_angle )
        );
                
        always @(posedge CLK) begin
            if (RESET) begin
                x_buf[i] <= 0;
                y_buf[i] <= 0;
                //rotation_buf[i] <= 0;
            end else if (CE) begin
                if (i == 0) begin
                    x_buf[i] <= rot_angle ? x_stage_1 + (y_stage_1 >>> (FIRST_ROTATION_SHIFT+i)) 
                                          : x_stage_1 - (y_stage_1 >>> (FIRST_ROTATION_SHIFT+i));
                    y_buf[i] <= rot_angle ? y_stage_1 - (x_stage_1 >>> (FIRST_ROTATION_SHIFT+i))
                                          : y_stage_1 + (x_stage_1 >>> (FIRST_ROTATION_SHIFT+i));
                    //rotation_buf[i] <= (rotation_stage_1 >> 1);
                end else begin
                    x_buf[i] <= rot_angle ? x_buf[i-1] + (y_buf[i-1] >>> (FIRST_ROTATION_SHIFT+i)) 
                                          : x_buf[i-1] - (y_buf[i-1] >>> (FIRST_ROTATION_SHIFT+i));
                    y_buf[i] <= rot_angle ? y_buf[i-1] - (x_buf[i-1] >>> (FIRST_ROTATION_SHIFT+i))
                                          : y_buf[i-1] + (x_buf[i-1] >>> (FIRST_ROTATION_SHIFT+i));
                    //rotation_buf[i] <= (rotation_buf[i-1] >> 1);
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
        x_out <= x_inverted + ROUNDING;// + neg_cos_delayed;
        y_out <= y_inverted + ROUNDING;// + neg_sin_delayed;
    end
end

//assign SIN = sin_cos_data_stage1[31:16];
//assign COS = sin_cos_data_stage1[15:0];
assign SIN = y_out[INTERNAL_DATA_BITS-1:INTERNAL_DATA_BITS-DATA_BITS];
assign COS = x_out[INTERNAL_DATA_BITS-1:INTERNAL_DATA_BITS-DATA_BITS];

endmodule
