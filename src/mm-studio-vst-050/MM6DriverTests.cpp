// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include <fstream>
#include <iostream>
#include <vector>
using namespace mm5studio;
struct Case {int voice,program,frame=0;std::ofstream* file;};
static void observed(void* context,const DriverCore::Write& w){auto& c=*static_cast<Case*>(context);*c.file<<c.program<<','<<c.voice<<','<<c.frame<<','<<w.address<<','<<w.value<<'\n';}
int main(int argc,char** argv){if(argc!=3)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<unsigned char> bank((std::istreambuf_iterator<char>(in)),{});std::ofstream out(argv[2]);
 for(int p=0;p<71;p++)for(int voice=0;voice<4;voice++){DriverCore core;if(!core.setup(bank.data(),int(bank.size()),48000))return 3;Case c{voice,p,0,&out};core.observerData=&c;core.observer=observed;core.setTempo(120);core.noteOn(voice,71+p,voice?48:3,voice==1?0:15,voice>=2?128:0);for(int frame=1;frame<=240;frame++){c.frame=frame;if(frame==120)core.noteOff(voice);core.tick();if(core.failed())return 4;}}
 std::cout<<"PASS: all 71 MM6 definitions across four voices, 120 held/120 release frames; register evidence written\n";
}
