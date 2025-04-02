Current Sensing Analog Front End for D-Lev Theremin
===================================================

* 3.3V power supply
* Compatible with D-Lev
* Uses current sensing method (Wallin Osillator)
* 8-BJT current sensor with internal integrator for 90 degrees phase shift
* 7 unbuffered inverters
* Drives LC tank with 13mA square for 2mH 120 Ohm inductor - with 400Vpp on antenna


LTSpice model: [dlev_afe_v01.asc](dlev_afe_v01.asc)
![Spice model](images/dlev_afe_v01_ltspice_model.png)




Simulation results
------------------

Drive waveform and inductor current: 
![Simulation results](images/dlev_afe_v01_sim_drive_and_inductor_current.png)

Inductor current and antenna voltage swing: 
![Simulation results](images/dlev_afe_v01_sim_inductor_current_and_antenna_voltage.png)

Current sensor sine and square outputs (REF, SENSE):
![Simulation results](images/dlev_afe_v01_sim_sense_and_outs.png)



Improved version with comparator
================================

A bit more expensive but better version (no issues with sensing of low current signal).

Conversion of low amplitude sine to square is a pain. Comparator may help here.

ADCMP600 is $1.83 on JLCPCB


Simulation results
------------------

LTSpice model: [dlev_afe_v01.asc](dlev_afe_v02.asc)
![Spice model](images/dlev_afe_v02_ltspice_model.png)

Current sensor sine and square outputs (REF, SENSE):
![Simulation results](images/dlev_afe_v02_sim_sense_and_outs.png)

