#include "dsputils.h"
#include <QtMath>
#include <string.h>

SinTable::SinTable(int tableSizeBits, int valueBits, double scale, int phaseBits)
    : table(nullptr), phaseBits(phaseBits)
{
    init(tableSizeBits, valueBits, scale);
}

SinTable::~SinTable() {
    if (table)
        delete[] table;
}

void SinTable::init(int tableSizeBits, int valueBits, double scale, int phaseBits) {
    Q_ASSERT(tableSizeBits >= 4 && tableSizeBits <= 18);
    Q_ASSERT(valueBits >= 4 && valueBits <= 16);
    Q_ASSERT(phaseBits >= 16 && phaseBits <= 48);
    if (table)
        delete[] table;
    this->phaseBits = phaseBits;
    this->tableSizeBits = tableSizeBits;
    this->valueBits = valueBits;
    this->tableSize = (1 << tableSizeBits);
    this->table = new int[tableSize];
    // valueBits includes sign, so positive has valueBits-1 filled with all-ones
    int maxIntValue = (1 << (valueBits-1)) - 1;
    double targetScale = maxIntValue * scale;
    for (int i = 0; i < tableSize; i++) {
        double phase = (i + 0.5) * 2*M_PI / tableSize;
        double sinValue = sin(phase);
        double scaled = targetScale * sinValue;
        int rounded = (int)round(scaled);
        table[i] = rounded;
    }

    // test for quarter table and full table getters
    int64_t sum = 0;
    uint32_t phase = 0xffff0000;
    for (;;) {
        int quarterTableValue = getForPhase(phase);
        int fullTableValue = getForPhaseFull(phase);
        Q_ASSERT(quarterTableValue == fullTableValue);
        sum += fullTableValue;
        if (phase == 0)
            break;
        phase -= 0x10000;
    }
    Q_ASSERT(sum == 0);

    // probably max scale w/o errors is 8.062/16.0
    //qDebug("Done. Range error count: %d", errCount);
}


int SinTable::operator [](int index) const {
    Q_ASSERT((index >= 0) && (index < tableSize));
    return table[index];
}

// get sine table value from full table entry index, using only 1/4 of table
int SinTable::get(int fullTableIndex) const {
    int resultSign = (fullTableIndex & (tableSize >> 1)) ? -1 : 1;
    Q_ASSERT((fullTableIndex >= 0) && (fullTableIndex < tableSize));
    int quarterTableMask = ((tableSize >> 2) - 1);
    int quarterTableIndex = fullTableIndex & quarterTableMask;
    Q_ASSERT((quarterTableIndex >= 0) && (quarterTableIndex < tableSize / 4));
    if ((fullTableIndex & (tableSize >> 2))) {
        quarterTableIndex = quarterTableIndex ^ quarterTableMask;
    }
    Q_ASSERT((quarterTableIndex >= 0) && (quarterTableIndex < tableSize / 4));
    int tableEntry = table[quarterTableIndex];
    return tableEntry * resultSign;
}

int SinTable::getForPhase(uint64_t phase) const {
    int fullTableIndex = (int)(phase >> (phaseBits-tableSizeBits));
    return get(fullTableIndex);
}
int SinTable::getForPhaseFull(uint32_t phase) const {
    int fullTableIndex = (int)(phase >> (phaseBits-tableSizeBits));
    Q_ASSERT((fullTableIndex >= 0) && (fullTableIndex < tableSize));
    return table[fullTableIndex];
}

