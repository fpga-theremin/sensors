Design of FPGA based theremin sensor with MCU and FPGA compatible interface
===========================================================================

* Drive signal is pure sine wave generated with FPGA + DAC [numerically controlled oscillator](https://en.wikipedia.org/wiki/Numerically_controlled_oscillator).
* Sallen-key filter to convert quantized DAC output into pure sine
* Filtered and buffered pure sine drive signal is fed to inductor of theremin LC tank (L = inductor, C = antenna).	
* Current sensing for drive current - measure voltage on sensing resistor between drive buffer and inductor.
* Sensed value of drive current is read by FPGA using ADC.
* FPGA internal logic can implement extremely sensitive and low latency phase error calculation - on LC tank resonant frequency drive current should have zero phase shift with drive voltage.
* FPGA implements Digital Phase Locked Loop (DPLL) - as in D-Lev theremin by Eric Wallin
* Flexible external interface of sensor module - pins assignment and behavior can be redefined in FPGA logic.
* Trying to minimize sensor module cost while getting maximum sensitivity

Choosing FPGA device
====================

Cheap but powerful enough to implement all the features of sensor.

Current candidates:

* Lattice ICE5LP4K-SG48ITR : Mouser price EUR 7.69 >34K in stock
* Lattice ICE5LP2K-SG48ITR : Mouser price EUR 6.6  ~3K  in stock

Smaller device (probably design will not fit):

* Lattice ICE5LP1K-SG48ITR : Mouser price EUR 5.22 ~1.6K in stock

Not in stock on JLCPCB. 
JLCPCB price is ICE5LP4K-SG48ITR is $11.63


SPI interface
=============

SPI slave interface in FPGA will be used for:

* updateing of FPGA configuration (once on powerup to program configuration SRAM or once by Diamond programmer - to upload configuration into non-volatile memory)
* controlling the sensor (after configuration is done) - by reading and writing of sensor registers and parameter tables.

SPI slave signals:

* SPI_CLK (SCLK)
* SPI_SS (CS, slave select, active 0)
* SPI_SI (MOSI)
* SPI_SO (MISO)

Configuration signals - to be used for programming.

* CRESET_B
* CDONE

FPGA configuration modes
========================

* Configure from NVCM (non-volatile configuration memory) - programming can be done once when needed, then configuration will be performed from NVCM on powerup
* Configure using slave SPI interface from external MCU - firmware should be uploaded each time on powerup

NVCM Programming
================

* option 1: use Diamond Programmer and the Lattice programming cable
* option 2: use EmbeddedProgramming - no information available, datasheet sais "The NVCM can be programmed using a processor. For more information, contact your local Lattice sales office."

Sensor SPI interface registers
==============================

| ------- | ----  | ----  | ----------- |
| Address | Name  | Size  | Description |
| ------- | ----  | ----  | ----------- |
| 0       | State | 8 bit | ----------- |

Sensor stream interface
=======================

Sensor may output measured values one per audio sample using I2S interface.

Depending on sensor firmware, sensor may send various types of data. Reasonable mode:

* pitch sensor: phase
* volume sensor: gain multiplier

Primitive synthesizer might just get wave table entry using phase as table index, and multiply to gain value to produce a new audio sample.


Sensor I2S Pins: 

* I2S_BCLK - bit clock
* I2S_LRCK - left/right channel
* I2S_DO   - I2S data output from sensor (16/24/32 bits) - left channel value contains measurement from this sensor, right channel value may replicate left channel value from I2S_DI
* I2S_DI   - I2S data input to sensor - may be used to merge streams from second sensor (left channel) and replicate as right channel in I2S_DO, connect to GND if there is no previous sensor in chain.

Chaining two sensors using I2S_DO and I2S_DI allows to use only single I2S stereo channel (DIN) of MCU to receive data streams from both pitch and volume sensors.

See I2S docs for details.

Streaming of sensor values as I2S data is a feasible transport - MCU may receive it as additional audio stream using DMA, and get nice per-sample interpolated values.

