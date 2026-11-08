// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DriverCore.h"
#include "SongLibrary.h"
namespace mm5studio {
// Separate CPU/APU: browser auditions never modify the live MIDI instrument.
class SongPreview {
public:
 bool setup(const uint8_t* bank,int size,int rate){sampleRate=rate;selected=-1;return driver.setup(bank,size,rate);}
 bool play(int index){if(index<0||index>=kSoundCount)return false;driver.silence();selected=index;position=0;next=0;driver.setTempo(kSongSounds[index].bpm);return true;}
 void stop(){driver.silence();selected=-1;}
 float render(){
  if(selected<0)return 0;const auto& s=kSongSounds[selected];const double tick=position*s.bpm*48/(60.*sampleRate);
  while(next<s.cueCount&&kSoundCues[s.firstCue+next].tick<=tick){const auto& c=kSoundCues[s.firstCue+next++];
   if(c.kind==0)driver.noteOn(s.voice,(s.gmNote>=0?s.preset:nativeInstrument(recipeBank(s),s.preset)),c.pitch,c.volume,s.duty,s.gate,0,c.porta,s.detune);
   else if(c.kind==1)driver.retune(s.voice,c.pitch,c.porta);else driver.noteOff(s.voice);
  }
  position++;const float sample=driver.render();
  const auto& end=kSoundCues[s.firstCue+s.cueCount-1];if(tick>end.tick+4*s.bpm*48/60){stop();}
  return sample;
 }
 bool failed()const{return driver.failed();}
 bool active()const{return selected>=0;}
private:
 DriverCore driver;int sampleRate=48000,selected=-1,next=0;double position=0;
};
}