static const char * SIN_TABLE_VERILOG_CODE[] = {
    "",
    "",
    "/* ROM size: we need to store only 1/4 of table entries, the rest may be obtained by mirroring and changing the sign */",
    "localparam MEMSIZE = 1 << (ADDR_WIDTH - 2);",
    "",
    "/* first quarter of table always has 0 in upper bit, so we can store only DATA_WIDTH-1 bits in the table */",
    "(* ram_style = \"block\" *)  ",
    "reg [DATA_WIDTH-2:0] memory[0:MEMSIZE-1];",
    "       ",
    "/* store input phase value in register */",
    "reg [ADDR_WIDTH-1:0] phase_stage0;",
    "always @(posedge CLK)",
    "    if (RESET)",
    "        phase_stage0 <= 0;",
    "    else if (CE)",
    "        phase_stage0 <= PHASE;",
    " ",
    "/* second half of table is the same as first, but negative */",
    "wire need_sign_change = phase_stage0[ADDR_WIDTH-1];",
    "/* second and third quarters of table are mirrored by phase */",
    "wire need_phase_inverse = phase_stage0[ADDR_WIDTH-2];",
    "/* invert phase to have mirrored value */",
    "wire [ADDR_WIDTH-3:0] table_entry_index = phase_stage0[ADDR_WIDTH-3:0] ^ { (ADDR_WIDTH-2) {need_phase_inverse} };",
    " ",
    "/* propagate sign change flag to support negative values */",
    "reg need_sign_change_stage1;",
    "reg need_sign_change_stage2;",
    "always @(posedge CLK)",
    "    if (CE) begin",
    "        need_sign_change_stage1 <= need_sign_change;",
    "        need_sign_change_stage2 <= need_sign_change_stage1;",
    "    end",
    "        ",
    "/* block RAM based ROM - first stage of pipeline */",
    "reg [DATA_WIDTH-2:0] rddata_stage1;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        rddata_stage1 <= memory[table_entry_index];",
    " ",
    "/* block RAM based ROM - second stage of pipeline */",
    "reg [DATA_WIDTH-2:0] rddata_stage2;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        rddata_stage2 <= rddata_stage1;",
    " ",
    "/* this is output stage register, need sign bit as well */",
    "reg [DATA_WIDTH-1:0] rddata_stage3;",
    "always @(posedge CLK)",
    "    if (RESET)",
    "        rddata_stage3 <= 0; ",
    "    else if (CE)",
    "        rddata_stage3 <= need_sign_change_stage2 ? -{1'b0, rddata_stage2} : {1'b0, rddata_stage2}; ",
    " ",
    "/* propagate value to output */",
    "assign VALUE = rddata_stage3;",
    "",
    "",
    nullptr

};

static const char * STANDARD_PARAMS_VERILOG_CODE[] = {
    "/* input clock                                           */",
    "input wire CLK,",
    "/* clock enable, 1 = enable pipeline, 0 = pause pipeline */",
    "input wire CE,",
    "/* reset signal, active 1                                */",
    "input wire RESET,",
    "",
    nullptr
};

static const char * SIN_TABLE_PARAMS_VERILOG_CODE[] = {
    "/* sine table address to get value from                  */",
    "input wire [ADDR_WIDTH-1:0] PHASE,",
    "/* sine sample value from ROM, delayed by 4 clock cycles */",
    "output wire [DATA_WIDTH-1:0] VALUE",
    "",
    nullptr
};

static void writeSourceCode(FILE * f, const char ** lines, int indent = 1) {
    for (int i = 0; lines[i]!=nullptr; i++) {
        if (indent >= 1)
            fprintf(f, "    %s\n", lines[i]);
        else
            fprintf(f, "%s\n", lines[i]);
    }
}

// generate verilog source code with table
bool SinTable::generateVerilog(const char * filePath) {
    char fileName[4096];
    char moduleName[256];
    sprintf(moduleName, "sin_table_%d_%dbit", tableSize, valueBits);
    sprintf(fileName, "%s%s.v", filePath, moduleName);
    FILE * f = fopen(fileName, "wt");
    if (!f) {
        qDebug("Cannot open file %s for writing", fileName);
        return false;
    }
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "/* SINE table of size %d and %d-bit values */\n", tableSize, valueBits);
    fprintf(f, "/* Latency: 4 CLK cycles                     */\n");
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "module %s #(\n", moduleName);
    fprintf(f, "    parameter DATA_WIDTH = %d,\n", valueBits);
    fprintf(f, "    parameter ADDR_WIDTH = %d\n", tableSizeBits);
    fprintf(f, ")\n");
    fprintf(f, "(\n");
    writeSourceCode(f, STANDARD_PARAMS_VERILOG_CODE);
    writeSourceCode(f, SIN_TABLE_PARAMS_VERILOG_CODE);
    fprintf(f, ");\n");
    writeSourceCode(f, SIN_TABLE_VERILOG_CODE);
    fprintf(f, "\n");
    fprintf(f, "    /* Initialization of ROM content with first quarter of SINE function */\n");
    fprintf(f, "    initial begin\n");
    for (int i = 0; i < tableSize / 4; i++) {
        fprintf(f, "        memory[%d] = %d;\n", i, table[i]);
    }
    fprintf(f, "    end\n");
    fprintf(f, "\n");
    fprintf(f, "endmodule\n");
    fclose(f);
    return true;
}

