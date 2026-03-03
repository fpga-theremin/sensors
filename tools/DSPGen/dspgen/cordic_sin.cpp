#include "cordic_sin.h"
#include <math.h>
#include <stdio.h>

//#define M_PI 3.14159265359f
#define ATAN_TAB_N 16
const int atantable[ATAN_TAB_N] = {  0x4000,   //atan(2^0) = 45 degrees
    0x25C8,   //atan(2^-1) = 26.5651
    0x13F6,   //atan(2^-2) = 14.0362
    0x0A22,   //7.12502
    0x0516,   //3.57633
    0x028B,   //1.78981
    0x0145,   //0.895174
    0x00A2,   //0.447614
    0x0051,   //0.223808
    0x0029,   //0.111904
    0x0014,   //0.05595
    0x000A,   //0.0279765
    0x0005,   //0.0139882
    0x0003,   //0.0069941
    0x0002,   //0.0035013
    0x0001    //0.0017485
};

const int ATAN_TABLE_17_010000[17] = {
    0x10000,   //    45.000000
    0x9720,   //    26.565051
    0x4fda,   //    14.036243
    0x2889,   //     7.125016
    0x1458,   //     3.576334
    0x0a2f,   //     1.789911
    0x0518,   //     0.895174
    0x028c,   //     0.447614
    0x0146,   //     0.223811
    0x00a3,   //     0.111906
    0x0051,   //     0.055953
    0x0029,   //     0.027976
    0x0014,   //     0.013988
    0x000a,   //     0.006994
    0x0005,   //     0.003497
    0x0003,   //     0.001749
    0x0001,   //     0.000874
};

const int ATAN_TABLE_16_010000[16] = {
    0x10000,   //    45.000000
    0x9720,   //    26.565051
    0x4fda,   //    14.036243
    0x2889,   //     7.125016
    0x1458,   //     3.576334
    0x0a2f,   //     1.789911
    0x0518,   //     0.895174
    0x028c,   //     0.447614
    0x0146,   //     0.223811
    0x00a3,   //     0.111906
    0x0051,   //     0.055953
    0x0029,   //     0.027976
    0x0014,   //     0.013988
    0x000a,   //     0.006994
    0x0005,   //     0.003497
    0x0003,   //     0.001749
};

const int ATAN_TABLE_16_004000[16] = {
    0x4000,   //    45.000000
    0x25c8,   //    26.565051
    0x13f6,   //    14.036243
    0x0a22,   //     7.125016
    0x0516,   //     3.576334
    0x028c,   //     1.789911
    0x0146,   //     0.895174
    0x00a3,   //     0.447614
    0x0051,   //     0.223811
    0x0029,   //     0.111906
    0x0014,   //     0.055953
    0x000a,   //     0.027976
    0x0005,   //     0.013988
    0x0003,   //     0.006994
    0x0001,   //     0.003497
    0x0001,   //     0.001749
};

const int ATAN_TABLE_18_020000[18] = {
    0x20000,   //    45.000000
    0x12e40,   //    26.565051
    0x9fb4,   //    14.036243
    0x5111,   //     7.125016
    0x28b1,   //     3.576334
    0x145d,   //     1.789911
    0x0a2f,   //     0.895174
    0x0518,   //     0.447614
    0x028c,   //     0.223811
    0x0146,   //     0.111906
    0x00a3,   //     0.055953
    0x0051,   //     0.027976
    0x0029,   //     0.013988
    0x0014,   //     0.006994
    0x000a,   //     0.003497
    0x0005,   //     0.001749
    0x0003,   //     0.000874
    0x0001,   //     0.000437
};

void gen_atan_table(int steps, int scale) {
    printf("const int ATAN_TABLE_%d_%06x[%d] = {\n", steps, scale, steps);
    double angle = 1.0;
    for (int i = 0; i < steps; i++) {
        double a = atan(angle);
        int da = round(a * scale / M_PI * 4);
        printf("    0x%04x,   //   %10.6f\n", da, a * 180 / M_PI);
        angle = angle * 0.5;
    }

    printf("};\n");
}

void cordic_sincos(int scale, int steps, int phase, int& vcos, int &vsin) {
    int extraBits = 2;
    int x = scale << extraBits; //0x4DBA; // 0.60725293
    int y = 0;
    int a = phase;
    const int * table = ATAN_TABLE_18_020000;
    for (int i = 0; i < steps; i++) {
        if (a >= 0) {
            int newx = x - (y >> i);
            int newy = y + (x >> i);
            x = newx;
            y = newy;
            a -= table[i];
        } else {
            int newx = x + (y >> i);
            int newy = y - (x >> i);
            x = newx;
            y = newy;
            a += table[i];
        }
    }
    vcos = x >> extraBits;
    vsin = y >> extraBits;
}

void gen_sin_table(int count, int bits) {
    double amp = (1 << bits) - 1;
    for (int i = 0; i < count; i++) {
        double a = (i + 0.5) * M_PI / 2 / count;
        double a0 = (i) * M_PI / 2 / count;
        double a1 = (i + 1.0) * M_PI / 2 / count;
        double vsin = sin(a);
        double vsin0 = sin(a0);
        double vsin1 = sin(a1);
        double scaled = vsin * amp;
        double scaled0 = vsin0 * amp;
        double scaled1 = vsin1 * amp;
        int v = round(scaled);
        int vm = round((scaled0 + scaled1) / 2);
        int dm = round((scaled1 - scaled0)*1);
        printf("    0x%04x,   // %d  \t 0x%x \t %d\n", v, i, vm, dm);
    }
}

void cordic_test() {
    int phaseScale = 0x20000;
    int steps = 18;
    int xscale = 0x20000;
    int scale = 0.60725293 * xscale; //0x4DBA;
    double a = -46.0;
    int i = 0;
    while (a <= 46.01) {
        double f = a * M_PI / 180;
        double refcos = cos(f) * xscale;
        double refsin = sin(f) * xscale;
        //double ra = atan2(refsin, refcos) * 180 / M_PI;
        int phase = a * phaseScale / 45;
        int vcos;
        int vsin;
        cordic_sincos(scale, steps, phase, vcos, vsin);
        double da = atan2((double)vsin, (double)vcos) * 180 / M_PI;
        int rphase = phase;
        int dphase = da / 45 * phaseScale;
        printf("%d:\t%.3f\t\t"
               "\t%10.2f\t%10.2f\t"
               "\t[%5d]\t%6d\t%6d\t\t%.3f\t"
               "\t%5d\t%5d\t%d\n",
               i, a,
               refcos, refsin,
               phase, vcos, vsin, da,
               rphase, dphase, dphase-rphase);
        a += 0.2;
        i++;
    }

    gen_atan_table(steps, phaseScale);
    gen_sin_table(4096, 17);
}
