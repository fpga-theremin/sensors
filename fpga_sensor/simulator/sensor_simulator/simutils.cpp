#include "simutils.h"
#include <QtMath>
#include <QDebug>

int quantizeSigned(double value, int bits) {
    // 1.0 will become maxAmpliture, -1.0 -- -maxAmplitude
    int maxAmplitude = (1 << (bits-1)) - 1;
    double scaled = value * maxAmplitude;
    int rounded = (int)round(scaled);
    return rounded;
}

double quantizeDouble(double value, int bits) {
    // 1.0 will become maxAmpliture, -1.0 -- -maxAmplitude
    int maxAmplitude = (1 << (bits-1)) - 1;
    double scaled = value * maxAmplitude;
    int rounded = (int)round(scaled);
    return rounded / (double)maxAmplitude;
}

double scaleDouble(double value, int bits) {
    // 1.0 will become maxAmpliture, -1.0 -- -maxAmplitude
    int maxAmplitude = (1 << (bits-1)) - 1;
    double scaled = value * maxAmplitude;
    return scaled;
}

//#define DEBUG_SIN_TABLE

QString SimParams::toString() {
    QString res = QString("SIN:%1bit[%2] ADC:%3MHz/%4bit(%5) avg:%8p freq:%6MHz phase:%7")
            .arg(ncoValueBits)
            .arg(sinTableSize)
            .arg(sampleRate/1000000)
            .arg(adcBits)
            .arg(senseAmplitude, 0, 'g')
            .arg(frequency / 1000000.0, 0, 'g', 7)
            .arg(sensePhaseShift, 0, 'g', 7)
            .arg(averagingPeriods)
            ;
    return res;
}

void SimParams::recalculate() {
    //qDebug("SimParams::recalculate()");
//    qDebug("  source:");
//    qDebug("    frequency:           %.5f", frequency);
//    qDebug("    sampleRate:          %d", sampleRate);
//    qDebug("    ncoPhaseBits:        %d", ncoPhaseBits);
//    qDebug("    ncoValueBits:        %d", ncoValueBits);
//    qDebug("    ncoSinTableSizeBits: %d", ncoSinTableSizeBits);

    // frequency, sampleRate
    double phaseInc = frequency / sampleRate;
    phaseModule = ((int64_t)1) << ncoPhaseBits;
    phaseIncrement = (int64_t)round(phaseInc * phaseModule);
    realFrequency = sampleRate / ((double)phaseModule / (double)phaseIncrement);
    sinTableSize = 1 << ncoSinTableSizeBits;
    sinTable.init(ncoSinTableSizeBits, ncoValueBits, 1.0);
    // correction by half of sin table - calculated phase centered at center of table step, not in beginning
    sinTableSizePhaseCorrection = 0; //1.0 / sinTableSize / 2.0;

//    qDebug("  calculated:");
//    qDebug("    realFrequency:         %.5f", realFrequency);
//    qDebug("    phaseIncrement:        %lld", phaseIncrement);
//    qDebug("    phaseModule:           %lld", phaseModule);
//    qDebug("    samplesPerPeriod:      %lld", phaseModule / phaseIncrement);
//    qDebug("    sinTableSize:          %d", sinTableSize);
//    qDebug("    sinTblPhaseCorrection: %d", sinTableSizePhaseCorrection);

#ifdef DEBUG_SIN_TABLE
    qDebug("  sin table:");
#endif
//    for (int i = 0; i < sinTableSize; i++) {
//        double phase = i * 2*M_PI / sinTableSize;
//        double sinValue = sin(phase);
//        sinTable[i] = quantizeSigned(sinValue, ncoValueBits);
//#ifdef DEBUG_SIN_TABLE
//        qDebug("    sin[%d] \t%.5f \t%.5f \t%.5f \t%d \tdelta=%d", i, sinValue,
//               quantizeDouble(sinValue, ncoValueBits),
//               scaleDouble(sinValue, ncoValueBits),
//               sinTable[i],
//               i>0 ? (sinTable[i] - sinTable[i-1]) : 0
//               );
//#endif
//    }

    int64_t sinCosProdSum = 0;
    for (int i = 0; i < sinTableSize; i++) {
        int j = (i + sinTableSize / 4) & (sinTableSize-1);
        int sinValue = sinTable.get(i);
        int cosValue = sinTable.get(j);
        sinCosProdSum += (int64_t)sinValue * cosValue;
    }
    Q_ASSERT(sinCosProdSum == 0);
}

int SimParams::tableEntryForPhase(int64_t phase) {
    int shiftedPhase = (int)(phase >> (ncoPhaseBits-ncoSinTableSizeBits));
    int shiftedMasked = shiftedPhase & (sinTableSize - 1);
    return sinTable.get(shiftedMasked);
}