static const char * SIN_TABLE_TB_PARAMS_VERILOG_CODE[] = {
    "logic [15:0] cycleCounter = 0;",
    "logic CLK;",
    "logic CE;",
    "logic RESET;",
    "logic [ADDR_WIDTH-1:0] PHASE;",
    "wire signed [DATA_WIDTH-1:0] VALUE;",
    "",
    nullptr
};

static const char * SIN_TABLE_TB_MAIN_VERILOG_CODE[] = {
    "#(",
    "    .DATA_WIDTH(DATA_WIDTH),",
    "    .ADDR_WIDTH(ADDR_WIDTH)",
    ")",
    "sin_table_instance",
    "(",
    "    .*",
    ");",
    "",
    "// drive CLK",
    "always begin",
    "    #1.472 CLK=0;",
    "    #1.472 CLK=1; ",
    "    cycleCounter = cycleCounter + 1;",
    "end",
    "",
    "task nextCycle(input [ADDR_WIDTH-1:0] phase);",
    "     begin",
    "         @(posedge CLK) #1 ;",
    "         PHASE = phase;",
    "         $display(\"    [%d]   nextCycle PHASE=%d\", cycleCounter, PHASE);",
    "     end",
    "endtask",
    " ",
    "task checkResult(input signed [DATA_WIDTH-1:0] expected_value);",
    "     begin",
    "         $display(\"    [%d]             VALUE=%d\", cycleCounter, VALUE);",
    "         if (VALUE != expected_value) begin",
    "             $display(\"    [%d] ERROR: VALUE does not match with expected result, expected %x actual %x\", cycleCounter, expected_value, VALUE);",
    "             $finish();",
    "         end",
    "     end",
    "endtask",
    " ",
    "// reset condition",
    "initial begin",
    "    RESET = 0;",
    "    CE = 0;",
    "    #200 @(posedge CLK) RESET = 1;",
    "    #100 @(posedge CLK) RESET = 0;",
    "    @(posedge CLK) #1 CE = 1;",
    "    PHASE = 0;",
    " ",
    "    $display(\"Starting verification\");",
    "",
    nullptr
};

static const char * SIN_TABLE_TB_END_VERILOG_CODE[] = {
    "    $display(\"Test passed\");",
    "    $finish();",
    "end",
    "",
    "endmodule",
    "",
    nullptr
};


// generate verilog source code for verification of sine table
bool SinTable::generateVerilogTestBench(const char * filePath) {
    char fileName[4096];
    char moduleName[256];
    sprintf(moduleName, "sin_table_%d_%dbit", tableSize, valueBits);
    sprintf(fileName, "%s%s_tb.sv", filePath, moduleName);
    FILE * f = fopen(fileName, "wt");
    if (!f) {
        qDebug("Cannot open file %s for writing", fileName);
        return false;
    }
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "/* SINE table of size %d and %d-bit values */\n", tableSize, valueBits);
    fprintf(f, "/* Latency: 4 CLK cycles                     */\n");
    fprintf(f, "/* TESTBENCH                                 */\n");
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "`timescale 1ns / 1ps\n");
    fprintf(f, "module %s_tb ();\n", moduleName);
    fprintf(f, "\n");
    fprintf(f, "localparam     DATA_WIDTH = %d;\n", valueBits);
    fprintf(f, "localparam     ADDR_WIDTH = %d;\n", tableSizeBits);
    fprintf(f, "\n");
    fprintf(f, "\n");
    writeSourceCode(f, SIN_TABLE_TB_PARAMS_VERILOG_CODE);
    fprintf(f, "    %s\n", moduleName);
    writeSourceCode(f, SIN_TABLE_TB_MAIN_VERILOG_CODE);
    fprintf(f, "\n");
    fprintf(f, "        /* Outputs are delayed by 4 clock cycles */\n");
    for (int i = 0; i < tableSize + 8; i++) {
        int phase = i % tableSize;
        int resultPhase = (tableSize + i - 4) % tableSize;
        int expectedResult = table[resultPhase];
        if (i < 4)
            fprintf(f, "        nextCycle(%d);\n", phase);
        else
            fprintf(f, "        nextCycle(%d); checkResult(%d);\n", phase, expectedResult);
    }

    writeSourceCode(f, SIN_TABLE_TB_END_VERILOG_CODE);
    fclose(f);
    return true;
}

