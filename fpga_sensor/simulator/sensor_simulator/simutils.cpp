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
    QString res = QString("SIN:%1bit[%2] ADC:%3MHz/%4bit(%5) freq:%6MHz phase:%7")
            .arg(ncoValueBits)
            .arg(sinTableSize)
            .arg(sampleRate/1000000)
            .arg(adcBits)
            .arg(senseAmplitude, 0, 'g')
            .arg(frequency / 1000000.0, 0, 'g', 7)
            .arg(sensePhaseShift, 0, 'g', 7)
            ;
    return res;
}

void SimParams::recalculate() {
    Q_ASSERT(guard1 == 0x11111111);
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
    if (sinTable) {
        Q_ASSERT(guard1 == 0x11111111);
        //Q_ASSERT(sinTable[sinTableSize] == 0x12345678);
    }
    sinTableSize = 1 << ncoSinTableSizeBits;
    if (sinTable) {
        delete [] sinTable;
    }
    sinTable = new int[sinTableSize + 1];
    for (int i = 0; i < sinTableSize; i++) {
        sinTable[i] = 0;
    }
    sinTable[sinTableSize] = 0x12345678;
    // correction by half of sin table - calculated phase centered at center of table step, not in beginning
    sinTableSizePhaseCorrection = 1.0 / sinTableSize / 2.0;

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
    for (int i = 0; i < sinTableSize; i++) {
        double phase = i * 2*M_PI / sinTableSize;
        double sinValue = sin(phase);
        sinTable[i] = quantizeSigned(sinValue, ncoValueBits);
#ifdef DEBUG_SIN_TABLE
        qDebug("    sin[%d] \t%.5f \t%.5f \t%.5f \t%d \tdelta=%d", i, sinValue,
               quantizeDouble(sinValue, ncoValueBits),
               scaleDouble(sinValue, ncoValueBits),
               sinTable[i],
               i>0 ? (sinTable[i] - sinTable[i-1]) : 0
               );
#endif
    }

    Q_ASSERT(guard1 == 0x11111111);
    Q_ASSERT(sinTable[sinTableSize] == 0x12345678);

    int64_t sinCosProdSum = 0;
    for (int i = 0; i < sinTableSize; i++) {
        int j = (i + sinTableSize / 4) & (sinTableSize-1);
        int sinValue = sinTable[i];
        int cosValue = sinTable[j];
        sinCosProdSum += (int64_t)sinValue * cosValue;
    }
    Q_ASSERT(sinCosProdSum == 0);
}

