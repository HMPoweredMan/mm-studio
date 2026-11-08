# MIDI controls

Use channels 1 and 2 for pulse, 3 for triangle and 10 for noise. Strict hardware mode permits one active note per voice. Triangle bass and toms share a voice.

Native banks use CC0 (bank MSB): 123 = MM3, 124 = MM4, 125 = MM5, 126 = MM6. CC32 (bank LSB) is 0. Send bank selection before Program Change; banks are committed by Program Change. The included REAPER .reabank supplies program names. MIDI files use zero-based program numbers.

| Message | Control |
| --- | --- |
| CC7 / CC11 | Volume / expression |
| CC20 | Pulse duty |
| CC21 | Native fine tune; 64 is centered |
| CC5 / CC65 | Glide time / portamento switch |
| CC68 | Legato switch |
| Pitch bend | Two semitones each direction; supported targets are whole semitones |

In 0.5.1, valid native-bank program changes also update the currently held native instrument through the driver's instrument-change operation. Pitch, gate and glide are preserved; the new instrument follows its native envelope transition. Repeating the same program does not retrigger it. Bank selection alone does not alter the sounding voice. Released tails and fixed-duration GM drum hits retain their attack recipe.

Use the plugin's settings and pad MIDI output, or drag its MIDI files into the piano roll, to capture complete recipes. See REAPER.md for routing. MIDI export and the public VST do not themselves convert a song into game data; the separate game importer is outside this release.
