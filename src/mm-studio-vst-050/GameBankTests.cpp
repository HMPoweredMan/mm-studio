// SPDX-License-Identifier: GPL-3.0-only
#include "StudioCore.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
using namespace mm5studio;
struct Case{int game,program,voice,variant,frame=0;std::ofstream* file;};
static void observed(void* context,const DriverCore::Write& w){auto& c=*static_cast<Case*>(context);*c.file<<c.game<<','<<c.program<<','<<c.voice<<','<<c.variant<<','<<c.frame<<','<<w.address<<','<<w.value<<'\n';}
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 check(argc==3,"bank and register output required");std::ifstream in(argv[1],std::ios::binary);std::vector<unsigned char> bank((std::istreambuf_iterator<char>(in)),{});std::ofstream out(argv[2]);
 int cases=0;
 for(const auto& b:kNativeBanks)if(b.game==3||b.game==4)for(int p=0;p<b.count;p++)for(int voice=0;voice<4;voice++)for(int variant=0;variant<2;variant++){
  DriverCore core;check(core.setup(bank.data(),int(bank.size()),48000),"driver setup");Case c{b.game,p,voice,variant,0,&out};core.observerData=&c;core.observer=observed;core.setTempo(variant?180:90);
  core.noteOn(voice,nativeInstrument(b.bank,p),voice?(variant?60:36):(variant?12:3),voice==1?0:variant?9:15,voice>=2?(variant?192:64):0);
  for(int frame=1;frame<=240;frame++){c.frame=frame;if(voice&&frame==60)core.detune(voice,variant?-3:2);if(voice&&frame==180)core.detune(voice,0);if(frame==120)core.noteOff(voice);core.tick();check(!core.failed(),"mapped driver execution");}cases++;
 }
 // Native reference sample comparison verifies all CC values while held and in release.
 auto studio=std::make_unique<StudioCore>();check(studio->setup(bank.data(),int(bank.size()),48000),"core setup");DriverCore ref;check(ref.setup(bank.data(),int(bank.size()),48000),"reference setup");
 for(int ch:{0,1,2}){
  const int voice=ch==0?3:ch==1?2:1;studio->reset();ref.silence();ref.setTempo(120);studio->bank(ch,false,123);studio->program(ch,9);studio->on(ch,60,1);ref.noteOn(voice,nativeInstrument(123,9),60-(voice==1?12:24),voice==1?0:15,voice>=2?128:0);
  for(int v=0;v<128;v++){studio->control(ch,21,v);ref.detune(voice,v-64);check(studio->nativeRegister(voice,0x714+0x28+voice)==uint8_t(v-64),"native signed detune memory");for(int i=0;i<1000;i++)check(studio->render()==ref.render(),"CC21 must preserve envelope/audio against native held detune");}
  studio->off(ch,60);ref.noteOff(voice);studio->control(ch,21,61);ref.detune(voice,-3);for(int i=0;i<4000;i++)check(studio->render()==ref.render(),"detune on release tail");studio->control(ch,121,0);check(studio->controlValue(ch,21)==64,"controller reset centers tuning");
 }
 Recipe r{};for(const auto& b:kNativeBanks){studio->bank(0,false,b.bank);studio->bank(0,true,0);studio->program(0,b.count-1);check(studio->resolve(0,60,r)&&r.instrument==nativeInstrument(b.bank,b.count-1),"last valid program maps");studio->program(0,b.count);check(!studio->resolve(0,60,r),"program beyond actual bank rejected");}
 std::cout<<"PASS: "<<cases<<" original-driver comparison fixtures; all 128 fine-tune values across three pitched voices, held/release audio, reset and program boundaries\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
