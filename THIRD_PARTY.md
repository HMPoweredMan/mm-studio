# Third-party code and game data

## Original MM Studio code

Copyright (C) 2026 HMPoweredMan. GPL-3.0-only; see LICENSE. This grant applies to original project code and excludes separately identified third-party code/data.

## Game Music Emu

NES CPU/APU emulation and Blip_Buffer, Shay Green and contributors, LGPL-2.1-or-later.
Source: https://github.com/libgme/game-music-emu
Upstream revision: f68963b1de0633149b05732113d3c3113c6f63a1.
The used source tree and full license are in tools/game-music-emu. build/vst3-studio/generated/StudioCpu.cpp is derived from Nes_Cpu.cpp; its only adapter change replaces the private I/O include with DriverCpuIO.h. It retains the upstream copyright/license notice.

## Steinberg VST3 SDK

Source: https://github.com/steinbergmedia/vst3sdk
Pinned revision: 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96.
MIT license in that revision. Downloaded separately into tools/vst3sdk for compilation. VST is a trademark of Steinberg Media Technologies GmbH.

## Extracted Capcom material (not GPL-licensed by this project)

- build/vst3-studio/generated/driver-bank.bin: original Mega Man 5 sound driver/data, with the guarded runtime performance patch applied to a private playback copy.
- build/vst3-studio-050/generated/NativeBanks.h and MM6Instruments.h: game instrument definitions from Mega Man 3/4/5/6.
- SongLibrary.h and derived collection/pad data: original game song notes, control sequences and articulations. Human-readable names and catalog logic are project work; the underlying game data remains separately identified.
- The twelve MIDI/REAPER examples reproduce original game compositions: Mega Man 3 Intro, Spark Man, Snake Man, Dust Man, Skull Man, Dark Man, Gravity Man, Napalm Man, Wave Man, Flame Man, Knight Man and Tomahawk Man.

Original game audio code/data and compositions are Capcom material. The project's GPL license and third-party library licenses do not grant rights to those bytes/compositions. No complete game ROM is included.

MM5 driver identification used plasticsmoke/megaman5-disassembly-ca65 at 7fd6512fadb69fdf10ac36a55d2c53ea4248066f. The reference had a NES 2.0 header; upstream's canonical iNES output matched its PRG/CHR. Source ROM SHA-256 records: MM3 d50c957bd76c6af2c2684983e3f8d1a3904c38811cd8424a7dd1480f1d21caaa; MM4 1e06f22247921dab39c2350ae1a96cddf74e6e9d25e9d9ddf53686269547bd7f; MM5 010673ff28a6e9e29857260a5acaaff20b44dd09487a48900f6ab71fbcc66d13; MM6 6791433fd1c639d3f281522d7ab7d1e80bd01f1cdf99bad42cdf9d25b730f6d8.
