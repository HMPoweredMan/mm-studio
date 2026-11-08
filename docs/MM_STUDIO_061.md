# MM Studio 0.6.1

DAW-exported MIDI can contain pan and RPN initialization, and adjacent notes can appear on separate MIDI tracks. Version 0.6.0 rejected these setup messages and sometimes treated adjacent percussion as simultaneous notes.

The exporter now releases notes before reading new attacks at the same tick, preserving bank/program/CC order. Pan is ignored with a mono-output notice. Pitch-bend-range RPN setup is recognized; non-default ranges are accepted for centered bends and rejected for non-centered bends because the plugin uses fixed +/-2 semitones. Unsupported parameter changes remain errors.

Timing quantization defaults on. With no loop markers, the full song is exported. Disable any option to use stricter validation. The default-enabled handling options are: **Ignore channels other than 1 / 2 / 3 / 10**, **Retrigger overlaps (cut previous note)**, and **End export at LOOP_END (exclude tail; cut held notes)**. The report counts excluded notes and interruptions. Retriggering does not resume an older held key. Full-file validation reports notes after LOOP_END. The explicit range option excludes later attacks/controllers before timing conversion and cuts held notes at the boundary, with counts in the report; it never rewrites the MIDI. Notes crossing LOOP_START, malformed marker pairs and genuinely missing note-offs remain errors. Overlap errors identify the actual voice and MIDI pitches.

Repeated diagnostics are grouped with occurrence counts and first/last ticks.

Regression tests cover same-tick attack/release ordering, pan invariance, RPN setup and unsafe bend ranges, explicit exclusions, retrigger gate lengths, loop-range exclusion, cut gates, missing loop markers, missing note-offs and unsupported events outside the selected range. The unchanged real-world test MIDI passes using the explicit policies and fails with actionable diagnostics when they are disabled. All 24 existing native example song binaries remain unchanged, independent NSF execution still passes, and all 47 VST3 validator checks pass. The homemade MIDI used to diagnose this issue is private and is not included in the public repository.

The source generation/build directory remains named `mm-studio-vst-060` / `vst3-studio-060`; the plugin reports version 0.6.1 and retains its stable identity. See [native export](NATIVE_EXPORT.md) for the workflow.
