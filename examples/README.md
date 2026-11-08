# Example tracks

Each folder contains a standard MIDI file and a REAPER project using MM Studio 0.5.1. Install/rescan the plugin before opening a project. No external samples or game ROM are required for playback.

| Track | Game | Bank MSB | Focus |
| --- | --- | ---: | --- |
| [Dark Man / Proto Man stages](Dark%20Man%20demo/) | Mega Man 5 | 125 | Full loop, glide/pitch bends and changing native sounds |
| [Snake Man](Snake%20Man%20demo/) | Mega Man 3 | 123 | MM3 bank, bass, short percussion and pulse interplay |
| [Gravity Man](Gravity%20Man%20demo/) | Mega Man 5 | 125 | Intro, native fine tune, rising level automation and sustained notes |
| [Knight Man](Knight%20Man%20demo/) | Mega Man 6 | 126 | Tempo changes, intro and native MM6 instruments |
| [Skull Man](Skull%20Man%20demo/) | Mega Man 4 | 124 | First-pass tuning, native held-program transitions |

Channels: 1 = pulse 1, 2 = pulse 2, 3 = triangle, 10 = native noise. Use Strict NES hardware mode and leave Play pads from MIDI disabled; the files already contain native notes and control events. REAPER projects include the plugin, four voice tracks and the intended repeat region.

LOOP_START/LOOP_END mark one main cycle. All examples contain only the opening and first loop. Gravity Man's boundary is shifted 10 native ticks to avoid cutting a sustained lead; the repeat duration stays one full native cycle. Skull Man repeats first-pass tuning, rather than the original game's later-loop detune carry-over.

Program changes during held notes use the native instrument transition in 0.5.1. These are editable sequencing examples, not bit-identical console recordings. See each folder's README and verification records.

Original game compositions/data are Capcom material, separately identified in ../THIRD_PARTY.md. The examples are not covered by the GPL grant for original MM Studio code.
