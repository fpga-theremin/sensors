#ifndef SIMUTILS_H
#define SIMUTILS_H

#include "dsputils.h"
#include <stdint.h>
#include <QString>
#include <QStringList>

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
    int averagingPeriods;

    // noise in adc output, in LSB (e.g. 0.5 means white noise 1/2 LSB)
    double adcNoise;
    // DC offset in adc output, in LSB (e.g. 0.5 means ADC returns + 1/2 LSB)
    double adcDCOffset;

    // recalculated
    double realFrequency;
    int64_t phaseIncrement;
    int64_t phaseModule; // 1 << ncoPhaseBits
    int sinTableSize;

    SinTable sinTable;
    //int * sinTable; // [SP_MAX_SIN_TABLE_SIZE];

    int guard1;
    SimParams() : frequency(1012345)
                , sampleRate(100000000)
                , ncoPhaseBits(32)
                , ncoValueBits(13)         // actual table size is 1/4 (1024) and 12 bits
                , ncoSinTableSizeBits(12)  // actual table size is 1/4 (1024) and 12 bits
                , sensePhaseShift(-0.0765)
                , senseAmplitude(0.9)
                , adcBits(10)
                , averagingPeriods(1)
                , adcNoise(0)
                , adcDCOffset(0)
                , sinTable(ncoSinTableSizeBits, ncoValueBits)
                , guard1(0x11111111)
    {
        // recalculate dependent parameters
        recalculate();
    }

    ~SimParams() {
    }
    void recalculate();
    int tableEntryForPhase(int64_t phase);
    // atan, phase corrected
    double phaseByAtan2(int64_t y, int64_t x);
    // compare phase with expected
    double phaseError(double phase);
    // number of exact bits in phase, from phase diff
    static int exactBits(double phaseDiff, int fractionCount = 1);
    // takes phase difference from expected value, and converts to nanoseconds
    double phaseErrorToNanoSeconds(double phaseErr);

    QString toString();

    SimParams(const SimParams & v) {
        *this = v;
    }
    SimParams & operator = (const SimParams & v) {
        frequency = v.frequency;
        sampleRate = v.sampleRate;
        ncoPhaseBits = v.ncoPhaseBits;
        ncoValueBits = v.ncoValueBits;
        ncoSinTableSizeBits = v.ncoSinTableSizeBits;
        sinTableSizePhaseCorrection = v.sinTableSizePhaseCorrection;

        sensePhaseShift = v.sensePhaseShift;
        senseAmplitude = v.senseAmplitude;

        adcBits = v.adcBits;
        averagingPeriods = v.averagingPeriods;
        adcNoise = v.adcNoise;
        adcDCOffset = v.adcDCOffset;
        realFrequency = v.realFrequency;
        phaseIncrement = v.phaseIncrement;
        phaseModule = v.phaseModule;
        sinTableSize = v.sinTableSize;
        guard1 = v.guard1;
        //sinTable = nullptr;
        recalculate();
        return *this;
    }
};

struct ExactBitStats {
private:
    int k;
    int exactBitsCounters[32*10];
    int totalCount;
    double exactBitsPercent[32*10];
    double exactBitsPercentLessOrEqual[32*10];
    double exactBitsPercentMoreOrEqual[32*10];
public:
    ExactBitStats(int k = 1) : k(k) { clear(); }
    QString headingString(int minBits = 10, int maxBits = 24);
    QString toString(int minBits = 10, int maxBits = 24);
    void incrementExactBitsCount(int exactBits) {
        exactBitsCounters[exactBits]++;
    }
    void clear();
    void updateStats();
    int bitFractionCount() const { return k; }
};

class SimParamMutator {
protected:
    SimParams params;
    int currentStage;
    int stageCount;
    QString heading;
    QString valueString;
    void setValue(int value) {
        valueString = QString(":%1").arg(value);
    }
public:
    SimParamMutator(SimParams * params, QString heading, int stages)
        : params(*params),
          currentStage(0),
          stageCount(stages),
          heading(heading)
    {

    }
    QString toString() { return valueString; }
    QString headingString() { return heading; }
    virtual void reset() {
        currentStage = 0;
    }
    virtual bool next() {
        params.recalculate();
        currentStage++;
        return currentStage <= stageCount;
    }
    void runTests(QStringList & results, int variations = 5, int bitFractionCount = 5);
};

