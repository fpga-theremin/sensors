#include "simutils.h"
#include <QtMath>
#include <QDebug>
#include <QRandomGenerator64>

#define END_OF_LIST -1000000

static const int sampleRates[] = {3, 4, 10, 20, 25, 40, 65, 80, 100, 125, 200, END_OF_LIST};
static const int phaseBits[] = {24, 26, 28, 30, 32, 34, 36, END_OF_LIST};

static const int atanStepSamples[] = {0, 1, 2, 3, 4, 5, 6, 7, END_OF_LIST};
static const int lpFilterStages[] = {1, 2, 3, 4, 5, 6, 7, 8, END_OF_LIST};
static const int lpFilterShiftBits[] = {3, 4, 5, 6, 7, 8, 9, 10, END_OF_LIST};
static const int mulDropBits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, END_OF_LIST};
static const int accDropBits[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, END_OF_LIST};
static const int ncoValueBits[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, /* 18,*/ END_OF_LIST};
static const int sinTableBits[] = {/*7,*/ 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, END_OF_LIST};
static const int adcValueBits[] = {/*6, 7,*/ 8, 9, 10, 11, 12, 13, 14, /*16,*/ END_OF_LIST};
static const int adcInterpolationRate[] = {1, 2, 3, 4, END_OF_LIST};
static const int adcMovingAvg[] = {1, 3, 5, 7, 15, END_OF_LIST};
static const double adcNoise[] = {0.0, 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, END_OF_LIST};
static const double adcDCOffset[] = {-10.0, -5.0, -2.5, -1.0, -0.5, 0, 0.5, 1.0, 2.5, 5.0, 10.0, END_OF_LIST};
static const double senseAmplitude[] = {0.1, 0.25, 0.5, 0.8, 0.9, 0.95, 1.0, END_OF_LIST};
static const int adcAveragingPeriods[] = {1, 2, 4, 8, 16, 32, 64, 128, /*256,*/ END_OF_LIST};
static const int edgeAccInterpolation[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, END_OF_LIST};
static const int lpFilterEnabled[] = {0, 1, END_OF_LIST};

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

    sensePhaseShiftInt = sensePhaseShift * phaseModule;

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
int64_t SimParams::phaseErrorInt(int64_t angle) {
    int64_t diff = (angle - sensePhaseShiftInt) & (phaseModule-1);
    while (diff > (phaseModule>>1))
        diff -= (phaseModule);
    while (diff < -(phaseModule>>1))
        diff += (phaseModule);
    return diff;
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
int SimParams::exactBitsInt(int64_t phaseDiff, int fractionCount) const {
    phaseDiff &= (phaseModule-1);
    if (phaseDiff >= (phaseModule>>1))
        phaseDiff -= phaseModule;
    if (phaseDiff < 0)
        phaseDiff = -phaseDiff;
    if (phaseDiff < 10)
        return 32*fractionCount - 1;
    double vlog2 = this->ncoPhaseBits - log2(phaseDiff);
    int bitCount = (int)floor(vlog2*fractionCount);
    if (bitCount >= 32 * fractionCount)
        bitCount = 32 * fractionCount - 1;
    return bitCount;
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

int LowPassFilter::stepResponseTime(int value, int limit) {
    reset(0);
    int maxCycles = 1000000;
    for (int i = 0; i < maxCycles; i++) {
        int64_t v = tick(value);
        if (v > limit) {
            return i;
        }
    }
    return maxCycles;
}

double SimState::getMovingAverageLatency() {
    if (!movingAvgEnabled)
        return 0;
    double onePeriodMicros = 1000000.0 / params->frequency;
    double latencyMicros = onePeriodMicros * params->averagingPeriods / 2;
    return latencyMicros;
}

double SimState::getLpFilterLatency() {
    LowPassFilter filter(params->lpFilterStages, params->lpFilterShiftBits);
    int value = 100000000;
    int limit = value / 2;
    int responseCycles = filter.stepResponseTime(value, limit);
    double clockPeriod = 1000000.0 / params->sampleRate;
    return responseCycles * clockPeriod;
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

// ensure angle in range -180..180
double clampAngleDegrees(double angle) {
    while (angle >= 180.0)
        angle -= 360.0;
    while (angle < -180.0)
        angle += 360.0;
    return angle;
}

void SimState::simulate(SimParams * newParams) {
    edgeArray.clear();
    periodIndex.clear();

    params = newParams;

    base1.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000]; // normal (cos)
    base2.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000]; // delayed by 90 (sin)
    senseExact.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    sense.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    senseRaw.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    senseMulBase1.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    senseMulBase2.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];

    // movingAverage filter
    movingAvgEnabled = params->movingAverageFilterMode != 0;
    movingAvgFirstSample = 0;
    senseMulAcc1.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    senseMulAcc2.init(params->simMaxSamples, 0); //[SP_SIM_MAX_SAMPLES + 1000];
    movingAvgOut1.init(params->simMaxSamples, 0);
    movingAvgOut2.init(params->simMaxSamples, 0);
    samplePhase.init(params->simMaxSamples, 0);
    angle.init(params->simMaxSamples, 0);
    angleFiltered.init(params->simMaxSamples, 0);

    // LP IIR filter
    lpFilterEnabled = params->lpFilterEnabled != 0;
    lpFilterFirstSample = 0;
    lpFilterOut1.init(params->simMaxSamples, 0);
    lpFilterOut2.init(params->simMaxSamples, 0);

    // =======================================================
    // SIN, COS, phase
    // =======================================================
    // phase for base1
    int64_t phase = 0;
    for (int i = 0; i < params->simMaxSamples; i++) {
        // phase for base2
        int64_t phase2 = phase - (params->phaseModule>>2);
        if (phase2 < 0)
            phase2 += params->phaseModule;
        base1[i] = params->tableEntryForPhase(phase);
        base2[i] = params->tableEntryForPhase(phase2);
        samplePhase[i] = phase;
        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

    // =======================================================
    // Sense (ADC)
    // =======================================================
    int64_t senseShift = (int64_t)round(params->sensePhaseShift * params->phaseModule);
    senseShift &= (params->phaseModule - 1);
    //senseShift -= params->phaseIncrement / 2;
    phase = senseShift;
    for (int i = 0; i < params->simMaxSamples; i++) {
        //double exactPhase = 2 * M_PI * phase / params->phaseModule;
        double exactSense = adcExactSensedValueForPhase(phase);
        senseExact[i] = exactSense;
        // quantization (rounding to integer)
        senseRaw[i] = adcExactToQuantized(exactSense);
        sense[i] = senseRaw[i];

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

        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

    //=============================================================
    // ADC filtering
    //=============================================================
    if (params->adcMovingAvg>1) {
        for (int i = params->adcMovingAvg/2; i < params->simMaxSamples - params->adcMovingAvg/2; i++) {
            int sum = 0;
            for (int j = -params->adcMovingAvg/2; j<=params->adcMovingAvg/2; j++) {
                sum += senseRaw[i + j];
            }
            sense[i] = sum;
        }
    }

    //=============================================================
    // ADC * SIN, ADC * COS
    //=============================================================
    phase = 0;
    int64_t acc1=0;
    int64_t acc2=0;
    for (int i = 0; i < params->simMaxSamples; i++) {
        // multiplied
        senseMulBase1[i] = (sense[i] * (int64_t)base1[i]) >> params->mulDropBits;
        senseMulBase2[i] = (sense[i] * (int64_t)base2[i]) >> params->mulDropBits;

        acc1 += senseMulBase1[i];
        acc2 += senseMulBase2[i];
        senseMulAcc1[i] = acc1;
        senseMulAcc2[i] = acc2;

        // increment phase
        phase += params->phaseIncrement;
        phase = phase & (params->phaseModule - 1);
    }

    //int64_t
    // angle from 3 points
//    double lastAngle = 0;
//    double lastPhase = 0;
//    double sumAngle = 0;
//    double sumPhase = 0;
//    double sumShift = 0;
//    int anglePhaseCount = 0;
    if (params->atan2StepSamples > 0) {
        // ATAN2 first pipeline
        for (int i = params->atan2StepSamples; i < params->simMaxSamples - params->atan2StepSamples * 2; i++) {
            int64_t x0 = senseMulBase2[i - params->atan2StepSamples];
            int64_t y0 = senseMulBase1[i - params->atan2StepSamples];
            int64_t x1 = senseMulBase2[i];
            int64_t y1 = senseMulBase1[i];
            int64_t x2 = senseMulBase2[i + params->atan2StepSamples];
            int64_t y2 = senseMulBase1[i + params->atan2StepSamples];
            int64_t x3 = senseMulBase2[i + params->atan2StepSamples*2];
            int64_t y3 = senseMulBase1[i + params->atan2StepSamples*2];

            int outx = 0;
            int outy = 0;
            circleCenter(outx, outy,
                         x0, y0, x1, y1, x2, y2, x3, y3);
            double angleFromCenterDouble = -atan2(outx, outy);
            uint64_t angleFromCenter = (uint64_t)(angleFromCenterDouble * (params->phaseModule>>1) / M_PI);
            angle[i] = angleFromCenter & (params->phaseModule-1);
//            double angleError = params->phaseErrorInt(angle[i]) * 360.0 / params->phaseModule;

//            if (i < 1000) {
//                qDebug("    [%d]\t%d\t%d\t %.9f  err=\t%.9f", i,
//                       outx, outy,
//                       angleFromCenterDouble,
//                       angleError);
//            }

#if 0
            int64_t x1 = senseMulBase2[i - params->atan2StepSamples];
            int64_t y1 = senseMulBase1[i - params->atan2StepSamples];
            int64_t x2 = senseMulBase2[i + params->atan2StepSamples];
            int64_t y2 = senseMulBase1[i + params->atan2StepSamples];
            phase = samplePhase[i]; // - params->phaseIncrement / 2; // + params->phaseIncrement / 2;
            phase = phase * 2;
            int64_t dx = (x2 - x1);
            int64_t dy = (y2 - y1);
            double angleFromCenterDouble = atan2(dy, -dx);
    //        double angleFromCenterDoubleDegrees = angleFromCenterDouble * 180.0 / M_PI;
    //        double phaseDoubleDegrees = phase * 360.0 / params->phaseModule;
            uint64_t angleFromCenter = (uint64_t)(angleFromCenterDouble * (params->phaseModule>>1) / M_PI);
            angle[i] = (angleFromCenter - phase) & (params->phaseModule-1);
            //double angleDegrees = clampAngleDegrees(angleFromCenterDoubleDegrees - phaseDoubleDegrees);
    //        if (i > params->angleDetectionPointsStep) {
    //            double diffAngle = clampAngleDegrees(lastAngle - angleFromCenterDoubleDegrees);
    //            double diffPhase = clampAngleDegrees(lastPhase - phaseDoubleDegrees);
    //            sumAngle += diffAngle;
    //            sumPhase += diffPhase;
    //            sumShift += angleDegrees;
    //            anglePhaseCount++;
    //        }
    //        lastAngle = clampAngleDegrees(angleFromCenterDoubleDegrees);
    //        lastPhase = clampAngleDegrees(phaseDoubleDegrees);
    //        double angleError = params->phaseErrorInt(angle[i]) * 360.0 / params->phaseModule;
    //        if (i < 1000) {
    //            qDebug("  [%d]  x1=%d y1=%d  x2=%d y2=%d "
    //                   "  dx=%d dy=%d "

    //                   " angleC = %08x"
    //                   " phase*2 = %08x"
    //                   " angle = %08x  "

    //                   " exactBits=%.1f"
    //                   " expected=%08x "
    //                   " angleError=%.3f"
    //                   " angleF=%.5f  phase2F=%.5f  diff=%.5f",
    //                   (int)i,
    //                   (int)x1, (int)y1, (int)x2, (int)y2,

    //                   (int)dx, (int)dy,

    //                   (uint32_t)angleFromCenter,
    //                   (uint32_t)(phase),
    //                   (uint32_t)angle[i],

    //                   params->exactBitsInt(angle[i]-params->sensePhaseShiftInt, 10)/10.0,
    //                   (uint32_t)params->sensePhaseShiftInt,

    //                   angleError,
    //                   angleFromCenterDoubleDegrees,
    //                   phaseDoubleDegrees,
    //                   clampAngleDegrees(angleFromCenterDoubleDegrees-phaseDoubleDegrees));
    //        }
#endif
        }
        // fill begin and end with nearest angle value
        for (int i = 0 ; i < params->atan2StepSamples; i++) {
            angle[i] = angle[params->atan2StepSamples];
        }
        for (int i = params->simMaxSamples - params->atan2StepSamples; i < params->simMaxSamples; i++) {
            angle[i] = angle[params->simMaxSamples - params->atan2StepSamples - 1];
        }


        // LP filter
        LowPassFilter lpFilter(params->lpFilterStages, params->lpFilterShiftBits, angle[0]);
        for (int i = 0; i < params->simMaxSamples - 1; i++) {
            angleFiltered[i] = lpFilterEnabled ? lpFilter.tick(angle[i]) : angle[i];
        }
    }

//    qDebug("*** angle step=\t%.9f\t phase step=\t%.9f\t shift angle=\t%.9f\t",
//           sumAngle/anglePhaseCount,
//           sumPhase/anglePhaseCount,
//           sumShift/anglePhaseCount
//           );

    int64_t movingAvg1 = 0;
    int64_t movingAvg2 = 0;

    int movingAvgMaxDelay = 128; // max BRAM is 128x32
    double samplesPerPeriod = params->sampleRate/params->frequency;
    int movingAvgPeriods = (params->movingAverageFilterMode == MOVING_AVG_PER_SAMPLE_MAX_PERIODS)
            ? (int)(movingAvgMaxDelay / samplesPerPeriod)
            : 1;
    int movingAvgDelay = (int)(movingAvgPeriods * samplesPerPeriod);
    if (movingAvgPeriods == 0) {
        movingAvgDelay = movingAvgMaxDelay;
    } else if (movingAvgDelay > movingAvgMaxDelay) {
        if (movingAvgPeriods > 1) {
            movingAvgPeriods--;
            movingAvgDelay = (int)(movingAvgPeriods * samplesPerPeriod);
            if (movingAvgDelay > movingAvgMaxDelay) {
                movingAvgDelay = movingAvgMaxDelay;
            }
        } else {
            movingAvgDelay = movingAvgMaxDelay;
        }
    }

    int movingAvgHalfPeriods = params->averagingPeriods*2;
    if (movingAvgHalfPeriods < 1)
        movingAvgHalfPeriods = 1; // even if disabled, perform averaging for half period
    for (int i = 0; i < params->simMaxSamples - 1; i++) {
        // number of half period
        periodIndex.add(edgeArray.length());
        // check if sign has been changed
        int a = sense[i];
        int b = sense[i+1];
        if ((a ^ b) < 0) {
            // edge detected: make interpolation
            int c = b;
            int64_t mulacc1a = senseMulAcc1[i] >> params->accDropBits;
            int64_t mulacc1b = senseMulAcc1[i+1] >> params->accDropBits;
            int64_t mulacc2a = senseMulAcc2[i] >> params->accDropBits;
            int64_t mulacc2b = senseMulAcc2[i+1] >> params->accDropBits;
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
            edge.sampleIndex = i;
            edgeArray.add(edge);

            int lastEdge = edgeArray.length() - 1;
            int startEdge = lastEdge - movingAvgHalfPeriods;
            if (startEdge >= 0) {
                movingAvg1 = edgeArray[lastEdge].mulAcc1 - edgeArray[startEdge].mulAcc1;
                movingAvg2 = edgeArray[lastEdge].mulAcc2 - edgeArray[startEdge].mulAcc2;
                if (startEdge == 0) {
                    movingAvgFirstSample = i;
                }
            }
        }
        if (params->movingAverageFilterMode != MOVING_AVG_ZERO_CROSS_N_PERIODS) {
            if (i - movingAvgDelay >= 0) {
                movingAvgOut1[i] = (senseMulAcc1[i] - senseMulAcc1[i - movingAvgDelay]) >> params->accDropBits;
                movingAvgOut2[i] = (senseMulAcc2[i] - senseMulAcc2[i - movingAvgDelay]) >> params->accDropBits;
            } else {
                movingAvgOut1[i] = (senseMulAcc1[i] * movingAvgDelay)  >> params->accDropBits;
                movingAvgOut2[i] = (senseMulAcc2[i] * movingAvgDelay)  >> params->accDropBits;
            }
        } else {
            movingAvgOut1[i] = movingAvg1;
            movingAvgOut2[i] = movingAvg2;
        }
    }
    assert(edgeArray.length() > 100);
    periodCount = edgeArray.length() - 1;

    // LP filter
    LowPassFilter lpFilter1(params->lpFilterStages, params->lpFilterShiftBits, 0);
    LowPassFilter lpFilter2(params->lpFilterStages, params->lpFilterShiftBits, 0);
    Array<int64_t> * lpFilterInput1 = movingAvgEnabled ? &movingAvgOut1 : &senseMulBase1;
    Array<int64_t> * lpFilterInput2 = movingAvgEnabled ? &movingAvgOut2 : &senseMulBase2;
    for (int i = 0; i < params->simMaxSamples - 1; i++) {
        lpFilterOut1[i] = lpFilterEnabled ? lpFilter1.tick((*lpFilterInput1)[i]) : (*lpFilterInput1)[i];
        lpFilterOut2[i] = lpFilterEnabled ? lpFilter2.tick((*lpFilterInput2)[i]) : (*lpFilterInput2)[i];
    }
    lpFilterFirstSample = lpFilterEnabled ? ((1<<params->lpFilterShiftBits) * params->lpFilterStages) : movingAvgFirstSample;

    //int avgMulBase1;
    //int avgMulBase2;

    int64_t sumMulBase1 = 0;
    int64_t sumMulBase2 = 0;
    for (int i = 0; i < params->simMaxSamples; i++) {
        sumMulBase1 += senseMulBase1[i];
        sumMulBase2 += senseMulBase2[i];
    }
    avgMulBase1 = (int)(sumMulBase1 / params->simMaxSamples);
    avgMulBase2 = (int)(sumMulBase2 / params->simMaxSamples);



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
    double v = exactBitsPercent[i]; // * k;
    //if (i == minBits*k)
    //    v = exactBitsPercentLessOrEqual[i] * k;
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

void ExactBitStats::updatePercents() {
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

class ADCInterpolationMutator : public SimParamMutator {
protected:
    int originalSampleRate;
    //int originalADCInterpolation;
public:
    ADCInterpolationMutator(SimParams * params, SimParameter paramType) : SimParamMutator(params, paramType) {
        originalSampleRate = params->sampleRate;
        //originalADCInterpolation = params->adcInterpolation;
    }
    virtual bool next() {
        bool res = SimParamMutator::next();
        if (!res)
            return res;
        // increase sample rate for interpolation
        params.sampleRate = originalSampleRate * params.adcInterpolation;
        params.recalculate();
        return res;
    }
};

SimParamMutator * SimParamMutator::create(SimParams * params, SimParameter paramType) {
    if (paramType == SIM_PARAM_ADC_INTERPOLATION)
        return new ADCInterpolationMutator(params, paramType);
    return new SimParamMutator(params, paramType);
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

void SimState::collectStats(ExactBitStats & stats) {
    if (params->atan2StepSamples > 0) {
        int startSample = params->simMaxSamples * 3 / 4; //movingAvgFirstSample + lpFilterFirstSample*2 + 2000;
        int step = 1;
        int maxPrecision = 32*stats.bitFractionCount();
        for (int i = startSample; i < params->simMaxSamples; i+=step) {
            int64_t err = params->phaseErrorInt(angleFiltered[i]);
            int exactBits = params->exactBitsInt(err, stats.bitFractionCount()); // SimParams::exactBits(err, stats.bitFractionCount());
            if (exactBits >= maxPrecision)
                exactBits = maxPrecision - 1;
            if (exactBits < 0) {
                qDebug("Exact bits < 0: err=%d bits=%d", (int)err, exactBits);
                exactBits = 0;
            }
            stats.incrementExactBitsCount(exactBits);
        }
    } else {
        int startSample = movingAvgFirstSample + lpFilterFirstSample*2 + 2000;
    //    if (startSample < lpFilterFirstSample)
    //        startSample = lpFilterFirstSample;
        int step = 10;
        for (int i = startSample; i < params->simMaxSamples; i+=step) {
            int64_t sumBase1 = lpFilterOut1[i];
            int64_t sumBase2 = lpFilterOut2[i];
            double angle = params->phaseByAtan2(sumBase2, sumBase1); //- atan2(sum2, sum1) / M_PI / 2;
            double err = params->phaseError(angle);
            int exactBits = SimParams::exactBits(err, stats.bitFractionCount());
            stats.incrementExactBitsCount(exactBits);
        }
    }
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

            state->collectStats(stats);
#if 0
            for (int i = 1; i < state->periodCount - params.averagingPeriods*2 -2; i++) {
                // bottom: avg for 1 period (2 halfperiods)
                double angle = state->phaseForPeriods(i, params.averagingPeriods*2);
                double err = params.phaseError(angle);
                int exactBits = SimParams::exactBits(err, stats.bitFractionCount());
                stats.incrementExactBitsCount(exactBits);
            }
#endif
        }
    }
    stats.updatePercents();
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
            addTest(SimParamMutator::create(&simParams, (SimParameter)i));
        }
    }
}


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

struct AdcMovingAvgMetadata : public SimParameterMetadata {
public:
    AdcMovingAvgMetadata() : SimParameterMetadata(SIM_PARAM_ADC_MOVING_AVG, QString("ADC Moving Avg"), adcMovingAvg, 7) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->adcMovingAvg = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->adcMovingAvg; }
    double getDouble(const SimParams * params) const override { return params->adcMovingAvg; }
    void setInt(SimParams * params, int value) const override { params->adcMovingAvg = value; }
    void setDouble(SimParams * params, double value) const override { params->adcMovingAvg = value; }
    void set(SimParams * params, QVariant value) const override { params->adcMovingAvg = value.toInt(); }
};
AdcMovingAvgMetadata ADC_MOVING_AVG_PARAMETER_METADATA;

