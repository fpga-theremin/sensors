#ifndef DSPUTILS_H
#define DSPUTILS_H

#include <stdint.h>

// sine lookup table, with phase and value quantized according number of bits specified during initialization
// implementation may use 1/4 of table entries, with flipping and inverting, to get value for any phase
class SinTable {
    int* table;
    int tableSizeBits;
    int tableSize;
    int valueBits;
    int phaseBits;
public:
    SinTable(int tableSizeBits = 10, int valueBits = 9, double scale = 1.0, int phaseBits=32);
    ~SinTable();
    // returns table size bits, e.g. 10 for 1024 entries table
    int getTableSizeBits() const { return tableSizeBits; }
    // returns table size as number of table entries
    int getTableSize() const { return tableSize; }
    // returns value range in bits
    int getValueBits() const { return valueBits; }
    // initialize lookup table of desired table size and value range
    void init(int tableSizeBits, int valueBits, double scale = 1.0, int phaseBits=32);

    // get sine table value from full table entry index, using only 1/4 of table entries algorithm
    int get(int fullTableIndex) const;
    // get sine table value from phase, using only 1/4 of table
    int getForPhase(uint64_t phase) const;
    // get sine table value from phase, using full phase
    int getForPhaseFull(uint32_t phase) const;
    // get table entry by index
    int operator [](int index) const;

    // generate verilog source code with sine table
    bool generateVerilog(const char * filePath);
    // generate verilog source code for verification of sine table
    bool generateVerilogTestBench(const char * filePath);
    // generate verilog source code with sin & cos table
    bool generateSinCosVerilog(const char * filePath);
    // generate verilog source code with verificatoin of sin & cos table
    bool generateSinCosVerilogTestBench(const char * filePath);
};


#endif // DSPUTILS_H
