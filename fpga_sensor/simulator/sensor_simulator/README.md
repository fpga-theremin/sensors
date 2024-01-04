Theremin current sensing FPGA based sensor simulator
====================================================

* simulate sensor behavior for different parameters (sample rate, ADC bits, SIN table size and precision, averaging, noise, DC offset, etc.)
* generate verilog code for SIN or SIN + COS lookup tables of different size and precision, with test bench SystemVerilog files
* generate PWL files of quantized sine waves for LTSpice simulations of DAC output processing


This is a C++/Qt application. Use qmake for building or just open this project in QtCreator.


Findings based on simulation results
====================================

SIN table size for DCO: it does not make sense to increase table length and sample value precision above ADC parameters.

Reasonable values:

    **SIN/COS table value bits**: ADC_bits + 1
    **SIN/COS table size**: 2 ^ (ADC_bits + 1)


