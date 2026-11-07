Gravity Man / Mega Man 5 - MM Studio 0.5.0 example

Open Gravity Man - MM Studio.rpp, or import the MIDI and tempo map.
Bank MSB 125/LSB 0; channels 1/2/3/10 = pulse 1/pulse 2/triangle/native noise.
Keep Strict NES hardware enabled and Play pads from MIDI disabled.
The original opening and first pass are retained; LOOP_START/LOOP_END enclose a settled native cycle.
Source attacks, pitches, gates, native levels, duty, fine tune, glide and held controls were checked against the original driver.
Program changes while held are retained but apply at the next attack in the VST. See validation.json for affected events.
Original game composition/data is Capcom material; see THIRD_PARTY.md.
The repeat boundary is shifted 10 native ticks past the phrase boundary so a sustained lead note is not cut. The repeat duration remains one complete native cycle.
