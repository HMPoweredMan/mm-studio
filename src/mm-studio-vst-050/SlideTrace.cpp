// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <iterator>
#include <sstream>
int main(int argc,char** argv){
 if(argc!=4)return 1;
 std::ifstream bankFile(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(bankFile)),{});
 mm5studio::DriverCore core;if(!core.setup(bank.data(),int(bank.size()),48000))return 2;core.setTempo(103);
 struct Note{int voice,start,end,preset,pitch,volume,duty,porta;};std::vector<Note> notes;
 std::ifstream input(argv[2]);Note n;while(input>>n.voice>>n.start>>n.end>>n.preset>>n.pitch>>n.volume>>n.duty>>n.porta)notes.push_back(n);
 int apu[24]{};core.observerData=apu;core.observer=[](void* p,const mm5studio::DriverCore::Write& w){static_cast<int*>(p)[w.address-0x4000]=w.value;};
 std::ofstream out(argv[3]);out<<"frame,porta,target,period,flags,apu2lo,apu2hi,phase2,env2\n";
 for(int frame=0;frame<4608;frame++){
  if(frame)core.tick();
  for(auto& e:notes)if(e.end==frame)core.noteOff(e.voice);
  for(auto& e:notes)if(e.start==frame)core.noteOn(e.voice,e.preset,e.pitch,e.volume,e.duty,255,0,e.porta);
  out<<frame<<','<<int(core.reg(0x743))<<','<<int(core.reg(0x747))<<','<<(int(core.reg(0x74b))+(int(core.reg(0x74f))<<8))<<','<<int(core.reg(0x72f))<<','<<apu[6]<<','<<apu[7]<<','<<int(core.reg(0x72e))<<','<<int(core.reg(0x732))<<'\n';
  if(core.failed())return 3;
 }
}
