Current sensing oscillator on single OTA.
=========================================

This directory contains a collection of current sensing oscillators intended for using in Theremin.


Simple single OTA current sensing oscillator
--------------------------------------------

LC tank current is being sensed as voltage on R_sense used to drive LC tank.

OTA amplifies R_sense voltage and provides its current output to drive LC tank.

R_sense value, OTA load resistor R1, and OTA current control I1 values should be tuned for particular inductor parameters.


LTSpice model: [bjt_current_sensing_oscillator_ota.asc](bjt_current_sensing_oscillator_ota.asc)

![Spice model](images/current_sensing_oscillator_ota_ltspice_model.png)

Simulation results: drive signal and LC tank current
![drive signal and LC tank current](images/current_sensing_oscillator_ota_ltspice_model_simulation_results_antenna_swing_and_lc_current.png)

Simulation results: LC tank current and antenn voltage swing
![LC tank current and antenn voltage swing](images/current_sensing_oscillator_ota_ltspice_model_simulation_results_drive_and_lc_tank_current.png)


Single OTA current sensing oscillator with automatic gain control
-----------------------------------------------------------------

Adding automatic gain control to previous model.

Now it can support wide range of inductor values, and even keep oscillation after touching of antenna.

In this model, I'm using 2mH inductor with 20 Ohm serial resistance.

LTSpice model: [bjt_current_sensing_oscillator_ota_v2.asc](bjt_current_sensing_oscillator_ota_v2.asc)
![Spice model](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model.png)

Simulation results: drive signal and LC tank current
![drive signal and LC tank current](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model_simulation_results_drive_and_lc_tank_current.png)

Simulation results: LC tank current and antenn voltage swing
![LC tank current and antenn voltage swing](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model_simulation_results_antenna_swing_and_lc_current.png)


Current sensing oscillator with automatic gain control - simple current mirrors
-------------------------------------------------------------------------------

Simplified version of previous model.
Only 8 BJTs.

LTSpice model: [bjt_current_sensing_oscillator_ota_v4.asc](bjt_current_sensing_oscillator_ota_v4.asc)
![Spice model](images/current_sensing_oscillator_ota_auto_gain_control_simplified_ltspice_model.png)