static const char * SIN_COS_TABLE_PARAMS_VERILOG_CODE[] = {
    "/* sine table address to get value from                  */",
    "input wire [ADDR_WIDTH-1:0] PHASE,",
    "/* sin sample value from ROM, delayed by 4 clock cycles */",
    "output wire [DATA_WIDTH-1:0] SIN_VALUE,",
    "/* cos sample value from ROM, delayed by 4 clock cycles */",
    "output wire [DATA_WIDTH-1:0] COS_VALUE",
    "",
    nullptr
};

static const char * SIN_COS_TABLE_VERILOG_CODE[] = {
    "",
    "",
    "/* ROM size: we need to store only 1/4 of table entries, the rest may be obtained by mirroring and changing the sign */",
    "localparam MEMSIZE = 1 << (ADDR_WIDTH - 2);",
    "",
    "/* first quarter of table always has 0 in upper bit, so we can store only DATA_WIDTH-1 bits in the table */",
    "(* ram_style = \"block\" *)  ",
    "reg [DATA_WIDTH-2:0] memory[0:MEMSIZE-1];",
    "       ",
    "/* store input phase value in register */",
    "reg [ADDR_WIDTH-1:0] phase_stage0;",
    "always @(posedge CLK)",
    "    if (RESET)",
    "        phase_stage0 <= 0;",
    "    else if (CE)",
    "        phase_stage0 <= PHASE;",
    " ",
    "wire [1:0] sin_phase_top = phase_stage0[ADDR_WIDTH-1:ADDR_WIDTH-2];",
    "wire [1:0] cos_phase_top = phase_stage0[ADDR_WIDTH-1:ADDR_WIDTH-2] + 1;",
    " ",
    "/* second half of table is the same as first, but negative */",
    "wire sin_need_sign_change = sin_phase_top[1];",
    "/* second and third quarters of table are mirrored by phase */",
    "wire sin_need_phase_inverse = sin_phase_top[0];",
    " ",
    "/* second half of table is the same as first, but negative */",
    "wire cos_need_sign_change = cos_phase_top[1];",
    "/* second and third quarters of table are mirrored by phase */",
    "wire cos_need_phase_inverse = cos_phase_top[0];",
    " ",
    "/* SIN: invert phase to have mirrored value */",
    "wire [ADDR_WIDTH-3:0] sin_table_entry_index = phase_stage0[ADDR_WIDTH-3:0] ^ { (ADDR_WIDTH-2) {sin_need_phase_inverse} };",
    "/* COS: invert phase to have mirrored value */",
    "wire [ADDR_WIDTH-3:0] cos_table_entry_index = phase_stage0[ADDR_WIDTH-3:0] ^ { (ADDR_WIDTH-2) {cos_need_phase_inverse} };",
    " ",
    "/* propagate sign change flag to support negative values */",
    "reg sin_need_sign_change_stage1;",
    "reg cos_need_sign_change_stage1;",
    "reg sin_need_sign_change_stage2;",
    "reg cos_need_sign_change_stage2;",
    " ",
    "always @(posedge CLK)",
    "    if (CE) begin",
    "        sin_need_sign_change_stage1 <= sin_need_sign_change;",
    "        sin_need_sign_change_stage2 <= sin_need_sign_change_stage1;",
    "        cos_need_sign_change_stage1 <= cos_need_sign_change;",
    "        cos_need_sign_change_stage2 <= cos_need_sign_change_stage1;",
    "    end",
    "        ",
    "/* SIN table: block RAM based ROM - first stage of pipeline */",
    "reg [DATA_WIDTH-2:0] sin_rddata_stage1;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        sin_rddata_stage1 <= memory[sin_table_entry_index];",
    " ",
    "/* COS table: block RAM based ROM - first stage of pipeline */",
    "reg [DATA_WIDTH-2:0] cos_rddata_stage1;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        cos_rddata_stage1 <= memory[cos_table_entry_index];",
    " ",
    "/* SIN table: block RAM based ROM - second stage of pipeline */",
    "reg [DATA_WIDTH-2:0] sin_rddata_stage2;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        sin_rddata_stage2 <= sin_rddata_stage1;",
    " ",
    "/* COS table: block RAM based ROM - second stage of pipeline */",
    "reg [DATA_WIDTH-2:0] cos_rddata_stage2;",
    "always @(posedge CLK)",
    "    if (CE)",
    "        cos_rddata_stage2 <= cos_rddata_stage1;",
    " ",
    "/* SIN: this is output stage register, need sign bit as well */",
    "reg [DATA_WIDTH-1:0] sin_rddata_stage3;",
    "always @(posedge CLK)",
    "    if (RESET)",
    "        sin_rddata_stage3 <= 0; ",
    "    else if (CE)",
    "        sin_rddata_stage3 <= sin_need_sign_change_stage2 ? -{1'b0, sin_rddata_stage2} : {1'b0, sin_rddata_stage2}; ",
    " ",
    "/* COS: this is output stage register, need sign bit as well */",
    "reg [DATA_WIDTH-1:0] cos_rddata_stage3;",
    "always @(posedge CLK)",
    "    if (RESET)",
    "        cos_rddata_stage3 <= 0; ",
    "    else if (CE)",
    "        cos_rddata_stage3 <= cos_need_sign_change_stage2 ? -{1'b0, cos_rddata_stage2} : {1'b0, cos_rddata_stage2}; ",
    " ",
    "/* propagate value to output */",
    "assign SIN_VALUE = sin_rddata_stage3;",
    "assign COS_VALUE = cos_rddata_stage3;",
    "",
    "",
    nullptr

};

