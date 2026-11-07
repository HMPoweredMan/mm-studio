# Build MM Studio 0.5.0

Windows x64 only. Install Git, CMake 3.25+, Visual Studio 2019 or 2022 with Desktop development with C++, and a Windows SDK.

```powershell
git clone https://github.com/HMPoweredMan/mm-studio.git
cd mm-studio
./tools/build.ps1
```

The script fetches the pinned VST3 SDK if absent, chooses an installed Visual Studio generator, builds the plugin and runs the native/processor/browser tests plus Steinberg's validator. Use `-Generator 'Visual Studio 16 2019' -WindowsSdkVersion '10.0.19041.0'` to match the original build toolchain.

The built bundle is `build/vst3-studio-050/cmake-release/VST3/Release/MM Studio 0.5.0.vst3`. Copy the complete folder into your host's VST3 search directory.

Generated driver, banks, song data and names are checked in, so no ROM or analysis pipeline is needed for a normal build. Changes to the sound extraction/measurement workflow are outside this standalone checkout; the existing data's provenance and limitations are documented in THIRD_PARTY.md and docs/MM_STUDIO_050.md.

Game Music Emu sources are vendored and can be modified/relinked. The source release additionally includes the pinned VST3 SDK, preserving a complete source snapshot for the binary release.
