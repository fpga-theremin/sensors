Current Sensing Theremint Oscillator With PLL and Differential Full Quadrant Sine Output
========================================================================================

- VCO is based on State Variable Filter with fully differential integrators.

- Full quadrant 0.5Vpp 1V differential voltage VCO outputs, allows to synthesize any other phases.

- Pure sine waveform 3Vpp drive signal for LC tank (less than -70dB harmonics)

- 4.5V power supply (draws about 20mA when driving LC tank with 10mA current)

- Stable amplitude for wide range of output frequencies (100KHz .. 2MHz)

- PCB transformer as a current sensor

- Analog multiplier as a precise phase detector


LTSpice model: [theremin_oscillator_bjt_pll_v01.asc](theremin_oscillator_bjt_pll_v01.asc)
![Spice model](images/theremin_oscillator_bjt_pll_spice_model.png)

The schematic is expected to be cheap, with PCB manufacturing and assembly using JLCPCB.

* about 60 BJTs, most of them should be matching pairs
* LMH6642 is used as LC tank drive buffer, can provide up to 75mA current, with close to R/R output voltage.
* MCP6121 is used as Control Voltage buffer


Antenna voltage and inductor current

![Simulation results](images/theremin_oscillator_bjt_pll_sim_ant_voltage_and_inductor_current.png)

Control voltage

![Simulation results](images/theremin_oscillator_bjt_pll_sim_control_voltage.png)

Antenna voltage and inductor current when locked

![Simulation results](images/theremin_oscillator_bjt_pll_sim_ant_voltage_and_inductor_current_locked.png)

Drive signal voltage and current when locked

![Simulation results](images/theremin_oscillator_bjt_pll_sim_drive_voltage_and_current_locked.png)

Drive signal FFT in locked state

![Simulation results](images/theremin_oscillator_bjt_pll_sim_drive_signal_fft.png)

VCO differential output

![Simulation results](images/theremin_oscillator_bjt_pll_sim_vco_output.png)


Simplified LM13700 based SVF VCO
================================

Simplified LM13700 based voltage controlled oscillator.

Still provides full quadrant output (two sine signals shifted by 90 degrees), but not differential as in previous case.

Opamps used in VCO may drive LC tank directly with close to rails amplitude (0.2V minimal offset from rails is recommended).

LTSpice model: [ota_svf_vco_v01.asc](ota_svf_vco_v01.asc)

![Spice model](images/ota_svg_vco_ltspice_model.png)

- Stable amplitude at frequency range 200KHz-2MHz

- Single LM13700, two LMH6642 (or single LMH6643), 4 BJTs. 

- Working range may be adjusted by R10, R20 to cover the desired frequency range

- R18 controls amplitude of the output signals, while R17 is responsible for keeping the amplitude constant for the range of frequencies

- R14, R12 provide fine-tuning for input bias, should be tuned to keep the same zero point of both VCO outputs.


Simulation results
------------------

Output signal amplitude in wide frequency range (sweep 400KHz to 1600KHz)


![Simulation results](images/ota_svg_vco_sim_outs_full_range.png)

Zoom in of VCO output signals

![Simulation results](images/ota_svg_vco_sim_outs_zoomin.png)


Resonance visible in antenna voltage and inductor current while drive frequency is crossing LC resonant frequency

![Simulation results](images/ota_svg_vco_sim_ant_voltage_and_drive_current.png)

							
Consumed current from power lines
							
![Simulation results](images/ota_svg_vco_sim_power.png)



Simplified LM13700 PLL based Theremin Sensor
============================================

Adding phase detector on LM13700 OTA.


LTSpice model: [ota_svf_vco_pll_sensor_v01.asc](ota_svf_vco_pll_sensor_v01.asc)

![Spice model](images/ota_svf_vco_pll_sensor_ltspice_model.png)

Simulation results
------------------

Antenna voltage

![Simulation results](images/ota_svf_vco_pll_sensor_sim_ant_voltage.png)

Control Voltage and inductor current - PLL locking process

![Simulation results](images/ota_svf_vco_pll_sensor_sim_control_voltage_ind_current.png)
	
Drive signal voltage and inductor current - when locked

![Simulation results](images/ota_svf_vco_pll_sensor_sim_drive_v_inductor_current.png)

Drive signal spectrum

![Simulation results](images/ota_svf_vco_pll_sensor_sim_drive_spectrum.png)
	
VCO outputs - two sines with 90 degrees phase shift

![Simulation results](images/ota_svf_vco_pll_sensor_sim_vco_sine_outputs.png)

Square outputs - 4 different phases

![Simulation results](images/ota_svf_vco_pll_sensor_sim_square_outputs.png)




