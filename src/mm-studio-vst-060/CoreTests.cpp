// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include "StudioCore.h"
#include <fstream>
#include <vector>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <memory>
#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(int argc,char** argv){try{
    CHECK(argc>=2);std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> bank((std::istreambuf_iterator<char>(file)),{});
    mm5studio::DriverCore core;CHECK(core.setup(bank.data(),int(bank.size()),48000));CHECK(!core.failed());
    std::ofstream trace;
    if(argc>=3){
        trace.open(argv[2]);core.observerData=&trace;
        core.observer=[](void* p,const mm5studio::DriverCore::Write& w){*static_cast<std::ofstream*>(p)<<w.frame<<','<<w.address<<','<<w.value<<'\n';};
    }
    core.setTempo(120);core.noteOn(3,0,36,15,128);CHECK(!core.failed());
    double peak=0;int first=-1;for(int i=0;i<24000;i++){double v=core.render();CHECK(std::isfinite(v));if(std::abs(v)>.001&&first<0)first=i;peak=std::max(peak,std::abs(v));}
    std::cout<<"Pulse C4 onset "<<first<<" samples, peak "<<peak<<'\n';
    CHECK(peak>.01);CHECK(first>=0&&first<1200);CHECK(!core.failed());
    core.noteOff(3);for(int i=0;i<96000;i++)core.render();CHECK(!core.failed());
    core.observer=nullptr;
    core.silence();core.setTempo(103);core.noteOn(3,6,48,9,0);
    for(int i=0;i<30;i++)core.tick();
    core.noteOn(3,6,59,9,0,255,0,127);CHECK((core.reg(0x718+0x2b)&127)==127);CHECK(!core.failed());
    for(int i=0;i<30;i++)core.tick();CHECK(!core.failed());
    core.noteOn(3,6,48,9,0);CHECK(core.reg(0x718+0x2b)==0);
    std::cout<<"PASS: native portamento starts and resets on subsequent ordinary notes\n";
    core.silence();core.setTempo(103);core.noteOn(3,6,58,9,0);
    core.tick();core.tick();const int phase=core.reg(0x72f)&7;
    core.retune(3,59,127);CHECK((core.reg(0x72f)&7)==phase);CHECK(core.reg(0x747)==60);
    CHECK((core.reg(0x74b)|(core.reg(0x74f)<<8))==5504);
    core.tick();CHECK((core.reg(0x74b)|(core.reg(0x74f)<<8))==5396);
    std::cout<<"PASS: held A#5 to B5 native glide reaches target without a new attack\n";
    int sounding=0;
    for(int voice=0;voice<4;voice++)for(int preset=0;preset<71;preset++){
        core.silence();core.setTempo(120);core.noteOn(voice,preset,voice?48:3,voice==1?0:15,128);
        double p=0;for(int i=0;i<12000;i++)p=std::max(p,std::abs(double(core.render())));
        CHECK(!core.failed());if(p>.0001)sounding++;
        core.noteOff(voice);for(int i=0;i<1000;i++)core.render();CHECK(!core.failed());
    }
    std::cout<<"PASS: all 284 native preset/voice combinations execute; "<<sounding<<" produce signal; silent table entry retained\n";
    auto studio=std::make_unique<mm5studio::StudioCore>();CHECK(studio->setup(bank.data(),int(bank.size()),48000));
    studio->on(4,60,1);CHECK(studio->activeCount()==0&&studio->reason==mm5studio::WrongChannel);
    studio->clearWarnings();studio->on(2,48,1,1);CHECK(studio->activeCount()==1);
    studio->on(9,50,1,2);CHECK(studio->activeCount()==1&&studio->reason==mm5studio::VoiceConflict);
    studio->off(2,48,1);CHECK(studio->activeCount()==1); // stale bass off cannot kill the tom
    studio->panic();studio->bank(0,false,125);studio->bank(0,true,0);studio->program(0,17);
    mm5studio::Recipe resolved{};CHECK(studio->resolve(0,60,resolved)&&resolved.instrument==17);
    studio->bank(0,false,124);CHECK(studio->resolve(0,60,resolved));studio->program(0,17);CHECK(!studio->resolve(0,60,resolved));
    studio->reset();studio->setStrict(false);studio->on(4,60,1);studio->on(4,64,1);CHECK(studio->activeCount()==2);
    studio->setStrict(true);CHECK(studio->activeCount()==0);studio->on(9,42,1);studio->off(9,42);CHECK(studio->activeCount()==1);
    studio->panic();CHECK(studio->activeCount()==0);
    std::cout<<"PASS: strict channels, triangle sharing, stale note-off, bank latch, free polyphony, mode panic and one-shots\n";
    if(argc>=4){
        std::ofstream all(argv[3]);struct Context {std::ofstream* file;int voice,preset;};Context context{&all,0,0};
        auto observe=[](void* p,const mm5studio::DriverCore::Write& w){auto& c=*static_cast<Context*>(p);*c.file<<c.voice<<','<<c.preset<<','<<w.frame<<','<<w.address<<','<<w.value<<'\n';};
        for(int v=0;v<4;v++)for(int p=0;p<71;p++){
            core.observer=nullptr;core.silence();core.setTempo(120);context.voice=v;context.preset=p;core.observerData=&context;core.observer=observe;
            core.noteOn(v,p,v?48:3,v==1?0:15,128);for(int frame=0;frame<60;frame++)core.tick();
            core.noteOff(v);for(int frame=0;frame<30;frame++)core.tick();CHECK(!core.failed());
        }
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
