# MM Studio 0.5.0

The native-instrument collection is first for each game. The legacy Reviewed GM kit mapping collection is hidden from the browser while existing saved MIDI/state addresses stay compatible.

MM3/MM4/MM6 sounds have measured waveform, envelope and modulation descriptions. The program selector uses voice-appropriate names. Noise pads use simple numbered families; 759 source articulations were measured against reviewed MM5 references, with 124 Kick, 287 Snare, 139 Cymbal and 209 generic Percussion results. Those role assignments are provisional pending listening review. Existing reviewed MM5 names remain.

31 fine-tuned variants previously grouped together are separate browser entries. Volume-only melodic duplicates remain merged. Native instrument tables and playback data retain the previous release's behavior.

Known limitations: program changes during a held note apply at the next attack; MM4 Epilogue has documented prefix coverage for its very long joint recurrence; percussion subtypes and uncertain roles need listening review; hardware-device latency depends on host/device settings. State-aware loop extraction, search/filter and normalized A/B comparison are future improvements.

Historical 0.5.0 qualification: 47 VST3 validator tests passed; native bank, performance, live MIDI/pad, settings and browser tests passed; 1,056 MM3/MM4 original-driver register probes matched; Knight Man REAPER playback was sample-identical to the preceding qualified release. The standalone public checkout reruns its native/processor tests and VST3 validator; see docs/verification/0.5.0 for results. Finite emulator/host checks are not hardware fidelity certification.
