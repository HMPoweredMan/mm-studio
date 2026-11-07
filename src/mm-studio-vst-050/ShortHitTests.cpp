// SPDX-License-Identifier: GPL-3.0-only
// Regress short MIDI gates against every offset within a native audio frame.
#include "DriverCore.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
#include <cmath>
int main(int argc,char** argv){
 if(argc<2)return 2;
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(f)),{});
 mm5studio::DriverCore core;if(!core.setup(bank.data(),int(bank.size()),48000))return 3;
 core.silence();core.noteOn(0,20,7,9,0);for(int i=0;i<3;i++)core.tick();
 const int phaseBefore=core.reg(0x730);core.noteOn(3,25,48,15,128);core.noteOn(0,20,7,9,0);
 const int phaseAfter=core.reg(0x730);std::cout<<"Noise modulation phase before/after retrigger across another preset: "<<phaseBefore<<'/'<<phaseAfter<<'\n';
 const bool badContext=phaseAfter!=0;
 int missed=0;
 for(int offset=0;offset<800;offset+=16){
  core.silence();core.setTempo(103);
  for(int i=0;i<offset;i++)core.render();
  core.noteOn(0,20,7,9,0);double peak=0;
  for(int i=0;i<583;i++)peak=std::max(peak,std::abs(double(core.render())));
  core.noteOff(0);
  for(int i=0;i<4000;i++)peak=std::max(peak,std::abs(double(core.render())));
  if(core.failed())return 4;
  if(peak<.0001)missed++;
  std::cout<<offset<<','<<peak<<'\n';
 }
 std::cout<<"MISSED "<<missed<<" / 50 short percussion hits\n";
 struct Volume{int max=0;};Volume volume;
 core.silence();core.setTempo(103);core.observerData=&volume;
 core.observer=[](void* p,const mm5studio::DriverCore::Write& w){if(w.address==0x400c)static_cast<Volume*>(p)->max=std::max(static_cast<Volume*>(p)->max,w.value&15);};
 int unstable=0;
 for(int hit=0;hit<1000;hit++){
  volume.max=0;core.noteOn(3,hit%2?14:6,48+hit%12,9,0);core.noteOn(0,20,hit%16,9,0);
  for(int i=0;i<583;i++)core.render();core.noteOff(0);
  for(int i=0;i<4000;i++)core.render();if(volume.max!=9||core.failed())unstable++;
 }
 std::cout<<"UNSTABLE "<<unstable<<" / 1000 short hits with melody, varying noise periods and changing frame phase\n";
 core.silence();core.noteOn(0,20,7,9,0);core.noteOff(0);core.noteOn(0,20,7,14,0);
 for(int i=0;i<2400;i++)core.render();const bool staleRelease=(core.reg(0x72c)&7)==3;
 core.silence();volume.max=0;core.noteOn(0,20,7,9,0);core.noteOff(0);core.noteOff(0,true);
 for(int i=0;i<4000;i++)core.render();const bool ghost=volume.max>0;
 std::cout<<"Stale queued release "<<staleRelease<<", sound after immediate cancellation "<<ghost<<'\n';
 int queued=0;for(int rate:{44100,48000,96000}){
  if(!core.setup(bank.data(),int(bank.size()),rate))return 5;
  core.noteOn(0,20,7,9,0);for(int i=0;i<rate*60;i++)core.render();queued=std::max(queued,core.queuedSamples());
 }
 std::cout<<"Queued audio after 60 seconds at 44.1/48/96kHz: "<<queued<<" samples\n";
 return missed||badContext||unstable||staleRelease||ghost||queued?1:0;
}