struct SinValueBitsMetadata : public SimParameterMetadata {
public:
    SinValueBitsMetadata() : SimParameterMetadata(SIM_PARAM_SIN_VALUE_BITS, QString("SinValueBits"), ncoValueBits, 16) {}
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
    SinTableSizeBitsMetadata() : SimParameterMetadata(SIM_PARAM_SIN_TABLE_SIZE_BITS, QString("SinTableSizeBits"), sinTableBits, 16) {}
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

struct Atan2StepSamplesMetadata : public SimParameterMetadata {
public:
    Atan2StepSamplesMetadata() : SimParameterMetadata(SIM_PARAM_ATAN2_STEP_SAMPLES, QString("AtanStep"), atanStepSamples, 7) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->atan2StepSamples = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->atan2StepSamples; }
    double getDouble(const SimParams * params) const override { return params->atan2StepSamples; }
    void setInt(SimParams * params, int value) const override { params->atan2StepSamples = value; }
    void setDouble(SimParams * params, double value) const override { params->atan2StepSamples = value; }
    void set(SimParams * params, QVariant value) const override { params->atan2StepSamples = value.toInt(); }
};
Atan2StepSamplesMetadata ATAN2_STEP_SAMPLES_PARAMETER_METADATA;

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

struct MulDropBitsMetadata : public SimParameterMetadata {
public:
    MulDropBitsMetadata() : SimParameterMetadata(SIM_PARAM_MUL_DROP_BITS, QString("MulDropBits"), mulDropBits, 0) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->mulDropBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->mulDropBits; }
    double getDouble(const SimParams * params) const override { return params->mulDropBits; }
    void setInt(SimParams * params, int value) const override { params->mulDropBits = value; }
    void setDouble(SimParams * params, double value) const override { params->mulDropBits = value; }
    void set(SimParams * params, QVariant value) const override { params->mulDropBits = value.toInt(); }
};
MulDropBitsMetadata MUL_DROP_BITS_PARAMETER_METADATA;

