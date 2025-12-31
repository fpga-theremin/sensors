Current Sensing Theremint Oscillator With PLL and Differential Full Quadrant Sine Output
========================================================================================

- Stable amplitude for wide range of output frequencies (100KHz .. 2MHz)

- VCO is based on State Variable Filter with fully differential integrators.

- Full quadrant 0.5Vpp 1V differential voltage VCO outputs, allows to synthesize any other phases.

- Pure sine waveform 3Vpp drive signal for LC tank (less than -70dB harmonics)

- 4.5V power supply

LTSpice model: [vco_and_lc_drive_v01.asc](theremin_oscillator_bjt_pll_v01)
![Spice model](images/theremin_oscillator_bjt_pll_spice_model.png)

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

