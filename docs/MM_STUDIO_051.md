# MM Studio 0.5.1

Native-bank Program Change now updates a sounding instrument through the original driver's opcode 08 handler. It preserves note pitch, gate, glide and the existing note-off. The native envelope transition is retained, rather than synthesizing a new note attack. Repeated identical programs do not restart the envelope. The held-key stack inherits changes, so legato return uses the current instrument. Released tails and fixed-duration GM one-shots retain their attack recipe.

The five example projects use the new plugin identity. Snake Man, Gravity Man and Skull Man are shortened to their openings and first main cycles. Dark Man and Knight Man already had the required length. Gravity Man's repeat boundary is shifted ten source ticks to avoid cutting a sustained lead. Skull Man repeats first-pass tuning; the original game's later-loop Pulse 2 detune carry-over remains a separate sequencing limitation.

Validation: standalone Windows build, native/processor/settings/browser tests and all 47 VST3 validator tests pass. An independent JSNES CPU execution of the original MM5 ROM matches 284 held-program fixtures exactly: 41,628 APU writes and 25,560 frame states of 17 RAM fields. Another 1,096 mapped held-change tests cover all four native game banks and compare StudioCore audio against DriverCore, including release, legato return, cross-bank selection, invalid programs and mute restoration. See verification/0.5.1 for records. These finite probes do not establish analogue console waveform equivalence.

Each example was rendered, saved and reopened in REAPER. Saved channel events are checked against the supplied MIDI. Existing 0.5.0 projects retain their old plugin identity; install 0.5.1 and replace the old instance explicitly when updating them. Historical builds and qualification remain preserved.

Other limitations remain: provisional percussion role names, MM4 Epilogue prefix coverage, and host/audio-device latency. Search/filter, state-aware loop extraction and normalized A/B comparison remain future improvements.
