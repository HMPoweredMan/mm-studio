// SPDX-License-Identifier: GPL-3.0-only
#include "StudioCore.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <memory>
#include <iostream>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::cerr<<__LINE__<<": "<<#x<<'\n';return 1;}}while(false)
using namespace mm5studio;
int main(int argc,char** argv){
 CHECK(argc==2);std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(file)),{});
 auto core=std::make_unique<StudioCore>();DriverCore reference;CHECK(core->setup(bank.data(),int(bank.size()),48000));CHECK(reference.setup(bank.data(),int(bank.size()),48000));
 auto patch=[&](int ch,int program){core->bank(ch,false,125);core->bank(ch,true,0);core->program(ch,program);};
 auto shadow=[&](int voice){return core->nativeRegister(voice,0x70c+0x28+voice);};
 for(int value=0;value<128;value++){core->reset();patch(0,0);core->control(0,20,value);core->on(0,60,1);CHECK((shadow(3)&0xc0)==(value/32)*64);}
 core->reset();patch(0,0);core->on(0,60,1);for(int i=0;i<10000;i++)core->render();
 const int phase=core->nativeRegister(3,0x704+0x2b),envelope=core->nativeRegister(3,0x710+0x2b);
 core->control(0,11,64);CHECK((shadow(3)&15)==8);core->control(0,7,64);CHECK((shadow(3)&15)==4);core->control(0,11,0);CHECK((shadow(3)&15)==0);core->control(0,20,0);CHECK((shadow(3)&0xc0)==0);
 CHECK(core->nativeRegister(3,0x704+0x2b)==phase&&core->nativeRegister(3,0x710+0x2b)==envelope);
 core->control(0,68,127);core->portamento(0,true,1);CHECK(core->bend(0,4096));core->control(0,121,0);
 CHECK(core->controlValue(0,7)==64&&core->controlValue(0,11)==127&&core->controlValue(0,20)==64&&core->controlValue(0,68)==0&&core->portaSpeed(0)==0);
 CHECK(core->patch(0).msb==125&&core->patch(0).program==0&&(shadow(3)&15)==8&&(shadow(3)&0xc0)==128);
 // Compare legato, return-to-held-note and release against direct native calls.
 core->reset();reference.silence();reference.setTempo(120);patch(0,6);core->control(0,68,127);core->on(0,60,1,10);reference.noteOn(3,6,36,15,128);
 for(int i=0;i<12000;i++)CHECK(core->render()==reference.render());
 core->on(0,64,1,11);reference.retune(3,40,0);for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->off(0,64,11);reference.retune(3,36,0);for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->off(0,60,10);reference.noteOff(3);for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 CHECK(core->activeCount()==0);
 // Releasing an older key must not release the current key.
 core->reset();patch(0,6);core->control(0,68,127);core->on(0,60,1,1);core->on(0,64,1,2);core->off(0,60,1);CHECK(core->activeCount()==1&&((core->voiceSnapshot(3)>>11)&127)==64);core->control(0,123,0);CHECK(core->activeCount()==0);
 // Hard silence kills release tails and pending attacks without changing other voices.
 core->reset();reference.silence();reference.setTempo(120);patch(0,0);core->on(0,60,1);reference.noteOn(3,0,36,15,128);
 for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->control(0,123,0);reference.noteOff(3);for(int i=0;i<1000;i++)CHECK(core->render()==reference.render());
 core->control(0,120,0);reference.kill(3);double killTail=0;for(int i=0;i<6000;i++){const auto sample=core->render();CHECK(sample==reference.render());if(i>4000)killTail=std::max(killTail,std::abs(double(sample)));}CHECK(killTail<.0001);
 core->reset();patch(0,6);core->on(0,60,1);core->control(0,120,0);double peak=0;for(int i=0;i<4000;i++)peak=std::max(peak,std::abs(double(core->render())));CHECK(peak==0);
 // Triangle volume is a gate, with native reattack on restore, not a fake gain control.
 core->reset();reference.silence();reference.setTempo(120);patch(2,0);core->on(2,48,1);reference.noteOn(1,0,36,0,0);
 for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->control(2,11,0);reference.kill(1);for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->control(2,11,1);reference.noteOn(1,0,36,0,0);for(int i=0;i<4000;i++)CHECK(core->render()==reference.render());
 core->bank(0,false,80);core->program(0,0);Recipe r{};CHECK(!core->resolve(0,60,r));core->bank(0,false,125);core->bank(0,true,1);core->program(0,0);CHECK(!core->resolve(0,60,r));
 // Controller state belongs to the channel; bass/tom ownership remains shared.
 core->reset();core->control(0,7,0);core->on(1,60,1);CHECK(((core->voiceSnapshot(2)>>18)&15)==15);core->on(2,48,1);core->on(9,41,1);CHECK(core->activeCount()==2);core->off(2,48);CHECK(core->activeCount()==2);
 core->reset();core->setStrict(false);core->control(0,68,127);core->on(0,60,1);core->on(0,64,1);CHECK(core->activeCount()==1);core->off(0,64);CHECK(core->activeCount()==1);core->setStrict(true);
 reference.silence();reference.noteOn(3,0,95,15,128);CHECK((reference.reg(0x720+0x2b)|(reference.reg(0x724+0x2b)<<8))==20);
 std::cout<<"PASS: 128 duty values; native level quantization and held changes; legato/held-key return; reset defaults; release versus hard silence; triangle gate; channel isolation; removed combination bank; shared bass/tom voice\n";
}
