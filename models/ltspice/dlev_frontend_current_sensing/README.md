Current sensing D-Lev Theremin Analog Front-End
===============================================

This model is intended to be used as D-Lev AFE drop-in replacement.

Advantages
----------

* Current sensing approach - less noise (sensing point is isolated from antenna by inductor)
* Sine drive waveform probably can reduce noise (3rd sallen-key LP filter used to convert square drive signal to sine wave)
* Can drive high Q inductors
* Although current through LC tank at resonance is in-phase with drive signal, this schematic provides 90 degrees D-Lev compatible phase shift at resonance.


Interface
---------

* Drive input: square 3.3V signal from FPGA
* Feedback output: squared copy of a drive signal passed to antenna, shifted by 90 degrees for D-Lev DPLL compatibility.
* Sense output: squared copy of current sense signal.
* Power supply: +5V, two linear regulators - for 4.5V and 3.3V
* Ground


Schematic
---------

LTSpice model: [dlev_frontend_curr_sens_v01.asc](dlev_frontend_curr_sens_v01.asc)

![Spice model](images/dlev_afe_current_sensing_ltspice_model.png)

In the simulation below, 2mH inductor having 120 Ohms serial resistance is used.

With such inductor and 2.4Vpp drive, draws about 15mA from power supply.


Simplified schematic - LP filter can reuse output buffer.

LTSpice model: [dlev_frontend_curr_sens_v01.asc](dlev_frontend_curr_sens_v02.asc)

![Spice model](images/dlev_afe_current_sensing_ltspice_model_simplified.png)



Converting 3.3V square drive signal to ~24Vpp sine centered near 2.25V:

![Sumulation results](images/ltspice_sim_square_input_to_sine.png)

Drive voltage and current (at resonance)

![Sumulation results](images/ltspice_sim_drive_voltage_and_current.png)

Inductor current and antenna voltage	

![Sumulation results](images/ltspice_sim_inductor_current_and_antenna_voltage.png)

Outputs when frequency is close to resonance

![Sumulation results](images/ltspice_sim_outputs_resonance.png)

Outputs when frequency is 10KHz higher than resonance

![Sumulation results](images/ltspice_sim_outputs_higher_freq.png)

Outputs when frequency is 10KHz lower than resonance

![Sumulation results](images/ltspice_sim_outputs_lower_freq.png)


Extreme mode: inductor current and antenna voltage with high Q inductor (20 Ohm serial resistance; R_sense reduced to 2 Ohms)\

Expected to see 1200 Vpp on antenna and 40mA drive strength. Draws about 30mA from power supply in this mode.

![Sumulation results](images/ltspice_sim_ind_current_ant_voltage_res_high_q.png)


Components
----------

It's possible to get rid of BJTs, by adding IC for current feedback opamp, and buffer opamp IC.


Current feedback opamp on discrete BJTs may be replaced with IC. Possible candidates:

* LT6210 - can work from 3V, up to 80mA drive current, EUR3.91 on Mouser, not in stock on JLCPCB (estimated price $2.16)
* LT6211 - dual, can work from 3V, up to 75mA drive current, EUR 3.99 on Mouser, not in stock on JLCPCB
* LMH6723 - can work from 4.5V EUR2.5 on Mouser, not in stock on JLCPCB
* LT1395 - min supply voltage 4V, up to 80mA drive current, Mouser price $3.38, not in stock on JLCPCB (estimated price $1.7)
* AD8014 - min supply 4.5V, drives 40-50mA load, EUR 4.54 on Mouser, in stock $2.99 on JLCPCB
* AD8000 - min supply 4.5V, drives 100mA, $5.7 on JLCPCB
* OPA2675 - dual, min supply 4.5V, can drive 1000mA!!! mouser price EUR3.28, in stock on JLCPCB $4.47
* AD8002 - dual, min supply 6V, up to 70mA drive, JLCPCB price $0.07 but manufacturer is Idchip, and is called Audio amplifier
* OPA683 - min supply voltage 5V
* EL5160 - min supply voltage 5V
* ADA4860 - min supply voltage 5V
* AD8007 - min supply voltage 5V
* OPA2673 - dual, min supply 5.75V but in stock on JLCPCB $1.67
* CLC450, CLC452 - datasheet says it can only work from 5V, not in stock on JLCPCB
* OPA695 - minimum supply 5V, $2.66 on JLCPCB
* TBD: more options?

LP filter output buffer can be replaced with some cheap opamp. Possible candidates:

* TBD

Comparators

* ADCMP600 - $2.05 on JLCPCB, big stock
* TBD: something cheaper?



