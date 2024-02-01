#include "dds.h"
#include <QtMath>

PhaseAccumulator::PhaseAccumulator()
    : phase(0)
    , increment(frequencyToIncrement(1009820.856, 33333333.333333))
{

}

uint32_t PhaseAccumulator::frequencyToIncrement(double signalFrequency, double sampleRate) {
    return (int32_t)round((1ull << 32) / (sampleRate / signalFrequency));
}

DDS::DDS(double sampleRate, int sinTableSizeBits, int sinValueBits, double scale)
    : sinTable(sinTableSizeBits, sinValueBits, scale)
    , sampleRate(sampleRate)
{
    setFrequency(1009820.856);
}

void DDS::setFrequency(double freq) {
    uint32_t increment = PhaseAccumulator::frequencyToIncrement(freq, sampleRate);
    phaseAccum.setIncrement(increment);
}

int DDS::nextSample() {
    uint32_t phase = phaseAccum.tick();
    int sample = sinTable.getForPhase(phase);
    return sample;
}


SigmaDelta::SigmaDelta(int inputBits, int outputBits)
    : inputBits(inputBits)
    , outputBits(outputBits)
{
    shift = inputBits - outputBits;
    acc1 = 0;
    acc2 = 0;
    outValue = 0;
    dacVal2 = 0;
}

int SigmaDelta::tick(int inputSample) {
    acc1 += inputSample - outValue;
    acc2 += acc1 - outValue;
    outValue = (acc2 >> shift) << shift;
    return outValue >> shift;
}

PWLFile::PWLFile(const char * fname)
    : f(nullptr)
    , minStep(0.0000000001)
    , minDiff(0.00001)
    , lastTime(0)
    , lastValue(0)
    , newTime(0)
    , newValue(0)
    , firstSample(true)
{
    f = fopen(fname, "wt");
    if (!f) {
        qDebug("Error while creating file %s", fname);
    }
}

void PWLFile::put(double t, double v) {
    if (f)
        fprintf(f, "%.11f\t%.6f\n", t, v);
    lastTime = t;
    lastValue = v;
    firstSample = false;
}

void PWLFile::step(double timeIncrement, double value) {
    if (firstSample) {
        put(0, value);
        newTime = 0;
        newValue = value;
    }
    double t = newTime + timeIncrement;
    double v = value;

    if (newTime - lastTime > minStep && timeIncrement > minStep) {
        double dt0 = newTime - lastTime;
        double dv0 = newValue - lastValue;
        double dt1 = t - lastTime;
        double dv1 = v - lastValue;
        double d0 = dv0 / dt0;
        double d1 = dv1 / dt1;
        double ddelta = (d1 - d0);
        if (ddelta < 0)
            ddelta = -ddelta;
        if (ddelta > minDiff) {
            put(newTime, newValue);
        }
    }

    newTime = t;
    newValue = v;
}

PWLFile::~PWLFile() {
    if (newTime > lastTime + 0.0000000001)
        put(newTime, newValue);
    if (f)
        fclose(f);
}

DACPWL::DACPWL(const char * file, double sampleRate, int bitsPerSample, double edge, double value0, double value1)
    : f(file)
    , sampleRate(sampleRate)
    , bitsPerSample(bitsPerSample)
    , value0(value0)
    , value1(value1)

{
    samplePeriod = 1.0 / sampleRate;
    edgeLength = samplePeriod * edge;
    hLength = samplePeriod * (1.0 - edge);
    middle = (value0 + value1) / 2;
    scale = (value1-value0) / (1 << bitsPerSample);
}

void DACPWL::put(int v) {
    double value = v * scale + middle;
    f.step(edgeLength, value);
    f.step(hLength, value);
}

PatternPWM::PatternPWM(const char * file, int sampleRate, double edge, double value0, double value1)
    : f(file)
    , sampleRate(sampleRate)
    , value0(value0)
    , value1(value1)

{
    bitPeriod = 1.0 / sampleRate;
    edgeLength = bitPeriod * edge;
    hLength = bitPeriod * (1.0 - edge);
}

