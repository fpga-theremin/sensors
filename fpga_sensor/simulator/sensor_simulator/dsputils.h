#ifndef DSPUTILS_H
#define DSPUTILS_H

#include <stdint.h>

// sine lookup table
class SinTable {
    int* table;
    int tableSizeBits;
    int tableSize;
    int valueBits;
    int phaseBits;
public:
    SinTable(int tableSizeBits = 10, int valueBits = 9, double scale = 1.0, int phaseBits=32);
    ~SinTable();
    int getTableSizeBits() const { return tableSizeBits; }
    int getTableSize() const { return tableSize; }
    int getValueBits() const { return valueBits; }
    void init(int tableSizeBits, int valueBits, double scale = 1.0, int phaseBits=32);

    // get sine table value from full table entry index, using only 1/4 of table
    int get(int fullTableIndex) const;
    // get sine table value from phase, using only 1/4 of table
    int getForPhase(uint64_t phase) const;
    int getForPhaseFull(uint32_t phase) const;
    // get table entry by index
    int operator [](int index);
};


#endif // DSPUTILS_H