// generate verilog source code with sin & cos table
bool SinTable::generateSinCosVerilog(const char * filePath) {
    char fileName[4096];
    char moduleName[256];
    sprintf(moduleName, "sin_cos_table_%d_%dbit", tableSize, valueBits);
    sprintf(fileName, "%s%s.v", filePath, moduleName);
    FILE * f = fopen(fileName, "wt");
    if (!f) {
        qDebug("Cannot open file %s for writing", fileName);
        return false;
    }
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "/* SINE table of size %d and %d-bit values */\n", tableSize, valueBits);
    fprintf(f, "/* Latency: 4 CLK cycles                     */\n");
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "module %s #(\n", moduleName);
    fprintf(f, "    parameter DATA_WIDTH = %d,\n", valueBits);
    fprintf(f, "    parameter ADDR_WIDTH = %d\n", tableSizeBits);
    fprintf(f, ")\n");
    fprintf(f, "(\n");
    writeSourceCode(f, STANDARD_PARAMS_VERILOG_CODE);
    writeSourceCode(f, SIN_COS_TABLE_PARAMS_VERILOG_CODE);
    fprintf(f, ");\n");
    writeSourceCode(f, SIN_COS_TABLE_VERILOG_CODE);
    fprintf(f, "\n");
    fprintf(f, "    /* Initialization of ROM content with first quarter of SINE function */\n");
    fprintf(f, "    initial begin\n");
    for (int i = 0; i < tableSize / 4; i++) {
        fprintf(f, "        memory[%d] = %d;\n", i, table[i]);
    }
    fprintf(f, "    end\n");
    fprintf(f, "\n");
    fprintf(f, "endmodule\n");
    fclose(f);
    return true;
}

static const char * SIN_COS_TABLE_TB_PARAMS_VERILOG_CODE[] = {
    "logic [15:0] cycleCounter = 0;",
    "logic CLK;",
    "logic CE;",
    "logic RESET;",
    "logic [ADDR_WIDTH-1:0] PHASE;",
    "wire signed [DATA_WIDTH-1:0] SIN_VALUE;",
    "wire signed [DATA_WIDTH-1:0] COS_VALUE;",
    "",
    nullptr
};