struct AccDropBitsMetadata : public SimParameterMetadata {
public:
    AccDropBitsMetadata() : SimParameterMetadata(SIM_PARAM_ACC_DROP_BITS, QString("AccDropBits"), accDropBits, 4) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->accDropBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->accDropBits; }
    double getDouble(const SimParams * params) const override { return params->accDropBits; }
    void setInt(SimParams * params, int value) const override { params->accDropBits = value; }
    void setDouble(SimParams * params, double value) const override { params->accDropBits = value; }
    void set(SimParams * params, QVariant value) const override { params->accDropBits = value.toInt(); }
};
AccDropBitsMetadata ACC_DROP_BITS_PARAMETER_METADATA;

struct LpFilterShiftBitsMetadata : public SimParameterMetadata {
public:
    LpFilterShiftBitsMetadata() : SimParameterMetadata(SIM_PARAM_LP_FILTER_SHIFT_BITS, QString("LpFltShiftBits"), lpFilterShiftBits, 8) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->lpFilterShiftBits = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->lpFilterShiftBits; }
    double getDouble(const SimParams * params) const override { return params->lpFilterShiftBits; }
    void setInt(SimParams * params, int value) const override { params->lpFilterShiftBits = value; }
    void setDouble(SimParams * params, double value) const override { params->lpFilterShiftBits = value; }
    void set(SimParams * params, QVariant value) const override { params->lpFilterShiftBits = value.toInt(); }
};
LpFilterShiftBitsMetadata LP_FILTER_SHIFT_BITS_PARAMETER_METADATA;

