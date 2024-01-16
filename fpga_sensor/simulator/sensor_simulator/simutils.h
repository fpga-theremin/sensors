#ifndef SIMUTILS_H
#define SIMUTILS_H

#include "dsputils.h"
#include <stdint.h>
#include <QString>
#include <QStringList>
#include <memory>

#define SP_SIM_DEFAULT_NUM_SAMPLES 20000

enum SimParameter {
    SIM_PARAM_MIN = 0,
    SIM_PARAM_ADC_BITS = SIM_PARAM_MIN,
    SIM_PARAM_ADC_SAMPLE_RATE,
    SIM_PARAM_SIN_VALUE_BITS,
    SIM_PARAM_SIN_TABLE_SIZE_BITS,
    SIM_PARAM_PHASE_BITS,
    SIM_PARAM_AVG_PERIODS,
    SIM_PARAM_ADC_INTERPOLATION,
    SIM_PARAM_EDGE_SUBSAMPLING_BITS,
    SIM_PARAM_SENSE_AMPLITUDE,
    SIM_PARAM_SENSE_DC_OFFSET,
    SIM_PARAM_SENSE_NOISE,
    SIM_PARAM_MUL_DROP_BITS,
    SIM_PARAM_ACC_DROP_BITS,
    SIM_PARAM_LP_FILTER_SHIFT_BITS,
    SIM_PARAM_LP_FILTER_STAGES,
    SIM_PARAM_LP_FILTER_ENABLED,
    SIM_PARAM_MOVING_AVG_FILTER_ENABLED,
    SIM_PARAM_MAX = SIM_PARAM_MOVING_AVG_FILTER_ENABLED
};

#define MAX_LP_FILTER_STAGES 16
struct LowPassFilter {
    int stageCount;
    int shiftBits;
    int64_t state[MAX_LP_FILTER_STAGES+1];
    LowPassFilter(int stages = 2, int shiftBits = 8, int64_t initValue = 0) : stageCount(stages), shiftBits(shiftBits) {
        reset(initValue);
    }
    void reset(int64_t initValue) {
        for (int i = 0; i <= stageCount; i++)
            state[i] = initValue;
    }
    // one cycle of filter, gets one input value, updates state, returns output value
    int64_t tick(int64_t inValue) {
        if (stageCount && shiftBits) {
            for (int i = stageCount-1; i >= 0; i--) {
                int64_t in = (i>0) ? state[i-1] : (inValue << shiftBits);
                int64_t diff = in - state[i];
                state[i] += diff >> shiftBits;
            }
            return state[stageCount-1] >> shiftBits;
        } else {
            // pass through if filter is disabled
            return inValue;
        }
    }
    int stepResponseTime(int value, int limit);
};

#define SIM_PARAM_ALL ((1<<(SIM_PARAM_MAX+1))-1)


int quantizeSigned(double value, int bits);
double quantizeDouble(double value, int bits);
double scaleDouble(double value, int bits);

//#define SP_MAX_SIN_TABLE_SIZE (65536)
struct SimParams {
    // SIM_PARAM_ADC_SAMPLE_RATE
    int sampleRate;
    // SIM_PARAM_PHASE_BITS
    int ncoPhaseBits;
    // SIM_PARAM_SIN_VALUE_BITS - SIN/COS table resolution
    int ncoValueBits;
    // SIM_PARAM_SIN_TABLE_SIZE_BITS - SIN/COS table size is (1<<ncoSinTableSizeBits)
    int ncoSinTableSizeBits;

    // SIM_PARAM_SENSE_AMPLITUDE
    double senseAmplitude;

    // SIM_PARAM_ADC_BITS
    int adcBits;
    // SIM_PARAM_ADC_INTERPOLATION 1: ADC samples with specified sample rate, N: ADC samples once per N cycles, linearly interpolating values missed cycles
    int adcInterpolation;
    // SIM_PARAM_AVG_PERIODS
    int averagingPeriods;
    // SIM_PARAM_EDGE_SUBSAMPLING_BITS
    int edgeAccInterpolation;

    // SIM_PARAM_SENSE_NOISE  noise in adc output, in LSB (e.g. 0.5 means white noise 1/2 LSB)
    double adcNoise;
    // SIM_PARAM_SENSE_DC_OFFSET DC offset in adc output, in LSB (e.g. 0.5 means ADC returns + 1/2 LSB)
    double adcDCOffset;
    // SIM_PARAM_MUL_DROP_BITS
    int mulDropBits;
    // SIM_PARAM_ACC_DROP_BITS
    int accDropBits;
    // SIM_PARAM_LP_FILTER_SHIFT_BITS
    int lpFilterShiftBits;
    // SIM_PARAM_LP_FILTER_STAGES
    int lpFilterStages;