static const char * SIN_COS_TABLE_TB_MAIN_VERILOG_CODE[] = {
    "#(",
    "    .DATA_WIDTH(DATA_WIDTH),",
    "    .ADDR_WIDTH(ADDR_WIDTH)",
    ")",
    "sin_table_instance",
    "(",
    "    .*",
    ");",
    "",
    "// drive CLK",
    "always begin",
    "    #1.472 CLK=0;",
    "    #1.472 CLK=1; ",
    "    cycleCounter = cycleCounter + 1;",
    "end",
    "",
    "task nextCycle(input [ADDR_WIDTH-1:0] phase);",
    "     begin",
    "         @(posedge CLK) #1 ;",
    "         PHASE = phase;",
    "         $display(\"    [%d]   nextCycle PHASE=%d\", cycleCounter, PHASE);",
    "     end",
    "endtask",
    " ",
    "task checkResult(input signed [DATA_WIDTH-1:0] expected_sin_value, input signed [DATA_WIDTH-1:0] expected_cos_value);",
    "     begin",
    "         $display(\"    [%d]             SIN=%d COS=%d\", cycleCounter, SIN_VALUE, COS_VALUE);",
    "         if (SIN_VALUE != expected_sin_value) begin",
    "             $display(\"    [%d] ERROR: SIN_VALUE does not match with expected result, expected %x actual %x\", cycleCounter, expected_sin_value, SIN_VALUE);",
    "             $finish();",
    "         end",
    "         if (COS_VALUE != expected_cos_value) begin",
    "             $display(\"    [%d] ERROR: COS_VALUE does not match with expected result, expected %x actual %x\", cycleCounter, expected_cos_value, COS_VALUE);",
    "             $finish();",
    "         end",
    "     end",
    "endtask",
    " ",
    "// reset condition",
    "initial begin",
    "    RESET = 0;",
    "    CE = 0;",
    "    #200 @(posedge CLK) RESET = 1;",
    "    #100 @(posedge CLK) RESET = 0;",
    "    @(posedge CLK) #1 CE = 1;",
    "    PHASE = 0;",
    " ",
    "    $display(\"Starting verification\");",
    "",
    nullptr
};


// generate verilog source code with verificatoin of sin & cos table
bool SinTable::generateSinCosVerilogTestBench(const char * filePath) {
    char fileName[4096];
    char moduleName[256];
    sprintf(moduleName, "sin_cos_table_%d_%dbit", tableSize, valueBits);
    sprintf(fileName, "%s%s_tb.sv", filePath, moduleName);
    FILE * f = fopen(fileName, "wt");
    if (!f) {
        qDebug("Cannot open file %s for writing", fileName);
        return false;
    }
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "/* SINE table of size %d and %d-bit values */\n", tableSize, valueBits);
    fprintf(f, "/* Latency: 4 CLK cycles                     */\n");
    fprintf(f, "/* TESTBENCH                                 */\n");
    fprintf(f, "/*********************************************/\n");
    fprintf(f, "`timescale 1ns / 1ps\n");
    fprintf(f, "module %s_tb ();\n", moduleName);
    fprintf(f, "\n");
    fprintf(f, "localparam     DATA_WIDTH = %d;\n", valueBits);
    fprintf(f, "localparam     ADDR_WIDTH = %d;\n", tableSizeBits);
    fprintf(f, "\n");
    fprintf(f, "\n");
    writeSourceCode(f, SIN_COS_TABLE_TB_PARAMS_VERILOG_CODE);
    fprintf(f, "    %s\n", moduleName);
    writeSourceCode(f, SIN_COS_TABLE_TB_MAIN_VERILOG_CODE);
    fprintf(f, "\n");
    fprintf(f, "        /* Outputs are delayed by 4 clock cycles */\n");
    for (int i = 0; i < tableSize + 8; i++) {
        int phase = i % tableSize;
        int resultPhase = (tableSize + i - 4) % tableSize;
        int resultCosPhase = (tableSize/4 + i - 4) % tableSize;
        int expectedResult = table[resultPhase];
        int expectedCosResult = table[resultCosPhase];
        if (i < 4)
            fprintf(f, "        nextCycle(%d);\n", phase);
        else
            fprintf(f, "        nextCycle(%d); checkResult(%d, %d);\n", phase, expectedResult, expectedCosResult);
    }

    writeSourceCode(f, SIN_TABLE_TB_END_VERILOG_CODE);
    fclose(f);
    return true;
}
