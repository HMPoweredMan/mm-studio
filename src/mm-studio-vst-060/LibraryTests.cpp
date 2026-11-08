// SPDX-License-Identifier: GPL-3.0-only
#include "SongPreview.h"
#include "StudioCore.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <memory>
#include <iostream>
#include <cmath>
#define CHECK(x) do{if(!(x)){std::cerr<<#x<<'\n';return 1;}}while(false)
int main(int argc,char** argv){
 CHECK(argc==2);std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(f)),{});
 auto live=std::make_unique<mm5studio::StudioCore>();CHECK(live->setup(bank.data(),int(bank.size()),48000));
 live->bank(0,false,125);live->bank(0,true,0);live->program(0,7);live->on(0,60,1,1);
 const auto snapshot=live->voiceSnapshot(3);mm5studio::SongPreview preview;CHECK(preview.setup(bank.data(),int(bank.size()),48000));
 int signal=0,phrases=0,detuned=0;CHECK(mm5studio::kSongCount==122);
 for(int i=0;i<mm5studio::kSoundCount;i++){
  CHECK(preview.play(i));double peak=0;const auto& r=mm5studio::kSongSounds[i];
  for(int n=0;n<12000;n++){const auto v=preview.render();CHECK(std::isfinite(v));peak=std::max(peak,std::abs(double(v)));}
  CHECK(!preview.failed());signal+=peak>.0001;phrases+=r.cueCount>2;detuned+=r.detune!=0;
  CHECK(live->voiceSnapshot(3)==snapshot);CHECK(live->patch(0).msb==125&&live->patch(0).lsb==0&&live->patch(0).program==7);
  preview.stop();CHECK(!preview.active()&&preview.render()==0);
 }
 CHECK(!preview.play(-1)&&!preview.play(mm5studio::kSoundCount));
 // Exercise every held bend/control cue even when beyond the 250ms audio probe.
 mm5studio::DriverCore driver;CHECK(driver.setup(bank.data(),int(bank.size()),48000));
 int cueCount=0;
 for(const auto& r:mm5studio::kSongSounds){driver.silence();driver.setTempo(r.bpm);
  for(int i=0;i<r.cueCount;i++){const auto& c=mm5studio::kSoundCues[r.firstCue+i];
   if(c.kind==0)driver.noteOn(r.voice,r.gmNote>=0?r.preset:mm5studio::nativeInstrument(mm5studio::recipeBank(r),r.preset),c.pitch,c.volume,r.duty,r.gate,0,c.porta,r.detune);
   else if(c.kind==1)driver.retune(r.voice,c.pitch,c.porta);else driver.noteOff(r.voice);
   driver.tick();CHECK(!driver.failed());cueCount++;
  }
 }
 std::cout<<"PASS: "<<mm5studio::kSoundCount<<" recipes, "<<cueCount<<" cues, "<<signal<<" audio probes with signal, "<<phrases<<" phrases, "<<detuned<<" detuned recipes; live MIDI snapshot/patch unchanged; preview stop and invalid indices pass\n";
}
