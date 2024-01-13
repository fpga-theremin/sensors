#ifndef SIMUTILS_H
#define SIMUTILS_H

#include "dsputils.h"
#include <stdint.h>
#include <QString>
#include <QStringList>
#include <memory>

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
    // 1: ADC samples with specified sample rate, N: ADC samples once per N cycles, linearly interpolating values missed cycles
    int adcInterpolation;
    int averagingPeriods;
    int edgeAccInterpolation;

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
                , sampleRate(40000000)
                , ncoPhaseBits(32)
                , ncoValueBits(13)         // actual table size is 1/4 (1024) and 12 bits
                , ncoSinTableSizeBits(12)  // actual table size is 1/4 (1024) and 12 bits
                , sensePhaseShift(-0.0765)
                , senseAmplitude(0.9)
                , adcBits(12)
                , adcInterpolation(1)
                , averagingPeriods(8)
                , edgeAccInterpolation(0)
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
        adcInterpolation = v.adcInterpolation;
        averagingPeriods = v.averagingPeriods;
        edgeAccInterpolation = v.edgeAccInterpolation;
        adcNoise = v.adcNoise;
        adcDCOffset = v.adcDCOffset;
        realFrequency = v.realFrequency;
        phaseIncrement = v.phaseIncrement;
        phaseModule = v.phaseModule;
        sinTableSize = v.sinTableSize;
        guard1 = v.guard1;
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
    void init(int bitFractionCount = 1) {
        k = bitFractionCount;
        clear();
    }
    QString headingString(int minBits = 8, int maxBits = 20);
    QString toString(int minBits = 8, int maxBits = 20);
    void incrementExactBitsCount(int exactBits) {
        exactBitsCounters[exactBits]++;
    }
    void clear();
    void updateStats();
    int bitFractionCount() const { return k; }
};

struct SimResultsLine {
    QString parameterValue;
    ExactBitStats bitStats;
    SimResultsLine() {
    }
    SimResultsLine(const SimResultsLine & v) {
        parameterValue = v.parameterValue;
        bitStats = v.bitStats;
    }
    SimResultsLine & operator =(const SimResultsLine & v) {
        parameterValue = v.parameterValue;
        bitStats = v.bitStats;
        return *this;
    }
};

struct SimResultsItem {
    QString name;
    QString description;
    Array<SimResultsLine> byParameter;
    QStringList text;
    SimResultsItem() {}
    SimResultsItem(const SimResultsItem& v) {
        name = v.name;
        description = v.description;
        byParameter = v.byParameter;
        text.append(v.text);
    }
    void operator = (const SimResultsItem& v) {
        name = v.name;
        description = v.description;
        byParameter = v.byParameter;
        text.clear();
        text.append(v.text);
    }
    void clear() {
        name = "";
        description = "";
        byParameter.clear();
        text.clear();
    }
    SimResultsLine & addLine() {
        SimResultsLine line;
        byParameter.add(line);
        return byParameter[byParameter.length()-1];
    }
};

struct SimResultsHolder {
    Array<SimResultsItem> byTest;
    QStringList text;
    SimResultsItem * addTest() {
        SimResultsItem item;
        byTest.add(item);
        return &byTest[byTest.length()-1];
    }
};

class ProgressListener {
public:
    virtual void onProgress(int currentStage, int totalStages, QString msg);
};

class SimParamMutator {
protected:

    SimParams params;
    int currentStage;
    int stageCount;
    QString heading;
    QString valueString;
    ProgressListener * progressListener;
    void setValue(int value) {
        valueString = QString(":%1").arg(value);
    }
public:
    SimParamMutator(SimParams * params, QString heading, int stages, ProgressListener * progressListener = nullptr)
        : params(*params),
          currentStage(0),
          stageCount(stages),
          heading(heading),
          progressListener(progressListener)
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
    void runTests(SimResultsItem & results, int freqVariations = 5, double freqStep = 1.0014325, int phaseVariations = 5, double phaseStep = 0.0143564, int bitFractionCount = 5);
};

class AveragingMutator : public SimParamMutator {
public:
    AveragingMutator(SimParams * params, int count = 9) : SimParamMutator(params, "AvgPeriods", count) {
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

class ADCInterpolationMutator : public SimParamMutator {
public:
    ADCInterpolationMutator(SimParams * params, int count = 4) : SimParamMutator(params, "ADCInterpolation", count) {
    }
    bool next() override {
        params.adcInterpolation = 1 + currentStage;
        setValue(params.adcInterpolation);
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

class PhaseBitsMutator : public SimParamMutator {
public:
    PhaseBitsMutator(SimParams * params, int count = 10) : SimParamMutator(params, "PhaseBits", count) {
    }
    bool next() override {
        params.ncoPhaseBits = 16 + currentStage;
        setValue(params.ncoPhaseBits);
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

class SimSuite : public ProgressListener {
protected:
    ProgressListener * progressListener;
    QString currentTest;
    int currentResultIndex;
    int totalResultsCount;
    Array<std::unique_ptr<SimParamMutator>> tests;
public:
    SimSuite(ProgressListener * progressListener = nullptr) : progressListener(progressListener), currentResultIndex(0), totalResultsCount(0) {
    }
    void addTest(SimParamMutator * test) {
        tests.add(std::unique_ptr<SimParamMutator>(test));
    }
    virtual void init() {}
    virtual void run() {}
    void onProgress(int currentStage, int totalStages, QString msg) override {
        currentResultIndex++;
        if (progressListener != nullptr) {
            progressListener->onProgress(currentResultIndex, totalResultsCount, currentTest + " " + msg);
        }
    }
};



struct Edge {
    int adcValue;
    int64_t mulAcc1;
    int64_t mulAcc2;
    Edge() : adcValue(0), mulAcc1(0), mulAcc2(0) {

    }
};

typedef Array<Edge> EdgeArray;

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
    int64_t senseMulAcc1[SP_SIM_MAX_SAMPLES + 1000];
    int64_t senseMulAcc2[SP_SIM_MAX_SAMPLES + 1000];

    EdgeArray edgeArray;

    Array<int> periodIndex;
    int avgMulBase1;
    int avgMulBase2;

    int guard4;

    //int64_t periodSumBase1[SP_SIM_MAX_SAMPLES + 1000];
    //int64_t periodSumBase2[SP_SIM_MAX_SAMPLES + 1000];
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

    int adcSensedValueForPhase(uint64_t phase);
    double adcExactSensedValueForPhase(uint64_t phase);
    int adcExactToQuantized(double value);

    void checkGuards();

};

void runSimTestSuite(SimParams * params, int freqVariations = 5, double freqStep = 1.0014325, int phaseVariations = 5, double phaseStep = 0.0143564, int bitFractionCount = 2);

#endif // SIMUTILS_H
