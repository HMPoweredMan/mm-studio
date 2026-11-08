// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "DriverCore.h"
#include "StudioMap.h"
#include <array>
#include <cmath>
namespace mm5studio {
enum Reason { Ok,WrongChannel,WrongBank,WrongProgram,PitchRange,VoiceConflict,EngineFault,UnsupportedEvent };
inline constexpr const char* kReasons[]={"Ready","Unsupported MIDI channel","Unsupported bank","Unsupported program","Pitch outside driver range","Hardware voice interrupted","Driver execution fault","Unsupported MIDI controller/event"};
inline constexpr int kPerformancePrograms[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
inline constexpr int kPerformanceBanks[16]={125,125,125,125,125,125,125,125,125,0,125,125,125,125,125,125};
class StudioCore {
public:
 struct Patch{int pendingMSB=0,pendingLSB=0,msb=0,lsb=0,program=0;};
 struct Held{bool down=false;int id=-1;uint64_t age=0;Recipe recipe{};float velocity=0;};
 struct Slot{bool active=false;int channel=-1,note=-1,id=-1;double remaining=0;uint64_t age=0;Recipe recipe{};float velocity=0;bool muted=false;};
 static constexpr int kMachines=8;
 bool setup(const uint8_t* bank,int size,int rate){sampleRate=rate;for(auto& c:cores)if(!c.setup(bank,size,rate))return false;reset();return true;}
 void reset(){for(auto& c:cores)c.silence();slots={};held={};lastVoices={};for(int ch=0;ch<16;ch++)patches[ch]={kPerformanceBanks[ch],0,kPerformanceBanks[ch],0,kPerformancePrograms[ch]};portaTime.fill(0);portaEnabled.fill(false);bends.fill(0);dutyCC.fill(64);detuneCC.fill(64);volumeCC.fill(127);expressionCC.fill(127);legato.fill(false);warnings=0;reason=Ok;lastChannel=0;serial=0;setTempo(120);}
 void panic(){for(auto& c:cores)c.silence();slots={};held={};lastVoices={};setTempo(bpm);}
 void setStrict(bool v){if(strict!=v){panic();strict=v;}}
 void setTempo(double v){if(std::isfinite(v)&&v>=10&&v<=1000){bpm=v;for(auto& c:cores)c.setTempo(v);}}
 void clearWarnings(){warnings=0;reason=Ok;}
 void warn(Reason why,int ch){reason=why;lastChannel=ch+1;warnings++;}
 void bank(int ch,bool lsb,int v){if(ch>=0&&ch<16)(lsb?patches[ch].pendingLSB:patches[ch].pendingMSB)=std::clamp(v,0,127);}
 void program(int ch,int v){
  if(ch<0||ch>=16)return;auto& p=patches[ch];const bool wasNative=nativeProgramCount(p.msb)&&p.lsb==0;
  p.program=std::clamp(v,0,127);p.msb=p.pendingMSB;p.lsb=p.pendingLSB;
  const int instrument=p.lsb==0?nativeInstrument(p.msb,p.program):-1;
  if(!wasNative||instrument<0)return;
  // Native-bank commits update held native voices through opcode 08. Keep
  // released tails and fixed-duration GM one-shots frozen at their attack.
  for(auto& h:held[ch])if(h.down&&!h.recipe.ticks)h.recipe.instrument=instrument;
  for(int i=0;i<kMachines*4;i++){
   auto& s=slots[i];if(!s.active||s.channel!=ch||s.recipe.ticks)continue;
   s.recipe.instrument=instrument;const int voice=i%4;
   lastVoices[voice].preset=instrument;lastVoices[voice].program=p.program;
   if(!s.muted)cores[i/4].instrument(voice,instrument);
   if(cores[i/4].failed())warn(EngineFault,ch);
  }
 }
 int portaSpeed(int ch)const{return portaEnabled[ch]?std::clamp(128-portaTime[ch],1,127):0;}
 void portamento(int ch,bool enable,int time){if(ch<0||ch>15)return;portaEnabled[ch]=enable;portaTime[ch]=std::clamp(time,0,127);for(int i=0;i<kMachines*4;i++)if(slots[i].channel==ch&&i%4)cores[i/4].portamento(i%4,portaSpeed(ch));}
 bool bend(int ch,int raw){
  if(ch<0||ch>15)return false;const double value=raw==8191?2.:raw/4096.;const int semi=int(std::lround(value));
  if(std::abs(value-semi)>1./8192){warn(UnsupportedEvent,ch);return false;}
  for(int i=0;i<kMachines*4;i++)if(slots[i].active&&slots[i].channel==ch&&i%4)if(slots[i].recipe.pitch+semi<0||slots[i].recipe.pitch+semi>95){warn(PitchRange,ch);return false;}
  bends[ch]=semi;for(int i=0;i<kMachines*4;i++)if(slots[i].active&&slots[i].channel==ch&&i%4&&!slots[i].muted)cores[i/4].retune(i%4,slots[i].recipe.pitch+semi,portaSpeed(ch));return true;
 }
 int controlValue(int ch,int cc)const{if(ch<0||ch>15)return 0;return cc==21?detuneCC[ch]:cc==7?volumeCC[ch]:cc==11?expressionCC[ch]:cc==20?dutyCC[ch]:cc==68?int(legato[ch])*127:cc==5?portaTime[ch]:cc==65?int(portaEnabled[ch])*127:0;}
 void control(int ch,int cc,int value){
  if(ch<0||ch>15)return;value=std::clamp(value,0,127);
  if(cc==120){allOff(ch,true);return;}if(cc==123){allOff(ch,false);return;}
  if(cc==5||cc==65){portamento(ch,cc==65?value>=64:portaEnabled[ch],cc==5?value:portaTime[ch]);return;}
  if(cc==21){if(kChannelVoices[ch]==0&&value!=64){warn(UnsupportedEvent,ch);return;}detuneCC[ch]=value;for(int i=0;i<kMachines*4;i++)if(slots[i].channel==ch&&i%4)cores[i/4].detune(i%4,value-64);return;}
  if(cc==121){detuneCC[ch]=64;for(int i=0;i<kMachines*4;i++)if(slots[i].channel==ch&&i%4)cores[i/4].detune(i%4,0);expressionCC[ch]=127;dutyCC[ch]=64;legato[ch]=false;portamento(ch,false,0);bend(ch,0);refresh(ch);return;}
  if(cc==68){legato[ch]=value>=64;return;}
  if(cc==7)volumeCC[ch]=value;else if(cc==11)expressionCC[ch]=value;else if(cc==20)dutyCC[ch]=value;else{warn(UnsupportedEvent,ch);return;}refresh(ch);
 }
 bool resolve(int ch,int note,Recipe& out)const{
  if(ch<0||ch>15||note<0||note>127)return false;int voice=kChannelVoices[ch];if(voice<0){if(strict)return false;voice=3;}
  const auto& p=patches[ch];out={0,voice,0,0,255,15,0};
  if(nativeProgramCount(p.msb)){if(p.lsb)return false;out.instrument=nativeInstrument(p.msb,p.program);if(out.instrument<0)return false;out.period=15-note;if(!voice&&note>15)return false;}
  else if(p.msb==0){
   if(ch==9){if(p.program||p.lsb)return false;bool found=false;for(const auto& d:kDrums)if(d.note==note){out=d.recipe;found=true;}return found;}
   if(p.lsb||p.program!=kDefaultPrograms[ch])return false;out.instrument=kGmInstruments[ch];
  }else return false;
  if(voice){out.pitch=note-(voice==1?12:24);if(out.pitch<9||out.pitch>95)return false;}return true;
 }
 void on(int ch,int note,float velocity,int id=-1){
  if(ch<0||ch>15||note<0||note>127)return;if(velocity<=0){off(ch,note,id);return;}Recipe r{};
  if(!resolve(ch,note,r)){const auto& p=patches[ch];warn(strict&&kChannelVoices[ch]<0?WrongChannel:p.msb!=0&&!nativeProgramCount(p.msb)?WrongBank:p.lsb?WrongBank:nativeProgramCount(p.msb)&&p.program>=nativeProgramCount(p.msb)?WrongProgram:PitchRange,ch);return;}
  if(r.voice&&(r.pitch+bends[ch]<0||r.pitch+bends[ch]>95)){warn(PitchRange,ch);return;}
  const uint64_t age=++serial;Held h{true,id,age,r,std::clamp(velocity,0.f,1.f)};int machine=0;const int voice=r.voice;
  if(!strict){
   bool reuse=false;if(legato[ch]&&voice>0&&!r.ticks)for(int m=0;m<kMachines;m++){const auto& s=slots[m*4+voice];if(s.active&&s.channel==ch&&!s.recipe.ticks){machine=m;reuse=true;break;}}
   if(!reuse){for(machine=0;machine<kMachines;machine++)if(!slots[machine*4+voice].active)break;if(machine==kMachines){machine=0;for(int m=1;m<kMachines;m++)if(slots[m*4+voice].age<slots[machine*4+voice].age)machine=m;warn(VoiceConflict,ch);}}
  }
  const int index=machine*4+voice;auto& previous=slots[index];
  const bool connected=legato[ch]&&voice>0&&!r.ticks&&previous.active&&previous.channel==ch&&!previous.recipe.ticks&&previous.recipe.instrument==r.instrument;
  if(previous.active&&!connected){warn(VoiceConflict,ch);for(auto& key:held[previous.channel])if(key.recipe.voice==voice)key.down=false;}
  held[ch][note]=h;start(index,ch,note,h,connected);
 }
 void off(int ch,int note,int id=-1){
  if(ch<0||ch>15||note<0||note>127)return;auto& key=held[ch][note];if(id>=0&&key.id>=0&&id!=key.id)return;key.down=false;
  for(int i=0;i<kMachines*4;i++){auto& s=slots[i];if(!s.active||s.channel!=ch||s.note!=note||(id>=0&&s.id>=0&&s.id!=id)||s.remaining>0)continue;
   int next=-1;if(legato[ch]&&i%4)for(int n=0;n<128;n++){const auto& h=held[ch][n];if(h.down&&!h.recipe.ticks&&h.recipe.voice==i%4&&(next<0||h.age>held[ch][next].age))next=n;}
   if(next>=0){const auto h=held[ch][next];start(i,ch,next,h,s.recipe.instrument==h.recipe.instrument);}
   else{cores[i/4].noteOff(i%4);s.active=false;}
  }
 }
 void allOff(int ch,bool immediate=true){if(ch<0||ch>15)return;for(auto& h:held[ch])h.down=false;for(int i=0;i<kMachines*4;i++)if(slots[i].channel==ch){if(immediate)cores[i/4].kill(i%4);else if(slots[i].active)cores[i/4].noteOff(i%4);slots[i].active=false;slots[i].remaining=0;}}
 float render(){float result=0;for(int m=0;m<(strict?1:kMachines);m++)result+=cores[m].render();for(int i=0;i<kMachines*4;i++){auto& s=slots[i];if(s.active&&s.remaining>0&&--s.remaining<=0){cores[i/4].noteOff(i%4);s.active=false;}}return result;}
 const Patch& patch(int ch)const{return patches[ch];}
 int activeCount()const{int n=0;for(const auto& s:slots)n+=s.active;return n;}
 int latency()const{return cores[0].outputLatency();}
 int nativeRegister(int voice,int address)const{return voice>=0&&voice<4?cores[0].reg(address):0;}
 uint64_t voiceSnapshot(int voice)const{
  const Slot* latest=nullptr;for(int i=voice;i<kMachines*4;i+=4)if(slots[i].channel>=0&&(!latest||slots[i].age>latest->age))latest=&slots[i];if(!latest)return 0;const int ch=latest->channel;const auto& v=lastVoices[voice];
  return (1ull<<35)|uint64_t(ch)|(uint64_t(v.program)<<4)|(uint64_t(latest->note)<<11)|(uint64_t(v.volume)<<18)|(uint64_t(v.duty/64)<<22)|(uint64_t(latest->active&&!latest->muted)<<24)|(uint64_t(bends[ch]+2)<<25)|(uint64_t(portaSpeed(ch))<<28);
 }
 uint32_t warnings=0;Reason reason=Ok;int lastChannel=0;
private:
 struct VoiceRecord{int preset=0,volume=0,duty=0,program=0;};
 int level(const Slot& s)const{if(s.recipe.voice==1)return volumeCC[s.channel]&&expressionCC[s.channel]?15:0;return std::clamp(int(std::floor(double(s.velocity)*s.recipe.volume*volumeCC[s.channel]*expressionCC[s.channel]/(127.*127.)+.5)),0,15);}
 void start(int index,int ch,int note,const Held& h,bool connected){
  auto& s=slots[index];const bool wasMuted=s.muted;s={true,ch,note,h.id,h.recipe.ticks?sampleRate*60./bpm*h.recipe.ticks/48.:0,h.age,h.recipe,h.velocity,false};
  const int voice=index%4,vol=level(s),duty=voice>=2?(dutyCC[ch]/32)*64:0;s.muted=voice==1&&!vol;
  lastVoices[voice]={h.recipe.instrument,voice==1?0:vol,duty,patches[ch].program};
  if(s.muted){cores[index/4].kill(voice);return;}
  if(connected&&!wasMuted){cores[index/4].detune(voice,detuneCC[ch]-64);cores[index/4].levelDuty(voice,voice==1?0:vol,duty);cores[index/4].retune(voice,h.recipe.pitch+bends[ch],portaSpeed(ch));}
  else cores[index/4].noteOn(voice,h.recipe.instrument,voice?h.recipe.pitch+bends[ch]:h.recipe.period,voice==1?0:vol,duty,h.recipe.gate,h.recipe.ticks,voice?portaSpeed(ch):0,voice?detuneCC[ch]-64:0);
  if(cores[index/4].failed())warn(EngineFault,ch);
 }
 void refresh(int ch){for(int i=0;i<kMachines*4;i++){auto& s=slots[i];if(s.channel!=ch)continue;const int voice=i%4,vol=level(s),duty=voice>=2?(dutyCC[ch]/32)*64:0;
  if(voice==1){if(!vol&&!s.muted){cores[i/4].kill(voice);s.muted=true;}else if(vol&&s.muted&&s.active){s.muted=false;cores[i/4].noteOn(voice,s.recipe.instrument,s.recipe.pitch+bends[ch],0,0,s.recipe.gate,0,portaSpeed(ch),detuneCC[ch]-64);}}
  else cores[i/4].levelDuty(voice,vol,duty);
  lastVoices[voice]={s.recipe.instrument,voice==1?0:vol,duty,lastVoices[voice].program};
 }}
 std::array<DriverCore,kMachines> cores;std::array<Slot,kMachines*4> slots{};std::array<Patch,16> patches{};
 std::array<std::array<Held,128>,16> held{};std::array<VoiceRecord,4> lastVoices{};
 std::array<int,16> portaTime{},bends{},dutyCC{},detuneCC{},volumeCC{},expressionCC{};std::array<bool,16> portaEnabled{},legato{};
 double bpm=120;int sampleRate=48000;bool strict=true;uint64_t serial=0;
};
}