// atan, phase corrected
double SimParams::phaseByAtan2(int64_t y, int64_t x) {
    return - atan2(y, x) / M_PI / 2 - sinTableSizePhaseCorrection;
}

// compare phase with expected
double SimParams::phaseError(double angle) {
    double diff = angle - sensePhaseShift;
    if (diff < -0.5)
        diff = diff + 1.0;
    else if (diff > 0.5)
        diff = diff - 1.0;
    return diff;
}

// takes phase difference from expected value, and converts to nanos
double SimParams::phaseErrorToNanoSeconds(double phaseErr) {
    double nanosecondsPerPeriod = 1000000000 / realFrequency;
    double absError = phaseErr < 0 ? -phaseErr : phaseErr;
    double res = nanosecondsPerPeriod * absError;
    //res = round(res * 1000) / 1000;
    return res;
}

// number of exact bits in phase, from phase diff
int SimParams::exactBits(double phaseDiff, int fractionCount) {
    double v = phaseDiff >= 0 ? phaseDiff : -phaseDiff;
    double vlog2 = -log2(v);
    int bitCount = (int)floor(vlog2*fractionCount);
    if (bitCount >= 32 * fractionCount)
        bitCount = 32 * fractionCount - 1;
    return bitCount;
}

double SimState::adcExactSensedValueForPhase(uint64_t phase) {
    int adcScale = 1<<(params->adcBits - 1);
    double exactPhase = 2 * M_PI * phase / params->phaseModule;
    double exactSense = sin(exactPhase) * params->senseAmplitude;
    return exactSense * adcScale;
}

int SimState::adcExactToQuantized(double exactSense) {
    // apply ADC DC offset
    double senseWithDCOffset = exactSense + params->adcDCOffset;
    // apply ADC Noise
    double noise = 0;
    double senseWithNoise = senseWithDCOffset + noise;
    // quantization (rounding to integer)
    return (int)round(senseWithNoise);
}

int SimState::adcSensedValueForPhase(uint64_t phase) {
    double exactSense = adcExactSensedValueForPhase(phase);
    return adcExactToQuantized(exactSense);
}

