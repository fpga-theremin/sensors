Current sensing oscillator on single OTA.
=========================================

This directory contains a collection of current sensing oscillators intended for using in Theremin.


Simple single OTA current sensing oscillator
--------------------------------------------

LC tank current is being sensed as voltage on R_sense used to drive LC tank.

LTSpice model: [bjt_current_sensing_oscillator_ota.asc](bjt_current_sensing_oscillator_ota.asc)


Simple single OTA current sensing oscillator
--------------------------------------------

Adding automatic gain control to previous model.
Now it can support wide range of inductor values, and even keep oscillation after touching of antenna.

LTSpice model: [bjt_current_sensing_oscillator_ota.asc](bjt_current_sensing_oscillator_ota_v2.asc)
![Spice model](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model.png)

Simulation results: drive signal and LC tank current
![Spice model](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model_simulation_results_drive_and_lc_tank_current.png)

Simulation results: LC tank current and antenn voltage swing
![Spice model](images/current_sensing_oscillator_ota_auto_gain_control_ltspice_model_simulation_results_antenna_swing_and_lc_current.png)