    // SIM_PARAM_LP_FILTER_ENABLED,
    int lpFilterEnabled;
    // SIM_PARAM_MOVING_AVG_FILTER_ENABLED,
    int movingAverageFilterEnabled;

    double frequency;
    // recalculated based on precision
    double realFrequency;
    int64_t phaseIncrement;
    int64_t phaseModule; // 1 << ncoPhaseBits
    int sinTableSize;
    double sinTableSizePhaseCorrection;
    double sensePhaseShift;


    int freqVariations;
    double freqStep;
    int phaseVariations;
    double phaseStep;
    int bitFractionCount;
    int simMaxSamples;
    int paramTypeMask;

    SinTable sinTable;

    SimParams() : sampleRate(40000000)
                , ncoPhaseBits(32)
                , ncoValueBits(13)         // actual table size is 1/4 (1024) and 12 bits
                , ncoSinTableSizeBits(12)  // actual table size is 1/4 (1024) and 12 bits
                , senseAmplitude(0.9)
                , adcBits(12)
                , adcInterpolation(1)
                , averagingPeriods(8)
                , edgeAccInterpolation(0)
                , adcNoise(0)
                , adcDCOffset(0)
                , mulDropBits(0)
                , accDropBits(0)
                , lpFilterShiftBits(8)
                , lpFilterStages(2)

                , lpFilterEnabled(1)
                , movingAverageFilterEnabled(1)

                , frequency(1012345)
                , sinTableSizePhaseCorrection(0)
                , sensePhaseShift(-0.0765)
                , freqVariations(5)
                , freqStep(0.0014325)
                , phaseVariations(5)
                , phaseStep(0.0143564)
                , bitFractionCount(2)
                , simMaxSamples(SP_SIM_DEFAULT_NUM_SAMPLES)
                , paramTypeMask(SIM_PARAM_ALL)
                , sinTable(ncoSinTableSizeBits, ncoValueBits)
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

        mulDropBits = v.mulDropBits;
        accDropBits = v.accDropBits;
        lpFilterShiftBits = v.lpFilterShiftBits;
        lpFilterStages = v.lpFilterStages;

        lpFilterEnabled = v.lpFilterEnabled;
        movingAverageFilterEnabled = v.movingAverageFilterEnabled;

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
        freqVariations = v.freqVariations;
        freqStep = v.freqStep;
        phaseVariations = v.phaseVariations;
        phaseStep = v.phaseStep;
        bitFractionCount = v.bitFractionCount;
        simMaxSamples = v.simMaxSamples;
        paramTypeMask = v.paramTypeMask;
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
    double getPercent(int index, int minBits = 8, int maxBits = 20);
    double getMaxPercent(int minBits = 8, int maxBits = 20);
    void incrementExactBitsCount(int exactBits) {
        exactBitsCounters[exactBits]++;
    }
    void clear();
    void updatePercents();
    int bitFractionCount() const { return k; }
    int getTotalCount() const { return totalCount; }
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
    SimParameter paramType;
    Array<SimResultsLine> byParameterValue;
    int originalValueIndex;
    int dataPoints;
    QStringList text;
    double maxPercent(int minBits = 8, int maxBits = 24);
    SimResultsItem() : originalValueIndex(0), dataPoints(0) {}
    SimResultsItem(const SimResultsItem& v) {
        name = v.name;
        description = v.description;
        byParameterValue = v.byParameterValue;
        paramType = v.paramType;
        originalValueIndex = v.originalValueIndex;
        dataPoints = v.dataPoints;
        text.append(v.text);
    }
    void operator = (const SimResultsItem& v) {
        name = v.name;
        description = v.description;
        byParameterValue = v.byParameterValue;
        paramType = v.paramType;
        originalValueIndex = v.originalValueIndex;
        dataPoints = v.dataPoints;
        text.clear();
        text.append(v.text);
    }
    void clear() {
        name = "";
        description = "";
        byParameterValue.clear();
        dataPoints = 0;
        text.clear();
    }
    SimResultsLine & addLine() {
        SimResultsLine line;
        byParameterValue.add(line);
        return byParameterValue[byParameterValue.length()-1];
    }
};

