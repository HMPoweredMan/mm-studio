# MM Studio

A Windows VST3 instrument for composing and performing with the Mega Man 3, 4, 5 and 6 NES sound-driver instruments.

[Download the Windows release](https://github.com/HMPoweredMan/mm-studio/releases/latest) Â· [REAPER setup](docs/REAPER.md) Â· [Build from source](BUILDING.md) Â· [MIDI controls](docs/MIDI.md)

## What it does

- Two pulse voices, triangle bass/toms and noise percussion, with a strict hardware mode.
- Game and song collections, named sound recipes and song-specific drum pads.
- Live duty, volume, expression, glide, pitch bend and fine-tune controls, with MIDI Learn.
- Program/CC output for recording settings, plus draggable settings and drum articulations.

Current release: **0.5.1**, Windows x64, VST3. REAPER is the tested host. Other operating systems and plugin formats are not currently supported. Normal playback uses the included instrument data and does not require loading a game ROM.

## Install

Download and extract the release ZIP. Copy the entire `MM Studio 0.5.1.vst3` folder to `%COMMONPROGRAMFILES%\VST3` or a VST3 directory configured in your DAW. Rescan plugins and add **MM Studio 0.5.1** as an instrument. The release includes the REAPER bank file and Dark Man, Snake Man, Gravity Man, Knight Man and Skull Man MIDI projects.

MIDI channels are 1 = Pulse 1, 2 = Pulse 2, 3 = Triangle, and 10 = Percussion. Toms share the triangle voice with bass. Use the game/song navigator to find sounds; program numbers select native instruments and CCs control their performance settings.

See [REAPER setup](docs/REAPER.md) for recording pad clicks or settings output and for dragging MIDI while stopped.

## Example projects

See [examples](examples/README.md) for five editable MIDI files and ready-to-play REAPER projects with tempo maps, sound settings and loop regions.

## Compatibility and limitations

Hardware voices are monophonic in strict mode. Toms share the triangle voice with bass. Native-bank program changes update sounding notes through the driver. Some percussion names are provisional. See [0.5.1 notes and validation](docs/MM_STUDIO_051.md).

This public project contains the instrument, presets, examples and build sources. Game integration and the separate MIDI importer are outside its scope.

## Source and licensing

MM Studio's original code is **GPL-3.0-only**; see [LICENSE](LICENSE). Game Music Emu components retain LGPL-2.1-or-later notices. Steinberg's VST3 SDK has its own license and is fetched at a pinned revision for builds.

The embedded Capcom sound-driver bytes, game instrument definitions and original-song collection/demo data are identified separately and are **not covered by the project's GPL grant**. This repository's license does not grant rights to that material. See [THIRD_PARTY.md](THIRD_PARTY.md) for provenance and scope. MM Studio is an unofficial project and is not affiliated with Capcom or Steinberg.