void SimState::simulate(SimParams * newParams) {
    edgeArray.clear();
    periodIndex.clear();
    guard1 = 0x11111111;
    guard2 = 0x22222222;
    guard3 = 0x33333333;
    guard4 = 0x44444444;
    guard5 = 0x55555555;
    guard6 = 0x66666666;


    params = newParams;

    checkGuards();
    // phase for base1
    int64_t phase = 0;
    for (int i = 0; i < SP_SIM_MAX_SAMPLES; i++) {
        // phase for base2
        int64_t phase2 = phase - (params->phaseModule>>2);
        if (phase2 < 0)
            phase2 += params->phaseModule;
        base1[i] = params->tableEntryForPhase(phase);
        base2[i] = params->tableEntryForPhase(phase2);
        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

    checkGuards();

    int64_t senseShift = (int64_t)round(params->sensePhaseShift * params->phaseModule);
    senseShift &= (params->phaseModule - 1);
    //senseShift -= params->phaseIncrement / 2;
    phase = senseShift;
    //int adcScale = 1<<(params->adcBits - 1);
    int64_t acc1=0;
    int64_t acc2=0;
    for (int i = 0; i < SP_SIM_MAX_SAMPLES; i++) {
        //double exactPhase = 2 * M_PI * phase / params->phaseModule;
        double exactSense = adcExactSensedValueForPhase(phase);
        senseExact[i] = exactSense;
        // quantization (rounding to integer)
        sense[i] = adcExactToQuantized(exactSense);

        if (params->adcInterpolation > 1) {
            int frac = i % params->adcInterpolation;
            if (frac != 0) {
                // interpolation is required
                int64_t prev_phase = phase - params->phaseIncrement * frac;
                prev_phase = prev_phase & (params->phaseModule - 1);
                int64_t next_phase = prev_phase + params->phaseIncrement * params->adcInterpolation;
                next_phase = next_phase & (params->phaseModule - 1);
                int prev_value = adcSensedValueForPhase(prev_phase);
                int next_value = adcSensedValueForPhase(next_phase);
                int this_value = prev_value + (next_value - prev_value) * frac / params->adcInterpolation;
                sense[i] = adcExactToQuantized(this_value);
            }
        }

        // multiplied
        senseMulBase1[i] = sense[i] * (int64_t)base1[i];
        senseMulBase2[i] = sense[i] * (int64_t)base2[i];
        acc1 += senseMulBase1[i];
        acc2 += senseMulBase2[i];
        senseMulAcc1[i] = acc1;
        senseMulAcc2[i] = acc2;

        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

    for (int i = 0; i < SP_SIM_MAX_SAMPLES - 1; i++) {
        // number of half period
        periodIndex.add(edgeArray.length());
        // check if sign has been changed
        int a = sense[i];
        int b = sense[i+1];
        if ((a ^ b) < 0) {
            // edge detected: make interpolation
            int c = b;
            int64_t mulacc1a = senseMulAcc1[i];
            int64_t mulacc1b = senseMulAcc1[i+1];
            int64_t mulacc2a = senseMulAcc2[i];
            int64_t mulacc2b = senseMulAcc2[i+1];
            int64_t c1 = mulacc1b;
            int64_t c2 = mulacc2b;
            for (int bit = 0; bit < params->edgeAccInterpolation; bit++) {
                c = a + b;
                c1 = mulacc1a + mulacc1b;
                c2 = mulacc2a + mulacc2b;
                if ((c ^ a) < 0) {
                    // left half
                    a = a + a;
                    b = c;
                    mulacc1a = mulacc1a + mulacc1a;
                    mulacc1b = c1;
                    mulacc2a = mulacc2a + mulacc2a;
                    mulacc2b = c2;
                } else if ((c ^ b) < 0) {
                    // right half
                    a = c;
                    b = b + b;
                    mulacc1a = c1;
                    mulacc1b = mulacc1b + mulacc1b;
                    mulacc2a = c2;
                    mulacc2b = mulacc2b + mulacc2b;
                } else {
                    // scale
                    a = a + a;
                    b = b + b;
                    mulacc1a = mulacc1a + mulacc1a;
                    mulacc1b = mulacc1b + mulacc1b;
                    mulacc2a = mulacc2a + mulacc2a;
                    mulacc2b = mulacc2b + mulacc2b;
                }
            }
            // c, c1, c2 are ready, with bits count increased
            c = c >> params->edgeAccInterpolation;
            c1 = c1 >> params->edgeAccInterpolation;
            c2 = c2 >> params->edgeAccInterpolation;
            Edge edge;
            edge.adcValue = c;
            edge.mulAcc1 = c1;
            edge.mulAcc2 = c2;
            edgeArray.add(edge);
        }
    }
    assert(edgeArray.length() > 100);
    periodCount = edgeArray.length() - 1;


    checkGuards();

    //int avgMulBase1;
    //int avgMulBase2;

    int64_t sumMulBase1 = 0;
    int64_t sumMulBase2 = 0;
    for (int i = 0; i < SP_SIM_MAX_SAMPLES; i++) {
        sumMulBase1 += senseMulBase1[i];
        sumMulBase2 += senseMulBase2[i];
    }
    avgMulBase1 = (int)(sumMulBase1 / SP_SIM_MAX_SAMPLES);
    avgMulBase2 = (int)(sumMulBase2 / SP_SIM_MAX_SAMPLES);

    checkGuards();


//    for (int i = 0; i < SP_SIM_MAX_SAMPLES/10; i++) {
//        periodSumBase1[i] = 0;
//        periodSumBase2[i] = 0;
//    }

    checkGuards();

//    periodCount = 0;
//    periodIndex[0] = 0;
//    int64_t sum1 = 0;
//    int64_t sum2 = 0;
//    for (int i = 1; i < SP_SIM_MAX_SAMPLES; i++) {
//        if ( (sense[i] ^ sense[i-1])>>(params->adcBits - 1) ) {
//            // difference in sign bit
//            periodSumBase1[periodCount] = sum1;
//            periodSumBase2[periodCount] = sum2;
//            sum1 = 0;
//            sum2 = 0;
//            periodCount++;
//        }
//        sum1 += senseMulBase1[i];
//        sum2 += senseMulBase2[i];
//        periodIndex[i] = periodCount;
//    }

    checkGuards();

    // averaging aligned by even period frames
    int avgPeriodsCount = (periodCount - 1) & 0xffffffe;
    alignedSumBase1 = edgeArray[avgPeriodsCount].mulAcc1 - edgeArray[0].mulAcc1;
    alignedSumBase2 = edgeArray[avgPeriodsCount].mulAcc2 - edgeArray[0].mulAcc2;
//    for (int i = 1; i <= avgPeriodsCount; i++) {
//        alignedSumBase1 += periodSumBase1[i];
//        alignedSumBase2 += periodSumBase2[i];
//    }
    alignedSensePhaseShift = params->phaseByAtan2(alignedSumBase2, alignedSumBase1);

    alignedSensePhaseShiftDiff = params->phaseError(alignedSensePhaseShift);
    //qDebug("alignedSensePhaseShiftDiff = %.6f", alignedSensePhaseShiftDiff);
    alignedSenseExactBits = SimParams::exactBits(alignedSensePhaseShiftDiff);

    checkGuards();
}

void SimState::checkGuards() {
//    Q_ASSERT(params->sinTable[params->sinTableSize] == 0x12345678);

    Q_ASSERT(guard1 == 0x11111111);
    Q_ASSERT(guard2 == 0x22222222);
    Q_ASSERT(guard3 == 0x33333333);
    Q_ASSERT(guard4 == 0x44444444);
    Q_ASSERT(guard5 == 0x55555555);
    Q_ASSERT(guard6 == 0x66666666);
}
void ExactBitStats::clear() {
    for (int i = 0; i < 32*10; i++) {
        exactBitsCounters[i] = 0;
        exactBitsPercent[i] = 0;
        exactBitsPercentLessOrEqual[i] = 0;
        exactBitsPercentMoreOrEqual[i] = 0;
    }
    totalCount = 0;
}

QString ExactBitStats::headingString(int minBits, int maxBits) {
    QString res;
    for (int i = minBits*k; i <= maxBits*k; i++) {
        if (k <= 1)
            res += QString(":%1\t").arg(i);
        else
            res += QString(":%1\t").arg(i / (double)k, 0, 'g');
    }
    return res;
}

double ExactBitStats::getMaxPercent(int minBits, int maxBits) {
    double res = getPercent(minBits*k, minBits, maxBits);
    for (int i = minBits*k; i<=maxBits*k; i++) {
        double p = getPercent(i, minBits, maxBits);
        if (res < p)
            res = p;
    }
    return res;
}

double ExactBitStats::getPercent(int i, int minBits, int maxBits) {
    double v = exactBitsPercent[i] * k;
    if (i == minBits*k)
        v = exactBitsPercentLessOrEqual[i] * k;
    //if (i == maxBits*k)
    //    v = exactBitsPercentMoreOrEqual[i] * k;
    return v;
}

QString ExactBitStats::toString(int minBits, int maxBits) {
    QString res;
    for (int i = minBits*k; i <= maxBits*k; i++) {
        double v = exactBitsPercent[i];
        if (i == minBits*k)
            v = exactBitsPercentLessOrEqual[i];
        if (i == maxBits*k)
            v = exactBitsPercentMoreOrEqual[i];
        res += QString("%1\t").arg(v, 0, 'g', 5);
    }
    return res;
}

void ExactBitStats::updateStats() {
    totalCount = 0;
    for (int i = 0; i < 32*k; i++) {
        totalCount += exactBitsCounters[i];
    }
    for (int i = 0; i < 32*k; i++) {
        exactBitsPercent[i] = exactBitsCounters[i] * 100.0 / totalCount;
    }
    double sum = 0;
    for (int i = 0; i < 32*k; i++) {
        sum += exactBitsPercent[i];
        exactBitsPercentLessOrEqual[i] = sum;
    }
    sum = 0;
    for (int i = 31*k; i >= 0; i--) {
        sum += exactBitsPercent[i];
        exactBitsPercentMoreOrEqual[i] = sum;
    }
}

double SimResultsItem::maxPercent(int minBits, int maxBits) {
    double res = 0;
    for (int i = 0; i < byParameterValue.length(); i++) {
        double p = byParameterValue[i].bitStats.getMaxPercent(minBits, maxBits);
        if (res < p)
            res = p;
    }
    return res;
}

void SimParamMutator::runTests(SimResultsItem & results) {
    SimState * state = new SimState();
    QString testName = heading + " : " + params.toString();
    results.text.append(testName);
    results.name = heading;
    results.description = testName;
    results.paramType = paramType;
    const SimParameterMetadata * metadata = SimParameterMetadata::get(paramType);
    results.originalValueIndex = metadata->getIndex(&params);
    //qDebug("SimParamMutator::runTests  %s  originalParamValue=%f", heading.toLocal8Bit().data(), originalParamValue);
    //qDebug(testName.toLocal8Bit().data());
    results.text.append(QString());
    //qDebug("");

    ExactBitStats statsHead(params.bitFractionCount);
    QString headers = heading + "\t" + statsHead.headingString();
    results.text.append(headers);
    //qDebug(headers.toLocal8Bit().data());

    while (next()) {
        SimResultsLine & line = results.addLine();
        line.bitStats.init(params.bitFractionCount);
        line.parameterValue = valueString;

        if (progressListener) {
            progressListener->onProgress(currentStage, stageCount, valueString);
        }
        //ExactBitStats stats(bitFractionCount);
        collectSimulationStats(&params, line.bitStats);
        QString res = valueString + "\t" + line.bitStats.toString();
        results.text.append(res);
        results.dataPoints += line.bitStats.getTotalCount();
        //qDebug(res.toLocal8Bit().data());
    }

    //qDebug("dataPoints = %d", results.dataPoints);

    results.text.append(QString());
    //qDebug("");
    results.text.append(QString());
    //qDebug("");
    results.text.append(QString());
    //qDebug("");
    results.text.append(QString());
    //qDebug("");
    delete state;
}

void collectSimulationStats(SimParams * newParams, ExactBitStats & stats) {
    SimState * state = new SimState();
    SimParams params = *newParams;
    double frequency = params.frequency;
    double phase = params.sensePhaseShift;
    for (int n1 = -params.freqVariations/2; n1 <= params.freqVariations / 2; n1++) {
        params.frequency = frequency * (1.0 + params.freqStep * n1);
        for (int n2 = -params.phaseVariations/2; n2 <= params.phaseVariations / 2; n2++) {
            params.sensePhaseShift = phase + params.freqStep * n2;
            params.recalculate();
            state->simulate(&params);
            for (int i = 1; i < state->periodCount - params.averagingPeriods*2 -2; i++) {
                // bottom: avg for 1 period (2 halfperiods)
                double angle = state->phaseForPeriods(i, params.averagingPeriods*2);
                double err = params.phaseError(angle);
                int exactBits = SimParams::exactBits(err, stats.bitFractionCount());
                stats.incrementExactBitsCount(exactBits);
            }
        }
    }
    stats.updateStats();
    delete state;
}

int64_t SimState::sumForPeriodsBase1(int startHalfperiod, int halfPeriodCount) {
    return edgeArray[startHalfperiod + halfPeriodCount].mulAcc1 - edgeArray[startHalfperiod].mulAcc1;
}

int64_t SimState::sumForPeriodsBase2(int startHalfperiod, int halfPeriodCount) {
    return edgeArray[startHalfperiod + halfPeriodCount].mulAcc2 - edgeArray[startHalfperiod].mulAcc2;
}

double SimState::phaseForPeriods(int startHalfperiod, int halfPeriodCount) {
    int64_t sumBase1 = sumForPeriodsBase1(startHalfperiod, halfPeriodCount);
    int64_t sumBase2 = sumForPeriodsBase2(startHalfperiod, halfPeriodCount);
    double angle = params->phaseByAtan2(sumBase2, sumBase1); //- atan2(sum2, sum1) / M_PI / 2;
    return angle;
}

void SimSuite::run() {
    addTests();
    totalResultsCount = 0;
    currentResultIndex = 0;
    results.clear();

    for (int i = 0; i < tests.size(); i++) {
        tests[i]->setParams(&simParams);
        tests[i]->setProgressListener(this);
        totalResultsCount += tests[i]->getStageCount();
    }

    for (int i = 0; i < tests.size(); i++) {
        SimResultsItem * result = results.addResult();
        currentTest = tests[i]->headingString();
        tests[i]->runTests(*result);
        results.text.append(result->text);
    }
}

FullSimSuite::FullSimSuite(ProgressListener * progressListener) : SimSuite(progressListener) {
}

void FullSimSuite::addTests() {
    for (int i = SimParameter::SIM_PARAM_MIN; i <= SimParameter::SIM_PARAM_MAX; i++) {
        if (simParams.paramTypeMask & (1 << i)) {
            addTest(new SimParamMutator(&simParams, (SimParameter)i));
        }
    }
}


//void runSimTestSuite(SimParams * params) {
//    //QStringList results;
//    SimResultsHolder results;

//    SimResultsItem * testResults = results.addTest();
//    AveragingMutator avgTest(params);
//    avgTest.runTests(*testResults);
//    results.text.append(testResults->text);

//    testResults = results.addTest();
//    ADCBitsMutator adcBitsTest(params);
//    adcBitsTest.runTests(*testResults);
//    results.text.append(testResults->text);

////    ADCInterpolationMutator adcInterpolationTest(params);
////    adcInterpolationTest.runTests(results, variations, bitFractionCount);

//    testResults = results.addTest();
//    SinValueBitsMutator sinValueBitsTest(params);
//    sinValueBitsTest.runTests(*testResults);
//    results.text.append(testResults->text);

//    testResults = results.addTest();
//    SinTableSizeMutator sinTableSizeTest(params);
//    sinTableSizeTest.runTests(*testResults);
//    results.text.append(testResults->text);

//    testResults = results.addTest();
//    SampleRateMutator sampleRateTest(params);
//    sampleRateTest.runTests(*testResults);
//    results.text.append(testResults->text);

//    testResults = results.addTest();
//    PhaseBitsMutator phaseBitsTest(params);
//    phaseBitsTest.runTests(*testResults);
//    results.text.append(testResults->text);

//}

#define END_OF_LIST -1000000

static const int sampleRates[] = {3, 4, 10, 20, 25, 40, 65, 80, 100, 125, 200, END_OF_LIST};
static const int phaseBits[] = {24, 26, 28, 30, 32, 34, 36, END_OF_LIST};
static const int ncoValueBits[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, END_OF_LIST};
static const int sinTableBits[] = {/*7,*/ 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, /* 18,*/ END_OF_LIST};
static const int adcValueBits[] = {/*6, 7,*/ 8, 9, 10, 11, 12, 13, 14, /*16,*/ END_OF_LIST};
static const int adcInterpolationRate[] = {1, 2, 3, 4, END_OF_LIST};
static const double adcNoise[] = {0.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, END_OF_LIST};
static const double adcDCOffset[] = {-10.0, -5.0, -2.5, -1.0, -0.5, 0, 0.5, 1.0, 2.5, 5.0, 10.0, END_OF_LIST};
static const double senseAmplitude[] = {0.1, 0.25, 0.5, 0.8, 0.9, 0.95, 1.0, END_OF_LIST};
static const int adcAveragingPeriods[] = {1, 2, 4, 8, 16, 32, 64, 128, /*256,*/ END_OF_LIST};
static const int edgeAccInterpolation[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, END_OF_LIST};

struct SampleRateMetadata : public SimParameterMetadata {
public:
    SampleRateMetadata() : SimParameterMetadata(SIM_PARAM_ADC_SAMPLE_RATE, QString("SampleRate"), sampleRates, 40, 1000000) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->sampleRate = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->sampleRate; }
    double getDouble(const SimParams * params) const override { return params->sampleRate; }
    void setInt(SimParams * params, int value) const override { params->sampleRate = value; }
    void setDouble(SimParams * params, double value) const override { params->sampleRate = value; }
    void set(SimParams * params, QVariant value) const override { params->sampleRate = value.toInt(); }
};
SampleRateMetadata SAMPLE_RATE_PARAMETER_METADATA;

struct ADCBitsMetadata : public SimParameterMetadata {
public:
    ADCBitsMetadata() : SimParameterMetadata(SIM_PARAM_ADC_SAMPLE_RATE, QString("ADCBits"), adcValueBits, 12) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->adcBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->adcBits; }
    double getDouble(const SimParams * params) const override { return params->adcBits; }
    void setInt(SimParams * params, int value) const override { params->adcBits = value; }
    void setDouble(SimParams * params, double value) const override { params->adcBits = value; }
    void set(SimParams * params, QVariant value) const override { params->adcBits = value.toInt(); }
};
ADCBitsMetadata ADC_BITS_PARAMETER_METADATA;

