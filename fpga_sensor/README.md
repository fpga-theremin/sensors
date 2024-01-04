Design of FPGA based current sensing theremin sensor with MCU and FPGA compatible interface
===========================================================================================

![Block Diagram](images/fpga_sensor_block_diagram.png)

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

FPGA device should be cheap but powerful enough to implement all the features of sensor, available in stock.

Current candidates (Lattice iCE40 Ultra family, QFN-48 package, 39 I/O):

| Device   |  Description | Mouser link | Mouser stock | Mouser price | JLCPCB link | JLCPCB price |
| ------   |  ----------- | ----------- | ------------ | ------------ | ----------- | ------------ |
| ICE5LP4K-SG48ITR | 3520LE, 20EBR, 4DSP | [842-ICE5LP4K-SG48ITR](https://eu.mouser.com/ProductDetail/Lattice/ICE5LP4K-SG48ITR?qs=ZwKJWZfDtNj9QagUtdRf4g%3D%3D) | 34K | EUR 7.69 | [C2651898](https://jlcpcb.com/partdetail/Lattice-ICE5LP4KSG48ITR/C2651898) | $11.63 |
| ICE5LP2K-SG48ITR | 2048LE, 20EBR, 4DSP | [842-ICE5LP2K-SG48ITR](https://eu.mouser.com/ProductDetail/Lattice/ICE5LP2K-SG48ITR?qs=ZwKJWZfDtNgAWzMHJRGDug%3D%3D) | 3K | EUR 6.60 | not available | - |
| ICE5LP1K-SG48ITR | 1100LE, 16EBR, 2DSP | [842-ICE5LP1K-SG48ITR](https://eu.mouser.com/ProductDetail/Lattice/ICE5LP1K-SG48ITR?qs=ZwKJWZfDtNg6punXjA%252B0Og%3D%3D) | 1.5K | EUR 5.22 | [C1550810](https://jlcpcb.com/partdetail/Lattice-ICE5LP1KSG48ITR/C1550810)  | $9.37 |

Resources in table above:

* **LE** - logic element (LUT4 + FF)
* **EBR** - memory block (single port 256x16)
* **DSP** - 16x16 mul + 32 bit accumulator

**ICE5LP1K-SG48ITR** is smaller device (probably design will not fit).


SPI interface
=============

SPI slave interface in FPGA will be used for:

* updateing of FPGA configuration (once on powerup to program configuration SRAM or once by Diamond programmer - to upload configuration into non-volatile memory)
* controlling the sensor (after configuration is done) - by reading and writing of sensor registers and parameter tables.

SPI slave signals:

| Signal name  | Direction | Description |
| -----------  | --------- | ----------- |
| **SPI_CLK**  | input     | SPI clock input (SCLK) |
| **SPI_SS**   | input     | slave select, active 0 (CS) |
| **SPI_SI**   | input     | SPI slave data input (MOSI) |
| **SPI_SO**   | output    | SPI slave data output (MISO) |

Configuration signals - to be used for programming.

| Signal name  | Direction | Description |
| -----------  | --------- | ----------- |
| **CRESET_B** | input     | configuration reset, 0 to start configuration |
| **CDONE**    | ouput     | configuration done |

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

| Address | Name   | Size  | Description                             |
| ------- | ----   | ----  | --------------------------------------- |
| 0       | Status | 8 bit | TODO                                    |

Some notes about required registers.

Flags:

    SENSOR_ENABLE   1bit  R/W  0: oscillator stopped, 1: oscillator is working
    FREQ_LOCK       1bit  R/W  1: use PHASE_INCREMENT_OVERRIDE for DCO, 0: use PLL

Long registers:

    PHASE_INCREMENT          32bit R/O  read: current phase increment
    PHASE_INCREMENT_OVERRIDE 32bit R/W  to allow fixing oscillation frequency



Sensor stream interface
=======================

Sensor may output measured values one per audio sample using I2S interface.

Depending on sensor firmware, sensor may send various types of data. Reasonable mode:

* pitch sensor: phase
* volume sensor: gain multiplier

Primitive synthesizer might just get wave table entry using phase as table index, and multiply to gain value to produce a new audio sample.


Sensor I2S Pins: 

| Signal name  | Direction | Description |
| -----------  | --------- | ----------- |
| **I2S_BCLK** | input     | bit clock   |
| **I2S_LRCK** | input     | left/right channel |
| **I2S_DO**   | output    | I2S data output from sensor (16/24/32 bits) - left channel value contains measurement from this sensor, right channel value may replicate left channel value from I2S_DI |
| **I2S_DI**   | input     | I2S data input to sensor - may be used to merge streams from second sensor (left channel) and replicate as right channel in I2S_DO, connect to GND if there is no previous sensor in chain. |

Chaining two sensors using I2S_DO and I2S_DI allows to use only single I2S stereo channel (DIN) of MCU to receive data streams from both pitch and volume sensors.

See I2S docs for details.

Streaming of sensor values as I2S data is a feasible transport - MCU may receive it as additional audio stream using DMA, and get nice per-sample interpolated values.

Components selection
====================

* **DAC**

| Device         | Package  | Bits | MHz | Mouser price | Mouser stock | JLCPCB price | JLCPCB stock |
| -------------- | -------  | ---- | --- | ------------ | ------------ | ------------ | ------------ |
| AD9748ACPZ     | LFCSP-32 | 8    | 210 | EUR 10.74    | 926          | $7.67        | 41           |
| AD9704BCPZ     | LFCSP-32 | 8    | 175 | EUR  9.52    | 4728         | $7.94        | 73           |
| AD9707BCPZ     | LFCSP-32 | 14   | 175 | EUR 19.21    | 734          | $10.23       | 64           |
| AD9744ACPZRL7  | LFCSP-32 | 14   | 210 | EUR 21.74    | 2759         | $3.9         | 10           |
| AD9744ARUZRL7  | TSSOP-28 | 14   | 210 | EUR 24.31    | 326          | $6.03        | 115          |


* **ADC**

| Device         | Package  | Bits | MHz | Mouser price | Mouser stock | JLCPCB price | JLCPCB stock |
| -------------- | -------  | ---- | --- | ------------ | ------------ | ------------ | ------------ |
| AD9283BRSZ-100 | SSOP-20  | 8    | 100 | EUR 15.74    | 645          | $3.9         | 42           |
| AD9215BRUZ-105 | TSSOP-28 | 10   | 105 | EUR 19.65    | 1695         | $14.08       | 65           |
| AD9215BCPZ-105 | LFCSP-32 | 10   | 105 | EUR 19.14    | 1035         | $17.76       | 1            |


Sensor simulator
================

I've created an application to simulate sensor and evaluate its precision for different parameters.

It's Qt application, source code is available [here](https://github.com/fpga-theremin/sensors/tree/main/fpga_sensor/simulator/sensor_simulator)

 

