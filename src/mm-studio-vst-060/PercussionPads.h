// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SongLibrary.h"
#include "ExactPads.h"
#include "PadLabels.h"
#include "SettingsMidi.h"
#include <vector>
#include <set>
#include <tuple>
#include <algorithm>
#include <cmath>
namespace mm5studio {
inline int padNote(const SongSound& r){return r.gmNote>=0?r.gmNote:15-r.pitch;}
inline double padBeats(const SongSound& r){return std::max(1,r.ticks)/48.;}
inline bool sourcePad(const SongSound& r){return r.gmNote<0&&r.voice==0&&r.song!=254&&r.song!=255&&r.song!=511&&r.song!=767&&r.song!=1023;}
inline int sourceVelocity(const SongSound& r){return sourcePad(r)?std::clamp(int(std::lround(r.volume*127./15)),1,127):127;}
inline SettingsSnapshot padSettings(SettingsSnapshot s,const SongSound& r){s.channel=9;s.bankMSB=recipeBank(r);s.bankLSB=0;s.program=r.gmNote>=0?0:r.preset;if(sourcePad(r)){s.volume=127;s.expression=127;}s.duty=64;s.legato=0;s.glide=0;s.portamento=0;s.bend=8192;s.detune=64;return s;}
inline std::vector<uint8_t> padMidi(SettingsSnapshot s,int index){if(index<0||index>=kSoundCount) return {};const auto& r=kSongSounds[index];if(r.voice!=0&&r.gmNote<0)return {};return settingsMidi(padSettings(s,r),padNote(r),int(padBeats(r)*480+.5),sourceVelocity(r),kPadNames[index]);}
// Keep pitch and note length: one native envelope can produce an entire kit.
// Song pads enumerate all source levels and effective lengths. Synthetic kits use their own mappings.
inline std::vector<int> percussionPads(int collection){
 std::vector<int> result;if(collection<0||collection>=kSongCount)return result;if(collection>=kMM6FirstCollection){for(int i=0;i<kCollectionPadCounts[collection];i++)result.push_back(kCollectionPadStarts[collection]+i);return result;}if(collection<27){for(int i=0;i<kExactPadCounts[collection];i++)result.push_back(kExactPadStarts[collection]+i);return result;}
 std::set<std::tuple<int,int,int,int,int>> seen;const auto& s=kSongs[collection];
 for(int i=s.first;i<s.first+s.count;i++){const auto& r=kSongSounds[i];if(r.voice!=0&&r.gmNote<0)continue;
  auto key=std::make_tuple(r.gmNote>=0?0:125,r.gmNote>=0?0:r.preset,padNote(r),r.gmNote>=0?0:r.ticks,r.gmNote>=0?0:r.gate);
  if(seen.insert(key).second)result.push_back(i);
 }return result;
}
}