struct AvgPeriodsMetadata : public SimParameterMetadata {
public:
    AvgPeriodsMetadata() : SimParameterMetadata(SIM_PARAM_AVG_PERIODS, QString("AvgPeriods"), adcAveragingPeriods, 8) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->averagingPeriods = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->averagingPeriods; }
    double getDouble(const SimParams * params) const override { return params->averagingPeriods; }
    void setInt(SimParams * params, int value) const override { params->averagingPeriods = value; }
    void setDouble(SimParams * params, double value) const override { params->averagingPeriods = value; }
    void set(SimParams * params, QVariant value) const override { params->averagingPeriods = value.toInt(); }
};
AvgPeriodsMetadata AVG_PERIODS_PARAMETER_METADATA;

struct AdcInterpolationMetadata : public SimParameterMetadata {
public:
    AdcInterpolationMetadata() : SimParameterMetadata(SIM_PARAM_ADC_INTERPOLATION, QString("ADCInterpolation"), adcInterpolationRate, 1) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->adcInterpolation = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->adcInterpolation; }
    double getDouble(const SimParams * params) const override { return params->adcInterpolation; }
    void setInt(SimParams * params, int value) const override { params->adcInterpolation = value; }
    void setDouble(SimParams * params, double value) const override { params->adcInterpolation = value; }
    void set(SimParams * params, QVariant value) const override { params->adcInterpolation = value.toInt(); }
};
AdcInterpolationMetadata ADC_INTERPOLATION_PARAMETER_METADATA;

