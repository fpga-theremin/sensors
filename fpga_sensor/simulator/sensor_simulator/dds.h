#ifndef DDS_H
#define DDS_H

#include "dsputils.h"

#include <stdio.h>

struct PhaseAccumulator {
private:
    uint32_t phase;
    uint32_t increment;
public:
    PhaseAccumulator();
    // convert frequency to phase increment for this accumulator
    static uint32_t frequencyToIncrement(double signalFrequency, double sampleRate);
    uint32_t getPhase() {
        return phase;
    }
    uint32_t getIncrement() {
        return increment;
    }
    uint32_t tick() {
        phase += increment;
        return phase;
    }
    void setIncrement(uint32_t newIncrement) {
        increment = newIncrement;
    }
    void correctIncrement(int32_t incrementDelta) {
        increment += incrementDelta;
    }
};

struct DDS {
    SinTable sinTable;
    PhaseAccumulator phaseAccum;
    double sampleRate;
    DDS(double sampleRate = 33333333.333, int sinTableSizeBits = 10, int sinValueBits = 9, double scale = 1.0);
    void setFrequency(double freq);
    int nextSample();
};

struct SigmaDelta {
    int inputBits;
    int outputBits;
    int shift;
    int acc1;
    int acc2;
    int outValue;
    int dacVal2;
    SigmaDelta(int inputBits, int outputBits);
    int tick(int inputSample);
};

struct PWLFile {
private:
    FILE * f;
    double minStep;
    double minDiff;
    double lastTime;
    double lastValue;
    double newTime;
    double newValue;
    bool firstSample;
    void put(double t, double v);
public:
    PWLFile(const char * fname);
    ~PWLFile();
    void step(double timeIncrement, double value);
};

struct PatternPWM {
    PWLFile f;
    int sampleRate;
    double bitPeriod;
    double edgeLength;
    double hLength;
    double value0;
    double value1;
    PatternPWM(const char * file, int sampleRate = 400000000, double edge=0.4, double value0 = 0.0, double value1 = 3.3);
    ~PatternPWM() {}
    void putBit(int v);
    void putBits(int v, int bitCount);
    // put 4-bit signed value as 12-bit PWM pattern
    void putPattern12(int v);
};

struct DACPWL {
    PWLFile f;
    int sampleRate;
    int bitsPerSample;
    double samplePeriod;
    double edgeLength;
    double hLength;
    double value0;
    double value1;
    double middle;
    double scale;
    DACPWL(const char * file, double sampleRate = 33333333.333, int bitsPerSample = 9, double edge=0.1, double value0 = 0.0, double value1 = 3.3);
    ~DACPWL() {}
    void put(int v);
};

struct SinCosCORDIC {
    // Phase bits [17:16] are implemented by flipping and sign change
    // table for angles 0..PI/2
    // upper 16 bits: sin(x), lower 16 bits: cos(x)
    bool usePiDiv4Table;
    uint32_t sinCosTable[1024];      // phase bits [15:9]
    uint16_t fracRotationTable[1024]; // phase bits [8:0]
    int step1PhaseBits;
    int step2PhaseBits;
    int steps1;
    int steps2;
    int startShift;
    int rotations;
    int extraPrecisionBits;
    double atanPowerOfTwo[32];
    double scale;
    int sinHighBitBound;
    SinCosCORDIC(bool usePiDiv4Table, int step1bits, int step2bits, int rotCount, int extraPrecisionBits);
    void initAngles(double bigStepAngle);
    void initPiDiv2();
    void initPiDiv4();
    void sinCos(int & outx, int & outy, uint32_t phase32);
    void sinCosPiDiv4(int & outx, int & outy, uint32_t phase32);
};

void testDDS();
void testCordic();

#endif // DDS_H
