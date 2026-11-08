# Example tracks

Each folder contains a standard MIDI file and a REAPER project using MM Studio 0.5.1. Install/rescan the plugin before opening a project. No external samples or game ROM are required for playback.

| Track | Game | Bank MSB | Focus |
| --- | --- | ---: | --- |
| [Dark Man / Proto Man stages](Dark%20Man%20demo/) | Mega Man 5 | 125 | Full loop, glide/pitch bends and changing native sounds |
| [Snake Man](Snake%20Man%20demo/) | Mega Man 3 | 123 | MM3 bank, bass, short percussion and pulse interplay |
| [Gravity Man](Gravity%20Man%20demo/) | Mega Man 5 | 125 | Intro, native fine tune, rising level automation and sustained notes |
| [Knight Man](Knight%20Man%20demo/) | Mega Man 6 | 126 | Tempo changes, intro and native MM6 instruments |
| [Skull Man](Skull%20Man%20demo/) | Mega Man 4 | 124 | First-pass tuning, native held-program transitions |
| [Mega Man 3 Intro](Mega%20Man%203%20Intro%20demo/) | Mega Man 3 | 123 | Full opening, tempo changes and held instrument changes |
| [Spark Man](Spark%20Man%20demo/) | Mega Man 3 | 123 | Pulse interplay, native instrument changes and short percussion |
| [Dust Man](Dust%20Man%20demo/) | Mega Man 4 | 124 | Opening, source fine tune and held instrument changes |
| [Napalm Man](Napalm%20Man%20demo/) | Mega Man 5 | 125 | Held pitch targets and a state-qualified repeat boundary |
| [Wave Man](Wave%20Man%20demo/) | Mega Man 5 | 125 | Showcase: 27 native glide attacks, held triangle bend and percussion |
| [Flame Man](Flame%20Man%20demo/) | Mega Man 6 | 126 | Five tempo settings, expressive levels and held instrument changes |
| [Tomahawk Man](Tomahawk%20Man%20demo/) | Mega Man 6 | 126 | Native MM6 instruments, triangle bass and source percussion |

Channels: 1 = pulse 1, 2 = pulse 2, 3 = triangle, 10 = native noise. Use Strict NES hardware mode and leave Play pads from MIDI disabled; the files already contain native notes and control events. REAPER projects include the plugin, four voice tracks and the intended repeat region.

LOOP_START/LOOP_END mark one main cycle. Examples contain the one-time opening followed by one repeat cycle. The seven added examples compare two source cycles before choosing a boundary, retaining first-pass state differences when necessary. Gravity Man's boundary is shifted 10 native ticks to avoid cutting a sustained lead; the repeat duration stays one full native cycle. Skull Man repeats first-pass tuning, rather than the original game's later-loop detune carry-over.

Program changes during held notes use the native instrument transition in 0.5.1. These are editable sequencing examples, not bit-identical console recordings. See each folder's README and verification records.

Original game compositions/data are Capcom material, separately identified in ../THIRD_PARTY.md. The examples are not covered by the GPL grant for original MM Studio code.