struct SinValueBitsMetadata : public SimParameterMetadata {
public:
    SinValueBitsMetadata() : SimParameterMetadata(SIM_PARAM_SIN_VALUE_BITS, QString("SinValueBits"), ncoValueBits, 13) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->ncoValueBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->ncoValueBits; }
    double getDouble(const SimParams * params) const override { return params->ncoValueBits; }
    void setInt(SimParams * params, int value) const override { params->ncoValueBits = value; }
    void setDouble(SimParams * params, double value) const override { params->ncoValueBits = value; }
    void set(SimParams * params, QVariant value) const override { params->ncoValueBits = value.toInt(); }
};
SinValueBitsMetadata SIN_VALUE_BITS_PARAMETER_METADATA;

struct SinTableSizeBitsMetadata : public SimParameterMetadata {
public:
    SinTableSizeBitsMetadata() : SimParameterMetadata(SIM_PARAM_SIN_TABLE_SIZE_BITS, QString("SinTableSizeBits"), sinTableBits, 12) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->ncoSinTableSizeBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->ncoSinTableSizeBits; }
    double getDouble(const SimParams * params) const override { return params->ncoSinTableSizeBits; }
    void setInt(SimParams * params, int value) const override { params->ncoSinTableSizeBits = value; }
    void setDouble(SimParams * params, double value) const override { params->ncoSinTableSizeBits = value; }
    void set(SimParams * params, QVariant value) const override { params->ncoSinTableSizeBits = value.toInt(); }
};
SinTableSizeBitsMetadata SIN_TABLE_SIZE_BITS_PARAMETER_METADATA;