struct LpFilterStagesMetadata : public SimParameterMetadata {
public:
    LpFilterStagesMetadata() : SimParameterMetadata(SIM_PARAM_LP_FILTER_STAGES, QString("LpFltStages"), lpFilterStages, 2) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->lpFilterStages = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->lpFilterStages; }
    double getDouble(const SimParams * params) const override { return params->lpFilterStages; }
    void setInt(SimParams * params, int value) const override { params->lpFilterStages = value; }
    void setDouble(SimParams * params, double value) const override { params->lpFilterStages = value; }
    void set(SimParams * params, QVariant value) const override { params->lpFilterStages = value.toInt(); }
};
LpFilterStagesMetadata LP_FILTER_STAGES_PARAMETER_METADATA;

struct LpFilterEnabledMetadata : public SimParameterMetadata {
public:
    LpFilterEnabledMetadata() : SimParameterMetadata(SIM_PARAM_LP_FILTER_ENABLED, QString("LpFltEnabled"), lpFilterEnabled, 1) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->lpFilterEnabled = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->lpFilterEnabled; }
    double getDouble(const SimParams * params) const override { return params->lpFilterEnabled; }
    void setInt(SimParams * params, int value) const override { params->lpFilterEnabled = value; }
    void setDouble(SimParams * params, double value) const override { params->lpFilterEnabled = value; }
    void set(SimParams * params, QVariant value) const override { params->lpFilterEnabled = value.toInt(); }
};
LpFilterEnabledMetadata LP_FILTER_ENABLED_PARAMETER_METADATA;

