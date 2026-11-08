Skull Man / Mega Man 4 - MM Studio 0.5.1 example

Open Skull Man - MM Studio.rpp, or import the MIDI and its tempo map.
Bank MSB 124/LSB 0; channels 1/2/3/10 = pulse 1/pulse 2/triangle/native noise.
Keep Strict NES hardware enabled and Play pads from MIDI disabled.
The example contains the opening and first main loop. LOOP_START/LOOP_END mark the repeat range.
Source notes, gates, levels, duty, fine tune, glide and held controls were checked against the original driver.
Program changes during held notes use the native instrument transition in MM Studio 0.5.1.
This shortened example repeats the first-pass tuning. In the original game, later passes retain a +1 native Pulse 2 detune in some phrases; that separate carry-over is not reproduced by repeating this first-pass MIDI.
These are editable examples, not bit-identical console recordings. Original game composition/data is Capcom material; see THIRD_PARTY.md.
