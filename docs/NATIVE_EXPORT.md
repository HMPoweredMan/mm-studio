# MIDI to NSF and native music data

The exporter is built into the Windows VST3. No separate application or game ROM is needed.

1. Compose using MM Studio, with channels 1/2/3/10 for pulse 1/pulse 2/triangle/noise. Include explicit bank MSB (CC0), bank LSB (CC32) and Program Change before each used channel's first note. Capture settings output or drag settings MIDI into the arrangement.
2. Export a standard MIDI file from the DAW, including tempo changes and the exact `LOOP_START` and `LOOP_END` markers if it should repeat. In REAPER, use File > Export project MIDI, select the intended tracks, and include the project tempo/time signature map and markers. Region boundaries alone do not supply the named MIDI markers.
3. Click **NSF / data...** in MM Studio. Browse to or paste the MIDI path. Choose **Mega Man 3, 4, 5, 6** or **Omni**, then click **Import and validate**.
4. Review the default-enabled **Ignore channels other than 1 / 2 / 3 / 10** (for intentionally excluded parts) and **Retrigger overlaps** (each new note cuts the preceding gate, with no return to the older key). Also use **End export at LOOP_END** when the export should stop at that marker: later attacks/controllers are excluded and held notes are cut there. Timing quantization and all three handling options default on; exclusions, cuts and retriggers are counted in the report. These policies affect only the export, never the source file.
5. Review the report. Choose NSF and/or native artifacts, enter a Save as path (or Browse), and click **Export files**. Changing import settings requires validation again. Disable the default-enabled options when testing strict validation. Existing destinations require overwrite confirmation.

Export runs outside the audio processing callback. The source MIDI is never rewritten. This imports a saved MIDI file; it does not capture the host's live arrangement directly.

## Target choice

**Game-specific** targets use that game's original driver code, duration tables and instrument definitions. A source instrument maps only when its eight-byte definition matches exactly. Unavailable sounds are errors, not approximations. The output song is native to the selected game, although a ROM hacker still needs to place/link it and update the game's song directory. A different game's sound can map when its bytes match. MIDI program numbers are translated into target-native instrument IDs.

**Omni** uses the MM5 driver with MM Studio's combined 226-instrument table and existing instant-legato patch. This permits sounds from multiple games. It requires that table and patch when integrating into a game; selecting Omni does not make every sound work with an unmodified MM5 instrument table. The report identifies the required changes. Neither target expands the native data layout or changes the original timing tables.

All twelve supplied songs fit the original 8 KB music-data bank in both their own game and Omni modes. Current sizes range from 1,354 to 3,638 bytes. The exporter compacts native gate lengths, tied notes and repeated instruction patterns. It does not preserve the original composition's exact byte arrangement.

## Files

- `.nsf`: self-contained, single-track, NTSC NSF v1, playable in standard NSF players. Contains the selected driver, instruments, song and INIT/PLAY wrapper. Title/author headers allow 31 ASCII characters; replacements/truncation are reported.
- `.bin`: native song header and streams at the selected origin, default `$A000`.
- `.asm`: equivalent ca65 source in segment `MUSIC`, with label-based, high-byte-first pointers. Link at the chosen origin, or relink the labels when moving it.
- `.driver.bin`: standalone 8 KB **one-song player image** at `$8000`, with its song directory pointing at the selected origin. It is not a complete game-bank replacement. Preserve unrelated game data when integrating.
- `.json`: target, byte count, loop positions, relocations, driver dependencies and diagnostics. Written with either export choice.

Native data must fit below `$C000`; the origin is configurable from `$A000` through `$BFF0`. Compiled size is checked. Raw `.bin` pointers are absolute: use the assembly or JSON relocation list when moving them. The exporter does not modify a game ROM.

## Supported MIDI and timing

Standard synchronous MIDI format 0/1 with PPQ timing is accepted. Timing is converted to the driver's 48 ticks per quarter. Off-grid events block export when **Quantize timing** is disabled; the report gives the number and maximum size of rounding adjustments. Tempo maps, native-bank program changes during sustained notes, volume, expression, duty, fine tune, native glide, legato, pitch bend and supported percussion recipes are translated.

Supported CCs: 0, 32, 7, 11, 20, 21, 5, 65, 68, 120, 121 and 123. Pitch bend uses the plugin's +/-2 semitone range and must resolve to a whole semitone. Pan (CC10) is ignored with a notice because native NES output is mono. RPN selectors CC101/100 and Data Entry CC6/38 recognize pitch-bend-range initialization. The plugin uses fixed +/-2 semitones: another configured range is accepted for centered bends, but a non-centered bend with that range is blocked rather than silently changing pitch. Other RPN Data Entry is rejected. SysEx, pressure, unsupported CCs, SMPTE timing and asynchronous format 2 are rejected. Same-tick note-offs are processed before note-ons across tracks, preventing false conflicts between adjacent drum hits. Strict hardware monophony applies; toms share triangle with bass. Missing banks/programs, voice collisions and unclosed notes produce diagnostics. Notes crossing LOOP_END require the explicit end-range policy to cut them there. Crossing LOOP_START remains an error.

Repeated diagnostics are grouped with occurrence counts and first/last ticks. With no loop markers, the full song is exported. When markers are present, they must form one exact pair. Opening state and tempo are restored at the loop boundary. Game-specific export rejects instantaneous held pitch changes that would require MM Studio's legato patch; use native glide or Omni. Native Program Change remains supported during a held note.

Native scheduling and gate counters run once per NSF PLAY frame, so an exported file is not a sample-identical render of the DAW. Completely joined release boundaries use the native `$FF` gate, up to one native tick early; these are counted in the report. Independent execution of all 24 example exports found attack errors up to two ticks and release errors up to four ticks, within two actual driver frame steps. The existing Skull Man first-pass/later-loop fine-tune distinction remains as documented in its example.
