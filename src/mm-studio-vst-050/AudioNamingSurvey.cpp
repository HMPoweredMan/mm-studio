// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include "SongLibrary.h"
#include "PresetCatalog.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <set>
#include <cmath>
#include <iostream>
#include <string>
using namespace mm5studio;
struct Registers{int voice,level=0,period=0;};
static void writeObserved(void* context,const DriverCore::Write& w){auto& s=*static_cast<Registers*>(context);const int base=0x4000+(s.voice^3)*4;if(w.address==base)s.level=w.value&15;if(w.address==base+2)s.period=(s.period&0x700)|w.value;if(w.address==base+3)s.period=(s.period&255)|((w.value&7)<<8);}
int main(int argc,char** argv){
 if(argc!=3)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(f)),{});const std::filesystem::path out=argv[2];std::filesystem::create_directories(out/"audio");
 DriverCore d;if(!d.setup(bank.data(),int(bank.size()),48000))return 2;
 std::ofstream frames(out/"preset-frames.csv");frames<<"bank,voice,program,frame,released,envelope,phase,volume,period\n";
 for(const auto& b:kNativeBanks)for(int voice:{3,1,0})for(int p=0;p<b.count;p++){
  d.silence();d.setTempo(120);Registers state{voice};d.observerData=&state;d.observer=writeObserved;d.noteOn(voice,nativeInstrument(b.bank,p),voice?48:6,voice==1?0:15,128);
  for(int frame=0;frame<240;frame++){if(frame==120)d.noteOff(voice);d.tick();if(d.failed())return 3;const int x=0x28+voice;frames<<b.bank<<','<<voice<<','<<p<<','<<frame<<','<<(frame>=120)<<','<<int(d.reg(0x710+x))<<','<<(d.reg(0x704+x)&7)<<','<<state.level<<','<<state.period<<'\n';}
 }
 // Every recipe gets its actual source pitch, level, tempo and already-gated duration.
 // Fixed extra tail allows spectral/envelope measurement without artificial truncation.
 std::ofstream manifest(out/"renders.csv");manifest<<"recipe,bank,voice,program,pitch,volume,duty,gate,ticks,bpm,detune,porta,group,frames\n";
 for(int i=0;i<kSoundCount;i++){
  const auto& r=kSongSounds[i];if(r.gmNote>=0)continue;const int b=recipeBank(r);d.silence();d.setTempo(r.bpm);d.observer=nullptr;
  d.noteOn(r.voice,nativeInstrument(b,r.preset),r.pitch,r.volume,r.duty,r.gate,0,r.porta,r.detune);
  const int held=std::max(1,int(std::lround(r.ticks*48000.*60/r.bpm/48))),count=std::min(held+48000,48000*12);
  std::ofstream audio(out/"audio"/(std::to_string(i)+".f32"),std::ios::binary);
  for(int n=0;n<count;n++){if(n==held)d.noteOff(r.voice);float sample=d.render();if(!std::isfinite(sample)||d.failed())return 4;audio.write(reinterpret_cast<const char*>(&sample),sizeof(sample));}
  manifest<<i<<','<<b<<','<<r.voice<<','<<r.preset<<','<<r.pitch<<','<<r.volume<<','<<r.duty<<','<<r.gate<<','<<r.ticks<<','<<r.bpm<<','<<r.detune<<','<<r.porta<<','<<recipeTimbre(i)<<','<<count<<'\n';
 }
 std::cout<<"PASS: four-game native envelope/modulation measurements and source-settings audio renders\n";
}