static const char * MOVING_AVG_MODE_NAMES[] = {"Disabled", "Fixed 1 period", "Fixed N periods", "Z-cross N Periods", nullptr};
struct MovingAvgFilterModeMetadata : public SimParameterMetadata {
public:
    MovingAvgFilterModeMetadata() : SimParameterMetadata(SIM_PARAM_MOVING_AVG_FILTER_MODE, QString("MovAvgFltMode"), MOVING_AVG_MODE_NAMES, 0) {}
    void setParamByIndex(SimParams * params, int index) override {
        params->movingAverageFilterMode = getValue(index).toInt();
    }
    int getInt(const SimParams * params) const override { return params->movingAverageFilterMode; }
    double getDouble(const SimParams * params) const override { return params->movingAverageFilterMode; }
    void setInt(SimParams * params, int value) const override { params->movingAverageFilterMode = value; }
    void setDouble(SimParams * params, double value) const override { params->movingAverageFilterMode = value; }
    void set(SimParams * params, QVariant value) const override { params->movingAverageFilterMode = value.toInt(); }
};
MovingAvgFilterModeMetadata MOVING_AVG_FILTER_MODE_PARAMETER_METADATA;


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
    case SIM_PARAM_ATAN2_STEP_SAMPLES: return &ATAN2_STEP_SAMPLES_PARAMETER_METADATA;
    case SIM_PARAM_AVG_PERIODS: return &AVG_PERIODS_PARAMETER_METADATA;
    case SIM_PARAM_ADC_INTERPOLATION: return &ADC_INTERPOLATION_PARAMETER_METADATA;
    case SIM_PARAM_ADC_MOVING_AVG: return &ADC_MOVING_AVG_PARAMETER_METADATA;
    case SIM_PARAM_EDGE_SUBSAMPLING_BITS: return &EDGE_SUBSAMPLING_BITS_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_AMPLITUDE: return &SENSE_AMPLITUDE_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_DC_OFFSET: return &SENSE_DC_OFFSET_PARAMETER_METADATA;
    case SIM_PARAM_SENSE_NOISE: return &SENSE_NOISE_PARAMETER_METADATA;
    case SIM_PARAM_MUL_DROP_BITS: return &MUL_DROP_BITS_PARAMETER_METADATA;
    case SIM_PARAM_ACC_DROP_BITS: return &ACC_DROP_BITS_PARAMETER_METADATA;
    case SIM_PARAM_LP_FILTER_STAGES: return &LP_FILTER_STAGES_PARAMETER_METADATA;
    case SIM_PARAM_LP_FILTER_SHIFT_BITS: return &LP_FILTER_SHIFT_BITS_PARAMETER_METADATA;
    case SIM_PARAM_LP_FILTER_ENABLED: return &LP_FILTER_ENABLED_PARAMETER_METADATA;
    case SIM_PARAM_MOVING_AVG_FILTER_MODE: return &MOVING_AVG_FILTER_MODE_PARAMETER_METADATA;
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

