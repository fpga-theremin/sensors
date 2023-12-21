#include "dsputils.h"
#include <QtMath>


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
    int errCount = 0;
    for (int i = 0; i < tableSize; i++) {
        double phase = (i + 0.5) * 2*M_PI / tableSize;
        double sinValue = sin(phase);
        double scaled = targetScale * sinValue;
        int rounded = (int)round(scaled);
        table[i] = rounded;
#if 0
        int fourBitValue = (rounded >> (valueBits-4)) & 0xf;
        bool err = (fourBitValue == 8)||(fourBitValue == 9)||(fourBitValue == 10)||(fourBitValue == 11)
                ||(fourBitValue == 5)||(fourBitValue == 6)||(fourBitValue == 7);
        if (err)
            errCount++;
        qDebug("    sin[%d] \t%.5f \t%.5f \t%d \tdelta=%d  %x   %s",
               i,
               sinValue,
               scaled,
               table[i],
               i>0 ? (table[i] - table[i-1]) : (table[i] - table[tableSize + i-1]),
               fourBitValue,
               (err ? "error" : "")
               );
#endif
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

int SinTable::operator [](int index) {
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

