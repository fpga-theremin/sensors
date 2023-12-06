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

void SimParams::recalculate() {
    qDebug("SimParams::recalculate()");
    /*
    double frequency;
    int sampleRate;
    // sin table size bits
    int ncoPhaseBits;
    int ncoValueBits;
    int ncoSinTableSizeBits;

    int64_t phaseIncrement;
    int64_t phaseModule; // 1 << ncoPhaseBits
    int sinTableSize;
     */
    qDebug("  source:");
    qDebug("    frequency:           %.5f", frequency);
    qDebug("    sampleRate:          %d", sampleRate);
    qDebug("    ncoPhaseBits:        %d", ncoPhaseBits);
    qDebug("    ncoValueBits:        %d", ncoValueBits);
    qDebug("    ncoSinTableSizeBits: %d", ncoSinTableSizeBits);

    // frequency, sampleRate
    double phaseInc = frequency / sampleRate;
    phaseModule = ((int64_t)1) << ncoPhaseBits;
    phaseIncrement = (int64_t)round(phaseInc * phaseModule);
    realFrequency = sampleRate / ((double)phaseModule / (double)phaseIncrement);
    for (int i = 0; i < SP_MAX_SIN_TABLE_SIZE; i++) {
        sinTable[i] = 0;
    }
    sinTableSize = 1 << ncoSinTableSizeBits;

    qDebug("  calculated:");
    qDebug("    realFrequency:       %.5f", realFrequency);
    qDebug("    phaseIncrement:      %lld", phaseIncrement);
    qDebug("    phaseModule:         %lld", phaseModule);
    qDebug("    samplesPerPeriod:    %lld", phaseModule / phaseIncrement);
    qDebug("    sinTableSize:        %d", sinTableSize);

#ifdef DEBUG_SIN_TABLE
    qDebug("  sin table:");
#endif
    for (int i = 0; i < sinTableSize; i++) {
        double phase = i * 2*M_PI / sinTableSize;
        double sinValue = sin(phase);
        sinTable[i] = quantizeSigned(sinValue, ncoValueBits);
#ifdef DEBUG_SIN_TABLE
        qDebug("    sin[%d] \t%.5f \t%.5f \t%.5f \t%d", i, sinValue,
               quantizeDouble(sinValue, ncoValueBits),
               scaleDouble(sinValue, ncoValueBits),
               sinTable[i]
               );
#endif
    }
}

int SimParams::tableEntryForPhase(int64_t phase) {
    int shiftedPhase = (int)(phase >> (ncoPhaseBits-ncoSinTableSizeBits));
    int shiftedMasked = shiftedPhase & (sinTableSize - 1);
    return sinTable[shiftedMasked];
}

void SimState::simulate(SimParams * newParams) {
    params = newParams;
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


    for (int i = 0; i < SP_SIM_MAX_SAMPLES/10; i++) {
        periodSumBase1[i] = 0;
        periodSumBase2[i] = 0;
    }

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
    // averaging aligned by even period frames
    int avgPeriodsCount = (periodCount - 1) & 0xffffffe;
    alignedSumBase1 = 0;
    alignedSumBase2 = 0;
    for (int i = 1; i <= avgPeriodsCount; i++) {
        alignedSumBase1 += periodSumBase1[i];
        alignedSumBase2 += periodSumBase2[i];
    }
    alignedSensePhaseShift = - atan2(alignedSumBase2, alignedSumBase1) / M_PI / 2;
    alignedSensePhaseShiftDiff = alignedSensePhaseShift - params->sensePhaseShift;
    if (alignedSensePhaseShiftDiff < -0.5)
        alignedSensePhaseShiftDiff += 1.0;
    else if (alignedSensePhaseShiftDiff > 0.5)
        alignedSensePhaseShiftDiff -= 1.0;
    qDebug("alignedSensePhaseShiftDiff = %.6f", alignedSensePhaseShiftDiff);
}

