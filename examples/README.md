# Example tracks

Each folder contains a standard MIDI file and a REAPER project using MM Studio 0.5.0. Install/rescan the plugin before opening a project. No external samples or game ROM are required for playback.

| Track | Game | Bank MSB | Focus |
| --- | --- | ---: | --- |
| [Dark Man / Proto Man stages](Dark%20Man%20demo/) | Mega Man 5 | 125 | Full loop, glide/pitch bends and changing native sounds |
| [Snake Man](Snake%20Man%20demo/) | Mega Man 3 | 123 | MM3 bank, bass, short percussion and pulse interplay |
| [Gravity Man](Gravity%20Man%20demo/) | Mega Man 5 | 125 | Intro, native fine tune, rising level automation and sustained notes |
| [Knight Man](Knight%20Man%20demo/) | Mega Man 6 | 126 | Tempo changes, intro and native MM6 instruments |
| [Skull Man](Skull%20Man%20demo/) | Mega Man 4 | 124 | First-pass/settled-loop tuning and held controls |

Channels: 1 = pulse 1, 2 = pulse 2, 3 = triangle, 10 = native noise. Use Strict NES hardware mode and leave Play pads from MIDI disabled; the files already contain native notes and control events. REAPER projects include the plugin, four voice tracks and the intended repeat region.

LOOP_START/LOOP_END mark the repeat range. Snake Man and Gravity Man retain the opening/first pass before repeating a settled main cycle. Gravity Man's boundary is shifted 10 native ticks to avoid cutting a sustained lead note; the repeat duration stays one full native cycle.

The MIDI transcriptions retain source program changes during held notes. The current VST applies those program changes at the next attack, so some sustained timbres in Snake Man, Gravity Man and Skull Man differ from the original driver. See each folder's README and validation.json for details. These are editable sequencing examples, not bit-identical console recordings.

Original game compositions/data are Capcom material, separately identified in ../THIRD_PARTY.md. The examples are not covered by the GPL grant for original MM Studio code.