void PatternPWM::putBit(int v) {
    double outValue = (v ? value1 : value0);
    f.step(edgeLength, outValue);
    f.step(hLength, outValue);
}

void PatternPWM::putBits(int v, int bitCount) {
    int mask = 1;
    for (int i = 0; i < bitCount; i++) {
        putBit(v & mask);
        mask = mask << 1;
    }
}

static const int PWM_PATTERN_TABLE_12[16] = {
    // INPUT  VALUE   PWM SEQUENCE
    // -----  -----   ------------
    0b001110001110, // 0000     0     001110001110
    0b001111001110, // 0001     1     001111001110
    0b011110011110, // 0010     2     011110011110
    0b001111111110, // 0011     3     001111111110
    0b011111111110, // 0100     4     011111111110
    0b011111111110, // 0101    (4)   (011111111110)
    0b011111111110, // 0110    (4)   (011111111110)
    0b011111111110, // 0111    (4)   (011111111110)
    // negative values
    0b000001100000, // 1000    (-4)  (000001100000)
    0b000001100000, // 1001    (-4)  (000001100000)
    0b000001100000, // 1010    (-4)  (000001100000)
    0b000001100000, // 1011    (-4)  (000001100000)
    0b000001100000, // 1100    -4     000001100000
    0b000001110000, // 1101    -3     000001110000
    0b001100001100, // 1110    -2     001100001100
    0b001110001100, // 1111    -1     001110001100
};
// put 4-bit signed value as 12-bit PWM pattern
void PatternPWM::putPattern12(int v) {
    int pattern = PWM_PATTERN_TABLE_12[v & 15];
    putBits(pattern, 12);
}

void testDDS1() {
    DDS dds(33333333.333, 10, 9, 0.45);
    DDS dds2(33333333.333, 10, 16, 0.45);
    dds.setFrequency(1009820.856);
    dds2.setFrequency(1009820.856);
    //dds.setFrequency(12820.856);
    //dds2.setFrequency(12820.856);
    qDebug("Generating PWL file for sine PWM");
    PatternPWM pwm("pwm_sine_12bit_pattern_400Msps_33MHz.txt");
    PatternPWM pwm2("pwm_sine_12bit_pattern_400Msps_33MHz_sigma_delta.txt");
    PatternPWM pwm3("pwm_sine_12bit_pattern_400Msps_33MHz_sigma_delta_16.txt");
    DACPWL dac("dac_sine_9bps_33MHz.txt");
    SigmaDelta sigmaDelta(9, 4);
    SigmaDelta sigmaDelta2(16, 4);
    int shift = dds.sinTable.getValueBits() - 4;
    //int shift2 = dds2.sinTable.valueBits - 4;
    for (int i = 0; i < 33*1000; i++) {
        int sample = dds.nextSample();
        int sample16 = dds2.nextSample();
        int index = (sample >> shift) & 15;
        if (index >= 5 && index <= 11) {
            qDebug("[%d] index out of bounds %d", i, index);
        }
        pwm.putPattern12(index);
        dac.put(sample);
        int sd = sigmaDelta.tick(sample);
        pwm2.putPattern12(sd);

        int sd2 = sigmaDelta2.tick(sample16);
        pwm3.putPattern12(sd2);
        //qDebug(" [%d] %d", i, sample);
    }
}

void testDDS2() {
    DDS dds(200000000.0, 10, 9, 1.0);
    dds.setFrequency(1009820.856);
    PatternPWM pwm("pwm_sine_1bit_200Msps.txt", 200000000);
    SigmaDelta sigmaDelta(9, 1);
    for (int i = 0; i < 33*1000; i++) {
        int sample = dds.nextSample();
        int sd = sigmaDelta.tick(sample);
        pwm.putBit(sd >= 0 ? 1 : 0);
    }
}

QString formatBin(int n, int bits) {
    return QString("%1").arg((int)n, (int)3, (int)2, QLatin1Char('0'));
}