struct PhaseBitsMetadata : public SimParameterMetadata {
public:
    PhaseBitsMetadata() : SimParameterMetadata(SIM_PARAM_PHASE_BITS, QString("PhaseBits"), phaseBits, 32) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->ncoPhaseBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->ncoPhaseBits; }
    double getDouble(const SimParams * params) const override { return params->ncoPhaseBits; }
    void setInt(SimParams * params, int value) const override { params->ncoPhaseBits = value; }
    void setDouble(SimParams * params, double value) const override { params->ncoPhaseBits = value; }
    void set(SimParams * params, QVariant value) const override { params->ncoPhaseBits = value.toInt(); }
};
PhaseBitsMetadata PHASE_BITS_PARAMETER_METADATA;

struct EdgeSubsamplingBitsMetadata : public SimParameterMetadata {
public:
    EdgeSubsamplingBitsMetadata() : SimParameterMetadata(SIM_PARAM_EDGE_SUBSAMPLING_BITS, QString("EdgeInterpolationBits"), edgeAccInterpolation, 4) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->edgeAccInterpolation = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->edgeAccInterpolation; }
    double getDouble(const SimParams * params) const override { return params->edgeAccInterpolation; }
    void setInt(SimParams * params, int value) const override { params->edgeAccInterpolation = value; }
    void setDouble(SimParams * params, double value) const override { params->edgeAccInterpolation = value; }
    void set(SimParams * params, QVariant value) const override { params->edgeAccInterpolation = value.toInt(); }
};
EdgeSubsamplingBitsMetadata EDGE_SUBSAMPLING_BITS_PARAMETER_METADATA;

