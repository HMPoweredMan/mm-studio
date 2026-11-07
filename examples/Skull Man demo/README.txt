Skull Man / Mega Man 4 - MM Studio 0.5.0 example

Open Skull Man - MM Studio.rpp for playback, or import the MIDI with its tempo.
Bank MSB 124 / LSB 0; channels 1 pulse 1, 2 pulse 2, 3 triangle, 10 native noise.
Keep Strict NES hardware enabled and Play pads from MIDI disabled.
The first pass is followed by one settled loop. REAPER repeats only the second pass.
This preserves the original Pulse 2 detune carry-over: the first pass begins centered; later loops retain the source +1 native offset where applicable. CC21 carries these offsets.
Both held-note bends use native glide plus pitch bend, without new note attacks.
Source gates, drum velocities, duty and held controls are retained.
Playback limitation: at ticks 4704 and 5472 of each pass, the native driver changes Pulse 1 instrument during a held note. Those program changes are retained in MIDI, but MM Studio 0.5.0 applies programs on the next attack, so those two sustained tails retain their preceding timbre in VST playback. LOOP_START and LOOP_END mark the repeat range.
The VST uses its existing CPU/APU scheduling model; this is not a bit-identical console audio recording.
These are MM4 programs. Current MM5 target import still requires expansion for definitions absent from MM5.
Source: Mega Man 4 (USA) (Rev 1), song $06, SHA256 1e06f22247921dab39c2350ae1a96cddf74e6e9d25e9d9ddf53686269547bd7f.
