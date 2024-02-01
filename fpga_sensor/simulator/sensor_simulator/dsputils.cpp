#include "dsputils.h"
#include <QtMath>
#include <string.h>

void SimClock::add(SimDevice * device) {
    if (_devicesCount >= _devicesSize) {
        if (!_devices) {
            _devicesSize = 16;
            _devices = new SimDevice *[_devicesSize];
        } else {
            int newSize = _devicesSize * 2;
            SimDevice ** newDevices = new SimDevice *[newSize];
            for (int i = 0; i < _devicesCount; i++) {
                newDevices[i] = _devices[i];
            }
            delete[] _devices;
            _devices = newDevices;
            _devicesSize = newSize;
        }
    }
    _devices[_devicesCount++] = device;
}

void SimClock::onUpdate() {
    for (int i = 0; i < _devicesCount; i++) {
        _devices[i]->onUpdate();
    }
}

void SimClock::onClock() {
    for (int i = 0; i < _devicesCount; i++) {
        _devices[i]->onClock();
    }
}

void SimClock::onReset() {
    for (int i = 0; i < _devicesCount; i++) {
        _devices[i]->onReset();
    }
}

SimClock::~SimClock() {
    if (_devices)
        delete[] _devices;
}

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
    Q_ASSERT(valueBits >= 4 && valueBits <= 18);
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

