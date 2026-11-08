# MM Studio 0.6.0

The instrument now includes MIDI-to-NSF/native-data export. The **NSF / data...** dialog supports Mega Man 3, 4, 5, 6 and Omni. It validates timing, explicit sound setup and hardware voice limits before writing anything. No separate executable or ROM is required by the user. See [export instructions](NATIVE_EXPORT.md).

The bundle and instrument are now named **MM Studio**. Processor/controller IDs and saved instrument state remain compatible with 0.5.1. Replace the old 0.5.1 bundle instead of installing both with the same ID. An existing 0.5.1 Dark Man REAPER project resolved automatically to the 0.6.0 binary in an isolated REAPER configuration. The twelve existing projects remain unchanged.

## Qualification

- Standalone Windows Release build and all 47 Steinberg VST3 validator checks pass, along with the existing settings, live performance, bank, browser, source-audition and held-program tests.
- Export tests cover malformed MIDI, missing bank/program initialization, unsupported events, overlapping notes, running status across metadata, off-grid timing, legato return, held controls, GM gate translation, relocation and unavailable target instruments.
- All twelve example MIDI files export twice: original game target and Omni. Every output fits the original 8 KB data bank, with unchanged original-game driver code and instrument tables. Only the standalone single-song directory is rewritten for those targets.
- Independent JSNES CPU execution loads each exported NSF using its header's INIT/PLAY addresses. It checks all first-pass attacks, programs, pitch targets, held controls, release gates and a repeated loop under genuine driver tempo. Maximum observed attack error is two native ticks; maximum release error is four, within two native frame steps. These are frame/timing bounds, not sample-identical waveform claims.
- Unmodified Game Music Emu loads all 24 NSFs and renders non-silent audio without player warnings. All 24 assembly exports assemble/link back to their corresponding binary.
- The actual native dialog validated and saved Gravity Man in MM5 and Omni modes. Plugin identity compatibility was checked in REAPER. The dialog uses the same tested compiler; the UI harness is a development test target, not a distributed app.

Records are in `docs/verification/0.6.0`. Existing synthesis qualification remains in the 0.5.1 records. These finite checks do not certify analogue hardware or every future MIDI file.

Game-specific targets require exact instrument definitions. Omni depends on the included combined table and existing instant-legato patch when integrating into a game. Neither uses an expanded music-data layout. The private game-port importer/integration tools remain separate.
