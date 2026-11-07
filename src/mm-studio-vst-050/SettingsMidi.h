// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
namespace mm5studio {
struct SettingsSnapshot {
 int channel=0,bankMSB=125,bankLSB=0,program=0,duty=64,volume=127,expression=127,legato=0,glide=0,portamento=0,bend=8192,detune=64;
};
struct SettingsMessage {uint8_t status,data1,data2,size;};
inline std::array<SettingsMessage,11> settingsMessages(const SettingsSnapshot& s){
 const uint8_t cc=uint8_t(0xb0|std::clamp(s.channel,0,15)),pc=uint8_t(0xc0|std::clamp(s.channel,0,15)),pb=uint8_t(0xe0|std::clamp(s.channel,0,15));
 const auto v=[](int value){return uint8_t(std::clamp(value,0,127));};const int bend=std::clamp(s.bend,0,16383);
 return {{{cc,0,v(s.bankMSB),3},{cc,32,v(s.bankLSB),3},{pc,v(s.program),0,2},{cc,20,v(s.duty),3},{cc,21,v(s.detune),3},{cc,7,v(s.volume),3},{cc,11,v(s.expression),3},{cc,68,v(s.legato),3},{cc,5,v(s.glide),3},{cc,65,v(s.portamento),3},{pb,uint8_t(bend&127),uint8_t(bend>>7),3}}};
}
inline std::vector<uint8_t> settingsMidi(const SettingsSnapshot& snapshot,int note=-1,int length=480,int velocity=127,const char* trackName=nullptr){
 std::vector<uint8_t> track;const std::string voice=snapshot.channel==0?"Pulse 1":snapshot.channel==1?"Pulse 2":snapshot.channel==2?"Triangle":snapshot.channel==9?"Percussion":"Channel "+std::to_string(snapshot.channel+1);
 const std::string name=trackName?trackName:"MM5 "+voice+" settings";track.insert(track.end(),{0,0xff,3,uint8_t(name.size())});track.insert(track.end(),name.begin(),name.end());
 for(const auto& msg:settingsMessages(snapshot)){track.push_back(0);track.push_back(msg.status);track.push_back(msg.data1);if(msg.size==3)track.push_back(msg.data2);}
 // One quarter note of visible clip length; no notes, tempo, loop or end-reset events.
 if(note>=0){track.insert(track.end(),{0,uint8_t(0x90|snapshot.channel),uint8_t(std::clamp(note,0,127)),uint8_t(std::clamp(velocity,1,127))});
  const uint32_t delta=uint32_t(std::max(1,length));uint8_t vlq[5];int count=0;uint32_t value=delta;vlq[count++]=uint8_t(value&127);while(value>>=7)vlq[count++]=uint8_t((value&127)|128);while(count)track.push_back(vlq[--count]);
  track.insert(track.end(),{uint8_t(0x80|snapshot.channel),uint8_t(std::clamp(note,0,127)),0,0,0xff,0x2f,0});
 }else track.insert(track.end(),{0x83,0x60,0xff,0x2f,0});
 std::vector<uint8_t> file{'M','T','h','d',0,0,0,6,0,0,0,1,1,0xe0,'M','T','r','k'};
 const uint32_t n=uint32_t(track.size());file.insert(file.end(),{uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)});file.insert(file.end(),track.begin(),track.end());return file;
}
}
