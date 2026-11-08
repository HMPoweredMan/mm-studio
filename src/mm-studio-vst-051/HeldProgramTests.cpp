// SPDX-License-Identifier: GPL-3.0-only
#include "StudioCore.h"
#include <fstream>
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
using namespace mm5studio;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Probe{int program,voice,frame=0;std::ofstream* writes;};
static void observe(void* ctx,const DriverCore::Write& w){auto& p=*static_cast<Probe*>(ctx);*p.writes<<p.program<<','<<p.voice<<','<<p.frame<<','<<w.address<<','<<w.value<<'\n';}
int main(int argc,char** argv){try{
 check(argc==4,"driver bank, writes CSV and state CSV required");
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(f)),{});
 std::ofstream writes(argv[2]),states(argv[3]);int cases=0;
 const int fields[]={0x700,0x704,0x708,0x70c,0x710,0x714,0x718,0x71c,0x720,0x724,0x728,0x72c,0x730,0x734,0x738,0x73c,0x740};
 for(int p=0;p<71;p++)for(int voice=0;voice<4;voice++){
  DriverCore c;check(c.setup(bank.data(),int(bank.size()),48000),"driver setup");c.setTempo(120);
  Probe probe{p,voice,0,&writes};c.observer=observe;c.observerData=&probe;
  c.noteOn(voice,p,voice?36:3,voice==1?0:9,voice>=2?64:0);
  for(int frame=1;frame<=90;frame++){
   probe.frame=frame;
   if(frame==20||frame==35)c.instrument(voice,(p+7)%71);
   if(frame==50)c.instrument(voice,p);
   if(frame==60)c.noteOff(voice);
   c.tick();check(!c.failed(),"native instrument execution");
   states<<p<<','<<voice<<','<<frame;for(int field:fields)states<<','<<int(c.reg(field+0x28+voice));states<<'\n';
  }cases++;
 }
 auto c=std::make_unique<StudioCore>();DriverCore ref;
 check(c->setup(bank.data(),int(bank.size()),48000)&&ref.setup(bank.data(),int(bank.size()),48000),"core setup");
 int mapped=0;
 for(const auto& b:kNativeBanks)for(int voice=0;voice<4;voice++){
  const int ch=voice==3?0:voice==2?1:voice==1?2:9;
  c->reset();ref.silence();ref.setTempo(120);c->bank(ch,false,b.bank);c->program(ch,0);
  c->on(ch,voice?60:5,1,44);ref.noteOn(voice,nativeInstrument(b.bank,0),voice?(voice==1?48:36):10,voice==1?0:15,voice>=2?128:0);
  for(int p=0;p<b.count;p++){
   const int x=0x28+voice,oldPitch=c->nativeRegister(voice,0x71c+x),gate=c->nativeRegister(voice,0x73c+x);
   c->program(ch,p);ref.instrument(voice,nativeInstrument(b.bank,p));
   check(c->activeCount()==1,"program restarted/dropped held key");
   check(c->nativeRegister(voice,0x700+x)==nativeInstrument(b.bank,p)+1,"active instrument not changed");
   check(c->nativeRegister(voice,0x71c+x)==oldPitch&&c->nativeRegister(voice,0x73c+x)==gate,"program changed pitch/gate");
   for(int i=0;i<900;i++)check(c->render()==ref.render(),"held program audio mismatch");mapped++;
  }
  c->off(ch,voice?60:5,44);ref.noteOff(voice);c->program(ch,0);
  for(int i=0;i<2500;i++)check(c->render()==ref.render(),"new program altered released tail");
 }
 // All held legato keys must inherit the current native instrument.
 c->reset();c->bank(0,false,125);c->program(0,2);c->control(0,68,127);c->on(0,60,1,1);c->on(0,64,1,2);c->program(0,11);
 c->off(0,64,2);check(c->nativeRegister(3,0x72b)==12,"legato return restored stale instrument");
 c->off(0,60,1);check(c->activeCount()==0,"original note-off failed");
 // Invalid programs do not corrupt the active voice; cross-bank commits do update it.
 c->reset();c->bank(0,false,125);c->program(0,2);c->on(0,60,1);c->program(0,127);check(c->nativeRegister(3,0x72b)==3,"invalid program changed voice");
 c->bank(0,false,123);c->program(0,5);check(c->nativeRegister(3,0x72b)==nativeInstrument(123,5)+1,"native cross-bank change");
 // Fixed-duration GM hits must retain their attack recipe when a new bank is selected.
 c->reset();c->on(9,36,1);const int drum=c->nativeRegister(0,0x728);c->bank(9,false,125);c->program(9,11);check(c->nativeRegister(0,0x728)==drum,"program altered fixed drum hit");
 // Muted triangle remembers the new instrument without creating a sound.
 c->reset();c->bank(2,false,125);c->program(2,0);c->control(2,11,0);c->on(2,48,1);c->program(2,21);c->control(2,11,127);check(c->nativeRegister(1,0x729)==22,"triangle restore lost program");
 std::cout<<"PASS: "<<cases<<" independent native trace cases; "<<mapped<<" mapped held program changes; pitch/gate and note-off preserved, legato return, cross-bank, released-tail, fixed-drum and mute semantics\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