void generateVerilogVectorLengthCases3x3() {
    qDebug("");
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int squareSum = x*x + y*y;
            int sq = (int)floor(sqrt(squareSum + 0.99999));
            if (sq > 7)
                sq = 7;
            QString line = QString("        6'b%1_%2: amplitude_stage1 <= %3;").arg(formatBin(y, 3)).arg(formatBin(x, 3)).arg(sq);
            qDebug("%s", line.toUtf8().data());
        }
    }
    qDebug("");
    for (int scale = 0; scale < 8; scale++) {
        for (int amp = 0; amp < 8; amp++) {
            int action = 0;
            int amp1 = amp == 0 ? 1 : amp;
            int halfPeriods = (scale + 1) * 2; // for 0, 1, 2: 2, 4, 6, 8...
            int halfPeriodsInc = halfPeriods + 2;
            int halfPeriodsDec = halfPeriods - 2;
            // expected amplitude after increment
            int ampInc = amp1 * halfPeriodsInc / halfPeriods;
            // expected amplitude after decrement
            int ampDec = amp1 * halfPeriodsDec / halfPeriods;
            if (amp >= 6) {
                if (scale > 0)
                    action = 1; // reduce scale
            } else if (ampInc < 5) {
                if (scale < 7)
                    action = 2; // increase scale
            }

            QString line = QString("        6'b%1_%2: action_stage2 <= %3;").arg(formatBin(scale, 3)).arg(formatBin(amp, 3)).arg(action);
            qDebug("%s", line.toUtf8().data());
        }
    }
    qDebug("");
}

void SinCosCORDIC::sinCos(int & outx, int & outy, uint32_t phase32) {
    int quadrant = (phase32 >> (14+16)) & 3;
    // 7 bits of tabular sin&cos
    int step1 = (phase32 >> (14+9)) & 127;
    // 9 bits of rotation
    int step2 = (phase32 >> (14+0)) & 511;

    uint32_t tableEntry = sinCosTable[step1];
    int x = tableEntry & 0xFFFF;
    int y = (tableEntry >> 16) & 0xFFFF;

    // second stage - rotation by small angle

    // final stage - quadrant processing
    switch (quadrant) {
    case 0: // 0 degrees
    default:
        outx = x;
        outy = y;
        break;
    case 1: // 90 degrees
        outx = -y;
        outy = x;
        break;
    case 2: // 180 degrees
        outx = -x;
        outy = -y;
        break;
    case 3: // 270 degrees
        outx = y;
        outy = -x;
        break;
    }

}

SinCosCORDIC::SinCosCORDIC() {
    double bigStepAngle = M_PI / 256.0;
    // init table for step 1
    for (int i = 0; i < 128; i++) {
        // 0..127 are angles in range (0..PI/2)
        double phase = (i + 0.5) * bigStepAngle;
        uint32_t x = (int)(cos(phase) * 65536.0);
        uint32_t y = (int)(sin(phase) * 65536.0);
        assert((x >= 0) && (x<=65535) && (y >= 0) && (y<=65535));
        qDebug(" [%d]\t%.9f\t%d\t%d", i, phase, x, y);
        sinCosTable[i] = (y<<16) | x;
    }
    for (int i = 0; i < 512; i++) {
        double angle = (i + 0.5) * bigStepAngle / 512;
    }
}

void testCordic() {
    qDebug("CORDIC tests");
    SinCosCORDIC();
    qDebug("CORDIC tests done");
}

void testDDS() {
    testCordic();

    SinTable sinTable(10, 12);
    sinTable.generateVerilog("");
    sinTable.generateMemInitFile("");
    sinTable.generateVerilogTestBench("");
    sinTable.generateSinCosVerilog("");
    SinTable sinTable2(12, 13);
    sinTable2.generateVerilog("");
    sinTable2.generateMemInitFile("");
    sinTable2.generateVerilogTestBench("");
    sinTable2.generateSinCosVerilog("");
    sinTable2.generateSinCosVerilogTestBench("");

    testDDS1();
    testDDS2();
    generateVerilogVectorLengthCases3x3();
}


