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


SinTable::SinTable(int tableSizeBits, int valueBits, double scale)
    : table(nullptr)
{
    init(tableSizeBits, valueBits, scale);
}

void SinTable::init(int tableSizeBits, int valueBits, double scale) {
    if (table)
        delete[] table;
    this->tableSizeBits = tableSizeBits;
    this->valueBits = valueBits;
    this->tableSize = (1 << tableSizeBits);
    this->table = new int[tableSize];
    // valueBits includes sign, so positive has valueBits-1 filled with all-ones
    int maxIntValue = (1 << (valueBits-1)) - 1;
    double targetScale = maxIntValue * scale;
    int errCount = 0;
    for (int i = 0; i < tableSize; i++) {
        double phase = i * 2*M_PI / tableSize;
        double sinValue = sin(phase);
        double scaled = targetScale * sinValue;
        int rounded = (int)round(scaled);
        table[i] = rounded;
        int fourBitValue = (rounded >> (valueBits-4)) & 0xf;
        bool err = (fourBitValue == 8)||(fourBitValue == 9)||(fourBitValue == 10)||(fourBitValue == 11)
                ||(fourBitValue == 5)||(fourBitValue == 6)||(fourBitValue == 7);
        if (err)
            errCount++;
        qDebug("    sin[%d] \t%.5f \t%.5f \t%d \tdelta=%d  %x   %s",
               i,
               sinValue,
               scaled,
               table[i],
               i>0 ? (table[i] - table[i-1]) : (table[i] - table[tableSize + i-1]),
               fourBitValue,
               (err ? "error" : "")
               );

    }
    // probably max scale w/o errors is 8.062/16.0
    qDebug("Done. Range error count: %d", errCount);
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
    int sample = sinTable.get(phase);
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

/*
module dsa_two #(
    parameter dac_bw = 16,
    parameter osr = 6
)(

    input wire clk,
    input wire rst_n,
    input wire [15:0] din,
    output wire dout
);

    localparam mid_val = 2**(dac_bw - 1) + 2**(osr + 2);

    localparam bw_ext = 2;
    localparam bw_tot = dac_bw + bw_ext;

    reg						dout_r;
    reg						dac_dout;
    reg signed		[bw_tot-1 : 0]	DAC_acc_1st;

    wire signed		[bw_tot-1 : 0]	max_val = mid_val;
    wire signed		[bw_tot-1 : 0]	min_val = -mid_val;
    wire signed		[bw_tot-1 : 0]	dac_val = (!dout_r) ? max_val : min_val;

    wire signed		[bw_tot-1 : 0]	in_ext = {{bw_ext{din[dac_bw - 1]}}, din};
    wire signed		[bw_tot-1 : 0]	delta_s0_c0 = in_ext + dac_val;
    wire signed		[bw_tot-1 : 0]	delta_s0_c1 = DAC_acc_1st + delta_s0_c0;

    always@(posedge clk)begin
        if(!rst_n)begin
            DAC_acc_1st <= 'd0;
        end else begin
            DAC_acc_1st <= delta_s0_c1;
        end
    end

    localparam bw_tot2 = bw_tot + osr;

    reg signed		[bw_tot2-1 : 0]	DAC_acc_2nd;

    wire signed		[bw_tot2-1 : 0]	max_val2 = mid_val;
    wire signed		[bw_tot2-1 : 0]	min_val2 = -mid_val;
    wire signed		[bw_tot2-1 : 0]	dac_val2 = (!dout_r) ? max_val2 : min_val2;

    wire signed		[bw_tot2-1 : 0]	in_ext2 = {{osr{delta_s0_c1[bw_tot - 1]}}, delta_s0_c1};
    wire signed		[bw_tot2-1 : 0]	delta_s1_c0 = in_ext2 + dac_val2;
    wire signed		[bw_tot2-1 : 0]	delta_s1_c1 = DAC_acc_2nd + delta_s1_c0;

    always@(posedge clk)begin
        if(!rst_n)begin
            DAC_acc_2nd <= 'd0;
        end else begin
            DAC_acc_2nd <= delta_s1_c1;
        end
    end

    always@(posedge clk)begin
        if(!rst_n)begin
            dout_r		<= 1'b0;
            dac_dout	<= 1'b0;
        end else begin
            dout_r		<= delta_s1_c1[bw_tot2-1];
            dac_dout	<= ~dout_r;
        end
    end

    assign dout = dout_r;

int output_val = (output==1) ? 128 : -128;
      accum  += input_val - output_val;
      accum2 += accum - output_val;
      output = (accum2 > 0) ? 1 : 0;
*/


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
    int shift = dds.sinTable.valueBits - 4;
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

void testDDS() {
    testDDS1();
    testDDS2();
}