struct SenseAmplitudeMetadata : public SimParameterMetadata {
public:
    SenseAmplitudeMetadata() : SimParameterMetadata(SIM_PARAM_SENSE_AMPLITUDE, QString("SenseAmplitude"), senseAmplitude, 0.9) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->senseAmplitude = getValue(index).toDouble();
    }
    int getInt(const SimParams * params) const override { return params->senseAmplitude; }
    double getDouble(const SimParams * params) const override { return params->senseAmplitude; }
    void setInt(SimParams * params, int value) const override { params->senseAmplitude = value; }
    void setDouble(SimParams * params, double value) const override { params->senseAmplitude = value; }
    void set(SimParams * params, QVariant value) const override { params->senseAmplitude = value.toDouble(); }
};
SenseAmplitudeMetadata SENSE_AMPLITUDE_PARAMETER_METADATA;

struct SenseDCOffsetMetadata : public SimParameterMetadata {
public:
    SenseDCOffsetMetadata() : SimParameterMetadata(SIM_PARAM_SENSE_DC_OFFSET, QString("SenseDCOffset"), adcDCOffset, 0) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->adcDCOffset = getValue(index).toDouble();
    }
    int getInt(const SimParams * params) const override { return params->adcDCOffset; }
    double getDouble(const SimParams * params) const override { return params->adcDCOffset; }
    void setInt(SimParams * params, int value) const override { params->adcDCOffset = value; }
    void setDouble(SimParams * params, double value) const override { params->adcDCOffset = value; }
    void set(SimParams * params, QVariant value) const override { params->adcDCOffset = value.toDouble(); }
};
SenseDCOffsetMetadata SENSE_DC_OFFSET_PARAMETER_METADATA;

struct SenseNoiseMetadata : public SimParameterMetadata {
public:
    SenseNoiseMetadata() : SimParameterMetadata(SIM_PARAM_SENSE_NOISE, QString("SenseNoise"), adcNoise, 0) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->adcNoise = getValue(index).toDouble();
    }
    int getInt(const SimParams * params) const override { return params->adcNoise; }
    double getDouble(const SimParams * params) const override { return params->adcNoise; }
    void setInt(SimParams * params, int value) const override { params->adcNoise = value; }
    void setDouble(SimParams * params, double value) const override { params->adcNoise = value; }
    void set(SimParams * params, QVariant value) const override { params->adcNoise = value.toDouble(); }
};
SenseNoiseMetadata SENSE_NOISE_PARAMETER_METADATA;

int SimParameterMetadata::getIndex(const SimParams * params) const {
    double v = getDouble(params);
    for (int i = 0; i < values.length(); i++) {
        double a = getDouble(i);
        if (v >= a-0.000001 && v <= a+0.0000001) {
            return i;
        }
    }
    return 0;
}

void SimParameterMetadata::applyDefaults(SimParams * params) {
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        SimParameter param = (SimParameter)i;
        const SimParameterMetadata * metadata = SimParameterMetadata::get(param);
        metadata->set(params, metadata->getDefaultValue());
    }
}

