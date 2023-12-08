#ifndef SIMUTILS_H
#define SIMUTILS_H

#include <stdint.h>

int quantizeSigned(double value, int bits);
double quantizeDouble(double value, int bits);
double scaleDouble(double value, int bits);

#define SP_MAX_SIN_TABLE_SIZE (65536)
struct SimParams {
    double frequency;
    int sampleRate;
    // sin table size bits
    int ncoPhaseBits;
    int ncoValueBits;
    int ncoSinTableSizeBits;
    double sinTableSizePhaseCorrection;

    double sensePhaseShift;
    double senseAmplitude;

    int adcBits;

    // noise in adc output, in LSB (e.g. 0.5 means white noise 1/2 LSB)
    double adcNoise;
    // DC offset in adc output, in LSB (e.g. 0.5 means ADC returns + 1/2 LSB)
    double adcDCOffset;

    // recalculated
    double realFrequency;
    int64_t phaseIncrement;
    int64_t phaseModule; // 1 << ncoPhaseBits
    int sinTableSize;
    int * sinTable; // [SP_MAX_SIN_TABLE_SIZE];
    SimParams() : frequency(1012345)
                , sampleRate(100000000)
                , ncoPhaseBits(32)
                , ncoValueBits(8)
                , ncoSinTableSizeBits(10)
                , sensePhaseShift(-0.0765)
                , senseAmplitude(0.8)
                , adcBits(8)
                , adcNoise(0)
                , adcDCOffset(0)
                , sinTable(NULL)
    {
        // recalculate dependent parameters
        recalculate();
    }

    ~SimParams() {
        if (sinTable)
            delete[] sinTable;
    }
    void recalculate();
    int tableEntryForPhase(int64_t phase);
    // atan, phase corrected
    double phaseByAtan2(int64_t y, int64_t x);
    // compare phase with expected
    double phaseError(double phase);
    // number of exact bits in phase, from phase diff
    static int exactBits(double phaseDiff);
    // takes phase difference from expected value, and converts to nanoseconds
    double phaseErrorToNanoSeconds(double phaseErr);
};

#define SP_SIM_MAX_SAMPLES 10000
struct SimState {
    SimParams * params;
    // two sines shifted by 90 degrees
    int base1[SP_SIM_MAX_SAMPLES]; // normal (cos)
    int base2[SP_SIM_MAX_SAMPLES]; // delayed by 90 (sin)

    // signal received from ADC
    double senseExact[SP_SIM_MAX_SAMPLES];
    int sense[SP_SIM_MAX_SAMPLES];

    int64_t senseMulBase1[SP_SIM_MAX_SAMPLES];
    int64_t senseMulBase2[SP_SIM_MAX_SAMPLES];

    int periodIndex[SP_SIM_MAX_SAMPLES];
    int avgMulBase1;
    int avgMulBase2;

    int64_t periodSumBase1[SP_SIM_MAX_SAMPLES/10];
    int64_t periodSumBase2[SP_SIM_MAX_SAMPLES/10];
    int periodCount;

    // period-aligned sums
    int64_t alignedSumBase1;
    int64_t alignedSumBase2;
    double alignedSensePhaseShift;
    double alignedSensePhaseShiftDiff;
    int alignedSenseExactBits;

    void simulate(SimParams * params);
    int64_t sumForPeriodsBase1(int startHalfperiod, int halfPeriodCount);
    int64_t sumForPeriodsBase2(int startHalfperiod, int halfPeriodCount);
    double phaseForPeriods(int startHalfperiod, int halfPeriodCount);
};

#endif // SIMUTILS_H
