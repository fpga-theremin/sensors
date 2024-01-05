create_clock -period 2.880 -name CLK -waveform {0.000 1.440} [get_ports -filter { NAME =~  "*CLK*" && DIRECTION == "IN" }]