const SimParameterMetadata * SimParameterMetadata::get(SimParameter index) {
    switch(index) {
    case SIM_PARAM_ADC_BITS: return &ADC_BITS_PARAMETER_METADATA;
    case SIM_PARAM_ADC_SAMPLE_RATE: return &SAMPLE_RATE_PARAMETER_METADATA;
    case SIM_PARAM_SIN_VALUE_BITS: return &SIN_VALUE_BITS_PARAMETER_METADATA;
    case SIM_PARAM_SIN_TABLE_SIZE_BITS: return &SIN_TABLE_SIZE_BITS_PARAMETER_METADATA;
    case SIM_PARAM_PHASE_BITS: return &PHASE_BITS_PARAMETER_METADATA;
    case SIM_PARAM_AVG_PERIODS: return &AVG_PERIODS_PARAMETER_METADATA;
    case SIM_PARAM_ADC_INTERPOLATION: return &ADC_INTERPOLATION_PARAMETER_METADATA;
    case SIM_PARAM_EDGE_SUBSAMPLING_BITS: return &EDGE_SUBSAMPLING_BITS_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_AMPLITUDE: return &SENSE_AMPLITUDE_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_DC_OFFSET: return &SENSE_DC_OFFSET_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_NOISE: return &SENSE_NOISE_PARAMETER_METADATA;

    default:
        assert(false);
        return nullptr;
    }
}

void debugDumpParameterMetadata() {
    SimParams params;
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        SimParameter param = (SimParameter)i;
        const SimParameterMetadata * metadata = SimParameterMetadata::get(param);
        qDebug("%d: %s  defaultValueIndex=%d   defaultValueInt=%d  defaultValueDouble=%f   currentValueInt=%d  currentValueDouble=%f", i, metadata->getName().toLocal8Bit().data(), metadata->getDefaultValueIndex(), metadata->getDefaultValueInt(), metadata->getDefaultValueDouble(), metadata->getInt(&params), metadata->getDouble(&params) );
        for (int j = 0; j < metadata->getValueCount(); j++) {
            qDebug("    value[%d]: %s   intValue=%d doubleValue=%f", j, metadata->getValueLabel(j).toLocal8Bit().data(), metadata->getValue(j).toInt(), metadata->getValue(j).toDouble());
        }
        metadata->set(&params, 1.2345 + i*10000);
    }
    qDebug("After setting to i*10000");
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        SimParameter param = (SimParameter)i;
        const SimParameterMetadata * metadata = SimParameterMetadata::get(param);
        qDebug("%d: %s  defaultValueIndex=%d   defaultValueInt=%d  defaultValueDouble=%f   currentValueInt=%d  currentValueDouble=%f", i, metadata->getName().toLocal8Bit().data(), metadata->getDefaultValueIndex(), metadata->getDefaultValueInt(), metadata->getDefaultValueDouble(), metadata->getInt(&params), metadata->getDouble(&params) );
    }
    SimParameterMetadata::applyDefaults(&params);
    qDebug("After apply defaults");
    for (int i = SIM_PARAM_MIN; i <= SIM_PARAM_MAX; i++) {
        SimParameter param = (SimParameter)i;
        const SimParameterMetadata * metadata = SimParameterMetadata::get(param);
        qDebug("%d: %s  defaultValueIndex=%d   defaultValueInt=%d  defaultValueDouble=%f   currentValueInt=%d  currentValueDouble=%f", i, metadata->getName().toLocal8Bit().data(), metadata->getDefaultValueIndex(), metadata->getDefaultValueInt(), metadata->getDefaultValueDouble(), metadata->getInt(&params), metadata->getDouble(&params) );
    }
}

SimParameterMetadata::SimParameterMetadata(SimParameter param, QString name, const int * intValues, int defaultValue, int multiplier)
    : param(param), name(name), defaultValueIndex(0)
{
    for (int i = 0; intValues[i] != END_OF_LIST; i++) {
        valueLabels.add(QString("%1").arg(intValues[i]));
        int v = intValues[i] * multiplier;
        if (intValues[i] == defaultValue) {
            defaultValueIndex = i;
        }
        values.add(QVariant(v));
    }
}

SimParameterMetadata::SimParameterMetadata(SimParameter param, QString name, const double * doubleValues, double defaultValue)
    : param(param), name(name), defaultValueIndex(0)
{
    for (int i = 0; doubleValues[i] < END_OF_LIST-0.00001 || doubleValues[i] > END_OF_LIST+0.00001; i++) {
        valueLabels.add(QString("%1").arg(doubleValues[i]));
        double v = doubleValues[i];
        if (v >= defaultValue - 0.0000001 && v <= defaultValue + 0.0000001) {
            defaultValueIndex = i;
        }
        values.add(QVariant(v));
    }
}