struct SimResultsHolder {
    Array<SimResultsItem> results;
    QStringList text;
    // get results by type, nullptr if not found
    SimResultsItem * byType(SimParameter paramType) {
        for (int i = 0; i < results.length(); i++) {
            if (results[i].paramType == paramType)
                return &results[i];
        }
        return nullptr;
    }
    SimResultsItem * addResult() {
        SimResultsItem item;
        results.add(item);
        return &results[results.length()-1];
    }
    void clear() {
        results.clear();
        text.clear();
    }
};

class ProgressListener {
public:
    virtual void onProgress(int currentStage, int totalStages, QString msg) = 0;
};

struct SimParameterMetadata {
protected:
    SimParameter param;
    QString name;
    Array<QString> valueLabels;
    Array<QVariant> values;
    int defaultValueIndex;
    SimParameterMetadata(SimParameter param, QString name, const int * intValues, int defaultValue, int multiplier = 1);
    SimParameterMetadata(SimParameter param, QString name, const double * doubleValues, double defaultValue);
public:
    SimParameter getType() const { return param; }
    QString getName() const { return name; }
    // possible parameter values for simulation
    int getValueCount() const { return values.length(); }
    QString getValueLabel(int valueIndex) const {
        assert(valueIndex >= 0 && valueIndex < values.length());
        return valueLabels[valueIndex];
    }
    QVariant getValue(int valueIndex) const {
        assert(valueIndex >= 0 && valueIndex < values.length());
        return values[valueIndex];
    }
    int getIndex(const SimParams * params) const;
    virtual int getInt(const SimParams * params) const = 0;
    virtual double getDouble(const SimParams * params) const = 0;
    virtual void setInt(SimParams * params, int value) const = 0;
    virtual void setDouble(SimParams * params, double value) const = 0;
    virtual void set(SimParams * params, QVariant value) const = 0;

    double getDouble(int index) const { return getValue(index).toDouble(); }
    int getValueInt(int index) const { return getValue(index).toInt(); }
    double getValueDouble(int index) const { return getValue(index).toDouble(); }
    QVariant getDefaultValue() const { return values[defaultValueIndex]; }
    int getDefaultValueInt() const { return values[defaultValueIndex].toInt(); }
    double getDefaultValueDouble() const { return values[defaultValueIndex].toDouble(); }
    int getDefaultValueIndex() const { return defaultValueIndex; }
    virtual void setParamByIndex(SimParams * params, int index) = 0;
    // get metadata for parameter type
    static const SimParameterMetadata * get(SimParameter simParameter);
    static void applyDefaults(SimParams * params);

};

class SimParamMutator {
protected:

    SimParams params;
    SimParameter paramType;
    const SimParameterMetadata * metadata;
    int currentStage;
    int stageCount;
    QString heading;
    QString valueString;
    QVariant value;
    ProgressListener * progressListener;
//    void setValue(int value) {
//        valueString = QString(":%1").arg(value);
//    }
    SimParamMutator(SimParams * params, SimParameter paramType, ProgressListener * progressListener = nullptr)
        : params(*params),
          paramType(paramType),
          metadata(SimParameterMetadata::get(paramType)),
          currentStage(0),
          stageCount(metadata->getValueCount()),
          heading(metadata->getName()),
          progressListener(progressListener)
    {

    }
public:
    // factory method
    static SimParamMutator * create(SimParams * params, SimParameter paramType);

    void setProgressListener(ProgressListener * listener) {
        this->progressListener = listener;
    }
    int getStageCount() {
        return this->stageCount;
    }
    int getCurrentStage() {
        return this->currentStage;
    }
    void setParams(SimParams * params) {
        this->params = *params;
    }
    QString toString() { return valueString; }
    QString headingString() { return heading; }
    virtual void reset() {
        currentStage = 0;
    }
    virtual bool next() {
        if (currentStage >= stageCount) {
            return false;
        }
        value = metadata->getValue(currentStage);
        metadata->set(&params, value);
        valueString = metadata->getValueLabel(currentStage);
        params.recalculate();
        currentStage++;
        return currentStage <= stageCount;
    }
    void runTests(SimResultsItem & results);
};


void collectSimulationStats(SimParams * newParams, ExactBitStats & stats);