// generate init file for $readmemh
bool SinTable::generateMemInitFile(const char * filePath) {
    char fileName[4096];
    char moduleName[256];
    sprintf(moduleName, "sin_table_%d_%dbit", tableSize, valueBits);
    sprintf(fileName, "%s%s.mem", filePath, moduleName);
    FILE * f = fopen(fileName, "wt");
    if (!f) {
        qDebug("Cannot open file %s for writing", fileName);
        return false;
    }
    for (int i = 0; i < tableSize / 4; i++) {
        if (valueBits-1 > 12) {
            fprintf(f, "%04x\n", table[i]);
        } else if (valueBits-1 > 8) {
            fprintf(f, "%03x\n", table[i]);
        } else {
            fprintf(f, "%02x\n", table[i]);
        }
    }
    fclose(f);
    return true;
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
    fprintf(f, "    localparam     DATA_WIDTH = %d;\n", valueBits);
    fprintf(f, "    localparam     ADDR_WIDTH = %d;\n", tableSizeBits);
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

// initial value for scaling unity length vector for CORDIC
#define CORDIC_SIN_COS_K 0.60725293500888111176

// t[n] = atan(2^(-n)) converted to 32-bit phase angle
static const int32_t CORDIC_ATAN_TABLE_INT32[30] = {
    0x20000000, // [0] 0.785398163397448
    0x12e4051d, // [1] 0.463647609000806
    0x09fb385b, // [2] 0.244978663126864
    0x051111d4, // [3] 0.124354994546761
    0x028b0d43, // [4] 0.062418809995957
    0x0145d7e1, // [5] 0.031239833430268
    0x00a2f61e, // [6] 0.015623728620477
    0x00517c55, // [7] 0.007812341060101
    0x0028be53, // [8] 0.003906230131967
    0x00145f2e, // [9] 0.001953122516479
    0x000a2f98, // [10] 0.000976562189559
    0x000517cc, // [11] 0.000488281211195
    0x00028be6, // [12] 0.000244140620149
    0x000145f3, // [13] 0.000122070311894
    0x0000a2f9, // [14] 0.000061035156174
    0x0000517c, // [15] 0.000030517578116
    0x000028be, // [16] 0.000015258789061
    0x0000145f, // [17] 0.000007629394531
    0x00000a2f, // [18] 0.000003814697266
    0x00000517, // [19] 0.000001907348633
    0x0000028b, // [20] 0.000000953674316
    0x00000145, // [21] 0.000000476837158
    0x000000a2, // [22] 0.000000238418579
    0x00000051, // [23] 0.000000119209290
    0x00000028, // [24] 0.000000059604645
    0x00000014, // [25] 0.000000029802322
    0x0000000a, // [26] 0.000000014901161
    0x00000005, // [27] 0.000000007450581
    0x00000002, // [28] 0.000000003725290
    0x00000001, // [29] 0.000000001862645
};

//
// init table for x,y for CORDIC SIN & COS
// 0..PI/2 range is divided into 128 pieces
// each piece is +/- 0.006135923, so CORDIC should start
//   with entry [8] (2^-8), +-0.00390623 +-0.001953123 +- 0.000976562 ...
// middle angle for t[i] in 32-bit phase scale a32 = (i << 23) + (1 << 22);
// cos(a) = t[i][0] 31-bit positive scaled by 0.60725293500888111176
// sin(a) = t[i][1] 31-bit positive scaled by 0.60725293500888111176
//
static int CORDIC_SIN_COS_INIT_TABLE_128_0_PI2[128][2] = {
    {0x7fff6216, 0x00c90f87}, // [0] middle angle=0.006135923151543 (00400000)
    {0x7ffa72d1, 0x025b26d7}, // [1] middle angle=0.018407769454628 (00c00000)
    {0x7ff09477, 0x03ed26e6}, // [2] middle angle=0.030679615757713 (01400000)
    {0x7fe1c76b, 0x057f0034}, // [3] middle angle=0.042951462060798 (01c00000)
    {0x7fce0c3e, 0x0710a344}, // [4] middle angle=0.055223308363883 (02400000)
    {0x7fb563b2, 0x08a2009a}, // [5] middle angle=0.067495154666968 (02c00000)
    {0x7f97cebc, 0x0a3308bc}, // [6] middle angle=0.079767000970053 (03400000)
    {0x7f754e7f, 0x0bc3ac35}, // [7] middle angle=0.092038847273138 (03c00000)
    {0x7f4de450, 0x0d53db92}, // [8] middle angle=0.104310693576224 (04400000)
    {0x7f2191b3, 0x0ee38765}, // [9] middle angle=0.116582539879309 (04c00000)
    {0x7ef0585f, 0x1072a047}, // [10] middle angle=0.128854386182394 (05400000)
    {0x7eba3a39, 0x120116d4}, // [11] middle angle=0.141126232485479 (05c00000)
    {0x7e7f3956, 0x138edbb0}, // [12] middle angle=0.153398078788564 (06400000)
    {0x7e3f57fe, 0x151bdf85}, // [13] middle angle=0.165669925091649 (06c00000)
    {0x7dfa98a7, 0x16a81304}, // [14] middle angle=0.177941771394734 (07400000)
    {0x7db0fdf7, 0x183366e8}, // [15] middle angle=0.190213617697819 (07c00000)
    {0x7d628ac5, 0x19bdcbf2}, // [16] middle angle=0.202485464000905 (08400000)
    {0x7d0f4217, 0x1b4732ef}, // [17] middle angle=0.214757310303990 (08c00000)
    {0x7cb72724, 0x1ccf8cb3}, // [18] middle angle=0.227029156607075 (09400000)
    {0x7c5a3d4f, 0x1e56ca1e}, // [19] middle angle=0.239301002910160 (09c00000)
    {0x7bf88830, 0x1fdcdc1a}, // [20] middle angle=0.251572849213245 (0a400000)
    {0x7b920b89, 0x2161b39f}, // [21] middle angle=0.263844695516330 (0ac00000)
    {0x7b26cb4f, 0x22e541ae}, // [22] middle angle=0.276116541819415 (0b400000)
    {0x7ab6cba3, 0x24677757}, // [23] middle angle=0.288388388122501 (0bc00000)
    {0x7a4210d8, 0x25e845b5}, // [24] middle angle=0.300660234425586 (0c400000)
    {0x79c89f6d, 0x27679df4}, // [25] middle angle=0.312932080728671 (0cc00000)
    {0x794a7c11, 0x28e5714a}, // [26] middle angle=0.325203927031756 (0d400000)
    {0x78c7aba1, 0x2a61b101}, // [27] middle angle=0.337475773334841 (0dc00000)
    {0x78403328, 0x2bdc4e6f}, // [28] middle angle=0.349747619637926 (0e400000)
    {0x77b417df, 0x2d553afb}, // [29] middle angle=0.362019465941011 (0ec00000)
    {0x77235f2d, 0x2ecc681e}, // [30] middle angle=0.374291312244096 (0f400000)
    {0x768e0ea5, 0x3041c760}, // [31] middle angle=0.386563158547182 (0fc00000)
    {0x75f42c0a, 0x31b54a5d}, // [32] middle angle=0.398835004850267 (10400000)
    {0x7555bd4b, 0x3326e2c2}, // [33] middle angle=0.411106851153352 (10c00000)
    {0x74b2c883, 0x3496824f}, // [34] middle angle=0.423378697456437 (11400000)
    {0x740b53fa, 0x36041ad8}, // [35] middle angle=0.435650543759522 (11c00000)
    {0x735f6626, 0x376f9e46}, // [36] middle angle=0.447922390062607 (12400000)
    {0x72af05a6, 0x38d8fe93}, // [37] middle angle=0.460194236365692 (12c00000)
    {0x71fa3948, 0x3a402dd1}, // [38] middle angle=0.472466082668777 (13400000)
    {0x71410804, 0x3ba51e29}, // [39] middle angle=0.484737928971863 (13c00000)
    {0x708378fe, 0x3d07c1d5}, // [40] middle angle=0.497009775274948 (14400000)
    {0x6fc19385, 0x3e680b2c}, // [41] middle angle=0.509281621578033 (14c00000)
    {0x6efb5f12, 0x3fc5ec97}, // [42] middle angle=0.521553467881118 (15400000)
    {0x6e30e349, 0x4121589a}, // [43] middle angle=0.533825314184203 (15c00000)
    {0x6d6227fa, 0x427a41d0}, // [44] middle angle=0.546097160487288 (16400000)
    {0x6c8f351c, 0x43d09aec}, // [45] middle angle=0.558369006790373 (16c00000)
    {0x6bb812d0, 0x452456bc}, // [46] middle angle=0.570640853093458 (17400000)
    {0x6adcc964, 0x46756827}, // [47] middle angle=0.582912699396544 (17c00000)
    {0x69fd614a, 0x47c3c22e}, // [48] middle angle=0.595184545699629 (18400000)
    {0x6919e320, 0x490f57ee}, // [49] middle angle=0.607456392002714 (18c00000)
    {0x683257aa, 0x4a581c9d}, // [50] middle angle=0.619728238305799 (19400000)
    {0x6746c7d7, 0x4b9e038f}, // [51] middle angle=0.632000084608884 (19c00000)
    {0x66573cbb, 0x4ce10034}, // [52] middle angle=0.644271930911969 (1a400000)
    {0x6563bf92, 0x4e210617}, // [53] middle angle=0.656543777215054 (1ac00000)
    {0x646c59bf, 0x4f5e08e3}, // [54] middle angle=0.668815623518140 (1b400000)
    {0x637114cc, 0x5097fc5e}, // [55] middle angle=0.681087469821225 (1bc00000)
    {0x6271fa69, 0x51ced46e}, // [56] middle angle=0.693359316124310 (1c400000)
    {0x616f146b, 0x53028517}, // [57] middle angle=0.705631162427395 (1cc00000)
    {0x60686cce, 0x5433027d}, // [58] middle angle=0.717903008730480 (1d400000)
    {0x5f5e0db3, 0x556040e2}, // [59] middle angle=0.730174855033565 (1dc00000)
    {0x5e50015d, 0x568a34a9}, // [60] middle angle=0.742446701336650 (1e400000)
    {0x5d3e5236, 0x57b0d256}, // [61] middle angle=0.754718547639735 (1ec00000)
    {0x5c290acc, 0x58d40e8c}, // [62] middle angle=0.766990393942821 (1f400000)
    {0x5b1035cf, 0x59f3de12}, // [63] middle angle=0.779262240245906 (1fc00000)
    {0x59f3de12, 0x5b1035cf}, // [64] middle angle=0.791534086548991 (20400000)
    {0x58d40e8c, 0x5c290acc}, // [65] middle angle=0.803805932852076 (20c00000)
    {0x57b0d256, 0x5d3e5236}, // [66] middle angle=0.816077779155161 (21400000)
    {0x568a34a9, 0x5e50015d}, // [67] middle angle=0.828349625458246 (21c00000)
    {0x556040e2, 0x5f5e0db3}, // [68] middle angle=0.840621471761331 (22400000)
    {0x5433027d, 0x60686cce}, // [69] middle angle=0.852893318064416 (22c00000)
    {0x53028517, 0x616f146b}, // [70] middle angle=0.865165164367502 (23400000)
    {0x51ced46e, 0x6271fa69}, // [71] middle angle=0.877437010670587 (23c00000)
    {0x5097fc5e, 0x637114cc}, // [72] middle angle=0.889708856973672 (24400000)
    {0x4f5e08e3, 0x646c59bf}, // [73] middle angle=0.901980703276757 (24c00000)
    {0x4e210617, 0x6563bf92}, // [74] middle angle=0.914252549579842 (25400000)
    {0x4ce10034, 0x66573cbb}, // [75] middle angle=0.926524395882927 (25c00000)
    {0x4b9e038f, 0x6746c7d7}, // [76] middle angle=0.938796242186012 (26400000)
    {0x4a581c9d, 0x683257aa}, // [77] middle angle=0.951068088489098 (26c00000)
    {0x490f57ee, 0x6919e320}, // [78] middle angle=0.963339934792183 (27400000)
    {0x47c3c22e, 0x69fd614a}, // [79] middle angle=0.975611781095268 (27c00000)
    {0x46756827, 0x6adcc964}, // [80] middle angle=0.987883627398353 (28400000)
    {0x452456bc, 0x6bb812d0}, // [81] middle angle=1.000155473701438 (28c00000)
    {0x43d09aec, 0x6c8f351c}, // [82] middle angle=1.012427320004523 (29400000)
    {0x427a41d0, 0x6d6227fa}, // [83] middle angle=1.024699166307608 (29c00000)
    {0x4121589a, 0x6e30e349}, // [84] middle angle=1.036971012610693 (2a400000)
    {0x3fc5ec97, 0x6efb5f12}, // [85] middle angle=1.049242858913779 (2ac00000)
    {0x3e680b2c, 0x6fc19385}, // [86] middle angle=1.061514705216864 (2b400000)
    {0x3d07c1d5, 0x708378fe}, // [87] middle angle=1.073786551519949 (2bc00000)
    {0x3ba51e29, 0x71410804}, // [88] middle angle=1.086058397823034 (2c400000)
    {0x3a402dd1, 0x71fa3948}, // [89] middle angle=1.098330244126119 (2cc00000)
    {0x38d8fe93, 0x72af05a6}, // [90] middle angle=1.110602090429204 (2d400000)
    {0x376f9e46, 0x735f6626}, // [91] middle angle=1.122873936732289 (2dc00000)
    {0x36041ad8, 0x740b53fa}, // [92] middle angle=1.135145783035374 (2e400000)
    {0x3496824f, 0x74b2c883}, // [93] middle angle=1.147417629338460 (2ec00000)
    {0x3326e2c2, 0x7555bd4b}, // [94] middle angle=1.159689475641545 (2f400000)
    {0x31b54a5d, 0x75f42c0a}, // [95] middle angle=1.171961321944630 (2fc00000)
    {0x3041c760, 0x768e0ea5}, // [96] middle angle=1.184233168247715 (30400000)
    {0x2ecc681e, 0x77235f2d}, // [97] middle angle=1.196505014550800 (30c00000)
    {0x2d553afb, 0x77b417df}, // [98] middle angle=1.208776860853885 (31400000)
    {0x2bdc4e6f, 0x78403328}, // [99] middle angle=1.221048707156970 (31c00000)
    {0x2a61b101, 0x78c7aba1}, // [100] middle angle=1.233320553460056 (32400000)
    {0x28e5714a, 0x794a7c11}, // [101] middle angle=1.245592399763141 (32c00000)
    {0x27679df4, 0x79c89f6d}, // [102] middle angle=1.257864246066226 (33400000)
    {0x25e845b5, 0x7a4210d8}, // [103] middle angle=1.270136092369311 (33c00000)
    {0x24677757, 0x7ab6cba3}, // [104] middle angle=1.282407938672396 (34400000)
    {0x22e541ae, 0x7b26cb4f}, // [105] middle angle=1.294679784975481 (34c00000)
    {0x2161b39f, 0x7b920b89}, // [106] middle angle=1.306951631278566 (35400000)
    {0x1fdcdc1a, 0x7bf88830}, // [107] middle angle=1.319223477581651 (35c00000)
    {0x1e56ca1e, 0x7c5a3d4f}, // [108] middle angle=1.331495323884736 (36400000)
    {0x1ccf8cb3, 0x7cb72724}, // [109] middle angle=1.343767170187822 (36c00000)
    {0x1b4732ef, 0x7d0f4217}, // [110] middle angle=1.356039016490907 (37400000)
    {0x19bdcbf2, 0x7d628ac5}, // [111] middle angle=1.368310862793992 (37c00000)
    {0x183366e8, 0x7db0fdf7}, // [112] middle angle=1.380582709097077 (38400000)
    {0x16a81304, 0x7dfa98a7}, // [113] middle angle=1.392854555400162 (38c00000)
    {0x151bdf85, 0x7e3f57fe}, // [114] middle angle=1.405126401703247 (39400000)
    {0x138edbb0, 0x7e7f3956}, // [115] middle angle=1.417398248006333 (39c00000)
    {0x120116d4, 0x7eba3a39}, // [116] middle angle=1.429670094309418 (3a400000)
    {0x1072a047, 0x7ef0585f}, // [117] middle angle=1.441941940612503 (3ac00000)
    {0x0ee38765, 0x7f2191b3}, // [118] middle angle=1.454213786915588 (3b400000)
    {0x0d53db92, 0x7f4de450}, // [119] middle angle=1.466485633218673 (3bc00000)
    {0x0bc3ac35, 0x7f754e7f}, // [120] middle angle=1.478757479521758 (3c400000)
    {0x0a3308bc, 0x7f97cebc}, // [121] middle angle=1.491029325824843 (3cc00000)
    {0x08a2009a, 0x7fb563b2}, // [122] middle angle=1.503301172127928 (3d400000)
    {0x0710a344, 0x7fce0c3e}, // [123] middle angle=1.515573018431013 (3dc00000)
    {0x057f0034, 0x7fe1c76b}, // [124] middle angle=1.527844864734099 (3e400000)
    {0x03ed26e6, 0x7ff09477}, // [125] middle angle=1.540116711037184 (3ec00000)
    {0x025b26d7, 0x7ffa72d1}, // [126] middle angle=1.552388557340269 (3f400000)
    {0x00c90f87, 0x7fff6216}, // [127] middle angle=1.564660403643354 (3fc00000)
};

// after initialization of CORDIC X and Y using 128 entries of CORDIC_SIN_COS_INIT_TABLE_128_0_PI2 table,
// start CORDIC from item 8 of CORDIC_ATAN_TABLE_INT32 table, using following CORDIC_SIN_COS_SMALL_ANGLE_SIGN_MASK_8 bitmask to get sign for each step
// t[lower_9_bits_of_phase] & (1<<0) gives sign for entry[0]
// t[lower_9_bits_of_phase] & (1<<1) gives sign for entry[1]
//                                   and so on
static int CORDIC_SIN_COS_SMALL_ANGLE_SIGN_MASK_8[512] = {
    0x003daa27, // [0] -0.006123938926637 (ffc02001)  -4186111=+2670163+1335086+667544-333772-166886+83443-41721-20860-10430+5215-2607+1303-651+325-162+81+40-20+10+5+2+1(+ -2)
    0x001c8fc7, // [1] -0.006099970476826 (ffc06000)  -4169728=+2670163+1335086+667544-333772-166886-83443+41721+20860+10430+5215+2607+1303-651-325-162+81-40-20+10+5+2-1(+ -1)
    0x000d31c7, // [2] -0.006076002027016 (ffc0a000)  -4153344=+2670163+1335086+667544-333772-166886-83443+41721+20860+10430-5215-2607-1303+651+325-162-81+40-20+10+5-2-1(+ -1)
    0x002414c7, // [3] -0.006052033577205 (ffc0e000)  -4136960=+2670163+1335086+667544-333772-166886-83443+41721+20860-10430-5215+2607-1303+651-325-162-81-40-20+10-5-2+1(+ -1)
    0x0005c347, // [4] -0.006028065127394 (ffc12000)  -4120576=+2670163+1335086+667544-333772-166886-83443+41721-20860+10430+5215-2607-1303-651-325+162+81+40-20+10-5-2-1(+ 1)
    0x0038fa47, // [5] -0.006004096677584 (ffc16000)  -4104192=+2670163+1335086+667544-333772-166886-83443+41721-20860-10430+5215-2607+1303+651+325+162+81-40-20-10+5+2+1(+ -1)
    0x00295f87, // [6] -0.005980128227773 (ffc1a000)  -4087808=+2670163+1335086+667544-333772-166886-83443-41721+20860+10430+5215+2607+1303+651-325+162-81+40-20-10+5-2+1(+ -1)
    0x00106987, // [7] -0.005956159777962 (ffc1e000)  -4071424=+2670163+1335086+667544-333772-166886-83443-41721+20860+10430-5215-2607+1303-651+325+162-81-40-20-10-5+2-1(+ -1)
    0x00018c87, // [8] -0.005932191328152 (ffc22000)  -4055040=+2670163+1335086+667544-333772-166886-83443-41721+20860-10430-5215+2607+1303-651-325-162+81+40-20-10-5-2-1(+ 1)
    0x001f3307, // [9] -0.005908222878341 (ffc26000)  -4038656=+2670163+1335086+667544-333772-166886-83443-41721-20860+10430+5215-2607-1303+651+325-162-81+40+20+10+5+2-1(+ -1)
    0x00361607, // [10] -0.005884254428530 (ffc2a001)  -4022271=+2670163+1335086+667544-333772-166886-83443-41721-20860-10430+5215+2607-1303+651-325-162-81-40+20+10-5+2+1(+ 0)
    0x0027c007, // [11] -0.005860285978719 (ffc2e001)  -4005887=+2670163+1335086+667544-333772-166886-83443-41721-20860-10430-5215-2607-1303-651-325+162+81+40+20+10-5-2+1(+ 0)
    0x0016f9fb, // [12] -0.005836317528909 (ffc32000)  -3989504=+2670163+1335086-667544+333772+166886+83443+41721+20860+10430-5215-2607+1303+651+325+162+81-40+20+10-5+2-1(+ -1)
    0x00075cfb, // [13] -0.005812349079098 (ffc36000)  -3973120=+2670163+1335086-667544+333772+166886+83443+41721+20860-10430-5215+2607+1303+651-325+162-81+40+20+10-5-2-1(+ 1)
    0x000a6b7b, // [14] -0.005788380629287 (ffc3a000)  -3956736=+2670163+1335086-667544+333772+166886+83443+41721-20860+10430+5215-2607+1303-651+325+162-81-40+20-10+5-2-1(+ -1)
    0x00138e7b, // [15] -0.005764412179477 (ffc3e000)  -3940352=+2670163+1335086-667544+333772+166886+83443+41721-20860-10430+5215+2607+1303-651-325-162+81+40+20-10-5+2-1(+ -1)
    0x0002b07b, // [16] -0.005740443729666 (ffc42000)  -3923968=+2670163+1335086-667544+333772+166886+83443+41721-20860-10430-5215-2607-1303+651+325-162+81-40+20-10-5-2-1(+ 1)
    0x001d15bb, // [17] -0.005716475279855 (ffc46000)  -3907584=+2670163+1335086-667544+333772+166886+83443-41721+20860+10430-5215+2607-1303+651-325-162-81+40-20+10+5+2-1(+ -1)
    0x003422bb, // [18] -0.005692506830044 (ffc4a000)  -3891200=+2670163+1335086-667544+333772+166886+83443-41721+20860-10430+5215-2607-1303-651+325-162-81-40-20+10-5+2+1(+ -1)
    0x000dfb3b, // [19] -0.005668538380234 (ffc4e000)  -3874816=+2670163+1335086-667544+333772+166886+83443-41721-20860+10430+5215-2607+1303+651+325+162+81+40-20+10+5-2-1(+ 1)
    0x0004de3b, // [20] -0.005644569930423 (ffc52000)  -3858432=+2670163+1335086-667544+333772+166886+83443-41721-20860-10430+5215+2607+1303+651-325+162+81-40-20+10-5-2-1(+ -1)
    0x0019683b, // [21] -0.005620601480612 (ffc56001)  -3842047=+2670163+1335086-667544+333772+166886+83443-41721-20860-10430-5215-2607+1303-651+325+162-81+40-20-10+5+2-1(+ 0)
    0x00084ddb, // [22] -0.005596633030802 (ffc5a000)  -3825664=+2670163+1335086-667544+333772+166886-83443+41721+20860+10430-5215+2607+1303-651-325+162-81-40-20-10+5-2-1(+ -1)
    0x0031b2db, // [23] -0.005572664580991 (ffc5e000)  -3809280=+2670163+1335086-667544+333772+166886-83443+41721+20860-10430+5215-2607-1303+651+325-162+81+40-20-10-5+2+1(+ -1)
    0x0000975b, // [24] -0.005548696131180 (ffc62000)  -3792896=+2670163+1335086-667544+333772+166886-83443+41721-20860+10430+5215+2607-1303+651-325-162+81-40-20-10-5-2-1(+ 1)
    0x002e215b, // [25] -0.005524727681369 (ffc66000)  -3776512=+2670163+1335086-667544+333772+166886-83443+41721-20860+10430-5215-2607-1303-651+325-162-81-40+20+10+5-2+1(+ -1)
    0x001ff85b, // [26] -0.005500759231559 (ffc6a000)  -3760128=+2670163+1335086-667544+333772+166886-83443+41721-20860-10430-5215-2607+1303+651+325+162+81+40+20+10+5+2-1(+ -1)
    0x000edd9b, // [27] -0.005476790781748 (ffc6e000)  -3743744=+2670163+1335086-667544+333772+166886-83443-41721+20860+10430-5215+2607+1303+651-325+162+81-40+20+10+5-2-1(+ 1)
    0x00276a9b, // [28] -0.005452822331937 (ffc72000)  -3727360=+2670163+1335086-667544+333772+166886-83443-41721+20860-10430+5215-2607+1303-651+325+162-81+40+20+10-5-2+1(+ -1)
    0x001a4f1b, // [29] -0.005428853882127 (ffc76000)  -3710976=+2670163+1335086-667544+333772+166886-83443-41721-20860+10430+5215+2607+1303-651-325+162-81-40+20-10+5+2-1(+ -1)
    0x000bb11b, // [30] -0.005404885432316 (ffc7a000)  -3694592=+2670163+1335086-667544+333772+166886-83443-41721-20860+10430-5215-2607-1303+651+325-162+81+40+20-10+5-2-1(+ -1)
    0x0022941b, // [31] -0.005380916982505 (ffc7e000)  -3678208=+2670163+1335086-667544+333772+166886-83443-41721-20860-10430-5215+2607-1303+651-325-162+81-40+20-10-5-2+1(+ -1)
    0x000323eb, // [32] -0.005356948532694 (ffc82001)  -3661823=+2670163+1335086-667544+333772-166886+83443+41721+20860+10430+5215-2607-1303-651+325-162-81+40+20-10-5-2-1(+ 0)
    0x002c06eb, // [33] -0.005332980082884 (ffc86000)  -3645440=+2670163+1335086-667544+333772-166886+83443+41721+20860-10430+5215+2607-1303-651-325-162-81-40-20+10+5-2+1(+ -1)
    0x001ddf6b, // [34] -0.005309011633073 (ffc8a000)  -3629056=+2670163+1335086-667544+333772-166886+83443+41721-20860+10430+5215+2607+1303+651-325+162+81+40-20+10+5+2-1(+ -1)
    0x0034e96b, // [35] -0.005285043183262 (ffc8e000)  -3612672=+2670163+1335086-667544+333772-166886+83443+41721-20860+10430-5215-2607+1303-651+325+162+81-40-20+10-5+2+1(+ -1)
    0x00054c6b, // [36] -0.005261074733452 (ffc92000)  -3596288=+2670163+1335086-667544+333772-166886+83443+41721-20860-10430-5215+2607+1303-651-325+162-81+40-20+10-5-2-1(+ -1)
    0x003873ab, // [37] -0.005237106283641 (ffc96000)  -3579904=+2670163+1335086-667544+333772-166886+83443-41721+20860+10430+5215-2607-1303+651+325+162-81-40-20-10+5+2+1(+ -1)
    0x000996ab, // [38] -0.005213137833830 (ffc9a000)  -3563520=+2670163+1335086-667544+333772-166886+83443-41721+20860-10430+5215+2607-1303+651-325-162+81+40-20-10+5-2-1(+ -1)
    0x0020a0ab, // [39] -0.005189169384019 (ffc9e000)  -3547136=+2670163+1335086-667544+333772-166886+83443-41721+20860-10430-5215-2607-1303-651+325-162+81-40-20-10-5-2+1(+ -1)
    0x003e052b, // [40] -0.005165200934209 (ffca2000)  -3530752=+2670163+1335086-667544+333772-166886+83443-41721-20860+10430-5215+2607-1303-651-325-162-81-40+20+10+5+2+1(+ -1)
    0x003fdc2b, // [41] -0.005141232484398 (ffca6001)  -3514367=+2670163+1335086-667544+333772-166886+83443-41721-20860-10430-5215+2607+1303+651-325+162+81+40+20+10+5+2+1(+ -2)
    0x001eebcb, // [42] -0.005117264034587 (ffcaa000)  -3497984=+2670163+1335086-667544+333772-166886-83443+41721+20860+10430+5215-2607+1303-651+325+162+81-40+20+10+5+2-1(+ -1)
    0x00374ecb, // [43] -0.005093295584777 (ffcae001)  -3481599=+2670163+1335086-667544+333772-166886-83443+41721+20860-10430+5215+2607+1303-651-325+162-81+40+20+10-5+2+1(+ -2)
    0x002670cb, // [44] -0.005069327134966 (ffcb2000)  -3465216=+2670163+1335086-667544+333772-166886-83443+41721+20860-10430-5215-2607-1303+651+325+162-81-40+20+10-5-2+1(+ -1)
    0x003b954b, // [45] -0.005045358685155 (ffcb6000)  -3448832=+2670163+1335086-667544+333772-166886-83443+41721-20860+10430-5215+2607-1303+651-325-162+81+40+20-10+5+2+1(+ -1)
    0x000aa24b, // [46] -0.005021390235344 (ffcba000)  -3432448=+2670163+1335086-667544+333772-166886-83443+41721-20860-10430+5215-2607-1303-651+325-162+81-40+20-10+5-2-1(+ 1)
    0x0013078b, // [47] -0.004997421785534 (ffcbe000)  -3416064=+2670163+1335086-667544+333772-166886-83443-41721+20860+10430+5215+2607-1303-651-325-162-81+40+20-10-5+2-1(+ -1)
    0x00223e8b, // [48] -0.004973453335723 (ffcc2000)  -3399680=+2670163+1335086-667544+333772-166886-83443-41721+20860-10430+5215+2607+1303+651+325-162-81-40+20-10-5-2+1(+ -1)
    0x0003e88b, // [49] -0.004949484885912 (ffcc6000)  -3383296=+2670163+1335086-667544+333772-166886-83443-41721+20860-10430-5215-2607+1303-651+325+162+81+40+20-10-5-2-1(+ 1)
    0x000ccd0b, // [50] -0.004925516436102 (ffcca000)  -3366912=+2670163+1335086-667544+333772-166886-83443-41721-20860+10430-5215+2607+1303-651-325+162+81-40-20+10+5-2-1(+ -1)
    0x0035720b, // [51] -0.004901547986291 (ffcce000)  -3350528=+2670163+1335086-667544+333772-166886-83443-41721-20860-10430+5215-2607-1303+651+325+162-81+40-20+10-5+2+1(+ -1)
    0x002457f3, // [52] -0.004877579536480 (ffcd2001)  -3334143=+2670163+1335086-667544-333772+166886+83443+41721+20860+10430+5215+2607-1303+651-325+162-81-40-20+10-5-2+1(+ 0)
    0x0039a1f3, // [53] -0.004853611086669 (ffcd6000)  -3317760=+2670163+1335086-667544-333772+166886+83443+41721+20860+10430-5215-2607-1303-651+325-162+81+40-20-10+5+2+1(+ -1)
    0x000884f3, // [54] -0.004829642636859 (ffcda000)  -3301376=+2670163+1335086-667544-333772+166886+83443+41721+20860-10430-5215+2607-1303-651-325-162+81-40-20-10+5-2-1(+ 1)
    0x00093d73, // [55] -0.004805674187048 (ffcde000)  -3284992=+2670163+1335086-667544-333772+166886+83443+41721-20860+10430-5215+2607+1303+651+325-162-81+40-20-10+5-2-1(+ 1)
    0x00001a73, // [56] -0.004781705737237 (ffce2000)  -3268608=+2670163+1335086-667544-333772+166886+83443+41721-20860-10430+5215-2607+1303+651-325-162-81-40-20-10-5-2-1(+ 1)
    0x0001cfb3, // [57] -0.004757737287427 (ffce6000)  -3252224=+2670163+1335086-667544-333772+166886+83443-41721+20860+10430+5215+2607+1303-651-325+162+81+40-20-10-5-2-1(+ 1)
    0x001f71b3, // [58] -0.004733768837616 (ffcea000)  -3235840=+2670163+1335086-667544-333772+166886+83443-41721+20860+10430-5215-2607-1303+651+325+162-81+40+20+10+5+2-1(+ -1)
    0x003654b3, // [59] -0.004709800387805 (ffcee000)  -3219456=+2670163+1335086-667544-333772+166886+83443-41721+20860-10430-5215+2607-1303+651-325+162-81-40+20+10-5+2+1(+ -1)
    0x0007a333, // [60] -0.004685831937994 (ffcf2000)  -3203072=+2670163+1335086-667544-333772+166886+83443-41721-20860+10430+5215-2607-1303-651+325-162+81+40+20+10-5-2-1(+ -1)
    0x002a8633, // [61] -0.004661863488184 (ffcf6000)  -3186688=+2670163+1335086-667544-333772+166886+83443-41721-20860-10430+5215+2607-1303-651-325-162+81-40+20-10+5-2+1(+ -1)
    0x003b3fd3, // [62] -0.004637895038373 (ffcfa000)  -3170304=+2670163+1335086-667544-333772+166886-83443+41721+20860+10430+5215+2607+1303+651+325-162-81+40+20-10+5+2+1(+ -1)
    0x003219d3, // [63] -0.004613926588562 (ffcfe001)  -3153919=+2670163+1335086-667544-333772+166886-83443+41721+20860+10430-5215-2607+1303+651-325-162-81-40+20-10-5+2+1(+ 0)
    0x0013ccd3, // [64] -0.004589958138752 (ffd02000)  -3137536=+2670163+1335086-667544-333772+166886-83443+41721+20860-10430-5215+2607+1303-651-325+162+81+40+20-10-5+2-1(+ -1)
    0x0002f353, // [65] -0.004565989688941 (ffd06000)  -3121152=+2670163+1335086-667544-333772+166886-83443+41721-20860+10430+5215-2607-1303+651+325+162+81-40+20-10-5-2-1(+ 1)
    0x002d5653, // [66] -0.004542021239130 (ffd0a000)  -3104768=+2670163+1335086-667544-333772+166886-83443+41721-20860-10430+5215+2607-1303+651-325+162-81+40-20+10+5-2+1(+ -1)
    0x00146053, // [67] -0.004518052789319 (ffd0e000)  -3088384=+2670163+1335086-667544-333772+166886-83443+41721-20860-10430-5215-2607-1303-651+325+162-81-40-20+10-5+2-1(+ -1)
    0x00058593, // [68] -0.004494084339509 (ffd12000)  -3072000=+2670163+1335086-667544-333772+166886-83443-41721+20860+10430-5215+2607-1303-651-325-162+81+40-20+10-5-2-1(+ -1)
    0x0004bc93, // [69] -0.004470115889698 (ffd16000)  -3055616=+2670163+1335086-667544-333772+166886-83443-41721+20860-10430-5215+2607+1303+651+325-162+81-40-20+10-5-2-1(+ 1)
    0x00091b13, // [70] -0.004446147439887 (ffd1a000)  -3039232=+2670163+1335086-667544-333772+166886-83443-41721-20860+10430+5215-2607+1303+651-325-162-81+40-20-10+5-2-1(+ -1)
    0x00102e13, // [71] -0.004422178990077 (ffd1e000)  -3022848=+2670163+1335086-667544-333772+166886-83443-41721-20860-10430+5215+2607+1303-651+325-162-81-40-20-10-5+2-1(+ -1)
    0x0021f013, // [72] -0.004398210540266 (ffd22000)  -3006464=+2670163+1335086-667544-333772+166886-83443-41721-20860-10430-5215-2607-1303+651+325+162+81+40-20-10-5-2+1(+ -1)
    0x0000d5e3, // [73] -0.004374242090455 (ffd26000)  -2990080=+2670163+1335086-667544-333772-166886+83443+41721+20860+10430-5215+2607-1303+651-325+162+81-40-20-10-5-2-1(+ 1)
    0x002e62e3, // [74] -0.004350273640644 (ffd2a001)  -2973695=+2670163+1335086-667544-333772-166886+83443+41721+20860-10430+5215-2607-1303-651+325+162-81-40+20+10+5-2+1(+ 0)
    0x00378763, // [75] -0.004326305190834 (ffd2e000)  -2957312=+2670163+1335086-667544-333772-166886+83443+41721-20860+10430+5215+2607-1303-651-325-162+81+40+20+10-5+2+1(+ -1)
    0x0016be63, // [76] -0.004302336741023 (ffd32000)  -2940928=+2670163+1335086-667544-333772-166886+83443+41721-20860-10430+5215+2607+1303+651+325-162+81-40+20+10-5+2-1(+ -1)
    0x003b1863, // [77] -0.004278368291212 (ffd36000)  -2924544=+2670163+1335086-667544-333772-166886+83443+41721-20860-10430-5215-2607+1303+651-325-162-81+40+20-10+5+2+1(+ -1)
    0x002a2da3, // [78] -0.004254399841402 (ffd3a000)  -2908160=+2670163+1335086-667544-333772-166886+83443-41721+20860+10430-5215+2607+1303-651+325-162-81-40+20-10+5-2+1(+ -1)
    0x000bf2a3, // [79] -0.004230431391591 (ffd3e000)  -2891776=+2670163+1335086-667544-333772-166886+83443-41721+20860-10430+5215-2607-1303+651+325+162+81+40+20-10+5-2-1(+ -1)
    0x0022d723, // [80] -0.004206462941780 (ffd42000)  -2875392=+2670163+1335086-667544-333772-166886+83443-41721-20860+10430+5215+2607-1303+651-325+162+81-40+20-10-5-2+1(+ -1)
    0x003d6123, // [81] -0.004182494491969 (ffd46000)  -2859008=+2670163+1335086-667544-333772-166886+83443-41721-20860+10430-5215-2607-1303-651+325+162-81+40-20+10+5+2+1(+ -1)
    0x000c4423, // [82] -0.004158526042159 (ffd4a000)  -2842624=+2670163+1335086-667544-333772-166886+83443-41721-20860-10430-5215+2607-1303-651-325+162-81-40-20+10+5-2-1(+ 1)
    0x002dbdc3, // [83] -0.004134557592348 (ffd4e001)  -2826239=+2670163+1335086-667544-333772-166886-83443+41721+20860+10430-5215+2607+1303+651+325-162+81+40-20+10+5-2+1(+ 0)
    0x00249ac3, // [84] -0.004110589142537 (ffd52000)  -2809856=+2670163+1335086-667544-333772-166886-83443+41721+20860-10430+5215-2607+1303+651-325-162+81-40-20+10-5-2+1(+ -1)
    0x00392f43, // [85] -0.004086620692727 (ffd56001)  -2793471=+2670163+1335086-667544-333772-166886-83443+41721-20860+10430+5215+2607+1303-651+325-162-81+40-20-10+5+2+1(+ -2)
    0x00080943, // [86] -0.004062652242916 (ffd5a000)  -2777088=+2670163+1335086-667544-333772-166886-83443+41721-20860+10430-5215-2607+1303-651-325-162-81-40-20-10+5-2-1(+ 1)
    0x0009d443, // [87] -0.004038683793105 (ffd5e000)  -2760704=+2670163+1335086-667544-333772-166886-83443+41721-20860-10430-5215+2607-1303+651-325+162+81+40-20-10+5-2-1(+ 1)
    0x0020e383, // [88] -0.004014715343294 (ffd62000)  -2744320=+2670163+1335086-667544-333772-166886-83443-41721+20860+10430+5215-2607-1303-651+325+162+81-40-20-10-5-2+1(+ -1)
    0x003e4683, // [89] -0.003990746893484 (ffd66000)  -2727936=+2670163+1335086-667544-333772-166886-83443-41721+20860-10430+5215+2607-1303-651-325+162-81-40+20+10+5+2+1(+ -1)
    0x003fbf03, // [90] -0.003966778443673 (ffd6a000)  -2711552=+2670163+1335086-667544-333772-166886-83443-41721-20860+10430+5215+2607+1303+651+325-162+81+40+20+10+5+2+1(+ -1)
    0x00369903, // [91] -0.003942809993862 (ffd6e000)  -2695168=+2670163+1335086-667544-333772-166886-83443-41721-20860+10430-5215-2607+1303+651-325-162+81-40+20+10-5+2+1(+ -1)
    0x00272c03, // [92] -0.003918841544052 (ffd72000)  -2678784=+2670163+1335086-667544-333772-166886-83443-41721-20860-10430-5215+2607+1303-651+325-162-81+40+20+10-5-2+1(+ -1)
    0x002a0bfd, // [93] -0.003894873094241 (ffd76000)  -2662400=+2670163-1335086+667544+333772+166886+83443+41721+20860+10430+5215-2607+1303-651-325-162-81-40+20-10+5-2+1(+ -1)
    0x002bd6fd, // [94] -0.003870904644430 (ffd7a000)  -2646016=+2670163-1335086+667544+333772+166886+83443+41721+20860-10430+5215+2607-1303+651-325+162+81+40+20-10+5-2+1(+ -1)
    0x0012e0fd, // [95] -0.003846936194619 (ffd7e000)  -2629632=+2670163-1335086+667544+333772+166886+83443+41721+20860-10430-5215-2607-1303-651+325+162+81-40+20-10-5+2-1(+ -1)
    0x0003457d, // [96] -0.003822967744809 (ffd82000)  -2613248=+2670163-1335086+667544+333772+166886+83443+41721-20860+10430-5215+2607-1303-651-325+162-81+40+20-10-5-2-1(+ 1)
    0x003c7c7d, // [97] -0.003798999294998 (ffd86000)  -2596864=+2670163-1335086+667544+333772+166886+83443+41721-20860-10430-5215+2607+1303+651+325+162-81-40-20+10+5+2+1(+ -1)
    0x000d9bbd, // [98] -0.003775030845187 (ffd8a000)  -2580480=+2670163-1335086+667544+333772+166886+83443-41721+20860+10430+5215-2607+1303+651-325-162+81+40-20+10+5-2-1(+ -1)
    0x0014aebd, // [99] -0.003751062395377 (ffd8e000)  -2564096=+2670163-1335086+667544+333772+166886+83443-41721+20860-10430+5215+2607+1303-651+325-162+81-40-20+10-5+2-1(+ -1)
    0x003908bd, // [100] -0.003727093945566 (ffd92000)  -2547712=+2670163-1335086+667544+333772+166886+83443-41721+20860-10430-5215-2607+1303-651-325-162-81+40-20-10+5+2+1(+ -1)
    0x0028353d, // [101] -0.003703125495755 (ffd96000)  -2531328=+2670163-1335086+667544+333772+166886+83443-41721-20860+10430-5215+2607-1303+651+325-162-81-40-20-10+5-2+1(+ -1)
    0x0009e23d, // [102] -0.003679157045944 (ffd9a000)  -2514944=+2670163-1335086+667544+333772+166886+83443-41721-20860-10430+5215-2607-1303-651+325+162+81+40-20-10+5-2-1(+ 1)
    0x0010c7dd, // [103] -0.003655188596134 (ffd9e000)  -2498560=+2670163-1335086+667544+333772+166886-83443+41721+20860+10430+5215+2607-1303-651-325+162+81-40-20-10-5+2-1(+ -1)
    0x00117edd, // [104] -0.003631220146323 (ffda2000)  -2482176=+2670163-1335086+667544+333772+166886-83443+41721+20860-10430+5215+2607+1303+651+325+162-81+40-20-10-5+2-1(+ -1)
    0x003f98dd, // [105] -0.003607251696512 (ffda6000)  -2465792=+2670163-1335086+667544+333772+166886-83443+41721+20860-10430-5215-2607+1303+651-325-162+81+40+20+10+5+2+1(+ -1)
    0x000ead5d, // [106] -0.003583283246702 (ffdaa000)  -2449408=+2670163-1335086+667544+333772+166886-83443+41721-20860+10430-5215+2607+1303-651+325-162+81-40+20+10+5-2-1(+ -1)
    0x00270a5d, // [107] -0.003559314796891 (ffdae000)  -2433024=+2670163-1335086+667544+333772+166886-83443+41721-20860-10430+5215-2607+1303-651-325-162-81+40+20+10-5-2+1(+ -1)
    0x0006379d, // [108] -0.003535346347080 (ffdb2000)  -2416640=+2670163-1335086+667544+333772+166886-83443-41721+20860+10430+5215+2607-1303+651+325-162-81-40+20+10-5-2-1(+ -1)
    0x003be19d, // [109] -0.003511377897269 (ffdb6000)  -2400256=+2670163-1335086+667544+333772+166886-83443-41721+20860+10430-5215-2607-1303-651+325+162+81+40+20-10+5+2+1(+ -1)
    0x000ac49d, // [110] -0.003487409447459 (ffdba000)  -2383872=+2670163-1335086+667544+333772+166886-83443-41721+20860-10430-5215+2607-1303-651-325+162+81-40+20-10+5-2-1(+ 1)
    0x000b7d1d, // [111] -0.003463440997648 (ffdbe000)  -2367488=+2670163-1335086+667544+333772+166886-83443-41721-20860+10430-5215+2607+1303+651+325+162-81+40+20-10+5-2-1(+ 1)
    0x00025a1d, // [112] -0.003439472547837 (ffdc2000)  -2351104=+2670163-1335086+667544+333772+166886-83443-41721-20860-10430+5215-2607+1303+651-325+162-81-40+20-10-5-2-1(+ 1)
    0x0003afed, // [113] -0.003415504098027 (ffdc6000)  -2334720=+2670163-1335086+667544+333772-166886+83443+41721+20860+10430+5215+2607+1303-651+325-162+81+40+20-10-5-2-1(+ 1)
    0x000c89ed, // [114] -0.003391535648216 (ffdca000)  -2318336=+2670163-1335086+667544+333772-166886+83443+41721+20860+10430-5215-2607+1303-651-325-162+81-40-20+10+5-2-1(+ 1)
    0x003534ed, // [115] -0.003367567198405 (ffdce000)  -2301952=+2670163-1335086+667544+333772-166886+83443+41721+20860-10430-5215+2607-1303+651+325-162-81+40-20+10-5+2+1(+ -1)
    0x0038136d, // [116] -0.003343598748594 (ffdd2000)  -2285568=+2670163-1335086+667544+333772-166886+83443+41721-20860+10430+5215-2607-1303+651-325-162-81-40-20-10+5+2+1(+ -1)
    0x0019c66d, // [117] -0.003319630298784 (ffdd6000)  -2269184=+2670163-1335086+667544+333772-166886+83443+41721-20860-10430+5215+2607-1303-651-325+162+81+40-20-10+5+2-1(+ -1)
    0x0018ffad, // [118] -0.003295661848973 (ffdda000)  -2252800=+2670163-1335086+667544+333772-166886+83443-41721+20860+10430+5215+2607+1303+651+325+162+81-40-20-10+5+2-1(+ -1)
    0x003159ad, // [119] -0.003271693399162 (ffdde000)  -2236416=+2670163-1335086+667544+333772-166886+83443-41721+20860+10430-5215-2607+1303+651-325+162-81+40-20-10-5+2+1(+ -1)
    0x00006cad, // [120] -0.003247724949352 (ffde2000)  -2220032=+2670163-1335086+667544+333772-166886+83443-41721+20860-10430-5215+2607+1303-651+325+162-81-40-20-10-5-2-1(+ -1)
    0x002e8b2d, // [121] -0.003223756499541 (ffde6000)  -2203648=+2670163-1335086+667544+333772-166886+83443-41721-20860+10430+5215-2607+1303-651-325-162+81-40+20+10+5-2+1(+ -1)
    0x000f362d, // [122] -0.003199788049730 (ffdea000)  -2187264=+2670163-1335086+667544+333772-166886+83443-41721-20860-10430+5215+2607-1303+651+325-162-81+40+20+10+5-2-1(+ -1)
    0x0006102d, // [123] -0.003175819599919 (ffdee000)  -2170880=+2670163-1335086+667544+333772-166886+83443-41721-20860-10430-5215-2607-1303+651-325-162-81-40+20+10-5-2-1(+ -1)
    0x0027c5cd, // [124] -0.003151851150109 (ffdf2000)  -2154496=+2670163-1335086+667544+333772-166886-83443+41721+20860+10430-5215+2607-1303-651-325+162+81+40+20+10-5-2+1(+ -1)
    0x0006fccd, // [125] -0.003127882700298 (ffdf6000)  -2138112=+2670163-1335086+667544+333772-166886-83443+41721+20860-10430-5215+2607+1303+651+325+162+81-40+20+10-5-2-1(+ -1)
    0x002b5b4d, // [126] -0.003103914250487 (ffdfa000)  -2121728=+2670163-1335086+667544+333772-166886-83443+41721-20860+10430+5215-2607+1303+651-325+162-81+40+20-10+5-2+1(+ -1)
    0x00326e4d, // [127] -0.003079945800677 (ffdfe000)  -2105344=+2670163-1335086+667544+333772-166886-83443+41721-20860-10430+5215+2607+1303-651+325+162-81-40+20-10-5+2+1(+ -1)
    0x0003884d, // [128] -0.003055977350866 (ffe02000)  -2088960=+2670163-1335086+667544+333772-166886-83443+41721-20860-10430-5215-2607+1303-651-325-162+81+40+20-10-5-2-1(+ 1)
    0x003cb58d, // [129] -0.003032008901055 (ffe06000)  -2072576=+2670163-1335086+667544+333772-166886-83443-41721+20860+10430-5215+2607-1303+651+325-162+81-40-20+10+5+2+1(+ -1)
    0x000d128d, // [130] -0.003008040451244 (ffe0a000)  -2056192=+2670163-1335086+667544+333772-166886-83443-41721+20860-10430+5215-2607-1303+651-325-162-81+40-20+10+5-2-1(+ 1)
    0x0024270d, // [131] -0.002984072001434 (ffe0e000)  -2039808=+2670163-1335086+667544+333772-166886-83443-41721-20860+10430+5215+2607-1303-651+325-162-81-40-20+10-5-2+1(+ -1)
    0x0015fe0d, // [132] -0.002960103551623 (ffe12000)  -2023424=+2670163-1335086+667544+333772-166886-83443-41721-20860-10430+5215+2607+1303+651+325+162+81+40-20+10-5+2-1(+ -1)
    0x0018d80d, // [133] -0.002936135101812 (ffe16000)  -2007040=+2670163-1335086+667544+333772-166886-83443-41721-20860-10430-5215-2607+1303+651-325+162+81-40-20-10+5+2-1(+ -1)
    0x00196df5, // [134] -0.002912166652002 (ffe1a000)  -1990656=+2670163-1335086+667544-333772+166886+83443+41721+20860+10430-5215+2607+1303-651+325+162-81+40-20-10+5+2-1(+ -1)
    0x00104af5, // [135] -0.002888198202191 (ffe1e000)  -1974272=+2670163-1335086+667544-333772+166886+83443+41721+20860-10430+5215-2607+1303-651-325+162-81-40-20-10-5+2-1(+ -1)
    0x0021b775, // [136] -0.002864229752380 (ffe22001)  -1957887=+2670163-1335086+667544-333772+166886+83443+41721-20860+10430+5215+2607-1303+651+325-162+81+40-20-10-5-2+1(+ 0)
    0x001f1175, // [137] -0.002840261302570 (ffe26000)  -1941504=+2670163-1335086+667544-333772+166886+83443+41721-20860+10430-5215-2607-1303+651-325-162-81+40+20+10+5+2-1(+ -1)
    0x000e2475, // [138] -0.002816292852759 (ffe2a000)  -1925120=+2670163-1335086+667544-333772+166886+83443+41721-20860-10430-5215+2607-1303-651+325-162-81-40+20+10+5-2-1(+ 1)
    0x002ffdb5, // [139] -0.002792324402948 (ffe2e000)  -1908736=+2670163-1335086+667544-333772+166886+83443-41721+20860+10430-5215+2607+1303+651+325+162+81+40+20+10+5-2+1(+ -1)
    0x0026dab5, // [140] -0.002768355953137 (ffe32000)  -1892352=+2670163-1335086+667544-333772+166886+83443-41721+20860-10430+5215-2607+1303+651-325+162+81-40+20+10-5-2+1(+ -1)
    0x003b6f35, // [141] -0.002744387503327 (ffe36001)  -1875967=+2670163-1335086+667544-333772+166886+83443-41721-20860+10430+5215+2607+1303-651+325+162-81+40+20-10+5+2+1(+ -2)
    0x000a4935, // [142] -0.002720419053516 (ffe3a000)  -1859584=+2670163-1335086+667544-333772+166886+83443-41721-20860+10430-5215-2607+1303-651-325+162-81-40+20-10+5-2-1(+ 1)
    0x0033b435, // [143] -0.002696450603705 (ffe3e000)  -1843200=+2670163-1335086+667544-333772+166886+83443-41721-20860-10430-5215+2607-1303+651+325-162+81+40+20-10-5+2+1(+ -1)
    0x000293d5, // [144] -0.002672482153895 (ffe42000)  -1826816=+2670163-1335086+667544-333772+166886-83443+41721+20860+10430+5215-2607-1303+651-325-162+81-40+20-10-5-2-1(+ -1)
    0x003d26d5, // [145] -0.002648513704084 (ffe46000)  -1810432=+2670163-1335086+667544-333772+166886-83443+41721+20860-10430+5215+2607-1303-651+325-162-81+40-20+10+5+2+1(+ -1)
    0x003400d5, // [146] -0.002624545254273 (ffe4a000)  -1794048=+2670163-1335086+667544-333772+166886-83443+41721+20860-10430-5215-2607-1303-651-325-162-81-40-20+10-5+2+1(+ -1)
    0x000dd955, // [147] -0.002600576804462 (ffe4e000)  -1777664=+2670163-1335086+667544-333772+166886-83443+41721-20860+10430-5215-2607+1303+651-325+162+81+40-20+10+5-2-1(+ 1)
    0x0024ec55, // [148] -0.002576608354652 (ffe52000)  -1761280=+2670163-1335086+667544-333772+166886-83443+41721-20860-10430-5215+2607+1303-651+325+162+81-40-20+10-5-2+1(+ -1)
    0x00394b95, // [149] -0.002552639904841 (ffe56000)  -1744896=+2670163-1335086+667544-333772+166886-83443-41721+20860+10430+5215-2607+1303-651-325+162-81+40-20-10+5+2+1(+ -1)
    0x00287695, // [150] -0.002528671455030 (ffe5a000)  -1728512=+2670163-1335086+667544-333772+166886-83443-41721+20860-10430+5215+2607-1303+651+325+162-81-40-20-10+5-2+1(+ -1)
    0x00119095, // [151] -0.002504703005220 (ffe5e000)  -1712128=+2670163-1335086+667544-333772+166886-83443-41721+20860-10430-5215-2607-1303+651-325-162+81+40-20-10-5+2-1(+ -1)
    0x0000a515, // [152] -0.002480734555409 (ffe62001)  -1695743=+2670163-1335086+667544-333772+166886-83443-41721-20860+10430-5215+2607-1303-651+325-162+81-40-20-10-5-2-1(+ 2)
    0x000e0215, // [153] -0.002456766105598 (ffe66000)  -1679360=+2670163-1335086+667544-333772+166886-83443-41721-20860-10430+5215-2607-1303-651-325-162-81-40+20+10+5-2-1(+ -1)
    0x003fdbe5, // [154] -0.002432797655787 (ffe6a000)  -1662976=+2670163-1335086+667544-333772-166886+83443+41721+20860+10430+5215-2607+1303+651-325+162+81+40+20+10+5+2+1(+ -1)
    0x000eeee5, // [155] -0.002408829205977 (ffe6e000)  -1646592=+2670163-1335086+667544-333772-166886+83443+41721+20860-10430+5215+2607+1303-651+325+162+81-40+20+10+5-2-1(+ -1)
    0x002748e5, // [156] -0.002384860756166 (ffe72000)  -1630208=+2670163-1335086+667544-333772-166886+83443+41721+20860-10430-5215-2607+1303-651-325+162-81+40+20+10-5-2+1(+ -1)
    0x003a7565, // [157] -0.002360892306355 (ffe76001)  -1613823=+2670163-1335086+667544-333772-166886+83443+41721-20860+10430-5215+2607-1303+651+325+162-81-40+20-10+5+2+1(+ -2)
    0x000b9265, // [158] -0.002336923856545 (ffe7a000)  -1597440=+2670163-1335086+667544-333772-166886+83443+41721-20860-10430+5215-2607-1303+651-325-162+81+40+20-10+5-2-1(+ -1)
    0x0032a7a5, // [159] -0.002312955406734 (ffe7e000)  -1581056=+2670163-1335086+667544-333772-166886+83443-41721+20860+10430+5215+2607-1303-651+325-162+81-40+20-10-5+2+1(+ -1)
    0x000301a5, // [160] -0.002288986956923 (ffe82000)  -1564672=+2670163-1335086+667544-333772-166886+83443-41721+20860+10430-5215-2607-1303-651-325-162-81+40+20-10-5-2-1(+ 1)
    0x003c38a5, // [161] -0.002265018507112 (ffe86000)  -1548288=+2670163-1335086+667544-333772-166886+83443-41721+20860-10430-5215-2607+1303+651+325-162-81-40-20+10+5+2+1(+ -1)
    0x001ded25, // [162] -0.002241050057302 (ffe8a000)  -1531904=+2670163-1335086+667544-333772-166886+83443-41721-20860+10430-5215+2607+1303-651+325+162+81+40-20+10+5+2-1(+ -1)
    0x0014ca25, // [163] -0.002217081607491 (ffe8e000)  -1515520=+2670163-1335086+667544-333772-166886+83443-41721-20860-10430+5215-2607+1303-651-325+162+81-40-20+10-5+2-1(+ -1)
    0x003577c5, // [164] -0.002193113157680 (ffe92000)  -1499136=+2670163-1335086+667544-333772-166886-83443+41721+20860+10430+5215+2607-1303+651+325+162-81+40-20+10-5+2+1(+ -1)
    0x003851c5, // [165] -0.002169144707870 (ffe96000)  -1482752=+2670163-1335086+667544-333772-166886-83443+41721+20860+10430-5215-2607-1303+651-325+162-81-40-20-10+5+2+1(+ -1)
    0x0029a4c5, // [166] -0.002145176258059 (ffe9a000)  -1466368=+2670163-1335086+667544-333772-166886-83443+41721+20860-10430-5215+2607-1303-651+325-162+81+40-20-10+5-2+1(+ -1)
    0x00208345, // [167] -0.002121207808248 (ffe9e000)  -1449984=+2670163-1335086+667544-333772-166886-83443+41721-20860+10430+5215-2607-1303-651-325-162+81-40-20-10-5-2+1(+ -1)
    0x00213a45, // [168] -0.002097239358437 (ffea2000)  -1433600=+2670163-1335086+667544-333772-166886-83443+41721-20860-10430+5215-2607+1303+651+325-162-81+40-20-10-5-2+1(+ -1)
    0x00001f85, // [169] -0.002073270908627 (ffea6000)  -1417216=+2670163-1335086+667544-333772-166886-83443-41721+20860+10430+5215+2607+1303+651-325-162-81-40-20-10-5-2-1(+ 3)
    0x002ec985, // [170] -0.002049302458816 (ffeaa000)  -1400832=+2670163-1335086+667544-333772-166886-83443-41721+20860+10430-5215-2607+1303-651-325+162+81-40+20+10+5-2+1(+ -1)
    0x000f7485, // [171] -0.002025334009005 (ffeae001)  -1384447=+2670163-1335086+667544-333772-166886-83443-41721+20860-10430-5215+2607-1303+651+325+162-81+40+20+10+5-2-1(+ 0)
    0x00065305, // [172] -0.002001365559195 (ffeb2001)  -1368063=+2670163-1335086+667544-333772-166886-83443-41721-20860+10430+5215-2607-1303+651-325+162-81-40+20+10-5-2-1(+ 0)
    0x003ba605, // [173] -0.001977397109384 (ffeb6001)  -1351679=+2670163-1335086+667544-333772-166886-83443-41721-20860-10430+5215+2607-1303-651+325-162+81+40+20-10+5+2+1(+ 0)
    0x00328005, // [174] -0.001953428659573 (ffeba000)  -1335296=+2670163-1335086+667544-333772-166886-83443-41721-20860-10430-5215-2607-1303-651-325-162+81-40+20-10-5+2+1(+ -1)
    0x000b39f9, // [175] -0.001929460209762 (ffebe000)  -1318912=+2670163-1335086-667544+333772+166886+83443+41721+20860+10430-5215-2607+1303+651+325-162-81+40+20-10+5-2-1(+ -1)
    0x00221cf9, // [176] -0.001905491759952 (ffec2000)  -1302528=+2670163-1335086-667544+333772+166886+83443+41721+20860-10430-5215+2607+1303+651-325-162-81-40+20-10-5-2+1(+ -1)
    0x0003cb79, // [177] -0.001881523310141 (ffec6000)  -1286144=+2670163-1335086-667544+333772+166886+83443+41721-20860+10430+5215-2607+1303-651-325+162+81+40+20-10-5-2-1(+ 1)
    0x001cf679, // [178] -0.001857554860330 (ffeca000)  -1269760=+2670163-1335086-667544+333772+166886+83443+41721-20860-10430+5215+2607-1303+651+325+162+81-40-20+10+5+2-1(+ -1)
    0x00355079, // [179] -0.001833586410520 (ffece000)  -1253376=+2670163-1335086-667544+333772+166886+83443+41721-20860-10430-5215-2607-1303+651-325+162-81+40-20+10-5+2+1(+ -1)
    0x002465b9, // [180] -0.001809617960709 (ffed2000)  -1236992=+2670163-1335086-667544+333772+166886+83443-41721+20860+10430-5215+2607-1303-651+325+162-81-40-20+10-5-2+1(+ -1)
    0x001982b9, // [181] -0.001785649510898 (ffed6001)  -1220607=+2670163-1335086-667544+333772+166886+83443-41721+20860-10430+5215-2607-1303-651-325-162+81+40-20-10+5+2-1(+ 0)
    0x0028bb39, // [182] -0.001761681061087 (ffeda001)  -1204223=+2670163-1335086-667544+333772+166886+83443-41721-20860+10430+5215-2607+1303+651+325-162+81-40-20-10+5-2+1(+ 0)
    0x00311e39, // [183] -0.001737712611277 (ffede001)  -1187839=+2670163-1335086-667544+333772+166886+83443-41721-20860-10430+5215+2607+1303+651-325-162-81+40-20-10-5+2+1(+ 0)
    0x00002839, // [184] -0.001713744161466 (ffee2001)  -1171455=+2670163-1335086-667544+333772+166886+83443-41721-20860-10430-5215-2607+1303-651+325-162-81-40-20-10-5-2-1(+ 2)
    0x0021f5d9, // [185] -0.001689775711655 (ffee6000)  -1155072=+2670163-1335086-667544+333772+166886-83443+41721+20860+10430-5215+2607-1303+651+325+162+81+40-20-10-5-2+1(+ -1)
    0x001f52d9, // [186] -0.001665807261845 (ffeea000)  -1138688=+2670163-1335086-667544+333772+166886-83443+41721+20860-10430+5215-2607-1303+651-325+162-81+40+20+10+5+2-1(+ -1)
    0x000e6759, // [187] -0.001641838812034 (ffeee000)  -1122304=+2670163-1335086-667544+333772+166886-83443+41721-20860+10430+5215+2607-1303-651+325+162-81-40+20+10+5-2-1(+ 1)
    0x00078159, // [188] -0.001617870362223 (ffef2000)  -1105920=+2670163-1335086-667544+333772+166886-83443+41721-20860+10430-5215-2607-1303-651-325-162+81+40+20+10-5-2-1(+ -1)
    0x0006b859, // [189] -0.001593901912412 (ffef6000)  -1089536=+2670163-1335086-667544+333772+166886-83443+41721-20860-10430-5215-2607+1303+651+325-162+81-40+20+10-5-2-1(+ 1)
    0x001b1d99, // [190] -0.001569933462602 (ffefa000)  -1073152=+2670163-1335086-667544+333772+166886-83443-41721+20860+10430-5215+2607+1303+651-325-162-81+40+20-10+5+2-1(+ -1)
    0x00322a99, // [191] -0.001545965012791 (ffefe000)  -1056768=+2670163-1335086-667544+333772+166886-83443-41721+20860-10430+5215-2607+1303-651+325-162-81-40+20-10-5+2+1(+ -1)
    0x0033f719, // [192] -0.001521996562980 (fff02001)  -1040383=+2670163-1335086-667544+333772+166886-83443-41721-20860+10430+5215+2607-1303+651+325+162+81+40+20-10-5+2+1(+ 0)
    0x003cd119, // [193] -0.001498028113170 (fff06001)  -1023999=+2670163-1335086-667544+333772+166886-83443-41721-20860+10430-5215-2607-1303+651-325+162+81-40-20+10+5+2+1(+ 0)
    0x002d6419, // [194] -0.001474059663359 (fff0a001)  -1007615=+2670163-1335086-667544+333772+166886-83443-41721-20860-10430-5215+2607-1303-651+325+162-81+40-20+10+5-2+1(+ 0)
    0x003443e9, // [195] -0.001450091213548 (fff0e001)  -991231=+2670163-1335086-667544+333772-166886+83443+41721+20860+10430+5215-2607-1303-651-325+162-81-40-20+10-5+2+1(+ 0)
    0x0035bae9, // [196] -0.001426122763737 (fff12000)  -974848=+2670163-1335086-667544+333772-166886+83443+41721+20860-10430+5215-2607+1303+651+325-162+81+40-20+10-5+2+1(+ -1)
    0x00049f69, // [197] -0.001402154313927 (fff16000)  -958464=+2670163-1335086-667544+333772-166886+83443+41721-20860+10430+5215+2607+1303+651-325-162+81-40-20+10-5-2-1(+ 1)
    0x00292969, // [198] -0.001378185864116 (fff1a000)  -942080=+2670163-1335086-667544+333772-166886+83443+41721-20860+10430-5215-2607+1303-651+325-162-81+40-20-10+5-2+1(+ -1)
    0x00100c69, // [199] -0.001354217414305 (fff1e000)  -925696=+2670163-1335086-667544+333772-166886+83443+41721-20860-10430-5215+2607+1303-651-325-162-81-40-20-10-5+2-1(+ -1)
    0x0011d3a9, // [200] -0.001330248964495 (fff22000)  -909312=+2670163-1335086-667544+333772-166886+83443-41721+20860+10430+5215-2607-1303+651-325+162+81+40-20-10-5+2-1(+ -1)
    0x0000e6a9, // [201] -0.001306280514684 (fff26000)  -892928=+2670163-1335086-667544+333772-166886+83443-41721+20860-10430+5215+2607-1303-651+325+162+81-40-20-10-5-2-1(+ 1)
    0x000e40a9, // [202] -0.001282312064873 (fff2a000)  -876544=+2670163-1335086-667544+333772-166886+83443-41721+20860-10430-5215-2607-1303-651-325+162-81-40+20+10+5-2-1(+ -1)
    0x000fb929, // [203] -0.001258343615062 (fff2e001)  -860159=+2670163-1335086-667544+333772-166886+83443-41721-20860+10430-5215-2607+1303+651+325-162+81+40+20+10+5-2-1(+ 0)
    0x00269c29, // [204] -0.001234375165252 (fff32001)  -843775=+2670163-1335086-667544+333772-166886+83443-41721-20860-10430-5215+2607+1303+651-325-162+81-40+20+10-5-2+1(+ 0)
    0x00072bc9, // [205] -0.001210406715441 (fff36001)  -827391=+2670163-1335086-667544+333772-166886-83443+41721+20860+10430+5215-2607+1303-651+325-162-81+40+20+10-5-2-1(+ 0)
    0x002a0ec9, // [206] -0.001186438265630 (fff3a001)  -811007=+2670163-1335086-667544+333772-166886-83443+41721+20860-10430+5215+2607+1303-651-325-162-81-40+20-10+5-2+1(+ 0)
    0x000bd0c9, // [207] -0.001162469815820 (fff3e000)  -794624=+2670163-1335086-667544+333772-166886-83443+41721+20860-10430-5215-2607-1303+651-325+162+81+40+20-10+5-2-1(+ -1)
    0x0012e549, // [208] -0.001138501366009 (fff42000)  -778240=+2670163-1335086-667544+333772-166886-83443+41721-20860+10430-5215+2607-1303-651+325+162+81-40+20-10-5+2-1(+ -1)
    0x003d4249, // [209] -0.001114532916198 (fff46000)  -761856=+2670163-1335086-667544+333772-166886-83443+41721-20860-10430+5215-2607-1303-651-325+162-81+40-20+10+5+2+1(+ -1)
    0x003c7b89, // [210] -0.001090564466387 (fff4a000)  -745472=+2670163-1335086-667544+333772-166886-83443-41721+20860+10430+5215-2607+1303+651+325+162-81-40-20+10+5+2+1(+ -1)
    0x000d9e89, // [211] -0.001066596016577 (fff4e000)  -729088=+2670163-1335086-667544+333772-166886-83443-41721+20860-10430+5215+2607+1303+651-325-162+81+40-20+10+5-2-1(+ -1)
    0x0024a889, // [212] -0.001042627566766 (fff52000)  -712704=+2670163-1335086-667544+333772-166886-83443-41721+20860-10430-5215-2607+1303-651+325-162+81-40-20+10-5-2+1(+ -1)
    0x00390d09, // [213] -0.001018659116955 (fff56001)  -696319=+2670163-1335086-667544+333772-166886-83443-41721-20860+10430-5215+2607+1303-651-325-162-81+40-20-10+5+2+1(+ 0)
    0x00083209, // [214] -0.000994690667145 (fff5a001)  -679935=+2670163-1335086-667544+333772-166886-83443-41721-20860-10430+5215-2607-1303+651+325-162-81-40-20-10+5-2-1(+ 0)
    0x0029e7f1, // [215] -0.000970722217334 (fff5e001)  -663551=+2670163-1335086-667544-333772+166886+83443+41721+20860+10430+5215+2607-1303-651+325+162+81+40-20-10+5-2+1(+ 0)
    0x0020c1f1, // [216] -0.000946753767523 (fff62001)  -647167=+2670163-1335086-667544-333772+166886+83443+41721+20860+10430-5215-2607-1303-651-325+162+81-40-20-10-5-2+1(+ 0)
    0x002178f1, // [217] -0.000922785317712 (fff66001)  -630783=+2670163-1335086-667544-333772+166886+83443+41721+20860-10430-5215-2607+1303+651+325+162-81+40-20-10-5-2+1(+ 0)
    0x003f9d71, // [218] -0.000898816867902 (fff6a000)  -614400=+2670163-1335086-667544-333772+166886+83443+41721-20860+10430-5215+2607+1303+651-325-162+81+40+20+10+5+2+1(+ -1)
    0x000eaa71, // [219] -0.000874848418091 (fff6e000)  -598016=+2670163-1335086-667544-333772+166886+83443+41721-20860-10430+5215-2607+1303-651+325-162+81-40+20+10+5-2-1(+ 1)
    0x00170fb1, // [220] -0.000850879968280 (fff72000)  -581632=+2670163-1335086-667544-333772+166886+83443-41721+20860+10430+5215+2607+1303-651-325-162-81+40+20+10-5+2-1(+ -1)
    0x000631b1, // [221] -0.000826911518470 (fff76000)  -565248=+2670163-1335086-667544-333772+166886+83443-41721+20860+10430-5215-2607-1303+651+325-162-81-40+20+10-5-2-1(+ 1)
    0x003be4b1, // [222] -0.000802943068659 (fff7a000)  -548864=+2670163-1335086-667544-333772+166886+83443-41721+20860-10430-5215+2607-1303-651+325+162+81+40+20-10+5+2+1(+ -1)
    0x0032c331, // [223] -0.000778974618848 (fff7e000)  -532480=+2670163-1335086-667544-333772+166886+83443-41721-20860+10430+5215-2607-1303-651-325+162+81-40+20-10-5+2+1(+ -1)
    0x00337a31, // [224] -0.000755006169037 (fff82001)  -516095=+2670163-1335086-667544-333772+166886+83443-41721-20860-10430+5215-2607+1303+651+325+162-81+40+20-10-5+2+1(+ 0)
    0x00225fd1, // [225] -0.000731037719227 (fff86001)  -499711=+2670163-1335086-667544-333772+166886-83443+41721+20860+10430+5215+2607+1303+651-325+162-81-40+20-10-5-2+1(+ 0)
    0x003da9d1, // [226] -0.000707069269416 (fff8a001)  -483327=+2670163-1335086-667544-333772+166886-83443+41721+20860+10430-5215-2607+1303-651+325-162+81+40-20+10+5+2+1(+ 0)
    0x00348cd1, // [227] -0.000683100819605 (fff8e001)  -466943=+2670163-1335086-667544-333772+166886-83443+41721+20860-10430-5215+2607+1303-651-325-162+81-40-20+10-5+2+1(+ -2)
    0x00153351, // [228] -0.000659132369795 (fff92001)  -450559=+2670163-1335086-667544-333772+166886-83443+41721-20860+10430+5215-2607-1303+651+325-162-81+40-20+10-5+2-1(+ 0)
    0x00381651, // [229] -0.000635163919984 (fff96000)  -434176=+2670163-1335086-667544-333772+166886-83443+41721-20860-10430+5215+2607-1303+651-325-162-81-40-20-10+5+2+1(+ -1)
    0x0029c051, // [230] -0.000611195470173 (fff9a000)  -417792=+2670163-1335086-667544-333772+166886-83443+41721-20860-10430-5215-2607-1303-651-325+162+81+40-20-10+5-2+1(+ -1)
    0x0028f991, // [231] -0.000587227020362 (fff9e000)  -401408=+2670163-1335086-667544-333772+166886-83443-41721+20860+10430-5215-2607+1303+651+325+162+81-40-20-10+5-2+1(+ -1)
    0x00315c91, // [232] -0.000563258570552 (fffa2000)  -385024=+2670163-1335086-667544-333772+166886-83443-41721+20860-10430-5215+2607+1303+651-325+162-81+40-20-10-5+2+1(+ -1)
    0x00006b11, // [233] -0.000539290120741 (fffa6000)  -368640=+2670163-1335086-667544-333772+166886-83443-41721-20860+10430+5215-2607+1303-651+325+162-81-40-20-10-5-2-1(+ 1)
    0x002e8e11, // [234] -0.000515321670930 (fffaa000)  -352256=+2670163-1335086-667544-333772+166886-83443-41721-20860-10430+5215+2607+1303-651-325-162+81-40+20+10+5-2+1(+ -1)
    0x00373011, // [235] -0.000491353221120 (fffae001)  -335871=+2670163-1335086-667544-333772+166886-83443-41721-20860-10430-5215-2607-1303+651+325-162-81+40+20+10-5+2+1(+ -2)
    0x001615e1, // [236] -0.000467384771309 (fffb2001)  -319487=+2670163-1335086-667544-333772-166886+83443+41721+20860+10430-5215+2607-1303+651-325-162-81-40+20+10-5+2-1(+ 0)
    0x0007c2e1, // [237] -0.000443416321498 (fffb6001)  -303103=+2670163-1335086-667544-333772-166886+83443+41721+20860-10430+5215-2607-1303-651-325+162+81+40+20+10-5-2-1(+ 0)
    0x003afb61, // [238] -0.000419447871687 (fffba001)  -286719=+2670163-1335086-667544-333772-166886+83443+41721-20860+10430+5215-2607+1303+651+325+162+81-40+20-10+5+2+1(+ -2)
    0x002b5e61, // [239] -0.000395479421877 (fffbe000)  -270336=+2670163-1335086-667544-333772-166886+83443+41721-20860-10430+5215+2607+1303+651-325+162-81+40+20-10+5-2+1(+ -1)
    0x00126861, // [240] -0.000371510972066 (fffc2000)  -253952=+2670163-1335086-667544-333772-166886+83443+41721-20860-10430-5215-2607+1303-651+325+162-81-40+20-10-5+2-1(+ -1)
    0x00038da1, // [241] -0.000347542522255 (fffc6000)  -237568=+2670163-1335086-667544-333772-166886+83443-41721+20860+10430-5215+2607+1303-651-325-162+81+40+20-10-5-2-1(+ -1)
    0x001cb2a1, // [242] -0.000323574072445 (fffca000)  -221184=+2670163-1335086-667544-333772-166886+83443-41721+20860-10430+5215-2607-1303+651+325-162+81-40-20+10+5+2-1(+ -1)
    0x000d1721, // [243] -0.000299605622634 (fffce000)  -204800=+2670163-1335086-667544-333772-166886+83443-41721-20860+10430+5215+2607-1303+651-325-162-81+40-20+10+5-2-1(+ 1)
    0x00042121, // [244] -0.000275637172823 (fffd2000)  -188416=+2670163-1335086-667544-333772-166886+83443-41721-20860+10430-5215-2607-1303-651+325-162-81-40-20+10-5-2-1(+ -1)
    0x0025f821, // [245] -0.000251668723012 (fffd6001)  -172031=+2670163-1335086-667544-333772-166886+83443-41721-20860-10430-5215-2607+1303+651+325+162+81+40-20+10-5-2+1(+ 0)
    0x0038ddc1, // [246] -0.000227700273202 (fffda001)  -155647=+2670163-1335086-667544-333772-166886-83443+41721+20860+10430-5215+2607+1303+651-325+162+81-40-20-10+5+2+1(+ -2)
    0x00296ac1, // [247] -0.000203731823391 (fffde001)  -139263=+2670163-1335086-667544-333772-166886-83443+41721+20860-10430+5215-2607+1303-651+325+162-81+40-20-10+5-2+1(+ 0)
    0x00104f41, // [248] -0.000179763373580 (fffe2001)  -122879=+2670163-1335086-667544-333772-166886-83443+41721-20860+10430+5215+2607+1303-651-325+162-81-40-20-10-5+2-1(+ 0)
    0x0001b141, // [249] -0.000155794923770 (fffe6001)  -106495=+2670163-1335086-667544-333772-166886-83443+41721-20860+10430-5215-2607-1303+651+325-162+81+40-20-10-5-2-1(+ 0)
    0x001f1441, // [250] -0.000131826473959 (fffea001)  -90111=+2670163-1335086-667544-333772-166886-83443+41721-20860-10430-5215+2607-1303+651-325-162-81+40+20+10+5+2-1(+ 0)
    0x000e2381, // [251] -0.000107858024148 (fffee000)  -73728=+2670163-1335086-667544-333772-166886-83443-41721+20860+10430+5215-2607-1303-651+325-162-81-40+20+10+5-2-1(+ 1)
    0x000ffa81, // [252] -0.000083889574337 (ffff2000)  -57344=+2670163-1335086-667544-333772-166886-83443-41721+20860-10430+5215-2607+1303+651+325+162+81+40+20+10+5-2-1(+ -1)
    0x0026df01, // [253] -0.000059921124527 (ffff6000)  -40960=+2670163-1335086-667544-333772-166886-83443-41721-20860+10430+5215+2607+1303+651-325+162+81-40+20+10-5-2+1(+ -1)
    0x003b6901, // [254] -0.000035952674716 (ffffa000)  -24576=+2670163-1335086-667544-333772-166886-83443-41721-20860+10430-5215-2607+1303-651+325+162-81+40+20-10+5+2+1(+ -1)
    0x000a4c01, // [255] -0.000011984224905 (ffffe000)  -8192=+2670163-1335086-667544-333772-166886-83443-41721-20860-10430-5215+2607+1303-651-325+162-81-40+20-10+5-2-1(+ 1)
    0x0035b3fe, // [256] 0.000011984224905 (00002000)  8192=-2670163+1335086+667544+333772+166886+83443+41721+20860+10430+5215-2607-1303+651+325-162+81+40-20+10-5+2+1(+ -1)
    0x000496fe, // [257] 0.000035952674716 (00006000)  24576=-2670163+1335086+667544+333772+166886+83443+41721+20860-10430+5215+2607-1303+651-325-162+81-40-20+10-5-2-1(+ 1)
    0x002920fe, // [258] 0.000059921124527 (0000a000)  40960=-2670163+1335086+667544+333772+166886+83443+41721+20860-10430-5215-2607-1303-651+325-162-81+40-20-10+5-2+1(+ -1)
    0x0010057e, // [259] 0.000083889574337 (0000e000)  57344=-2670163+1335086+667544+333772+166886+83443+41721-20860+10430-5215+2607-1303-651-325-162-81-40-20-10-5+2-1(+ -1)
    0x0031dc7e, // [260] 0.000107858024148 (00012000)  73728=-2670163+1335086+667544+333772+166886+83443+41721-20860-10430-5215+2607+1303+651-325+162+81+40-20-10-5+2+1(+ -1)
    0x0020ebbe, // [261] 0.000131826473959 (00015fff)  90111=-2670163+1335086+667544+333772+166886+83443-41721+20860+10430+5215-2607+1303-651+325+162+81-40-20-10-5-2+1(+ 0)
    0x003e4ebe, // [262] 0.000155794923770 (00019fff)  106495=-2670163+1335086+667544+333772+166886+83443-41721+20860-10430+5215+2607+1303-651-325+162-81-40+20+10+5+2+1(+ 0)
    0x002fb0be, // [263] 0.000179763373580 (0001dfff)  122879=-2670163+1335086+667544+333772+166886+83443-41721+20860-10430-5215-2607-1303+651+325-162+81+40+20+10+5-2+1(+ 0)
    0x0016953e, // [264] 0.000203731823391 (00021fff)  139263=-2670163+1335086+667544+333772+166886+83443-41721-20860+10430-5215+2607-1303+651-325-162+81-40+20+10-5+2-1(+ 0)
    0x003b223e, // [265] 0.000227700273202 (00025fff)  155647=-2670163+1335086+667544+333772+166886+83443-41721-20860-10430+5215-2607-1303-651+325-162-81+40+20-10+5+2+1(+ -2)
    0x001a07de, // [266] 0.000251668723012 (00029fff)  172031=-2670163+1335086+667544+333772+166886-83443+41721+20860+10430+5215+2607-1303-651-325-162-81-40+20-10+5+2-1(+ 0)
    0x001bdede, // [267] 0.000275637172823 (0002e000)  188416=-2670163+1335086+667544+333772+166886-83443+41721+20860-10430+5215+2607+1303+651-325+162+81+40+20-10+5+2-1(+ -1)
    0x0032e8de, // [268] 0.000299605622634 (00032000)  204800=-2670163+1335086+667544+333772+166886-83443+41721+20860-10430-5215-2607+1303-651+325+162+81-40+20-10-5+2+1(+ -1)
    0x00034d5e, // [269] 0.000323574072445 (00036000)  221184=-2670163+1335086+667544+333772+166886-83443+41721-20860+10430-5215+2607+1303-651-325+162-81+40+20-10-5-2-1(+ -1)
    0x001c725e, // [270] 0.000347542522255 (0003a000)  237568=-2670163+1335086+667544+333772+166886-83443+41721-20860-10430+5215-2607-1303+651+325+162-81-40-20+10+5+2-1(+ -1)
    0x000d979e, // [271] 0.000371510972066 (0003e000)  253952=-2670163+1335086+667544+333772+166886-83443-41721+20860+10430+5215+2607-1303+651-325-162+81+40-20+10+5-2-1(+ -1)
    0x0024a19e, // [272] 0.000395479421877 (00042000)  270336=-2670163+1335086+667544+333772+166886-83443-41721+20860+10430-5215-2607-1303-651+325-162+81-40-20+10-5-2+1(+ -1)
    0x0039049e, // [273] 0.000419447871687 (00045fff)  286719=-2670163+1335086+667544+333772+166886-83443-41721+20860-10430-5215+2607-1303-651-325-162-81+40-20-10+5+2+1(+ -2)
    0x00383d1e, // [274] 0.000443416321498 (00049fff)  303103=-2670163+1335086+667544+333772+166886-83443-41721-20860+10430-5215+2607+1303+651+325-162-81-40-20-10+5+2+1(+ 0)
    0x0029ea1e, // [275] 0.000467384771309 (0004dfff)  319487=-2670163+1335086+667544+333772+166886-83443-41721-20860-10430+5215-2607+1303-651+325+162+81+40-20-10+5-2+1(+ 0)
    0x0030cfee, // [276] 0.000491353221120 (00051fff)  335871=-2670163+1335086+667544+333772-166886+83443+41721+20860+10430+5215+2607+1303-651-325+162+81-40-20-10-5+2+1(+ -2)
    0x002171ee, // [277] 0.000515321670930 (00056000)  352256=-2670163+1335086+667544+333772-166886+83443+41721+20860+10430-5215-2607-1303+651+325+162-81+40-20-10-5-2+1(+ -1)
    0x003f94ee, // [278] 0.000539290120741 (0005a000)  368640=-2670163+1335086+667544+333772-166886+83443+41721+20860-10430-5215+2607-1303+651-325-162+81+40+20+10+5+2+1(+ -1)
    0x000ea36e, // [279] 0.000563258570552 (0005e000)  385024=-2670163+1335086+667544+333772-166886+83443+41721-20860+10430+5215-2607-1303-651+325-162+81-40+20+10+5-2-1(+ 1)
    0x0027066e, // [280] 0.000587227020362 (00062000)  401408=-2670163+1335086+667544+333772-166886+83443+41721-20860-10430+5215+2607-1303-651-325-162-81+40+20+10-5-2+1(+ -1)
    0x00263fae, // [281] 0.000611195470173 (00066000)  417792=-2670163+1335086+667544+333772-166886+83443-41721+20860+10430+5215+2607+1303+651+325-162-81-40+20+10-5-2+1(+ -1)
    0x0007e9ae, // [282] 0.000635163919984 (0006a000)  434176=-2670163+1335086+667544+333772-166886+83443-41721+20860+10430-5215-2607+1303-651+325+162+81+40+20+10-5-2-1(+ 1)
    0x002accae, // [283] 0.000659132369795 (0006dfff)  450559=-2670163+1335086+667544+333772-166886+83443-41721+20860-10430-5215+2607+1303-651-325+162+81-40+20-10+5-2+1(+ 0)
    0x0033732e, // [284] 0.000683100819605 (00071fff)  466943=-2670163+1335086+667544+333772-166886+83443-41721-20860+10430+5215-2607-1303+651+325+162-81+40+20-10-5+2+1(+ -2)
    0x0002562e, // [285] 0.000707069269416 (00075fff)  483327=-2670163+1335086+667544+333772-166886+83443-41721-20860-10430+5215+2607-1303+651-325+162-81-40+20-10-5-2-1(+ 0)
    0x001da02e, // [286] 0.000731037719227 (00079fff)  499711=-2670163+1335086+667544+333772-166886+83443-41721-20860-10430-5215-2607-1303-651+325-162+81+40-20+10+5+2-1(+ 0)
    0x000c85ce, // [287] 0.000755006169037 (0007dfff)  516095=-2670163+1335086+667544+333772-166886-83443+41721+20860+10430-5215+2607-1303-651-325-162+81-40-20+10+5-2-1(+ 0)
    0x000d3cce, // [288] 0.000778974618848 (00082000)  532480=-2670163+1335086+667544+333772-166886-83443+41721+20860-10430-5215+2607+1303+651+325-162-81+40-20+10+5-2-1(+ 1)
    0x00041b4e, // [289] 0.000802943068659 (00086000)  548864=-2670163+1335086+667544+333772-166886-83443+41721-20860+10430+5215-2607+1303+651-325-162-81-40-20+10-5-2-1(+ 1)
    0x0039ce4e, // [290] 0.000826911518470 (0008a000)  565248=-2670163+1335086+667544+333772-166886-83443+41721-20860-10430+5215+2607+1303-651-325+162+81+40-20-10+5+2+1(+ -1)
    0x0008f04e, // [291] 0.000850879968280 (0008e000)  581632=-2670163+1335086+667544+333772-166886-83443+41721-20860-10430-5215-2607-1303+651+325+162+81-40-20-10+5-2-1(+ -1)
    0x0031558e, // [292] 0.000874848418091 (00092000)  598016=-2670163+1335086+667544+333772-166886-83443-41721+20860+10430-5215+2607-1303+651-325+162-81+40-20-10-5+2+1(+ -1)
    0x0000628e, // [293] 0.000898816867902 (00096000)  614400=-2670163+1335086+667544+333772-166886-83443-41721+20860-10430+5215-2607-1303-651+325+162-81-40-20-10-5-2-1(+ 1)
    0x001e870e, // [294] 0.000922785317712 (00099fff)  630783=-2670163+1335086+667544+333772-166886-83443-41721-20860+10430+5215+2607-1303-651-325-162+81-40+20+10+5+2-1(+ 0)
    0x001f3e0e, // [295] 0.000946753767523 (0009dfff)  647167=-2670163+1335086+667544+333772-166886-83443-41721-20860-10430+5215+2607+1303+651+325-162-81+40+20+10+5+2-1(+ 0)
    0x0016180e, // [296] 0.000970722217334 (000a1fff)  663551=-2670163+1335086+667544+333772-166886-83443-41721-20860-10430-5215-2607+1303+651-325-162-81-40+20+10-5+2-1(+ 0)
    0x0037cdf6, // [297] 0.000994690667145 (000a5fff)  679935=-2670163+1335086+667544-333772+166886+83443+41721+20860+10430-5215+2607+1303-651-325+162+81+40+20+10-5+2+1(+ 0)
    0x0006f2f6, // [298] 0.001018659116955 (000a9fff)  696319=-2670163+1335086+667544-333772+166886+83443+41721+20860-10430+5215-2607-1303+651+325+162+81-40+20+10-5-2-1(+ 0)
    0x002b5776, // [299] 0.001042627566766 (000ae000)  712704=-2670163+1335086+667544-333772+166886+83443+41721-20860+10430+5215+2607-1303+651-325+162-81+40+20-10+5-2+1(+ -1)
    0x00126176, // [300] 0.001066596016577 (000b2000)  729088=-2670163+1335086+667544-333772+166886+83443+41721-20860+10430-5215-2607-1303-651+325+162-81-40+20-10-5+2-1(+ -1)
    0x00038476, // [301] 0.001090564466387 (000b6000)  745472=-2670163+1335086+667544-333772+166886+83443+41721-20860-10430-5215+2607-1303-651-325-162+81+40+20-10-5-2-1(+ 1)
    0x0002bdb6, // [302] 0.001114532916198 (000ba000)  761856=-2670163+1335086+667544-333772+166886+83443-41721+20860+10430-5215+2607+1303+651+325-162+81-40+20-10-5-2-1(+ 1)
    0x000d1ab6, // [303] 0.001138501366009 (000be000)  778240=-2670163+1335086+667544-333772+166886+83443-41721+20860-10430+5215-2607+1303+651-325-162-81+40-20+10+5-2-1(+ -1)
    0x00142f36, // [304] 0.001162469815820 (000c2000)  794624=-2670163+1335086+667544-333772+166886+83443-41721-20860+10430+5215+2607+1303-651+325-162-81-40-20+10-5+2-1(+ -1)
    0x0015f136, // [305] 0.001186438265630 (000c5fff)  811007=-2670163+1335086+667544-333772+166886+83443-41721-20860+10430-5215-2607-1303+651+325+162+81+40-20+10-5+2-1(+ 0)
    0x0038d436, // [306] 0.001210406715441 (000c9fff)  827391=-2670163+1335086+667544-333772+166886+83443-41721-20860-10430-5215+2607-1303+651-325+162+81-40-20-10+5+2+1(+ 0)
    0x001963d6, // [307] 0.001234375165252 (000cdfff)  843775=-2670163+1335086+667544-333772+166886-83443+41721+20860+10430+5215-2607-1303-651+325+162-81+40-20-10+5+2-1(+ 0)
    0x003046d6, // [308] 0.001258343615062 (000d1fff)  860159=-2670163+1335086+667544-333772+166886-83443+41721+20860-10430+5215+2607-1303-651-325+162-81-40-20-10-5+2+1(+ 0)
    0x0011bf56, // [309] 0.001282312064873 (000d6000)  876544=-2670163+1335086+667544-333772+166886-83443+41721-20860+10430+5215+2607+1303+651+325-162+81+40-20-10-5+2-1(+ -1)
    0x003f1956, // [310] 0.001306280514684 (000da000)  892928=-2670163+1335086+667544-333772+166886-83443+41721-20860+10430-5215-2607+1303+651-325-162-81+40+20+10+5+2+1(+ -1)
    0x000e2c56, // [311] 0.001330248964495 (000de000)  909312=-2670163+1335086+667544-333772+166886-83443+41721-20860-10430-5215+2607+1303-651+325-162-81-40+20+10+5-2-1(+ -1)
    0x000ff396, // [312] 0.001354217414305 (000e2000)  925696=-2670163+1335086+667544-333772+166886-83443-41721+20860+10430+5215-2607-1303+651+325+162+81+40+20+10+5-2-1(+ -1)
    0x0026d696, // [313] 0.001378185864116 (000e6000)  942080=-2670163+1335086+667544-333772+166886-83443-41721+20860-10430+5215+2607-1303+651-325+162+81-40+20+10-5-2+1(+ -1)
    0x003b6096, // [314] 0.001402154313927 (000ea000)  958464=-2670163+1335086+667544-333772+166886-83443-41721+20860-10430-5215-2607-1303-651+325+162-81+40+20-10+5+2+1(+ -1)
    0x000a4516, // [315] 0.001426122763737 (000ee000)  974848=-2670163+1335086+667544-333772+166886-83443-41721-20860+10430-5215+2607-1303-651-325+162-81-40+20-10+5-2-1(+ 1)
    0x000bbc16, // [316] 0.001450091213548 (000f1fff)  991231=-2670163+1335086+667544-333772+166886-83443-41721-20860-10430-5215+2607+1303+651+325-162+81+40+20-10+5-2-1(+ 0)
    0x00129be6, // [317] 0.001474059663359 (000f5fff)  1007615=-2670163+1335086+667544-333772-166886+83443+41721+20860+10430+5215-2607+1303+651-325-162+81-40+20-10-5+2-1(+ 0)
    0x00032ee6, // [318] 0.001498028113170 (000f9fff)  1023999=-2670163+1335086+667544-333772-166886+83443+41721+20860-10430+5215+2607+1303-651+325-162-81+40+20-10-5-2-1(+ 0)
    0x000c08e6, // [319] 0.001521996562980 (000fdfff)  1040383=-2670163+1335086+667544-333772-166886+83443+41721+20860-10430-5215-2607+1303-651-325-162-81-40-20+10+5-2-1(+ 0)
    0x000dd566, // [320] 0.001545965012791 (00102000)  1056768=-2670163+1335086+667544-333772-166886+83443+41721-20860+10430-5215+2607-1303+651-325+162+81+40-20+10+5-2-1(+ 1)
    0x0004e266, // [321] 0.001569933462602 (00106000)  1073152=-2670163+1335086+667544-333772-166886+83443+41721-20860-10430+5215-2607-1303-651+325+162+81-40-20+10-5-2-1(+ -1)
    0x003947a6, // [322] 0.001593901912412 (0010a000)  1089536=-2670163+1335086+667544-333772-166886+83443-41721+20860+10430+5215+2607-1303-651-325+162-81+40-20-10+5+2+1(+ -1)
    0x00187ea6, // [323] 0.001617870362223 (0010e000)  1105920=-2670163+1335086+667544-333772-166886+83443-41721+20860-10430+5215+2607+1303+651+325+162-81-40-20-10+5+2-1(+ -1)
    0x003198a6, // [324] 0.001641838812034 (00112000)  1122304=-2670163+1335086+667544-333772-166886+83443-41721+20860-10430-5215-2607+1303+651-325-162+81+40-20-10-5+2+1(+ -1)
    0x0000ad26, // [325] 0.001665807261845 (00116000)  1138688=-2670163+1335086+667544-333772-166886+83443-41721-20860+10430-5215+2607+1303-651+325-162+81-40-20-10-5-2-1(+ -1)
    0x002e0a26, // [326] 0.001689775711655 (0011a000)  1155072=-2670163+1335086+667544-333772-166886+83443-41721-20860-10430+5215-2607+1303-651-325-162-81-40+20+10+5-2+1(+ -1)
    0x003fd7c6, // [327] 0.001713744161466 (0011dfff)  1171455=-2670163+1335086+667544-333772-166886-83443+41721+20860+10430+5215+2607-1303+651-325+162+81+40+20+10+5+2+1(+ -2)
    0x000ee1c6, // [328] 0.001737712611277 (00122000)  1187840=-2670163+1335086+667544-333772-166886-83443+41721+20860+10430-5215-2607-1303-651+325+162+81-40+20+10+5-2-1(+ 1)
    0x001744c6, // [329] 0.001761681061087 (00125fff)  1204223=-2670163+1335086+667544-333772-166886-83443+41721+20860-10430-5215+2607-1303-651-325+162-81+40+20+10-5+2-1(+ 0)
    0x00067d46, // [330] 0.001785649510898 (0012a000)  1220608=-2670163+1335086+667544-333772-166886-83443+41721-20860+10430-5215+2607+1303+651+325+162-81-40+20+10-5-2-1(+ -1)
    0x001b9a46, // [331] 0.001809617960709 (0012dfff)  1236991=-2670163+1335086+667544-333772-166886-83443+41721-20860-10430+5215-2607+1303+651-325-162+81+40+20-10+5+2-1(+ 0)
    0x000aaf86, // [332] 0.001833586410520 (00132000)  1253376=-2670163+1335086+667544-333772-166886-83443-41721+20860+10430+5215+2607+1303-651+325-162+81-40+20-10+5-2-1(+ 1)
    0x00230986, // [333] 0.001857554860330 (00135fff)  1269759=-2670163+1335086+667544-333772-166886-83443-41721+20860+10430-5215-2607+1303-651-325-162-81+40+20-10-5-2+1(+ 0)
    0x003c3486, // [334] 0.001881523310141 (0013a000)  1286144=-2670163+1335086+667544-333772-166886-83443-41721+20860-10430-5215+2607-1303+651+325-162-81-40-20+10+5+2+1(+ -1)
    0x001de306, // [335] 0.001905491759952 (0013dfff)  1302527=-2670163+1335086+667544-333772-166886-83443-41721-20860+10430+5215-2607-1303-651+325+162+81+40-20+10+5+2-1(+ 0)
    0x0014c606, // [336] 0.001929460209762 (00142000)  1318912=-2670163+1335086+667544-333772-166886-83443-41721-20860-10430+5215+2607-1303-651-325+162+81-40-20+10-5+2-1(+ -1)
    0x000d7ffa, // [337] 0.001953428659573 (00146000)  1335296=-2670163+1335086-667544+333772+166886+83443+41721+20860+10430+5215+2607+1303+651+325+162-81+40-20+10+5-2-1(+ 1)
    0x000459fa, // [338] 0.001977397109384 (00149fff)  1351679=-2670163+1335086-667544+333772+166886+83443+41721+20860+10430-5215-2607+1303+651-325+162-81-40-20+10-5-2-1(+ 0)
    0x0019acfa, // [339] 0.002001365559195 (0014e000)  1368064=-2670163+1335086-667544+333772+166886+83443+41721+20860-10430-5215+2607+1303-651+325-162+81+40-20-10+5+2-1(+ -1)
    0x00308b7a, // [340] 0.002025334009005 (00151fff)  1384447=-2670163+1335086-667544+333772+166886+83443+41721-20860+10430+5215-2607+1303-651-325-162+81-40-20-10-5+2+1(+ 0)
    0x0021367a, // [341] 0.002049302458816 (00156000)  1400832=-2670163+1335086-667544+333772+166886+83443+41721-20860-10430+5215+2607-1303+651+325-162-81+40-20-10-5-2+1(+ -1)
    0x003fe07a, // [342] 0.002073270908627 (00159fff)  1417215=-2670163+1335086-667544+333772+166886+83443+41721-20860-10430-5215-2607-1303-651+325+162+81+40+20+10+5+2+1(+ -4)
    0x002ec5ba, // [343] 0.002097239358437 (0015e000)  1433600=-2670163+1335086-667544+333772+166886+83443-41721+20860+10430-5215+2607-1303-651-325+162+81-40+20+10+5-2+1(+ -1)
    0x001f7cba, // [344] 0.002121207808248 (00161fff)  1449983=-2670163+1335086-667544+333772+166886+83443-41721+20860-10430-5215+2607+1303+651+325+162-81+40+20+10+5+2-1(+ 0)
    0x00265b3a, // [345] 0.002145176258059 (00166000)  1466368=-2670163+1335086-667544+333772+166886+83443-41721-20860+10430+5215-2607+1303+651-325+162-81-40+20+10-5-2+1(+ -1)
    0x0007ae3a, // [346] 0.002169144707870 (00169fff)  1482751=-2670163+1335086-667544+333772+166886+83443-41721-20860-10430+5215+2607+1303-651+325-162+81+40+20+10-5-2-1(+ 0)
    0x000a883a, // [347] 0.002193113157680 (0016e000)  1499136=-2670163+1335086-667544+333772+166886+83443-41721-20860-10430-5215-2607+1303-651-325-162+81-40+20-10+5-2-1(+ 1)
    0x000b35da, // [348] 0.002217081607491 (00172000)  1515520=-2670163+1335086-667544+333772+166886-83443+41721+20860+10430-5215+2607-1303+651+325-162-81+40+20-10+5-2-1(+ -1)
    0x002212da, // [349] 0.002241050057302 (00175fff)  1531903=-2670163+1335086-667544+333772+166886-83443+41721+20860-10430+5215-2607-1303+651-325-162-81-40+20-10-5-2+1(+ 0)
    0x0003c75a, // [350] 0.002265018507112 (0017a000)  1548288=-2670163+1335086-667544+333772+166886-83443+41721-20860+10430+5215+2607-1303-651-325+162+81+40+20-10-5-2-1(+ 1)
    0x003cfe5a, // [351] 0.002288986956923 (0017dfff)  1564671=-2670163+1335086-667544+333772+166886-83443+41721-20860-10430+5215+2607+1303+651+325+162+81-40-20+10+5+2+1(+ -2)
    0x000d585a, // [352] 0.002312955406734 (00182000)  1581056=-2670163+1335086-667544+333772+166886-83443+41721-20860-10430-5215-2607+1303+651-325+162-81+40-20+10+5-2-1(+ 1)
    0x00346d9a, // [353] 0.002336923856545 (00185fff)  1597439=-2670163+1335086-667544+333772+166886-83443-41721+20860+10430-5215+2607+1303-651+325+162-81-40-20+10-5+2+1(+ 0)
    0x00398a9a, // [354] 0.002360892306355 (0018a000)  1613824=-2670163+1335086-667544+333772+166886-83443-41721+20860-10430+5215-2607+1303-651-325-162+81+40-20-10+5+2+1(+ -1)
    0x0018b71a, // [355] 0.002384860756166 (0018dfff)  1630207=-2670163+1335086-667544+333772+166886-83443-41721-20860+10430+5215+2607-1303+651+325-162+81-40-20-10+5+2-1(+ 0)
    0x0011111a, // [356] 0.002408829205977 (00192000)  1646592=-2670163+1335086-667544+333772+166886-83443-41721-20860+10430-5215-2607-1303+651-325-162-81+40-20-10-5+2-1(+ -1)
    0x0000241a, // [357] 0.002432797655787 (00196000)  1662976=-2670163+1335086-667544+333772+166886-83443-41721-20860-10430-5215+2607-1303-651+325-162-81-40-20-10-5-2-1(+ 1)
    0x0011fdea, // [358] 0.002456766105598 (0019a000)  1679360=-2670163+1335086-667544+333772-166886+83443+41721+20860+10430-5215+2607+1303+651+325+162+81+40-20-10-5+2-1(+ -1)
    0x003f5aea, // [359] 0.002480734555409 (0019e000)  1695744=-2670163+1335086-667544+333772-166886+83443+41721+20860-10430+5215-2607+1303+651-325+162-81+40+20+10+5+2+1(+ -1)
    0x002e6f6a, // [360] 0.002504703005220 (001a1fff)  1712127=-2670163+1335086-667544+333772-166886+83443+41721-20860+10430+5215+2607+1303-651+325+162-81-40+20+10+5-2+1(+ 0)
    0x0027896a, // [361] 0.002528671455030 (001a6000)  1728512=-2670163+1335086-667544+333772-166886+83443+41721-20860+10430-5215-2607+1303-651-325-162+81+40+20+10-5-2+1(+ -1)
    0x0006b46a, // [362] 0.002552639904841 (001a9fff)  1744895=-2670163+1335086-667544+333772-166886+83443+41721-20860-10430-5215+2607-1303+651+325-162+81-40+20+10-5-2-1(+ 0)
    0x002b13aa, // [363] 0.002576608354652 (001ae000)  1761280=-2670163+1335086-667544+333772-166886+83443-41721+20860+10430+5215-2607-1303+651-325-162-81+40+20-10+5-2+1(+ -1)
    0x003226aa, // [364] 0.002600576804462 (001b1fff)  1777663=-2670163+1335086-667544+333772-166886+83443-41721+20860-10430+5215+2607-1303-651+325-162-81-40+20-10-5+2+1(+ -2)
    0x000bff2a, // [365] 0.002624545254273 (001b6000)  1794048=-2670163+1335086-667544+333772-166886+83443-41721-20860+10430+5215+2607+1303+651+325+162+81+40+20-10+5-2-1(+ 1)
    0x0002d92a, // [366] 0.002648513704084 (001b9fff)  1810431=-2670163+1335086-667544+333772-166886+83443-41721-20860+10430-5215-2607+1303+651-325+162+81-40+20-10-5-2-1(+ 0)
    0x001d6c2a, // [367] 0.002672482153895 (001be000)  1826816=-2670163+1335086-667544+333772-166886+83443-41721-20860-10430-5215+2607+1303-651+325+162-81+40-20+10+5+2-1(+ -1)
    0x000c4bca, // [368] 0.002696450603705 (001c2000)  1843200=-2670163+1335086-667544+333772-166886-83443+41721+20860+10430+5215-2607+1303-651-325+162-81-40-20+10+5-2-1(+ 1)
    0x0035b6ca, // [369] 0.002720419053516 (001c6000)  1859584=-2670163+1335086-667544+333772-166886-83443+41721+20860-10430+5215+2607-1303+651+325-162+81+40-20+10-5+2+1(+ -1)
    0x003890ca, // [370] 0.002744387503327 (001ca000)  1875968=-2670163+1335086-667544+333772-166886-83443+41721+20860-10430-5215-2607-1303+651-325-162+81-40-20-10+5+2+1(+ -1)
    0x0019254a, // [371] 0.002768355953137 (001cdfff)  1892351=-2670163+1335086-667544+333772-166886-83443+41721-20860+10430-5215+2607-1303-651+325-162-81+40-20-10+5+2-1(+ 0)
    0x0020024a, // [372] 0.002792324402948 (001d2000)  1908736=-2670163+1335086-667544+333772-166886-83443+41721-20860-10430+5215-2607-1303-651-325-162-81-40-20-10-5-2+1(+ -1)
    0x0031db8a, // [373] 0.002816292852759 (001d5fff)  1925119=-2670163+1335086-667544+333772-166886-83443-41721+20860+10430+5215-2607+1303+651-325+162+81+40-20-10-5+2+1(+ -2)
    0x0000ee8a, // [374] 0.002840261302570 (001da000)  1941504=-2670163+1335086-667544+333772-166886-83443-41721+20860-10430+5215+2607+1303-651+325+162+81-40-20-10-5-2-1(+ -1)
    0x001e488a, // [375] 0.002864229752380 (001ddfff)  1957887=-2670163+1335086-667544+333772-166886-83443-41721+20860-10430-5215-2607+1303-651-325+162-81-40+20+10+5+2-1(+ 0)
    0x000fb50a, // [376] 0.002888198202191 (001e2000)  1974272=-2670163+1335086-667544+333772-166886-83443-41721-20860+10430-5215+2607-1303+651+325-162+81+40+20+10+5-2-1(+ -1)
    0x0026920a, // [377] 0.002912166652002 (001e5fff)  1990655=-2670163+1335086-667544+333772-166886-83443-41721-20860-10430+5215-2607-1303+651-325-162+81-40+20+10-5-2+1(+ 0)
    0x000727f2, // [378] 0.002936135101812 (001ea000)  2007040=-2670163+1335086-667544-333772+166886+83443+41721+20860+10430+5215+2607-1303-651+325-162-81+40+20+10-5-2-1(+ -1)
    0x000a01f2, // [379] 0.002960103551623 (001ee000)  2023424=-2670163+1335086-667544-333772+166886+83443+41721+20860+10430-5215-2607-1303-651-325-162-81-40+20-10+5-2-1(+ -1)
    0x002bd8f2, // [380] 0.002984072001434 (001f2000)  2039808=-2670163+1335086-667544-333772+166886+83443+41721+20860-10430-5215-2607+1303+651-325+162+81+40+20-10+5-2+1(+ -1)
    0x0032ed72, // [381] 0.003008040451245 (001f6000)  2056192=-2670163+1335086-667544-333772+166886+83443+41721-20860+10430-5215+2607+1303-651+325+162+81-40+20-10-5+2+1(+ -1)
    0x00034a72, // [382] 0.003032008901055 (001f9fff)  2072575=-2670163+1335086-667544-333772+166886+83443+41721-20860-10430+5215-2607+1303-651-325+162-81+40+20-10-5-2-1(+ 0)
    0x003c77b2, // [383] 0.003055977350866 (001fe000)  2088960=-2670163+1335086-667544-333772+166886+83443-41721+20860+10430+5215+2607-1303+651+325+162-81-40-20+10+5+2+1(+ -1)
    0x000d91b2, // [384] 0.003079945800677 (00201fff)  2105343=-2670163+1335086-667544-333772+166886+83443-41721+20860+10430-5215-2607-1303+651-325-162+81+40-20+10+5-2-1(+ 0)
    0x0024a4b2, // [385] 0.003103914250487 (00206000)  2121728=-2670163+1335086-667544-333772+166886+83443-41721+20860-10430-5215+2607-1303-651+325-162+81-40-20+10-5-2+1(+ -1)
    0x00390332, // [386] 0.003127882700298 (00209fff)  2138111=-2670163+1335086-667544-333772+166886+83443-41721-20860+10430+5215-2607-1303-651-325-162-81+40-20-10+5+2+1(+ 0)
    0x00283a32, // [387] 0.003151851150109 (0020e000)  2154496=-2670163+1335086-667544-333772+166886+83443-41721-20860-10430+5215-2607+1303+651+325-162-81-40-20-10+5-2+1(+ -1)
    0x0039efd2, // [388] 0.003175819599919 (00211fff)  2170879=-2670163+1335086-667544-333772+166886-83443+41721+20860+10430+5215+2607+1303-651+325+162+81+40-20-10+5+2+1(+ 0)
    0x0010c9d2, // [389] 0.003199788049730 (00216000)  2187264=-2670163+1335086-667544-333772+166886-83443+41721+20860+10430-5215-2607+1303-651-325+162+81-40-20-10-5+2-1(+ -1)
    0x002174d2, // [390] 0.003223756499541 (0021a000)  2203648=-2670163+1335086-667544-333772+166886-83443+41721+20860-10430-5215+2607-1303+651+325+162-81+40-20-10-5-2+1(+ -1)
    0x001f9352, // [391] 0.003247724949352 (0021e000)  2220032=-2670163+1335086-667544-333772+166886-83443+41721-20860+10430+5215-2607-1303+651-325-162+81+40+20+10+5+2-1(+ -1)
    0x000ea652, // [392] 0.003271693399162 (00222000)  2236416=-2670163+1335086-667544-333772+166886-83443+41721-20860-10430+5215+2607-1303-651+325-162+81-40+20+10+5-2-1(+ 1)
    0x00070052, // [393] 0.003295661848973 (00226000)  2252800=-2670163+1335086-667544-333772+166886-83443+41721-20860-10430-5215-2607-1303-651-325-162-81+40+20+10-5-2-1(+ -1)
    0x00063992, // [394] 0.003319630298784 (0022a000)  2269184=-2670163+1335086-667544-333772+166886-83443-41721+20860+10430-5215-2607+1303+651+325-162-81-40+20+10-5-2-1(+ -1)
    0x0007ec92, // [395] 0.003343598748594 (0022dfff)  2285567=-2670163+1335086-667544-333772+166886-83443-41721+20860-10430-5215+2607+1303-651+325+162+81+40+20+10-5-2-1(+ 0)
    0x000acb12, // [396] 0.003367567198405 (00232000)  2301952=-2670163+1335086-667544-333772+166886-83443-41721-20860+10430+5215-2607+1303-651-325+162+81-40+20-10+5-2-1(+ 1)
    0x00337612, // [397] 0.003391535648216 (00235fff)  2318335=-2670163+1335086-667544-333772+166886-83443-41721-20860-10430+5215+2607-1303+651+325+162-81+40+20-10-5+2+1(+ -2)
    0x003c5012, // [398] 0.003415504098027 (0023a000)  2334720=-2670163+1335086-667544-333772+166886-83443-41721-20860-10430-5215-2607-1303+651-325+162-81-40-20+10+5+2+1(+ -1)
    0x003da5e2, // [399] 0.003439472547837 (0023dfff)  2351103=-2670163+1335086-667544-333772-166886+83443+41721+20860+10430-5215+2607-1303-651+325-162+81+40-20+10+5+2+1(+ -2)
    0x003482e2, // [400] 0.003463440997648 (00242000)  2367488=-2670163+1335086-667544-333772-166886+83443+41721+20860-10430+5215-2607-1303-651-325-162+81-40-20+10-5+2+1(+ -1)
    0x00353b62, // [401] 0.003487409447459 (00246000)  2383872=-2670163+1335086-667544-333772-166886+83443+41721-20860+10430+5215-2607+1303+651+325-162-81+40-20+10-5+2+1(+ -1)
    0x00041e62, // [402] 0.003511377897269 (0024a000)  2400256=-2670163+1335086-667544-333772-166886+83443+41721-20860-10430+5215+2607+1303+651-325-162-81-40-20+10-5-2-1(+ 1)
    0x0019c862, // [403] 0.003535346347080 (0024e000)  2416640=-2670163+1335086-667544-333772-166886+83443+41721-20860-10430-5215-2607+1303-651-325+162+81+40-20-10+5+2-1(+ -1)
    0x0018f5a2, // [404] 0.003559314796891 (00251fff)  2433023=-2670163+1335086-667544-333772-166886+83443-41721+20860+10430-5215+2607-1303+651+325+162+81-40-20-10+5+2-1(+ 0)
    0x001152a2, // [405] 0.003583283246702 (00256000)  2449408=-2670163+1335086-667544-333772-166886+83443-41721+20860-10430+5215-2607-1303+651-325+162-81+40-20-10-5+2-1(+ -1)
    0x00006722, // [406] 0.003607251696512 (00259fff)  2465791=-2670163+1335086-667544-333772-166886+83443-41721-20860+10430+5215+2607-1303-651+325+162-81-40-20-10-5-2-1(+ 0)
    0x000e8122, // [407] 0.003631220146323 (0025e000)  2482176=-2670163+1335086-667544-333772-166886+83443-41721-20860+10430-5215-2607-1303-651-325-162+81-40+20+10+5-2-1(+ -1)
    0x002f3822, // [408] 0.003655188596134 (00261fff)  2498559=-2670163+1335086-667544-333772-166886+83443-41721-20860-10430-5215-2607+1303+651+325-162-81+40+20+10+5-2+1(+ 0)
    0x00361dc2, // [409] 0.003679157045944 (00266000)  2514944=-2670163+1335086-667544-333772-166886-83443+41721+20860+10430-5215+2607+1303+651-325-162-81-40+20+10-5+2+1(+ -1)
    0x0017cac2, // [410] 0.003703125495755 (00269fff)  2531327=-2670163+1335086-667544-333772-166886-83443+41721+20860-10430+5215-2607+1303-651-325+162+81+40+20+10-5+2-1(+ 0)
    0x0006f742, // [411] 0.003727093945566 (0026e000)  2547712=-2670163+1335086-667544-333772-166886-83443+41721-20860+10430+5215+2607-1303+651+325+162+81-40+20+10-5-2-1(+ 1)
    0x000b5142, // [412] 0.003751062395377 (00272000)  2564096=-2670163+1335086-667544-333772-166886-83443+41721-20860+10430-5215-2607-1303+651-325+162-81+40+20-10+5-2-1(+ -1)
    0x00126442, // [413] 0.003775030845187 (00276000)  2580480=-2670163+1335086-667544-333772-166886-83443+41721-20860-10430-5215+2607-1303-651+325+162-81-40+20-10-5+2-1(+ -1)
    0x00038382, // [414] 0.003798999294998 (0027a000)  2596864=-2670163+1335086-667544-333772-166886-83443-41721+20860+10430+5215-2607-1303-651-325-162+81+40+20-10-5-2-1(+ 1)
    0x003cba82, // [415] 0.003822967744809 (0027dfff)  2613247=-2670163+1335086-667544-333772-166886-83443-41721+20860-10430+5215-2607+1303+651+325-162+81-40-20+10+5+2+1(+ -2)
    0x000d1f02, // [416] 0.003846936194619 (00282000)  2629632=-2670163+1335086-667544-333772-166886-83443-41721-20860+10430+5215+2607+1303+651-325-162-81+40-20+10+5-2-1(+ -1)
    0x00142902, // [417] 0.003870904644430 (00285fff)  2646015=-2670163+1335086-667544-333772-166886-83443-41721-20860+10430-5215-2607+1303-651+325-162-81-40-20+10-5+2-1(+ 0)
    0x0025f402, // [418] 0.003894873094241 (0028a000)  2662400=-2670163+1335086-667544-333772-166886-83443-41721-20860-10430-5215+2607-1303+651+325+162+81+40-20+10-5-2+1(+ -1)
    0x0018d3fc, // [419] 0.003918841544052 (0028dfff)  2678783=-2670163-1335086+667544+333772+166886+83443+41721+20860+10430+5215-2607-1303+651-325+162+81-40-20-10+5+2-1(+ 0)
    0x000966fc, // [420] 0.003942809993862 (00292000)  2695168=-2670163-1335086+667544+333772+166886+83443+41721+20860-10430+5215+2607-1303-651+325+162-81+40-20-10+5-2-1(+ 1)
    0x000040fc, // [421] 0.003966778443673 (00296000)  2711552=-2670163-1335086+667544+333772+166886+83443+41721+20860-10430-5215-2607-1303-651-325+162-81-40-20-10-5-2-1(+ 1)
    0x0001b97c, // [422] 0.003990746893484 (0029a000)  2727936=-2670163-1335086+667544+333772+166886+83443+41721-20860+10430-5215-2607+1303+651+325-162+81+40-20-10-5-2-1(+ 1)
    0x002f1c7c, // [423] 0.004014715343294 (0029e000)  2744320=-2670163-1335086+667544+333772+166886+83443+41721-20860-10430-5215+2607+1303+651-325-162-81+40+20+10+5-2+1(+ -1)
    0x00362bbc, // [424] 0.004038683793105 (002a2000)  2760704=-2670163-1335086+667544+333772+166886+83443-41721+20860+10430+5215-2607+1303-651+325-162-81-40+20+10-5+2+1(+ -1)
    0x0037f6bc, // [425] 0.004062652242916 (002a6000)  2777088=-2670163-1335086+667544+333772+166886+83443-41721+20860-10430+5215+2607-1303+651+325+162+81+40+20+10-5+2+1(+ -1)
    0x003ad0bc, // [426] 0.004086620692727 (002a9fff)  2793471=-2670163-1335086+667544+333772+166886+83443-41721+20860-10430-5215-2607-1303+651-325+162+81-40+20-10+5+2+1(+ -2)
    0x002b653c, // [427] 0.004110589142537 (002ae000)  2809856=-2670163-1335086+667544+333772+166886+83443-41721-20860+10430-5215+2607-1303-651+325+162-81+40+20-10+5-2+1(+ -1)
    0x0012423c, // [428] 0.004134557592348 (002b1fff)  2826239=-2670163-1335086+667544+333772+166886+83443-41721-20860-10430+5215-2607-1303-651-325+162-81-40+20-10-5+2-1(+ 0)
    0x0033bbdc, // [429] 0.004158526042159 (002b6000)  2842624=-2670163-1335086+667544+333772+166886-83443+41721+20860+10430+5215-2607+1303+651+325-162+81+40+20-10-5+2+1(+ -1)
    0x00029edc, // [430] 0.004182494491969 (002b9fff)  2859007=-2670163-1335086+667544+333772+166886-83443+41721+20860-10430+5215+2607+1303+651-325-162+81-40+20-10-5-2-1(+ 0)
    0x002d28dc, // [431] 0.004206462941780 (002be000)  2875392=-2670163-1335086+667544+333772+166886-83443+41721+20860-10430-5215-2607+1303-651+325-162-81+40-20+10+5-2+1(+ -1)
    0x00140d5c, // [432] 0.004230431391591 (002c2000)  2891776=-2670163-1335086+667544+333772+166886-83443+41721-20860+10430-5215+2607+1303-651-325-162-81-40-20+10-5+2-1(+ -1)
    0x0025d25c, // [433] 0.004254399841402 (002c6000)  2908160=-2670163-1335086+667544+333772+166886-83443+41721-20860-10430+5215-2607-1303+651-325+162+81+40-20+10-5-2+1(+ -1)
    0x0004e79c, // [434] 0.004278368291212 (002ca000)  2924544=-2670163-1335086+667544+333772+166886-83443-41721+20860+10430+5215+2607-1303-651+325+162+81-40-20+10-5-2-1(+ 1)
    0x0009419c, // [435] 0.004302336741023 (002ce000)  2940928=-2670163-1335086+667544+333772+166886-83443-41721+20860+10430-5215-2607-1303-651-325+162-81+40-20-10+5-2-1(+ -1)
    0x0008789c, // [436] 0.004326305190834 (002d2000)  2957312=-2670163-1335086+667544+333772+166886-83443-41721+20860-10430-5215-2607+1303+651+325+162-81-40-20-10+5-2-1(+ 1)
    0x00119d1c, // [437] 0.004350273640644 (002d5fff)  2973695=-2670163-1335086+667544+333772+166886-83443-41721-20860+10430-5215+2607+1303+651-325-162+81+40-20-10-5+2-1(+ 0)
    0x003f2a1c, // [438] 0.004374242090455 (002da000)  2990080=-2670163-1335086+667544+333772+166886-83443-41721-20860-10430+5215-2607+1303-651+325-162-81+40+20+10+5+2+1(+ -1)
    0x001e0fec, // [439] 0.004398210540266 (002ddfff)  3006463=-2670163-1335086+667544+333772-166886+83443+41721+20860+10430+5215+2607+1303-651-325-162-81-40+20+10+5+2-1(+ 0)
    0x000fd1ec, // [440] 0.004422178990077 (002e2000)  3022848=-2670163-1335086+667544+333772-166886+83443+41721+20860+10430-5215-2607-1303+651-325+162+81+40+20+10+5-2-1(+ -1)
    0x0036e4ec, // [441] 0.004446147439887 (002e5fff)  3039231=-2670163-1335086+667544+333772-166886+83443+41721+20860-10430-5215+2607-1303-651+325+162+81-40+20+10-5+2+1(+ 0)
    0x003b436c, // [442] 0.004470115889698 (002ea000)  3055616=-2670163-1335086+667544+333772-166886+83443+41721-20860+10430+5215-2607-1303-651-325+162-81+40+20-10+5+2+1(+ -1)
    0x001a7a6c, // [443] 0.004494084339509 (002ee000)  3072000=-2670163-1335086+667544+333772-166886+83443+41721-20860-10430+5215-2607+1303+651+325+162-81-40+20-10+5+2-1(+ -1)
    0x000b9fac, // [444] 0.004518052789319 (002f2000)  3088384=-2670163-1335086+667544+333772-166886+83443-41721+20860+10430+5215+2607+1303+651-325-162+81+40+20-10+5-2-1(+ -1)
    0x0022a9ac, // [445] 0.004542021239130 (002f6000)  3104768=-2670163-1335086+667544+333772-166886+83443-41721+20860+10430-5215-2607+1303-651+325-162+81-40+20-10-5-2+1(+ -1)
    0x003d0cac, // [446] 0.004565989688941 (002fa000)  3121152=-2670163-1335086+667544+333772-166886+83443-41721+20860-10430-5215+2607+1303-651-325-162-81+40-20+10+5+2+1(+ -1)
    0x000c332c, // [447] 0.004589958138752 (002fe000)  3137536=-2670163-1335086+667544+333772-166886+83443-41721-20860+10430+5215-2607-1303+651+325-162-81-40-20+10+5-2-1(+ -1)
    0x000de62c, // [448] 0.004613926588562 (00301fff)  3153919=-2670163-1335086+667544+333772-166886+83443-41721-20860-10430+5215+2607-1303-651+325+162+81+40-20+10+5-2-1(+ 0)
    0x0004c02c, // [449] 0.004637895038373 (00306000)  3170304=-2670163-1335086+667544+333772-166886+83443-41721-20860-10430-5215-2607-1303-651-325+162+81-40-20+10-5-2-1(+ 1)
    0x001579cc, // [450] 0.004661863488184 (00309fff)  3186687=-2670163-1335086+667544+333772-166886-83443+41721+20860+10430-5215-2607+1303+651+325+162-81+40-20+10-5+2-1(+ 0)
    0x00185ccc, // [451] 0.004685831937994 (0030e000)  3203072=-2670163-1335086+667544+333772-166886-83443+41721+20860-10430-5215+2607+1303+651-325+162-81-40-20-10+5+2-1(+ -1)
    0x0009ab4c, // [452] 0.004709800387805 (00311fff)  3219455=-2670163-1335086+667544+333772-166886-83443+41721-20860+10430+5215-2607+1303-651+325-162+81+40-20-10+5-2-1(+ 0)
    0x00008e4c, // [453] 0.004733768837616 (00316000)  3235840=-2670163-1335086+667544+333772-166886-83443+41721-20860-10430+5215+2607+1303-651-325-162+81-40-20-10-5-2-1(+ -1)
    0x003e304c, // [454] 0.004757737287427 (0031a000)  3252224=-2670163-1335086+667544+333772-166886-83443+41721-20860-10430-5215-2607-1303+651+325-162-81-40+20+10+5+2+1(+ -1)
    0x003fe58c, // [455] 0.004781705737237 (0031e000)  3268608=-2670163-1335086+667544+333772-166886-83443-41721+20860+10430-5215+2607-1303-651+325+162+81+40+20+10+5+2+1(+ -1)
    0x0036c28c, // [456] 0.004805674187048 (00322000)  3284992=-2670163-1335086+667544+333772-166886-83443-41721+20860-10430+5215-2607-1303-651-325+162+81-40+20+10-5+2+1(+ -1)
    0x00377b0c, // [457] 0.004829642636859 (00326000)  3301376=-2670163-1335086+667544+333772-166886-83443-41721-20860+10430+5215-2607+1303+651+325+162-81+40+20+10-5+2+1(+ -1)
    0x00065e0c, // [458] 0.004853611086669 (0032a000)  3317760=-2670163-1335086+667544+333772-166886-83443-41721-20860-10430+5215+2607+1303+651-325+162-81-40+20+10-5-2-1(+ 1)
    0x001ba80c, // [459] 0.004877579536480 (0032dfff)  3334143=-2670163-1335086+667544+333772-166886-83443-41721-20860-10430-5215-2607+1303-651+325-162+81+40+20-10+5+2-1(+ 0)
    0x000a8df4, // [460] 0.004901547986291 (00332000)  3350528=-2670163-1335086+667544-333772+166886+83443+41721+20860+10430-5215+2607+1303-651-325-162+81-40+20-10+5-2-1(+ 1)
    0x003332f4, // [461] 0.004925516436102 (00335fff)  3366911=-2670163-1335086+667544-333772+166886+83443+41721+20860-10430+5215-2607-1303+651+325-162-81+40+20-10-5+2+1(+ 0)
    0x003c1774, // [462] 0.004949484885912 (0033a000)  3383296=-2670163-1335086+667544-333772+166886+83443+41721-20860+10430+5215+2607-1303+651-325-162-81-40-20+10+5+2+1(+ -1)
    0x001dc174, // [463] 0.004973453335723 (0033dfff)  3399679=-2670163-1335086+667544-333772+166886+83443+41721-20860+10430-5215-2607-1303-651-325+162+81+40-20+10+5+2-1(+ 0)
    0x000cf874, // [464] 0.004997421785534 (00342000)  3416064=-2670163-1335086+667544-333772+166886+83443+41721-20860-10430-5215-2607+1303+651+325+162+81-40-20+10+5-2-1(+ -1)
    0x00355db4, // [465] 0.005021390235344 (00346000)  3432448=-2670163-1335086+667544-333772+166886+83443-41721+20860+10430-5215+2607+1303+651-325+162-81+40-20+10-5+2+1(+ -1)
    0x00046ab4, // [466] 0.005045358685155 (0034a000)  3448832=-2670163-1335086+667544-333772+166886+83443-41721+20860-10430+5215-2607+1303-651+325+162-81-40-20+10-5-2-1(+ 1)
    0x00298f34, // [467] 0.005069327134966 (0034e000)  3465216=-2670163-1335086+667544-333772+166886+83443-41721-20860+10430+5215+2607+1303-651-325-162+81+40-20-10+5-2+1(+ -1)
    0x0030b134, // [468] 0.005093295584777 (00351fff)  3481599=-2670163-1335086+667544-333772+166886+83443-41721-20860+10430-5215-2607-1303+651+325-162+81-40-20-10-5+2+1(+ -2)
    0x00011434, // [469] 0.005117264034587 (00356000)  3497984=-2670163-1335086+667544-333772+166886+83443-41721-20860-10430-5215+2607-1303+651-325-162-81+40-20-10-5-2-1(+ -1)
    0x000023d4, // [470] 0.005141232484398 (00359fff)  3514367=-2670163-1335086+667544-333772+166886-83443+41721+20860+10430+5215-2607-1303-651+325-162-81-40-20-10-5-2-1(+ 2)
    0x0001fad4, // [471] 0.005165200934209 (0035e000)  3530752=-2670163-1335086+667544-333772+166886-83443+41721+20860-10430+5215-2607+1303+651+325+162+81+40-20-10-5-2-1(+ 1)
    0x001f5f54, // [472] 0.005189169384019 (00361fff)  3547135=-2670163-1335086+667544-333772+166886-83443+41721-20860+10430+5215+2607+1303+651-325+162-81+40+20+10+5+2-1(+ 0)
    0x00166954, // [473] 0.005213137833830 (00366000)  3563520=-2670163-1335086+667544-333772+166886-83443+41721-20860+10430-5215-2607+1303-651+325+162-81-40+20+10-5+2-1(+ -1)
    0x00078c54, // [474] 0.005237106283641 (00369fff)  3579903=-2670163-1335086+667544-333772+166886-83443+41721-20860-10430-5215+2607+1303-651-325-162+81+40+20+10-5-2-1(+ 0)
    0x001ab394, // [475] 0.005261074733452 (0036e000)  3596288=-2670163-1335086+667544-333772+166886-83443-41721+20860+10430+5215-2607-1303+651+325-162+81-40+20-10+5+2-1(+ -1)
    0x000b1694, // [476] 0.005285043183262 (00372000)  3612672=-2670163-1335086+667544-333772+166886-83443-41721+20860-10430+5215+2607-1303+651-325-162-81+40+20-10+5-2-1(+ 1)
    0x00022094, // [477] 0.005309011633073 (00376000)  3629056=-2670163-1335086+667544-333772+166886-83443-41721+20860-10430-5215-2607-1303-651+325-162-81-40+20-10-5-2-1(+ -1)
    0x0023f914, // [478] 0.005332980082884 (0037a000)  3645440=-2670163-1335086+667544-333772+166886-83443-41721-20860+10430-5215-2607+1303+651+325+162+81+40+20-10-5-2+1(+ -1)
    0x003cdc14, // [479] 0.005356948532694 (0037dfff)  3661823=-2670163-1335086+667544-333772+166886-83443-41721-20860-10430-5215+2607+1303+651-325+162+81-40-20+10+5+2+1(+ 0)
    0x002d6be4, // [480] 0.005380916982505 (00382000)  3678208=-2670163-1335086+667544-333772-166886+83443+41721+20860+10430+5215-2607+1303-651+325+162-81+40-20+10+5-2+1(+ -1)
    0x00344ee4, // [481] 0.005404885432316 (00385fff)  3694591=-2670163-1335086+667544-333772-166886+83443+41721+20860-10430+5215+2607+1303-651-325+162-81-40-20+10-5+2+1(+ 0)
    0x0005b0e4, // [482] 0.005428853882127 (0038a000)  3710976=-2670163-1335086+667544-333772-166886+83443+41721+20860-10430-5215-2607-1303+651+325-162+81+40-20+10-5-2-1(+ -1)
    0x00189564, // [483] 0.005452822331937 (0038dfff)  3727359=-2670163-1335086+667544-333772-166886+83443+41721-20860+10430-5215+2607-1303+651-325-162+81-40-20-10+5+2-1(+ 0)
    0x00312264, // [484] 0.005476790781748 (00392000)  3743744=-2670163-1335086+667544-333772-166886+83443+41721-20860-10430+5215-2607-1303-651+325-162-81+40-20-10-5+2+1(+ -1)
    0x000007a4, // [485] 0.005500759231559 (00396000)  3760128=-2670163-1335086+667544-333772-166886+83443-41721+20860+10430+5215+2607-1303-651-325-162-81-40-20-10-5-2-1(+ -1)
    0x0021dea4, // [486] 0.005524727681369 (0039a000)  3776512=-2670163-1335086+667544-333772-166886+83443-41721+20860-10430+5215+2607+1303+651-325+162+81+40-20-10-5-2+1(+ -1)
    0x003f68a4, // [487] 0.005548696131180 (0039e000)  3792896=-2670163-1335086+667544-333772-166886+83443-41721+20860-10430-5215-2607+1303-651+325+162-81+40+20+10+5+2+1(+ -1)
    0x000e4d24, // [488] 0.005572664580991 (003a2000)  3809280=-2670163-1335086+667544-333772-166886+83443-41721-20860+10430-5215+2607+1303-651-325+162-81-40+20+10+5-2-1(+ 1)
    0x0017b224, // [489] 0.005596633030802 (003a6000)  3825664=-2670163-1335086+667544-333772-166886+83443-41721-20860-10430+5215-2607-1303+651+325-162+81+40+20+10-5+2-1(+ -1)
    0x002697c4, // [490] 0.005620601480612 (003a9fff)  3842047=-2670163-1335086+667544-333772-166886-83443+41721+20860+10430+5215+2607-1303+651-325-162+81-40+20+10-5-2+1(+ 0)
    0x001b21c4, // [491] 0.005644569930423 (003ae000)  3858432=-2670163-1335086+667544-333772-166886-83443+41721+20860+10430-5215-2607-1303-651+325-162-81+40+20-10+5+2-1(+ -1)
    0x003204c4, // [492] 0.005668538380234 (003b1fff)  3874815=-2670163-1335086+667544-333772-166886-83443+41721+20860-10430-5215+2607-1303-651-325-162-81-40+20-10-5+2+1(+ -2)
    0x000bdd44, // [493] 0.005692506830044 (003b6000)  3891200=-2670163-1335086+667544-333772-166886-83443+41721-20860+10430-5215+2607+1303+651-325+162+81+40+20-10+5-2-1(+ 1)
    0x0022ea44, // [494] 0.005716475279855 (003b9fff)  3907583=-2670163-1335086+667544-333772-166886-83443+41721-20860-10430+5215-2607+1303-651+325+162+81-40+20-10-5-2+1(+ 0)
    0x003d4f84, // [495] 0.005740443729666 (003be000)  3923968=-2670163-1335086+667544-333772-166886-83443-41721+20860+10430+5215+2607+1303-651-325+162-81+40-20+10+5+2+1(+ -1)
    0x000c7184, // [496] 0.005764412179477 (003c2000)  3940352=-2670163-1335086+667544-333772-166886-83443-41721+20860+10430-5215-2607-1303+651+325+162-81-40-20+10+5-2-1(+ -1)
    0x00159484, // [497] 0.005788380629287 (003c6000)  3956736=-2670163-1335086+667544-333772-166886-83443-41721+20860-10430-5215+2607-1303+651-325-162+81+40-20+10-5+2-1(+ -1)
    0x0038a304, // [498] 0.005812349079098 (003ca000)  3973120=-2670163-1335086+667544-333772-166886-83443-41721-20860+10430+5215-2607-1303-651+325-162+81-40-20-10+5+2+1(+ -1)
    0x00090604, // [499] 0.005836317528909 (003ce000)  3989504=-2670163-1335086+667544-333772-166886-83443-41721-20860-10430+5215+2607-1303-651-325-162-81+40-20-10+5-2-1(+ -1)
    0x00283ff8, // [500] 0.005860285978719 (003d2000)  4005888=-2670163-1335086-667544+333772+166886+83443+41721+20860+10430+5215+2607+1303+651+325-162-81-40-20-10+5-2+1(+ -1)
    0x0009e9f8, // [501] 0.005884254428530 (003d5fff)  4022271=-2670163-1335086-667544+333772+166886+83443+41721+20860+10430-5215-2607+1303-651+325+162+81+40-20-10+5-2-1(+ 0)
    0x0000ccf8, // [502] 0.005908222878341 (003da000)  4038656=-2670163-1335086-667544+333772+166886+83443+41721+20860-10430-5215+2607+1303-651-325+162+81-40-20-10-5-2-1(+ -1)
    0x003e7378, // [503] 0.005932191328152 (003ddfff)  4055039=-2670163-1335086-667544+333772+166886+83443+41721-20860+10430+5215-2607-1303+651+325+162-81-40+20+10+5+2+1(+ -2)
    0x000f9678, // [504] 0.005956159777962 (003e2000)  4071424=-2670163-1335086-667544+333772+166886+83443+41721-20860-10430+5215+2607-1303+651-325-162+81+40+20+10+5-2-1(+ -1)
    0x0016a078, // [505] 0.005980128227773 (003e5fff)  4087807=-2670163-1335086-667544+333772+166886+83443+41721-20860-10430-5215-2607-1303-651+325-162+81-40+20+10-5+2-1(+ 0)
    0x000705b8, // [506] 0.006004096677584 (003ea000)  4104192=-2670163-1335086-667544+333772+166886+83443-41721+20860+10430-5215+2607-1303-651-325-162-81+40+20+10-5-2-1(+ 1)
    0x003a3cb8, // [507] 0.006028065127394 (003ee000)  4120576=-2670163-1335086-667544+333772+166886+83443-41721+20860-10430-5215+2607+1303+651+325-162-81-40+20-10+5+2+1(+ -1)
    0x002beb38, // [508] 0.006052033577205 (003f2000)  4136960=-2670163-1335086-667544+333772+166886+83443-41721-20860+10430+5215-2607+1303-651+325+162+81+40+20-10+5-2+1(+ -1)
    0x0012ce38, // [509] 0.006076002027016 (003f6000)  4153344=-2670163-1335086-667544+333772+166886+83443-41721-20860-10430+5215+2607+1303-651-325+162+81-40+20-10-5+2-1(+ -1)
    0x00037038, // [510] 0.006099970476826 (003fa000)  4169728=-2670163-1335086-667544+333772+166886+83443-41721-20860-10430-5215-2607-1303+651+325+162-81+40+20-10-5-2-1(+ -1)
    0x003c55d8, // [511] 0.006123938926637 (003fe000)  4186112=-2670163-1335086-667544+333772+166886-83443+41721+20860+10430-5215+2607-1303+651-325+162-81-40-20+10+5+2+1(+ -1)
};

double cordicScale(int startShift, int endShift) {
    double s = 1.0;
    for (int i = startShift; i < endShift; i++) {
        s = s + s / (1LL << i);
    }
    qDebug("calc cordic scale [%d .. %d]: "
           "sum = %.9f  1/sum=%0.9f"
           "sqrt(sum)=%.9f  1/sqrt(sum)=%.09f",
           startShift, endShift, s, 1/s,
           sqrt(s), 1/sqrt(s));
    return 1/s;
}

void sinCosCORDIC2(uint32_t phase, int & outSin, int & outCos) {
    double refangle = phase * 2 * M_PI / (1ULL<<32);
    double refcos = cos(refangle);
    double refsin = sin(refangle);
    qDebug("sinCosCORDIC2( %.9f )  - expected x=%.9f y=%.9f", refangle, refcos, refsin);
    cordicScale(0, 31);
    cordicScale(1, 31);
    cordicScale(2, 31);
    cordicScale(3, 31);
    int startShift = 3;
    // start=0 : 0.607252935   4.768462054
    // start=1 : 0.858785336   2.384231027
    // start=2 : 0.960151195   1.589487351
    // start=3 : 0.989701199
    double len = 1; //0.607252935;
    double a = M_PI/3;
    double x = cos(a) * len;
    double y = sin(a) * len;
    a = refangle - a;

    for (int shift = startShift; shift < 32; shift++) {
        double angleStep = atan(1.0 / (1LL<<shift));
        int sign = a < 0 ? -1 : 1;
        double newx = x - sign*y/(1LL<<shift);
        double newy = y + sign*x/(1LL<<shift);
        qDebug("    [%d] x %.9f  y %.9f   a %.9f",
               shift, x, y, a);
        x = newx;
        y = newy;
        a = a - sign*angleStep;
    }
    qDebug("Result: x %.9f  y %.9f   a %.9f", x, y, a);
    qDebug("   expected x=%.9f y=%.9f    a= %.9f", refcos, refsin, refangle);
    qDebug("   ratio x=%.9f y=%.9f    a= %.9f", refcos/x, refsin/y, refangle-a);
    qDebug("Done");
}

void sinCosCORDIC(uint32_t phase, int & outSin, int & outCos) {
    double angle = phase * 2 * M_PI / (1ULL<<32);
    double refcos = cos(angle);
    double refsin = sin(angle);
    qDebug("sinCosCORDIC(%08x)  %.9f    expected results: sin = %.9f  cos=%9f", phase, angle, refsin, refcos);
    // top 2 bits of phase define quadrant - change sign of result and
    int quadrant = (phase >> (32-2)) & 3;
    // next 7 bitts - lookup 128-entry init x,y
    int phaseTop = (phase >> (32-2-7)) & 127;

    // next 9 bits - angle signs lookup table index
    int phaseBottom = (phase >> (32-2-7)) & 127;
    // keep 16 bits + 1 bit sign
    int x = CORDIC_SIN_COS_INIT_TABLE_128_0_PI2[phaseTop][0] >> 15;
    int y = CORDIC_SIN_COS_INIT_TABLE_128_0_PI2[phaseTop][1] >> 15;
    //x /= CORDIC_SIN_COS_K;
    //y /= CORDIC_SIN_COS_K;
    double step2angle = atan2(y, x);
    uint32_t step2angle32 = step2angle * (1LL<<31) / M_PI;
    double xf = (double)x /(1<<16);
    double yf = (double)y /(1<<16);
    double step2length = sqrt((double)x*x + (double)y*y);
    double step2lengthf = sqrt((double)x*x + (double)y*y);
    int middleAngle = (phase & 0x3F800000)|0x00400000;
    int angleDelta = phase-middleAngle;
    qDebug("angle to rotate = %d %08x", angleDelta, angleDelta);
    qDebug("length=%.9f  lengthf=%.9f", step2length, step2lengthf);
    qDebug("Step 2: starting angle=%.9f (%08x) "
           " x=%d (%.9f) y=%d (%.9f) "
           "  cos=%.9f sin=%.9f "
           " scaled    cos=%.9f sin=%.9f  "
           " length=%.5f %.5f",
           step2angle, step2angle32,
           x, xf,
           y, yf,
           cos(step2angle), sin(step2angle),
           cos(step2angle)*CORDIC_SIN_COS_K, sin(step2angle)*CORDIC_SIN_COS_K,
           step2length, step2length/65536.0
           );
    int signMask = CORDIC_SIN_COS_SMALL_ANGLE_SIGN_MASK_8[phaseBottom];
    for (int p = 8; p < 8+10; p++) {
        //xf = (double)x /(1<<16);
        //yf = (double)y /(1<<16);
        int sign = (signMask & 1) ? 1 : -1;
        int sign2 = (angleDelta > 0) ? 1 : -1;
        int angleStep = CORDIC_ATAN_TABLE_INT32[p];
        int shift = p;
        qDebug("[%d]\tsign=%d\t sign2=%d\t "
               " angleDelta=%d angleStep=%d "
               "   x=%d (%.9f)\ty=%d (%.9f)\t x>>p=%d\t y>>p=%d"
               " \t atanxy=%08x",
               p, sign, sign2,
               angleDelta, angleStep,
               x, xf,
               y, yf,
               x>>shift, y>>shift,
               (uint32_t)(atan2(yf, xf)*(1LL<<31)/M_PI)
               );

        int newx = x - sign2*(y >> shift);
        int newy = y + sign2*(x >> shift);
        double newxf = xf - sign2*yf / (1<<shift);
        double newyf = yf + sign2*xf / (1<<shift);

        uint32_t angleBefore32 = (uint32_t)(atan2(yf, xf)*(1LL<<31)/M_PI);
        uint32_t angleAfter32 = (uint32_t)(atan2(yf, xf)*(1LL<<31)/M_PI);
        qDebug("    angle before: %08x after: %08x");

        x = newx;
        y = newy;
        xf = newxf;
        yf = newyf;

        angleDelta = angleDelta - sign2*angleStep;

        signMask >>= 1;
    }
    xf = (double)x / (1<<16);
    yf = (double)y / (1<<16);
    x >>= 1;
    y >>= 1;
    double aoutangle = atan2(y, x);
    uint32_t aoutangle32 = aoutangle * (1LL<<31) / M_PI;
    qDebug("Results: \tcos=%d\t%.9f\tsin=%d\t%.9f  \t angle=%.9f   %08x   rotated by %08x %d", x, xf, y, yf, aoutangle, aoutangle32, (aoutangle32 - middleAngle), (aoutangle32 - middleAngle));
    outSin = y;
    outCos = x;
}

void testCORDIC() {
    qDebug("CORDIC tests");

    double twoPowerMinusN = 1.0;
    double atanRadians = 0;
    double angleScale1 = 0;

    double zi = 0.2;
    int    zi32 = (int)(zi * (1LL<<32));
    double x = CORDIC_SIN_COS_K; //1.0; //0.607252; //1.0;
    double y = 0.0;
    int x32 = (int)(x * (1LL<<32));
    int y32 = 0;
    qDebug("CORDIC: table of angles");
    for (int i = 0; i < 48; i++) {
        atanRadians = atan(twoPowerMinusN);
        angleScale1 = atanRadians / M_PI / 2.0;
        int64_t angle32bit = (int64_t)(angleScale1 * (1LL<<32));
        int di = zi < 0 ? -1 : 1;
        double astep = di * angleScale1;
        int astep32 = di * angle32bit;
        qDebug("[%d] \t%.9f \t%.9f \t%.15f \tangle \t%08x \t zi %.9f \t zi32 %8x \t x %.15f \t x32 %08x \t y %.15f \t y32 %08x",
                 i, twoPowerMinusN,
                            atanRadians, angleScale1,
                                                 angle32bit, zi, zi32,
                                                                          x, x32, y, y32
               );
        zi = zi - astep;
        zi32 = zi32 - astep32;
        double newx = x - di * twoPowerMinusN * y;
        double newy = y + di * twoPowerMinusN * x;
        int newx32 = x32 - (di * y32) >> i;
        int newy32 = y32 + (di * x32) >> i;
        x = newx;
        y = newy;
        x32 = newx32;
        y32 = newy32;
        //zi = zi - astep;
        twoPowerMinusN = twoPowerMinusN / 2;
    }
    double len = sqrt(x*x + y*y);
    double k = 1.0 / len;
    qDebug("Vector length = %.20f  scale k = %.20f", len, k);

    twoPowerMinusN = 1.0;
    double atanPowerOf2Table[48];
    int atanIntTable32[48];
    for (int i = 0; i < 48; i++) {
        double angle = atan(twoPowerMinusN);
        atanPowerOf2Table[i] = angle;
        double angle32f = (angle * (1LL<<31) / M_PI);
        atanIntTable32[i] = (int)angle32f;
        twoPowerMinusN = twoPowerMinusN / 2;
        qDebug("    0x%08x, // [%d] %.15f", atanIntTable32[i], i, atanPowerOf2Table[i]);
    }

    qDebug("Angle range 0..PI/2 divided into 128 parts");
    for (int i = 0; i < 128; i++) {
        double a = (i + 0.5) / 128 * M_PI / 2;
        double a32f = a * (1LL<<31) / M_PI;
        int a32 = (i << 23) + (1 << 22);

        // sin and cos scaled for CORDIC
        double x = cos(a); // * CORDIC_SIN_COS_K;
        double y = sin(a); // * CORDIC_SIN_COS_K;
        int64_t x31 = (int64_t)(x * (1LL<<31));
        int64_t y31 = (int64_t)(y * (1LL<<31));

//        qDebug("[%d] a=%.9f a32f=%9f a32=\t%d \t%08x\tx=%.15f\ty=%.15f\t%08x\t%08x",
//               i, a, a32f, a32, a32,
//               x, y, x31, y31);
        qDebug("    {0x%08x, 0x%08x}, // [%d] middle angle=%.15f (%08x)", x31, y31, i, a, a32);
    }

    // M_PI/2/128, M_PI/2/128/2
    double fullStepAngle = M_PI/2/128;
    double halfStepAngle = M_PI/2/128/2;
    qDebug("For PI/2 divided into 128 parts, single area angle is %.15f, half is %.15f", fullStepAngle, halfStepAngle);
    // each piece is +/- 0.006135923, so CORDIC should start
    //   with entry [8] (2^-8), +-0.00390623 +-0.001953123 +- 0.000976562 ...
    // To have 16bits of phase per PI/2 range, we need to add 512 more items (9 bits)
    for (int i = 0; i < 512; i++) {
        // current substep middle angle offset from middle of step
        double a = (i + 0.5) * fullStepAngle / 512 - halfStepAngle;
        int a32 = (int)(a * (1LL << 31) / M_PI);
        // CORDIC_ATAN_TABLE_INT32
        //        0x0028be53, // [8] 0.003906230131967
        //        0x00145f2e, // [9] 0.001953122516479
        //        0x000a2f98, // [10] 0.000976562189559
        int angleTablePos = 8;
        int currentAngle = a32;
        // angleSignMask bit 0 is sign to apply [8] entry
        // angleSignMask bit 1 is sign to apply [9] entry and so on...
        int angleSignMask = 0;
        QString signString;
        for (int j = 0; j+8 < 30; j++) {
            if (currentAngle < 0) {
                angleSignMask |= (1<<j);
                currentAngle += CORDIC_ATAN_TABLE_INT32[angleTablePos];
                signString = signString + "+" + (QString("%1").arg(CORDIC_ATAN_TABLE_INT32[angleTablePos]));
            } else {
                currentAngle -= CORDIC_ATAN_TABLE_INT32[angleTablePos];
                signString = signString + "-" + (QString("%1").arg(CORDIC_ATAN_TABLE_INT32[angleTablePos]));
            }
            angleTablePos++;
        }
        qDebug("    0x%08x, // [%d] %.15f (%08x)  %d=%s(+ %d)", angleSignMask, i, a, a32, a32, signString.toLocal8Bit().data(), currentAngle);
    }

    int outSin;
    int outCos;
    double samplePhase = 0.2;
    uint32_t samplePhase32 = ((uint32_t)(samplePhase * (1LL<<32)));
    //sinCosCORDIC(samplePhase32, outSin, outCos);
    sinCosCORDIC2(samplePhase32, outSin, outCos);

    qDebug("Tests finished");
}

#define DSP_DELAY_BUFFER_SIZE 32
struct DelayBuffer {
    int delayCycles;
    int values[DSP_DELAY_BUFFER_SIZE];
    int wrpos;
    DelayBuffer(int delay=6) : delayCycles(delay), wrpos(0) {
        for (int i = 0; i < DSP_DELAY_BUFFER_SIZE; i++)
            values[i] = 0;
    }
    int push(int v) {
        wrpos = (wrpos + 1) & (DSP_DELAY_BUFFER_SIZE-1);
        return peek();
    }
    int peek() {
        int rdpos = (wrpos - delayCycles + DSP_DELAY_BUFFER_SIZE) & (DSP_DELAY_BUFFER_SIZE-1);
        return values[rdpos];
    }
};

// find circle center by 4 points
void circleCenter(int &outx, int &outy, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
    int dx10 = x1 - x0;
    int dx21 = x2 - x1;
    int dx32 = x3 - x2;
    int dy10 = y1 - y0;
    int dy21 = y2 - y1;
    int dy32 = y3 - y2;
    int ddx20 = dx21 - dx10;
    int ddx31 = dx32 - dx21;
    int ddy20 = dy21 - dy10;
    int ddy31 = dy32 - dy21;
    int dddx = ddx31 - ddx20;
    int dddy = ddy31 - ddy20;
    int adx21 = dx21 > 0 ? dx21 : -dx21;
    int ady21 = dy21 > 0 ? dy21 : -dy21;
    int adddx = dddx > 0 ? dddx : -dddx;
    int adddy = dddy > 0 ? dddy : -dddy;
    int x = x1;
    int y = y1;
    int dx = ddx20;
    int dy = ddy20;
    int dm = (adx21 > ady21) ? adddx : adddy;
    int m = (adx21 > ady21) ? adx21 : ady21;

    // scaling
    while (dm < (m>>1)) {
        dx <<= 1;
        dy <<= 1;
        dm <<= 1;
    }

    for (int i = 0; i < 24; i++) {
        if (m > 0) {
            x += dx;
            y += dy;
            m -= dm;
        } else {
            x -= dx;
            y -= dy;
            m += dm;
        }
        dm >>= 1;
        dx >>= 1;
        dy >>= 1;
        if (dm == 0)
            break;
    }
    outx = x;
    outy = y;
}

// find circle center by 3 points
void circleCenter3(int &outx, int &outy, int x0, int y0, int x1, int y1, int x2, int y2) {
    // p10 = (x10,y10), p21 = (x21,y21) -- centers between points p0,p1 and p1,p2
    int x10 = (x0 + x1) / 2;
    int y10 = (y0 + y1) / 2;
    //int x21 = (x1 + x2) / 2;
    //int y21 = (y1 + y2) / 2;
    // vector from p10 to p21
    int dx20 = (x2-x0) / 2;
    int dy20 = (y2-y0) / 2;
    // vectors from p0 to p1 (dx10,dy10) and p1 to p2 (dx21,dy21)
    int dx10 = x1 - x0;
    int dx21 = x2 - x1;
    int dy10 = y1 - y0;
    int dy21 = y2 - y1;
    // vector from p10 and p21 to circle center - rotate 90 degrees CCW
    int cdx10 = -dy10;
    int cdy10 = dx10;
    int cdx21 = -dy21;
    int cdy21 = dx21;
    // vector from end points of p10 moved to center to p21 moved to center
    int ddx = cdx21 - cdx10;
    int ddy = cdy21 - cdy10;
    if (dx20 < 0) {
        dx20 = -dx20;
        ddx = -ddx;
    }
    if (dy20 < 0) {
        dy20 = -dy20;
        ddy = -ddy;
    }
    int m = (dy20 > dx20) ? dy20 : dx20;
    int dm = (dy20 > dx20) ? -ddy : -ddx;
    int x = x10;
    int y = y10;
    int dx = cdx10;
    int dy = cdy10;
    // scaling right
    while (dm > m) {
        dx >>= 1;
        dy >>= 1;
        dm >>= 1;
    }
    // scaling left
    while (dm < (m>>1)) {
        dx <<= 1;
        dy <<= 1;
        dm <<= 1;
    }

    for (int i = 0; i < 24; i++) {
        if (m > dm) {
            x += dx;
            y += dy;
            m -= dm;
        }/* else {
            x -= dx;
            y -= dy;
            m += dm;
        }*/
        dm >>= 1;
        dx >>= 1;
        dy >>= 1;
        if (dm == 0)
            break;
    }
    outx = x;
    outy = y;
}

// on input, there is a sequence of points on a circle.
// ouput - attempt
struct CenterCircleFilter {
    // optimal is 1/6 of circle = 1/12 of signal period.
    int pointDelay;
    // 0: x, 1: y
    int phase;
    DelayBuffer delay1;
    DelayBuffer delay2;
    DelayBuffer delay3;

    int p0;
    int p1;
    int p2;
    int p3;
    int d10;
    int d21;
    int d32;
    int dd20;
    int dd31;
    int ddd;
    int ad21;
    int addd;

    int prev_p0;
    int prev_p1;
    int prev_p2;
    int prev_p3;
    int prev_d10;
    int prev_d21;
    int prev_d32;
    int prev_dd20;
    int prev_dd31;
    int prev_ddd;
    int prev_ad21;
    int prev_addd;
public:
    CenterCircleFilter(int delay) : pointDelay(delay), phase(0), delay1(delay), delay2(delay), delay3(delay){
        p0 = 0;
        p1 = 0;
        p2 = 0;
        p3 = 0;
        d10 = 0;
        d21 = 0;
        d32 = 0;
        dd20 = 0;
        dd31 = 0;
        ddd = 0;
        ad21 = 0;
        addd = 0;

        prev_p0 = 0;
        prev_p1 = 0;
        prev_p2 = 0;
        prev_p3 = 0;
        prev_d10 = 0;
        prev_d21 = 0;
        prev_d32 = 0;
        prev_dd20 = 0;
        prev_dd31 = 0;
        prev_ddd = 0;
        prev_ad21 = 0;
        prev_addd = 0;
    }
    // one cycle, v is either x or y, interleaved for odd/even cycles
    // output is calculated circle center, x or y
    int tick(int v) {
        prev_p0 = p0;
        prev_p1 = p1;
        prev_p2 = p2;
        prev_p3 = p3;
        prev_d10 = d10;
        prev_d21 = d21;
        prev_d32 = d32;
        prev_dd20 = dd20;
        prev_dd31 = dd31;
        prev_ddd = ddd;
        prev_ad21 = ad21;
        prev_addd = addd;

        p0 = v;
        p1 = delay1.push(p0);
        p2 = delay2.push(p1);
        p3 = delay3.push(p2);
        d10 = p1 - p0;
        d21 = p2 - p1;
        d32 = p3 - p2;
        dd20 = d21 - d10;
        dd31 = d32 - d21;
        ddd = dd31 - dd20;
        ad21 = d21 > 0 ? d21 : -d21;
        addd = ddd > 0 ? ddd : -ddd;

        int pt = p1;
        int dpt = dd20;
        int dm = (ad21 > prev_ad21) ? addd : prev_addd;
        int m = (ad21 > prev_ad21) ? ad21 : prev_ad21;

        for (int i = 0; i < 24; i++) {
            if (m > 0) {
                pt += dpt;
                m -= dm;
            } else {
                pt -= dpt;
                m += dm;
            }
            dpt >>= 1;
            dm >>= 1;
        }

        phase = phase ? 0 : 1;
        return 0;
    }
};