SimParameterMetadata::SimParameterMetadata(SimParameter param, QString name, int defaultValue)
    : param(param), name(name), defaultValueIndex(0)
{
    defaultValueIndex = 0;
}

SimParameterMetadata::SimParameterMetadata(SimParameter param, QString name, const char * * valueNames, int defaultValue)
    : param(param), name(name), defaultValueIndex(defaultValue)
{
    for (int i = 0; valueNames[i]; i++) {
        valueLabels.add(valueNames[i]);
        values.add(QVariant(i));
    }
}

SimParameterMetadata::SimParameterMetadata(SimParameter param, QString name, QStringList valueNames, int defaultValue)
    : param(param), name(name), defaultValueIndex(defaultValue)
{
    for (int i = 0; i < valueNames.length(); i++) {
        valueLabels.add(valueNames[i]);
        values.add(QVariant(i));
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

void atan2BitsTest() {
    QRandomGenerator64 rnd(13);
    int iterationCount = 10000;
    ExactBitStats stats[33];
    for (int i = 0; i < iterationCount; i++) {
        double randomPhase = (rnd.generateDouble() - 0.5) * M_PI * 2;
        int64_t exactPhase = ((int64_t)(randomPhase / M_PI / 2 * (((int64_t)1)<<32))) & 0xffffffff;
        double rsin = sin(randomPhase);
        double rcos = cos(randomPhase);
        //qDebug("phase %f %d  sin=%f cos=%f", randomPhase, (int)exactPhase, rsin, rcos);
        for (int bits = 5; bits<32; bits++) {
            int64_t scale = (((int64_t)1)<<(bits-1));
            int64_t dsin = (int64_t)(rsin*scale);
            int64_t dcos = (int64_t)(rcos*scale);
            double dphase = atan2(dsin, dcos) / M_PI / 2;
            int64_t iphase = ((int64_t)(dphase * (((int64_t)1)<<32))) & 0xffffffff;
            int phaseDiff = (int)(iphase - exactPhase);
            if (phaseDiff < 0)
                phaseDiff = -phaseDiff;
            int exactBits = (phaseDiff==0) ? 32 : (32-(int)log2(phaseDiff));
            //qDebug("[%d] : %d exact bits   dsin=%d dcos=%d  dphase=%f iphase=%d  phaseDiff=%d", bits, exactBits, dsin, dcos, dphase, (int)iphase, phaseDiff);
            stats[bits].incrementExactBitsCount(exactBits);
        }
    }
    QString head = stats[5].headingString(5, 32);
    qDebug("atan2 stats");
    qDebug("\t%s", head.toLocal8Bit().data());
    for (int bits = 5; bits <= 32; bits++) {
        stats[bits].updatePercents();
        qDebug("%d:\t%s", bits, stats[bits].toString(5, 32).toLocal8Bit().data());
    }
}