class SimSuite : public ProgressListener {
protected:
    SimParams simParams;
    ProgressListener * progressListener;
    QString currentTest;
    int currentResultIndex;
    int totalResultsCount;
    std::vector<std::unique_ptr<SimParamMutator>> tests;
    SimResultsHolder results;
public:
    SimSuite(ProgressListener * progressListener = nullptr) : progressListener(progressListener), currentResultIndex(0), totalResultsCount(0)
    {
    }
    SimResultsHolder * cloneResults() {
        SimResultsHolder * res = new SimResultsHolder();
        *res = results;
        return res;
    }
    void setParams(SimParams * params) {
        simParams = *params;
    }
    virtual void addTests() = 0;
    void addTest(SimParamMutator * test) {
        tests.push_back(std::unique_ptr<SimParamMutator>(test));
    }
    void runTest(int index, SimResultsItem & results) {
        if (index >= 0 && index < tests.size()) {
            tests[index]->runTests(results);
        }
    }
    int testCount() {
        return (int)tests.size();
    }
    virtual void run();
    void onProgress(int currentStage, int totalStages, QString msg) override {
        currentResultIndex++;
        if (progressListener != nullptr) {
            progressListener->onProgress(currentResultIndex, totalResultsCount, currentTest + " " + msg);
        }
    }
};

class FullSimSuite : public SimSuite {
public:
    FullSimSuite(ProgressListener * progressListener = nullptr);
    void addTests() override;
};

struct Edge {
    int sampleIndex;
    int adcValue;
    int64_t mulAcc1;
    int64_t mulAcc2;
    Edge() : sampleIndex(0), adcValue(0), mulAcc1(0), mulAcc2(0) {

    }
};

typedef Array<Edge> EdgeArray;

//#define SP_SIM_MAX_SAMPLES 20000
struct SimState {

    SimParams * params;
    // two sines shifted by 90 degrees
    Array<int> base1; //[SP_SIM_MAX_SAMPLES + 1000]; // normal (cos)
    //int guard1;
    Array<int> base2; //[SP_SIM_MAX_SAMPLES + 1000]; // delayed by 90 (sin)
    //int guard2;

    // signal received from ADC
    Array<double>  senseExact; //[SP_SIM_MAX_SAMPLES + 1000];
    Array<int> sense; //[SP_SIM_MAX_SAMPLES + 1000];

    //int guard3;

    Array<int64_t> senseMulBase1; //[SP_SIM_MAX_SAMPLES + 1000];
    Array<int64_t> senseMulBase2; //[SP_SIM_MAX_SAMPLES + 1000];

    //================================================
    // moving average filter
    bool movingAvgEnabled;
    Array<int64_t> senseMulAcc1; //[SP_SIM_MAX_SAMPLES + 1000];
    Array<int64_t> senseMulAcc2; //[SP_SIM_MAX_SAMPLES + 1000];
    EdgeArray edgeArray;
    int movingAvgFirstSample;
    Array<int64_t> movingAvgOut1;
    Array<int64_t> movingAvgOut2;
    Array<int> periodIndex;

    bool lpFilterEnabled;
    int lpFilterFirstSample;
    Array<int64_t> lpFilterOut1;
    Array<int64_t> lpFilterOut2;


    int avgMulBase1;
    int avgMulBase2;

    //int guard4;

    //int64_t periodSumBase1[SP_SIM_MAX_SAMPLES + 1000];
    //int64_t periodSumBase2[SP_SIM_MAX_SAMPLES + 1000];
    int periodCount;

    //int guard5;

    // period-aligned sums
    int64_t alignedSumBase1;
    int64_t alignedSumBase2;
    double alignedSensePhaseShift;
    double alignedSensePhaseShiftDiff;
    int alignedSenseExactBits;

    //int guard6;

    void simulate(SimParams * params);
    int64_t sumForPeriodsBase1(int startHalfperiod, int halfPeriodCount);
    int64_t sumForPeriodsBase2(int startHalfperiod, int halfPeriodCount);
    double phaseForPeriods(int startHalfperiod, int halfPeriodCount);

    void collectStats(ExactBitStats & stats);

    int adcSensedValueForPhase(uint64_t phase);
    double adcExactSensedValueForPhase(uint64_t phase);
    int adcExactToQuantized(double value);

    // microseconds -- half of window length
    double getMovingAverageLatency();
    // microseconds -- time to reach 1/2 of step
    double getLpFilterLatency();
};

//void runSimTestSuite(SimParams * params);
void debugDumpParameterMetadata();

void atan2BitsTest();

#endif // SIMUTILS_H