int SimParams::tableEntryForPhase(int64_t phase) {
    int shiftedPhase = (int)(phase >> (ncoPhaseBits-ncoSinTableSizeBits));
    int shiftedMasked = shiftedPhase & (sinTableSize - 1);
    return sinTable[shiftedMasked];
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
int SimParams::exactBits(double phaseDiff) {
    double v = phaseDiff >= 0 ? phaseDiff : -phaseDiff;
    int bitCount = 0;
    while (bitCount < 32) {
        v = v * 2;
        if (v >= 1.0)
            break;
        bitCount++;
    }
    return bitCount;
}

void SimState::simulate(SimParams * newParams) {
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
    int adcScale = 1<<(params->adcBits - 1);
    for (int i = 0; i < SP_SIM_MAX_SAMPLES; i++) {
        double exactPhase = 2 * M_PI * phase / params->phaseModule;
        double exactSense = sin(exactPhase) * params->senseAmplitude;
        exactSense *= adcScale;
        senseExact[i] = exactSense;
        // apply ADC DC offset
        double senseWithDCOffset = exactSense + params->adcDCOffset;
        // apply ADC Noise
        double noise = 0;
        double senseWithNoise = senseWithDCOffset + noise;
        // quantization (rounding to integer)
        sense[i] = (int)round(senseWithNoise);

        // multiplied
        senseMulBase1[i] = sense[i] * (int64_t)base1[i];
        senseMulBase2[i] = sense[i] * (int64_t)base2[i];

        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

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


    for (int i = 0; i < SP_SIM_MAX_SAMPLES/10; i++) {
        periodSumBase1[i] = 0;
        periodSumBase2[i] = 0;
    }

    checkGuards();

    periodCount = 0;
    periodIndex[0] = 0;
    int64_t sum1 = 0;
    int64_t sum2 = 0;
    for (int i = 1; i < SP_SIM_MAX_SAMPLES; i++) {
        if ( (sense[i] ^ sense[i-1])>>(params->adcBits - 1) ) {
            // difference in sign bit
            periodSumBase1[periodCount] = sum1;
            periodSumBase2[periodCount] = sum2;
            sum1 = 0;
            sum2 = 0;
            periodCount++;
        }
        sum1 += senseMulBase1[i];
        sum2 += senseMulBase2[i];
        periodIndex[i] = periodCount;
    }

    checkGuards();

    // averaging aligned by even period frames
    int avgPeriodsCount = (periodCount - 1) & 0xffffffe;
    alignedSumBase1 = 0;
    alignedSumBase2 = 0;
    for (int i = 1; i <= avgPeriodsCount; i++) {
        alignedSumBase1 += periodSumBase1[i];
        alignedSumBase2 += periodSumBase2[i];
    }
    alignedSensePhaseShift = params->phaseByAtan2(alignedSumBase2, alignedSumBase1);

    alignedSensePhaseShiftDiff = params->phaseError(alignedSensePhaseShift);
    //qDebug("alignedSensePhaseShiftDiff = %.6f", alignedSensePhaseShiftDiff);
    alignedSenseExactBits = SimParams::exactBits(alignedSensePhaseShiftDiff);

    checkGuards();
}

void SimState::checkGuards() {
    Q_ASSERT(params->sinTable[params->sinTableSize] == 0x12345678);

    Q_ASSERT(guard1 == 0x11111111);
    Q_ASSERT(guard2 == 0x22222222);
    Q_ASSERT(guard3 == 0x33333333);
    Q_ASSERT(guard4 == 0x44444444);
    Q_ASSERT(guard5 == 0x55555555);
    Q_ASSERT(guard6 == 0x66666666);
}
void ExactBitStats::clear() {
    for (int i = 0; i < 32; i++) {
        exactBitsCounters[i] = 0;
        exactBitsPercent[i] = 0;
        exactBitsPercentLessOrEqual[i] = 0;
        exactBitsPercentMoreOrEqual[i] = 0;
    }
    totalCount = 0;
}

QString ExactBitStats::headingString(int minBits, int maxBits) {
    QString res;
    for (int i = minBits; i <= maxBits; i++) {
        res += QString(":%1\t").arg(i);
    }
    return res;
}

QString ExactBitStats::toString(int minBits, int maxBits) {
    QString res;
    for (int i = minBits; i <= maxBits; i++) {
        double v = exactBitsPercent[i];
        if (i == minBits)
            v = exactBitsPercentLessOrEqual[i];
        if (i == maxBits)
            v = exactBitsPercentMoreOrEqual[i];
        res += QString("%1\t").arg(v, 0, 'g', 5);
    }
    return res;
}

void ExactBitStats::updateStats() {
    totalCount = 0;
    for (int i = 0; i < 32; i++) {
        totalCount += exactBitsCounters[i];
    }
    for (int i = 0; i < 32; i++) {
        exactBitsPercent[i] = exactBitsCounters[i] * 100.0 / totalCount;
    }
    double sum = 0;
    for (int i = 0; i < 32; i++) {
        sum += exactBitsPercent[i];
        exactBitsPercentLessOrEqual[i] = sum;
    }
    sum = 0;
    for (int i = 31; i >= 0; i--) {
        sum += exactBitsPercent[i];
        exactBitsPercentMoreOrEqual[i] = sum;
    }
}

void SimParamMutator::runTests(QStringList & results, int variations) {
    SimState * state = new SimState();
    QString testName = heading + " : " + params.toString();
    results.append(testName);
    qDebug(testName.toLocal8Bit().data());
    results.append(QString());
    qDebug("");

    QString headers = heading + "\t" + ExactBitStats::headingString();
    results.append(headers);
    qDebug(headers.toLocal8Bit().data());
    while (next()) {
        ExactBitStats stats;
        collectSimulationStats(&params, params.averagingPeriods*2, variations, 0.0012345, variations, 0.00156789, stats);
        QString res = valueString + "\t" + stats.toString();
        results.append(res);
        qDebug(res.toLocal8Bit().data());
    }

    results.append(QString());
    qDebug("");
    results.append(QString());
    qDebug("");
    delete state;
}

void collectSimulationStats(SimParams * newParams, int averagingHalfPeriods, int freqVariations, double freqK, int phaseVariations, double phaseK, ExactBitStats & stats) {
    SimState * state = new SimState();
    SimParams params = *newParams;
    double frequency = params.frequency;
    double phase = params.sensePhaseShift;
    for (int n1 = -freqVariations/2; n1 <= freqVariations; n1++) {
        params.frequency = frequency * (1.0 + freqK * n1);
        for (int n2 = -phaseVariations/2; n2 <= phaseVariations; n2++) {
            params.sensePhaseShift = phase + phaseK * n2;
            params.recalculate();
            state->simulate(&params);
            for (int i = 1; i < state->periodCount-2; i++) {
                // bottom: avg for 1 period (2 halfperiods)
                double angle = state->phaseForPeriods(i, averagingHalfPeriods);
                double err = params.phaseError(angle);
                int exactBits = SimParams::exactBits(err);
                if (exactBits > 31)
                    exactBits = 31;
                stats.exactBitsCounters[exactBits]++;
            }
        }
    }
    stats.updateStats();
    delete state;
}

int64_t SimState::sumForPeriodsBase1(int startHalfperiod, int halfPeriodCount) {
    int64_t res = 0;
    for (int i = 0; i < halfPeriodCount; i++) {
        res += periodSumBase1[startHalfperiod + i];
    }
    return res;
}

int64_t SimState::sumForPeriodsBase2(int startHalfperiod, int halfPeriodCount) {
    int64_t res = 0;
    for (int i = 0; i < halfPeriodCount; i++) {
        res += periodSumBase2[startHalfperiod + i];
    }
    return res;
}

double SimState::phaseForPeriods(int startHalfperiod, int halfPeriodCount) {
    int64_t sumBase1 = sumForPeriodsBase1(startHalfperiod, halfPeriodCount);
    int64_t sumBase2 = sumForPeriodsBase2(startHalfperiod, halfPeriodCount);
    double angle = params->phaseByAtan2(sumBase2, sumBase1); //- atan2(sum2, sum1) / M_PI / 2;
    return angle;
}


void runSimTestSuite(SimParams * params, int variations) {
    QStringList results;
    AveragingMutator avgTest(params);
    avgTest.runTests(results, variations);
    ADCBitsMutator adcBitsTest(params);
    adcBitsTest.runTests(results, variations);
    SinValueBitsMutator sinValueBitsTest(params);
    sinValueBitsTest.runTests(results, variations);
    SinTableSizeMutator sinTableSizeTest(params);
    sinTableSizeTest.runTests(results, variations);
    SampleRateMutator sampleRateTest(params);
    sampleRateTest.runTests(results, variations);
}