class AveragingMutator : public SimParamMutator {
public:
    AveragingMutator(SimParams * params, int count = 5) : SimParamMutator(params, "AvgPeriods", count) {
    }
    bool next() override {
        params.averagingPeriods = 1 << currentStage;
        setValue(params.averagingPeriods);
        return SimParamMutator::next();
    }
};

class ADCBitsMutator : public SimParamMutator {
public:
    ADCBitsMutator(SimParams * params, int count = 6) : SimParamMutator(params, "ADCBits", count) {
    }
    bool next() override {
        params.adcBits = 8 + currentStage;
        setValue(params.adcBits);
        return SimParamMutator::next();
    }
};

class SinValueBitsMutator : public SimParamMutator {
public:
    SinValueBitsMutator(SimParams * params, int count = 8) : SimParamMutator(params, "SinValueBits", count) {
    }
    bool next() override {
        params.ncoValueBits = 8 + currentStage;
        setValue(params.ncoValueBits);
        return SimParamMutator::next();
    }
};

class SinTableSizeMutator : public SimParamMutator {
public:
    SinTableSizeMutator(SimParams * params, int count = 8) : SimParamMutator(params, "SinTableSize", count) {
    }
    bool next() override {
        params.ncoSinTableSizeBits = 8 + currentStage;
        setValue(params.ncoSinTableSizeBits);
        return SimParamMutator::next();
    }
};

class SampleRateMutator : public SimParamMutator {
public:
    SampleRateMutator(SimParams * params, int count = 9) : SimParamMutator(params, "SampleRate", count) {
    }
    bool next() override {
        params.sampleRate = 20000000 + currentStage * 20000000;
        setValue(params.sampleRate/1000000);
        return SimParamMutator::next();
    }
};

void collectSimulationStats(SimParams * newParams, int averagingHalfPeriods, int freqVariations, double freqK, int phaseVariations, double phaseK, ExactBitStats & stats);

#define SP_SIM_MAX_SAMPLES 10000
struct SimState {
    SimParams * params;
    // two sines shifted by 90 degrees
    int base1[SP_SIM_MAX_SAMPLES + 1000]; // normal (cos)
    int guard1;
    int base2[SP_SIM_MAX_SAMPLES + 1000]; // delayed by 90 (sin)
    int guard2;

    // signal received from ADC
    double senseExact[SP_SIM_MAX_SAMPLES + 1000];
    int sense[SP_SIM_MAX_SAMPLES + 1000];

    int guard3;

    int64_t senseMulBase1[SP_SIM_MAX_SAMPLES + 1000];
    int64_t senseMulBase2[SP_SIM_MAX_SAMPLES + 1000];

    int periodIndex[SP_SIM_MAX_SAMPLES + 1000];
    int avgMulBase1;
    int avgMulBase2;

    int guard4;

    int64_t periodSumBase1[SP_SIM_MAX_SAMPLES/3 + 1000];
    int64_t periodSumBase2[SP_SIM_MAX_SAMPLES/3 + 1000];
    int periodCount;

    int guard5;

    // period-aligned sums
    int64_t alignedSumBase1;
    int64_t alignedSumBase2;
    double alignedSensePhaseShift;
    double alignedSensePhaseShiftDiff;
    int alignedSenseExactBits;

    int guard6;

    void simulate(SimParams * params);
    int64_t sumForPeriodsBase1(int startHalfperiod, int halfPeriodCount);
    int64_t sumForPeriodsBase2(int startHalfperiod, int halfPeriodCount);
    double phaseForPeriods(int startHalfperiod, int halfPeriodCount);

    void checkGuards();

};

void runSimTestSuite(SimParams * params, int variations=10, int bitFractionCount = 2);

#endif // SIMUTILS_H
