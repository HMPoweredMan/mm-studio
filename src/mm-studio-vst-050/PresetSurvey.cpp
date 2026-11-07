// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
struct Output{int voice=0,volume=0,period=0;};
int main(int argc,char** argv){if(argc!=3)return 1;std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(f)),{});mm5studio::DriverCore d;if(!d.setup(bank.data(),int(bank.size()),48000))return 2;std::ofstream out(argv[2]);out<<"voice,preset,frame,released,envelope,phase,volume,period\n";
 for(int voice:{3,1,0})for(int p=0;p<71;p++){d.silence();d.setTempo(120);Output state{voice};d.observerData=&state;d.observer=[](void* ptr,const mm5studio::DriverCore::Write& w){auto& s=*static_cast<Output*>(ptr);const int base=0x4000+(s.voice^3)*4;if(w.address==base)s.volume=w.value&15;if(w.address==base+2)s.period=(s.period&0x700)|w.value;if(w.address==base+3)s.period=(s.period&255)|((w.value&7)<<8);};d.noteOn(voice,p,voice?48:6,voice==1?0:15,128);
  for(int frame=0;frame<240;frame++){if(frame==120)d.noteOff(voice);d.tick();if(d.failed())return 3;const int x=0x28+voice;out<<voice<<','<<p<<','<<frame<<','<<(frame>=120)<<','<<int(d.reg(0x710+x))<<','<<(d.reg(0x704+x)&7)<<','<<state.volume<<','<<state.period<<'\n';}
 }
 std::cout<<"PASS: measured 71 native presets on pulse, triangle and noise, 120 held +120 release frames each\n";
}
