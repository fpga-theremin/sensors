#ifndef SIMUTILS_H
#define SIMUTILS_H

#include <stdint.h>

int quantizeSigned(double value, int bits);
double quantizeDouble(double value, int bits);
double scaleDouble(double value, int bits);

#define SP_MAX_SIN_TABLE_SIZE 4096
struct SimParams {
    double frequency;
    int sampleRate;
    // sin table size bits
    int ncoPhaseBits;
    int ncoValueBits;
    int ncoSinTableSizeBits;

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
    int sinTable [SP_MAX_SIN_TABLE_SIZE];
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
    {
        // recalculate dependent parameters
        recalculate();
    }

    void recalculate();
    int tableEntryForPhase(int64_t phase);
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

    int senseMulBase1[SP_SIM_MAX_SAMPLES];
    int senseMulBase2[SP_SIM_MAX_SAMPLES];

    int periodIndex[SP_SIM_MAX_SAMPLES];
    int avgMulBase1;
    int avgMulBase2;

    int periodSumBase1[SP_SIM_MAX_SAMPLES/10];
    int periodSumBase2[SP_SIM_MAX_SAMPLES/10];

    void simulate(SimParams * params);
};

#endif // SIMUTILS_H
