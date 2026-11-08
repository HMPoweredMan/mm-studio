// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "StudioMap.h"
#include "NativeBanks.h"
#include "PerformanceDriverPatch.h"
#include "ExportDrivers.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mm5studio::exporter {
using Bytes=std::vector<uint8_t>;
struct Options {int game=0,origin=0xa000;bool quantize=true,ignoreUnusedChannels=true,retriggerOverlaps=true,endAtLoopEnd=true;std::string title="MM Studio song",author;};
struct Issue {bool error;std::string text;int tick=-1,channel=-1,count=1,lastTick=-1;};
struct Event {int tick=0,order=0,type=0,ch=0,a=0,b=0;std::string text;};
struct Values {int instrument=0,pitch=0,period=0,level=15,duty=128,detune=0,porta=0;bool operator==(const Values& b)const{return instrument==b.instrument&&pitch==b.pitch&&period==b.period&&level==b.level&&duty==b.duty&&detune==b.detune&&porta==b.porta;}};
struct Change {int tick;Values value;};
struct Note {int start=0,end=0,ch=0,voice=0,key=0,gate=255;Values value;std::vector<Change> changes;};
struct Tempo {int tick=0,us=500000;};
struct Relocation {int position=0,target=0;};
struct Result {
 std::vector<Issue> issues;std::vector<Note> notes;std::vector<Tempo> tempos;std::array<std::vector<Change>,4> controls;
 Bytes song,driver,nsf;std::vector<Relocation> relocations;std::array<int,4> offsets{};
 int end=0,loopStart=-1,loopEnd=-1,ppq=0;bool requiresPerformancePatch=false;std::string assembly,report;
 bool failed=false;int suppressedIssues=0;bool ok()const{return !failed;}
 void issue(std::string text,int tick=-1,int ch=-1,bool error=true){failed=failed||error;for(auto& i:issues)if(i.error==error&&i.channel==ch&&i.text==text){i.count++;i.lastTick=std::max(i.lastTick,tick);return;}if(issues.size()<200)issues.push_back({error,std::move(text),tick,ch,1,tick});else suppressedIssues++;}
 std::string summary()const{std::ostringstream s;s<<(ok()?"Validation passed":"Export blocked")<<"\r\n"<<notes.size()<<" notes; "<<tempos.size()<<" tempo settings; "<<song.size()<<" native bytes.\r\n";if(loopStart>=0)s<<"Loop: ticks "<<loopStart<<".."<<loopEnd<<" (48 ticks per quarter).\r\n";for(const auto& i:issues){s<<(i.error?"ERROR: ":"NOTE: ")<<i.text;if(i.tick>=0)s<<" [first tick "<<i.tick<<']';if(i.channel>=0)s<<" [MIDI channel "<<i.channel+1<<']';if(i.count>1){s<<" ["<<i.count<<" occurrences";if(i.lastTick>=0)s<<", last tick "<<i.lastTick;s<<']';}s<<"\r\n";}if(suppressedIssues)s<<suppressedIssues<<" additional diagnostics omitted (200 distinct-message limit).\r\n";return s.str();}
};
inline std::string quoted(const std::string& text){std::ostringstream s;s<<'"';for(unsigned char c:text){if(c=='"'||c=='\\')s<<'\\'<<c;else if(c<32)s<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;else s<<c;}return s.str()+'"';}
struct Reader {
 const Bytes& data;size_t p=0,end;
 explicit Reader(const Bytes& b):data(b),end(b.size()){}
 int byte(){if(p>=end)throw std::runtime_error("Truncated MIDI data.");return data[p++];}
 uint32_t be(int n){uint32_t v=0;while(n--)v=(v<<8)|byte();return v;}
 uint32_t vlq(){uint32_t v=0;for(int i=0;i<4;i++){int b=byte();v=(v<<7)|(b&127);if(!(b&128))return v;}throw std::runtime_error("Invalid MIDI variable-length number.");}
 void skip(size_t n){if(n>end-p)throw std::runtime_error("Truncated MIDI event.");p+=n;}
};
inline std::vector<Event> parse(const Bytes& data,Result& r){
 if(data.size()>16*1024*1024)throw std::runtime_error("MIDI exceeds the 16 MiB import limit.");Reader rd(data);
 if(rd.be(4)!=0x4d546864)throw std::runtime_error("Expected a standard .mid file (MThd header).");const auto h=rd.be(4);if(h<6||h>rd.end-rd.p)throw std::runtime_error("Invalid MIDI header.");
 int format=rd.be(2),tracks=rd.be(2);r.ppq=rd.be(2);rd.skip(h-6);
 if(format>1||tracks<1||tracks>256||(format==0&&tracks!=1))throw std::runtime_error("Only synchronous MIDI format 0 or 1 is supported (up to 256 tracks).");
 if(!r.ppq||(r.ppq&0x8000))throw std::runtime_error("MIDI must use ticks per quarter, not SMPTE timecode.");std::vector<Event> events;int order=0;
 for(int tr=0;tr<tracks;tr++){
  if(rd.be(4)!=0x4d54726b)throw std::runtime_error("Expected a MIDI track chunk.");size_t length=rd.be(4);if(length>rd.end-rd.p)throw std::runtime_error("Truncated MIDI track.");size_t trackEnd=rd.p+length,oldEnd=rd.end;rd.end=trackEnd;uint64_t tick=0;int running=0;bool ended=false;
  while(rd.p<trackEnd){if(events.size()>200000)throw std::runtime_error("MIDI exceeds 200,000 relevant events.");tick+=rd.vlq();if(tick*48/r.ppq>1000000)throw std::runtime_error("Song exceeds one million native ticks.");double exact=double(tick)*48/r.ppq;int time=int(std::lround(exact));
   time=int(tick);int status=rd.byte(),first=-1;if(status<128){if(!running)throw std::runtime_error("MIDI running status has no preceding channel message.");first=status;status=running;}
   if(status==255){int type=rd.byte();size_t n=rd.vlq();if(n>rd.end-rd.p)throw std::runtime_error("Truncated MIDI metadata.");
    if(type==0x51){if(n!=3)throw std::runtime_error("Invalid MIDI tempo event.");int us=rd.be(3);if(us<60000||us>6000000)throw std::runtime_error("Tempo must be within MM Studio's 10..1000 BPM range.");events.push_back({time,order++,0x51,0,us});}
    else if(type==6){std::string text(data.begin()+rd.p,data.begin()+rd.p+n);rd.skip(n);if(text=="LOOP_START"||text=="LOOP_END")events.push_back({time,order++,6,0,0,0,text});}
    else if(type==0x2f){if(n)throw std::runtime_error("Invalid end-of-track metadata.");ended=true;if(rd.p!=trackEnd)throw std::runtime_error("Unexpected bytes after end of MIDI track.");}
    else rd.skip(n);
    continue;
   }
   if(status==0xf0||status==0xf7){running=0;size_t n=rd.vlq();rd.skip(n);events.push_back({time,order++,0xf0});continue;}
   if(status<0x80||status>0xef)throw std::runtime_error("Unsupported MIDI system status.");running=status;int type=status&0xf0,ch=status&15,a=first>=0?first:rd.byte(),b=type==0xc0||type==0xd0?0:rd.byte();
   if(a>127||b>127)throw std::runtime_error("Invalid MIDI channel-data byte.");
   events.push_back({time,order++,type,ch,a,b});if(events.size()>200000)throw std::runtime_error("MIDI exceeds 200,000 relevant events.");
  }
  r.end=std::max(r.end,int(tick));if(!ended)throw std::runtime_error("MIDI track is missing its end-of-track event.");rd.end=oldEnd;
 }
 if(rd.p!=rd.end)throw std::runtime_error("Unexpected trailing MIDI chunks.");
 std::stable_sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.tick<b.tick;});return events;
}
inline void retime(std::vector<Event>& events,Result& r,bool quantize){
 int originalEnd=r.end,quantized=0;double maxError=0;std::map<std::pair<int,int>,int> on;
 for(auto& e:events){int original=e.tick;if(e.type==0x90&&e.b)on[{e.ch,e.a}]=original;else if(e.type==0x80||(e.type==0x90&&!e.b)){auto i=on.find({e.ch,e.a});if(i!=on.end()&&original<=i->second)r.issue("A MIDI note has zero duration.",original,e.ch);on.erase({e.ch,e.a});}
  double exact=double(original)*48/r.ppq;
  e.tick=int(std::lround(exact));if(std::abs(exact-e.tick)>1e-6){if(quantize){quantized++;maxError=std::max(maxError,std::abs(exact-e.tick));}else r.issue("Event is off the native 48-tick grid. Quantize it in the DAW or enable timing quantization.",e.tick,e.ch);}
 }
 r.end=int(std::lround(double(originalEnd)*48/r.ppq));
 if(quantized){std::ostringstream s;s<<"Timing snapped to 48 ticks per quarter; "<<quantized<<" events adjusted, maximum rounding "<<std::fixed<<std::setprecision(3)<<maxError<<" native ticks.";r.issue(s.str(),-1,-1,false);}
}
// Range selection is an import policy, never a rewrite of the MIDI source.
// Apply before quantization: events outside the chosen range cannot affect it.
inline void selectExportRange(std::vector<Event>& events,Result& r,const Options& options){
 if(!options.endAtLoopEnd)return;
 int start=-1,end=-1,starts=0,ends=0;for(const auto& e:events)if(e.type==6){if(e.text=="LOOP_START"){start=e.tick;starts++;}else{end=e.tick;ends++;}}
 if(!starts&&!ends)return; // Without a marked loop the full song remains the export range.
 if(starts!=1||ends!=1||start<0||end<=start||end>r.end){r.issue("End export at LOOP_END requires exactly one valid LOOP_START / LOOP_END pair.");return;}
 std::array<std::array<bool,128>,16> held{},laterOff{};std::vector<Event> selected;int omittedNotes=0,omittedOther=0,clipped=0;
 auto off=[](const Event& e){return e.type==0x80||(e.type==0x90&&!e.b);};
 for(const auto& e:events){
  if(options.ignoreUnusedChannels&&e.type>=0x80&&kChannelVoices[e.ch]<0){if(e.tick<end)selected.push_back(e);else if(e.type==0x90&&e.b)omittedNotes++;else omittedOther++;continue;}
  bool include=e.tick<end||(e.tick==end&&(e.type==6||off(e)));
  if(include){selected.push_back(e);if(e.type==0x90&&e.b)held[e.ch][e.a]=true;else if(off(e))held[e.ch][e.a]=false;}
  else{if(e.type==0x90&&e.b)omittedNotes++;else omittedOther++;if(off(e))laterOff[e.ch][e.a]=true;}
 }
 for(int ch=0;ch<16;ch++)for(int key=0;key<128;key++)if(held[ch][key]){if(!laterOff[ch][key]){r.issue("A note is missing its final note-off; range selection cannot infer its intended length.",end,ch);continue;}selected.push_back({end,0,0x80,ch,key,0});clipped++;}
 std::stable_sort(selected.begin(),selected.end(),[](const Event& a,const Event& b){return a.tick<b.tick;});events=std::move(selected);r.end=end;
 r.issue("Export ends at LOOP_END: "+std::to_string(omittedNotes)+" later note attacks and "+std::to_string(omittedOther)+" other events excluded; "+std::to_string(clipped)+" held notes cut at the boundary. Source MIDI unchanged.",-1,-1,false);
}
struct Channel {int pendingMSB=-1,pendingLSB=-1,msb=-1,lsb=-1,program=-1,volume=127,expression=127,duty=64,detune=64,portaTime=0,bend=0,rpnMSB=127,rpnLSB=127,bendSemitones=2,bendCents=0;bool porta=false,legato=false;};
struct Key {bool down=false;int ch=0,key=0,velocity=0;uint64_t age=0;Recipe recipe{};};
struct Slot {bool active=false,muted=false,killed=false;int ch=-1,key=-1,note=-1,fixedEnd=-1;Key held;Values value;};
inline int stockInstrument(int instrument,const uint8_t* bank){if(instrument<71)return instrument;const auto* p=kAdditionalInstruments+(instrument-71)*8;for(int i=0;i<71;i++)if(std::equal(p,p+8,bank+0xadb+i*8))return i;return -1;}
inline const uint8_t* targetDriver(int game,const uint8_t* mm5){return game==3?kExportDriver3:game==4?kExportDriver4:game==6?kExportDriver6:mm5;}
inline int targetInstrument(int instrument,const uint8_t* mm5,int game,int preferred=-1){if(!game)return instrument;const auto* source=instrument<71?mm5+0xadb+instrument*8:kAdditionalInstruments+(instrument-71)*8;const auto* target=targetDriver(game,mm5);int offset=game==3?0xab5:game==4?0xad1:game==6?0xb0f:0xadb,count=game==3?61:71;if(game==5&&instrument<71)preferred=instrument;if(preferred>=0&&preferred<count&&std::equal(source,source+8,target+offset+preferred*8))return preferred;for(int i=0;i<count;i++)if(std::equal(source,source+8,target+offset+i*8))return i;return -1;}
inline bool recipe(const Channel& s,int ch,int key,Recipe& out,Result& r,int tick,const Options& options,const uint8_t* driver){
 if(kChannelVoices[ch]<0){r.issue("Only channels 1, 2, 3 and 10 map to hardware voices.",tick,ch);return false;}
 if(s.program<0||s.msb<0||s.lsb<0){r.issue("Missing explicit bank MSB, bank LSB or Program Change before the first note.",tick,ch);return false;}
 int voice=kChannelVoices[ch];out={0,voice,0,0,255,15,0};
 if(nativeProgramCount(s.msb)&&s.lsb==0){out.instrument=nativeInstrument(s.msb,s.program);out.period=15-key;if(out.instrument<0){r.issue("Program is outside the selected game's instrument bank.",tick,ch);return false;}if(!voice&&key>15){r.issue("Native noise notes must be 0..15; GM drums require bank 0, program 0.",tick,ch);return false;}}
 else if(s.msb==0&&s.lsb==0){if(ch==9){if(s.program!=0){r.issue("GM percussion requires program 0.",tick,ch);return false;}bool found=false;for(const auto& d:kDrums)if(d.note==key){out=d.recipe;found=true;break;}if(!found){r.issue("GM drum note has no reviewed MM Studio mapping.",tick,ch);return false;}int mapped=targetInstrument(out.instrument,driver,options.game);if(mapped<0){r.issue("This GM drum sound is unavailable in the selected game. Choose Omni or another articulation.",tick,ch);return false;}out.instrument=mapped;return true;}
  if(s.program!=kDefaultPrograms[ch]){r.issue("GM program has no MM Studio voice mapping. Select a native game bank.",tick,ch);return false;}out.instrument=kGmInstruments[ch];
 }else{r.issue("Unsupported bank. Use a native bank 123..126, LSB 0, or a supported GM mapping.",tick,ch);return false;}
 if(options.game){int mapped=targetInstrument(out.instrument,driver,options.game,s.msb==120+options.game?s.program:-1);if(mapped<0){r.issue("This instrument is absent from the selected game. Choose Omni or another sound.",tick,ch);return false;}out.instrument=mapped;}
 if(voice){out.pitch=key-(voice==1?12:24);if(out.pitch<9||out.pitch>95){r.issue("Note is outside the native voice pitch range.",tick,ch);return false;}}return true;
}
inline Values values(const Key& key,const Channel& s){const auto& rec=key.recipe;Values v;v.instrument=rec.instrument;v.pitch=rec.pitch+s.bend;v.period=rec.period;v.level=rec.voice==1?(s.volume&&s.expression?15:0):std::clamp(int(std::floor(double(key.velocity)*rec.volume*s.volume*s.expression/(127.*127.*127.)+.5)),0,15);v.duty=rec.voice>=2?(s.duty/32)*64:0;v.detune=(s.detune-64)&255;v.porta=rec.voice&&s.porta?std::clamp(128-s.portaTime,1,127):0;return v;}
inline void model(const std::vector<Event>& events,Result& r,const Options& options,const uint8_t* driver){
 std::array<Channel,16> channels;std::array<std::array<Key,128>,16> keys{};std::array<std::array<bool,128>,16> rejected{};std::array<Slot,4> slots;uint64_t age=0;int starts=0,ends=0;bool anyNotes=false;
 auto finish=[&](Slot& s,int tick){if(s.note>=0){auto& n=r.notes[s.note];n.end=tick;r.end=std::max(r.end,tick);while(!n.changes.empty()&&n.changes.back().tick>=tick)n.changes.pop_back();if(tick<=n.start)r.issue("Zero-length or same-tick notes cannot be exported.",tick,s.ch);}s.note=-1;s.active=false;};
 auto begin=[&](Slot& s,int tick){s.value=values(s.held,channels[s.ch]);s.muted=s.held.recipe.voice==1&&!s.value.level;s.active=true;s.killed=false;if(s.muted){s.note=-1;return;}Note n;n.start=tick;n.ch=s.ch;n.voice=s.held.recipe.voice;n.key=s.key;n.gate=s.held.recipe.gate;n.value=s.value;s.note=int(r.notes.size());r.notes.push_back(n);};
 auto change=[&](Slot& s,int tick){auto v=values(s.held,channels[s.ch]);if(v==s.value)return;const int voice=s.held.recipe.voice;if(voice&&(v.pitch<0||v.pitch>95)){r.issue("Held pitch target is outside the native range.",tick,s.ch);return;}
  if(voice==1&&!v.level){if(s.note>=0)finish(s,tick);s.active=true;s.muted=true;s.value=v;return;}
  if(s.active&&s.muted){begin(s,tick);return;}s.value=v;
  if(s.note>=0){auto& n=r.notes[s.note];if(tick==n.start)n.value=v;else if(!n.changes.empty()&&n.changes.back().tick==tick)n.changes.back().value=v;else n.changes.push_back({tick,v});}
  else if(!s.killed&&voice!=1)r.controls[voice].push_back({tick,v});
 };
 auto expire=[&](int tick){for(auto& s:slots)if(s.active&&s.fixedEnd>=0&&s.fixedEnd<=tick)finish(s,s.fixedEnd);};
 std::array<bool,16> panNoted{},rpnNoted{};std::array<int,16> ignoredNotes{},retriggeredNotes{};
 for(const auto& e:events){expire(e.tick);
  if(e.type==0xf0){r.issue("System-exclusive MIDI messages are unsupported inside the export range.",e.tick);continue;}
  if(e.type>=0x80&&options.ignoreUnusedChannels&&kChannelVoices[e.ch]<0){if(e.type==0x90&&e.b)ignoredNotes[e.ch]++;continue;}
  if(e.type==0x51){if(!r.tempos.empty()&&r.tempos.back().tick==e.tick)r.tempos.back().us=e.a;else if(r.tempos.empty()||r.tempos.back().us!=e.a)r.tempos.push_back({e.tick,e.a});continue;}
  if(e.type==6){if(e.text=="LOOP_START"){starts++;r.loopStart=e.tick;}else{ends++;r.loopEnd=e.tick;}continue;}
  auto& s=channels[e.ch];
  if(e.type==0xc0){const bool wasNative=nativeProgramCount(s.msb)&&s.lsb==0;s.program=e.a;s.msb=s.pendingMSB;s.lsb=s.pendingLSB;
   if(wasNative&&nativeProgramCount(s.msb)&&!s.lsb){int inst=nativeInstrument(s.msb,s.program);if(inst<0){r.issue("Invalid native program.",e.tick,e.ch);continue;}if(options.game){inst=targetInstrument(inst,driver,options.game,s.msb==120+options.game?s.program:-1);if(inst<0){r.issue("Held instrument change is unavailable in the selected game; choose Omni.",e.tick,e.ch);continue;}}
    for(auto& key:keys[e.ch])if(key.down&&!key.recipe.ticks)key.recipe.instrument=inst;
    for(auto& slot:slots)if(slot.active&&slot.ch==e.ch&&!slot.held.recipe.ticks){slot.held.recipe.instrument=inst;change(slot,e.tick);}
   }continue;
  }
  if(e.type==0xb0){const int cc=e.a,v=e.b;if(cc==0){s.pendingMSB=v;continue;}if(cc==32){s.pendingLSB=v;continue;}
   if(cc==10){if(!panNoted[e.ch])r.issue("Pan (CC10) ignored: native NES output is mono.",e.tick,e.ch,false);panNoted[e.ch]=true;continue;}
   if(cc==101){s.rpnMSB=v;continue;}if(cc==100){s.rpnLSB=v;continue;}
   if(cc==6||cc==38){if(s.rpnMSB||s.rpnLSB){r.issue("Data Entry requires a supported RPN selection (only pitch-bend range RPN 0 is recognized).",e.tick,e.ch);continue;}if(cc==6)s.bendSemitones=v;else s.bendCents=v;rpnNoted[e.ch]=true;if(s.bend&&(s.bendSemitones!=2||s.bendCents))r.issue("Changing RPN pitch-bend range away from +/-2 during a non-centered bend is unsupported.",e.tick,e.ch);continue;}
   if(cc==120||cc==123){for(auto& k:keys[e.ch])k.down=false;for(auto& slot:slots)if(slot.ch==e.ch){if(slot.active)finish(slot,e.tick);slot.fixedEnd=-1;if(cc==120){slot.killed=true;auto value=slot.value;value.level=0;r.controls[slot.held.recipe.voice].push_back({e.tick,value});}}continue;}
   switch(cc){case 7:s.volume=v;break;case 11:s.expression=v;break;case 20:s.duty=v;break;case 21:if(e.ch==9&&v!=64)r.issue("Fine tuning is unavailable on the noise channel.",e.tick,e.ch);s.detune=v;break;case 5:s.portaTime=v;break;case 65:s.porta=v>=64;break;case 68:s.legato=v>=64;continue;case 121:s.expression=127;s.duty=64;s.detune=64;s.portaTime=0;s.porta=false;s.legato=false;s.bend=0;s.rpnMSB=127;s.rpnLSB=127;break;default:r.issue("Unsupported MIDI controller CC"+std::to_string(cc)+'.',e.tick,e.ch);continue;}
   for(auto& slot:slots)if(slot.ch==e.ch&&(slot.active||cc==7||cc==11||cc==20||cc==21||cc==121))change(slot,e.tick);continue;
  }
  if(e.type==0xe0){int raw=e.a+(e.b<<7)-8192;if(raw&&(s.bendSemitones!=2||s.bendCents)){r.issue("Non-centered pitch bend with a non-default RPN range is unsupported; MM Studio uses fixed +/-2 semitones. Set RPN 0 to 2 semitones, or flatten the bend into notes.",e.tick,e.ch);continue;}double bend=raw==8191?2.:raw/4096.;if(std::abs(bend-std::round(bend))>1./8192){r.issue("Pitch bend targets must be whole semitones within +/-2.",e.tick,e.ch);continue;}if(e.ch==9&&raw){r.issue("Noise pitch bend is unsupported.",e.tick,e.ch);continue;}s.bend=int(std::lround(bend));for(auto& slot:slots)if(slot.active&&slot.ch==e.ch)change(slot,e.tick);continue;}
  if(e.type==0x90&&e.b){anyNotes=true;auto& key=keys[e.ch][e.a];if(key.down){r.issue("Repeated note-on before its note-off.",e.tick,e.ch);continue;}Recipe rec;if(!recipe(s,e.ch,e.a,rec,r,e.tick,options,driver)){rejected[e.ch][e.a]=true;continue;}
   key={true,e.ch,e.a,e.b,++age,rec};auto& slot=slots[rec.voice];bool connected=slot.active&&s.legato&&rec.voice&&!rec.ticks&&slot.ch==e.ch&&!slot.held.recipe.ticks&&slot.held.recipe.instrument==rec.instrument;
   if(slot.active&&!connected){if(options.retriggerOverlaps)retriggeredNotes[e.ch]++;else{const char* voices[]={"noise", "triangle", "Pulse 2", "Pulse 1"};r.issue(std::string("Overlapping notes compete for ")+voices[rec.voice]+": new note "+std::to_string(e.a)+" interrupts held note "+std::to_string(slot.key)+". Shorten the earlier note or enable Retrigger overlaps.",e.tick,e.ch);}finish(slot,e.tick);}
   slot.ch=e.ch;slot.key=e.a;slot.held=key;slot.fixedEnd=rec.ticks?e.tick+rec.ticks:-1;
   if(connected)change(slot,e.tick);else begin(slot,e.tick);if(slot.value.pitch<0||slot.value.pitch>95)r.issue("Bent note is outside native pitch range.",e.tick,e.ch);continue;
  }
  if(e.type==0x80||(e.type==0x90&&!e.b)){if(rejected[e.ch][e.a]){rejected[e.ch][e.a]=false;continue;}auto& key=keys[e.ch][e.a];if(!key.down){r.issue("Note-off has no matching note-on.",e.tick,e.ch);continue;}key.down=false;auto& slot=slots[key.recipe.voice];if(!slot.active||slot.ch!=e.ch||slot.key!=e.a||slot.fixedEnd>=0)continue;
   Key* next=nullptr;if(s.legato&&key.recipe.voice)for(auto& other:keys[e.ch])if(other.down&&!other.recipe.ticks&&other.recipe.voice==key.recipe.voice&&(!next||other.age>next->age))next=&other;
   if(next){bool connected=next->recipe.instrument==slot.held.recipe.instrument;slot.held=*next;slot.key=next->key;if(connected)change(slot,e.tick);else{finish(slot,e.tick);begin(slot,e.tick);}}else finish(slot,e.tick);continue;
  }
  r.issue("Unsupported MIDI event (aftertouch and pressure are not native controls).",e.tick,e.ch);
 }
 for(int ch=0;ch<16;ch++){if(ignoredNotes[ch])r.issue(std::to_string(ignoredNotes[ch])+" notes excluded by Ignore unused channels.",-1,ch,false);if(retriggeredNotes[ch])r.issue(std::to_string(retriggeredNotes[ch])+" overlapping notes retriggered; previous notes cut at the new attack (no held-key return).",-1,ch,false);if(rpnNoted[ch])r.issue("RPN pitch-bend-range setup recognized ("+std::to_string(channels[ch].bendSemitones)+" semitones, "+std::to_string(channels[ch].bendCents)+" cents). MM Studio uses fixed +/-2; centered bends do not change pitch, and non-centered bends with another range are blocked.",-1,ch,false);}
 expire(r.end);for(auto& slot:slots)if(slot.active){if(slot.fixedEnd>=0){r.end=std::max(r.end,slot.fixedEnd);finish(slot,slot.fixedEnd);}else finish(slot,r.end);}
 for(int ch=0;ch<16;ch++)for(const auto& key:keys[ch])if(key.down)r.issue("A note is missing its final note-off.",r.end,ch);
 if(!anyNotes)r.issue("The MIDI contains no notes.");if(r.tempos.empty()||r.tempos.front().tick!=0)r.tempos.insert(r.tempos.begin(),{0,500000});
 if(starts||ends){if(starts!=1||ends!=1||r.loopStart<0||r.loopEnd<=r.loopStart||r.loopEnd>r.end)r.issue("Use exactly one LOOP_START and LOOP_END, in chronological order within the song.");
  else{for(const auto& n:r.notes)if(n.start<r.loopStart&&n.end>r.loopStart||n.end>r.loopEnd)r.issue("A note crosses a loop boundary or lies after LOOP_END.",n.start,n.ch);for(const auto& t:r.tempos)if(t.tick>r.loopEnd)r.issue("Tempo event lies after LOOP_END.",t.tick);r.end=r.loopEnd;}
 }
}
struct Duration {int ticks,index;bool triplet,dotted=false;};
inline std::vector<Duration> durations(int ticks,int minimum=1,int forbiddenCut=-1){
 if(ticks<0||ticks>1000000)throw std::runtime_error("Invalid native duration.");
 std::vector<Duration> choices;for(int i=0;i<7;i++){choices.push_back({2<<i,i,true});choices.push_back({3<<i,i,false});if(i>=1&&i<=5)choices.push_back({9<<(i-1),i,false,true});}
 std::sort(choices.begin(),choices.end(),[](auto a,auto b){return a.ticks>b.ticks;});
 std::vector<int64_t> cost(ticks+1,INT64_MAX/4);std::vector<int> pick(ticks+1,-1);cost[0]=0;
 for(int t=1;t<=ticks;t++){if(t==forbiddenCut&&t!=ticks)continue;for(int j=0;j<int(choices.size());j++)if(choices[j].ticks<=t){auto candidate=cost[t-choices[j].ticks]+1+(choices[j].ticks<minimum?1000000:0);if(candidate<cost[t]){cost[t]=candidate;pick[t]=j;}}}
 if(pick[ticks]<0&&ticks)throw std::runtime_error("A one-tick token is not available in the native driver. Adjust the controller/rest spacing or quantization.");
 std::vector<Duration> out;int at=ticks;while(at){auto d=choices[pick[at]];out.push_back(d);at-=d.ticks;}std::reverse(out.begin(),out.end());return out;
}
inline int tempoStep(int us){constexpr double fps=60.098811862348406;double bpm=60e6/us;if(bpm<10||bpm>1000)throw std::runtime_error("Tempo must be between 10 and 1000 BPM.");return std::clamp(int(std::lround(48e6/us/fps*256)),1,65535);}
inline void compressPatterns(Bytes& data,int& loopOffset,std::vector<Relocation>& jumps,bool looping){
 // Repeat identical *whole instructions*. Counter slot 0 returns to zero at
 // every completed pattern; the main song loop is unconditional. Never fold
 // across the intro/loop label or across existing pointer-bearing commands.
 const Bytes original=data;const auto originalJumps=jumps;Bytes packed;std::vector<Relocation> reloc;std::map<int,int> locations;
 std::vector<int> segments{0,int(original.size())};if(looping)segments.push_back(loopOffset);std::sort(segments.begin(),segments.end());segments.erase(std::unique(segments.begin(),segments.end()),segments.end());
 for(size_t segment=0;segment+1<segments.size();segment++){
  int begin=segments[segment],end=segments[segment+1];std::vector<int> positions{begin};
  for(int p=begin;p<end;){int code=original[p],length=code>=32||code<4?1:code==5||code==22?3:code>=14&&code<=21?4:2;if(p+length>end)throw std::runtime_error("Internal native instruction boundary mismatch.");p+=length;positions.push_back(p);}
  for(int i=0;i+1<int(positions.size());){int old=positions[i],bestTokens=0,bestRepeats=0,bestSaving=0;locations[old]=int(packed.size());
   for(int k=1;k<=256&&i+2*k<int(positions.size());k++){
    int bytes=positions[i+k]-old,repeats=1;if(original[old]!=original[positions[i+k]])continue;
    // Address-bearing instructions would need target remapping on each pass.
    bool safe=true;for(int j=i;j<i+k;j++)if(original[positions[j]]>=14&&original[positions[j]]<=23){safe=false;break;}if(!safe)continue;
    while(repeats<256&&i+(repeats+1)*k<int(positions.size())&&positions[i+(repeats+1)*k]-positions[i+repeats*k]==bytes&&std::equal(original.begin()+old,original.begin()+old+bytes,original.begin()+positions[i+repeats*k]))repeats++;
    int saving=(repeats-1)*bytes-4;if(saving>bestSaving){bestTokens=k;bestRepeats=repeats;bestSaving=saving;}
   }
   if(bestTokens){int target=int(packed.size());packed.insert(packed.end(),original.begin()+old,original.begin()+positions[i+bestTokens]);packed.push_back(14);packed.push_back(uint8_t(bestRepeats-1));reloc.push_back({int(packed.size()),target});packed.insert(packed.end(),{0,0});i+=bestTokens*bestRepeats;}
   else{int next=positions[i+1],destination=int(packed.size());packed.insert(packed.end(),original.begin()+old,original.begin()+next);for(auto j:originalJumps)if(j.position>=old&&j.position<next)reloc.push_back({destination+j.position-old,j.target});i++;}
  }
 }
 if(looping){int originalLoop=loopOffset;loopOffset=locations.at(originalLoop);for(auto& j:reloc)if(j.target==originalLoop&&j.position+2==int(packed.size()))j.target=loopOffset;}
 data=std::move(packed);jumps=std::move(reloc);
}
inline void compile(Result& r,const Options& options,const uint8_t* original){
 const int limit=0xc000;if(options.origin<0xa000||options.origin>0xbff0){r.issue("Song origin must be in the original $A000-$BFFF data bank.");return;}
 auto selected=targetDriver(options.game,original);r.driver.assign(selected,selected+8192);
 if(!options.game){if(!applyPerformanceDriverPatch(r.driver.data(),8192)){r.issue("Embedded driver does not match the guarded existing legato patch.");return;}
  std::copy(std::begin(kAdditionalInstruments),std::end(kAdditionalInstruments),r.driver.begin()+0xadb+71*8);
  r.issue("Omni uses the combined MM Studio instrument table and existing instant-legato patch. Native timing, lengths and bank layout remain unchanged.",-1,-1,false);
 }
 std::array<Bytes,4> streams;std::array<int,4> loopOffsets{};std::array<std::vector<Relocation>,4> jumps;
 int minimum=1,roundedGates=0;for(const auto& t:r.tempos)minimum=std::max(minimum,(tempoStep(t.us)+255)/256);
 for(int voice=0;voice<4;voice++){
  auto& data=streams[voice];std::map<int,int> cache;bool triplet=false,tie=false;int cursor=0;std::vector<Note*> notes;
  for(auto& n:r.notes)if(n.voice==voice)notes.push_back(&n);std::sort(notes.begin(),notes.end(),[](auto a,auto b){return a->start<b->start;});
  std::map<int,Values> control;for(const auto& c:r.controls[voice])if(c.tick<=r.end)control[c.tick]=c.value;
  std::map<int,int> tempo;if(voice==3){for(const auto& t:r.tempos)tempo[t.tick]=tempoStep(t.us);if(r.loopStart>=0){int us=500000;for(const auto& t:r.tempos)if(t.tick<=r.loopStart)us=t.us;tempo[r.loopStart]=tempoStep(us);}}
  auto op=[&](int code,int value){if(!cache.count(code)||cache[code]!=value){data.push_back(uint8_t(code));data.push_back(uint8_t(value));cache[code]=value;}};
  auto connect=[&](bool on){if(tie!=on){data.push_back(1);tie=on;}};
  auto controls=[&](const Values& v,bool instrument){if(instrument)op(8,v.instrument);op(7,voice==1?0:v.level);if(voice>=2)op(24,v.duty);if(voice)op(12,v.detune);};
  auto boundary=[&](int tick){if(tick==r.loopStart){connect(false);loopOffsets[voice]=int(data.size());data.insert(data.end(),{4,0});triplet=false;cache.clear();}auto t=tempo.find(tick);if(t!=tempo.end())data.insert(data.end(),{5,uint8_t(t->second>>8),uint8_t(t->second)});auto c=control.find(tick);if(c!=control.end())controls(c->second,false);};
  auto token=[&](Duration d,int pitch){if(triplet!=d.triplet){data.push_back(0);triplet=d.triplet;}if(d.dotted)data.push_back(2);data.push_back(uint8_t(((d.index+1)<<5)|pitch));};
  std::set<int> emitted;auto at=[&](int t){if(emitted.insert(t).second)boundary(t);};
  auto rest=[&](int until){if(until<cursor)throw std::runtime_error("Overlapping notes in a hardware voice.");std::set<int> cuts{cursor,until};for(auto c:control)if(c.first>cursor&&c.first<until)cuts.insert(c.first);for(auto t:tempo)if(t.first>cursor&&t.first<until)cuts.insert(t.first);if(r.loopStart>cursor&&r.loopStart<until)cuts.insert(r.loopStart);int previous=cursor;for(int end:cuts){if(end==previous)continue;at(previous);connect(false);for(auto d:durations(end-previous,minimum))token(d,0);previous=end;}cursor=until;};
  for(size_t ni=0;ni<notes.size();ni++){
   auto* n=notes[ni];rest(n->start);at(n->start);int finish=ni+1<notes.size()?notes[ni+1]->start:r.end;if(r.loopStart>n->start)finish=std::min(finish,r.loopStart);if(finish<n->end)throw std::runtime_error("A note crosses a hardware/loop boundary.");
   if(r.loopStart<0&&ni+1==notes.size()&&finish==n->end)finish++; // room for a native release gate before the tail
   int release=n->gate==255?n->end:n->start+std::max(1,(n->end-n->start)*n->gate/256);
   std::map<int,Values> changes{{n->start,n->value}};for(auto c:n->changes)changes[c.tick]=c.value;
   std::set<int> cuts{n->start,finish};for(auto c:changes)cuts.insert(c.first);for(auto t:tempo)if(t.first>n->start&&t.first<finish)cuts.insert(t.first);for(auto c:control)if(c.first>n->start&&c.first<finish)cuts.insert(c.first);
   std::vector<int> boundaries(cuts.begin(),cuts.end());Values value=n->value;bool first=true;int previousPitch=value.pitch;
   struct Part{int start;Duration duration;Values value;};std::vector<Part> parts;
   for(size_t i=0;i+1<boundaries.size();i++){int start=boundaries[i],end=boundaries[i+1];if(changes.count(start))value=changes[start];for(auto d:durations(end-start,minimum,release-start)){parts.push_back({start,d,value});start+=d.ticks;}}
   for(size_t j=0;j<parts.size();j++){
    const auto& part=parts[j];at(part.start);bool held=part.start<release;controls(part.value,held);
    if(!held){connect(false);token(part.duration,0);continue;}
    bool keep=j+1<parts.size()&&parts[j+1].start<release;connect(keep);int gate=255;
    if(!keep){int hold=release-part.start;if(hold<part.duration.ticks){gate=std::clamp((hold*256+part.duration.ticks-1)/part.duration.ticks,1,255);if(std::max(1,part.duration.ticks*gate/256)!=hold)throw std::runtime_error("Native gate cannot represent this release point.");}else roundedGates++;}
    op(6,gate);if(voice){op(9,part.value.pitch/12);int speed=part.value.porta;if(!first&&part.value.pitch!=previousPitch&&!speed){if(options.game)throw std::runtime_error("Instant held pitch changes require the existing MM Studio legato patch; select Omni or use native glide.");data.insert(data.end(),{13,128});cache[13]=0;r.requiresPerformancePatch=true;}else op(13,speed);}
    token(part.duration,voice?part.value.pitch%12+1:16-part.value.period);first=false;previousPitch=part.value.pitch;
   }
   connect(false);cursor=finish;
  }
  if(r.loopStart>=0){rest(r.end);at(r.end);if(!emitted.count(r.loopStart))throw std::runtime_error("Loop start is not a native token boundary.");data.push_back(22);jumps[voice].push_back({int(data.size()),loopOffsets[voice]});data.insert(data.end(),{0,0});}
  else {rest(r.end+192);data.insert(data.end(),{23,0});}
 }
 if(roundedGates)r.issue(std::to_string(roundedGates)+" fully joined release boundaries use the native $FF gate (up to one tick early). Add release space for an exact gate.",-1,-1,false);
 for(int v=0;v<4;v++)compressPatterns(streams[v],loopOffsets[v],jumps[v],r.loopStart>=0);
 r.song={0};int address=options.origin+9;
 // Header pointers are pulse 1, pulse 2, triangle, noise (high byte first).
 for(int voice=3;voice>=0;voice--){r.offsets[voice]=address-options.origin;r.relocations.push_back({int(r.song.size()),address-options.origin});r.song.push_back(uint8_t(address>>8));r.song.push_back(uint8_t(address));address+=int(streams[voice].size());}
 if(address>limit){r.issue("Compiled song exceeds the selected target's data area. Use a smaller song or lower origin; no automatic layout expansion is performed.");r.song.clear();return;}
 for(int voice=3;voice>=0;voice--){for(auto jump:jumps[voice]){int target=options.origin+r.offsets[voice]+jump.target;streams[voice][jump.position]=uint8_t(target>>8);streams[voice][jump.position+1]=uint8_t(target);r.relocations.push_back({r.offsets[voice]+jump.position,r.offsets[voice]+jump.target});}r.song.insert(r.song.end(),streams[voice].begin(),streams[voice].end());}
 std::map<int,std::string> labels{{0,"song"}};const char* names[]={"noise","triangle","pulse2","pulse1"};for(int v=0;v<4;v++)labels[r.offsets[v]]=std::string(names[v])+"_stream";
 for(auto j:r.relocations)if(!labels.count(j.target))labels[j.target]="loop_"+std::to_string(j.target);
 std::map<int,int> reloc;for(auto j:r.relocations)reloc[j.position]=j.target;
 std::ostringstream a;a<<"; MM Studio native song; origin $"<<std::hex<<std::uppercase<<options.origin<<"; "<<(!options.game?"Omni: combined instrument table / existing legato patch":"original game driver target")<<"\n; Place the MUSIC segment at the origin above, or relink these label-based pointers.\n.segment \"MUSIC\"\n";
 for(int i=0;i<int(r.song.size());){if(labels.count(i))a<<labels[i]<<":\n";if(reloc.count(i)){a<<".byte >"<<labels[reloc[i]]<<", <"<<labels[reloc[i]]<<"\n";i+=2;}else{a<<".byte $"<<std::setw(2)<<std::setfill('0')<<int(r.song[i++])<<"\n";}}r.assembly=a.str();
}
inline void makeNsf(Result& r,const Options& options){
 // Self-contained, non-bankswitched NSF. Original song directory becomes one
 // pointer. Music retains the original 8 KiB data bank. Wrapper at $E000.
 r.driver[0xa40]=1;r.driver[0xa43]=uint8_t(options.origin>>8);r.driver[0xa44]=uint8_t(options.origin);
 Bytes init={0x78,0xd8,0xa2,0x00,0xa9,0x00};int loop=int(init.size());
 init.insert(init.end(),{0x95,0x00});for(int page=2;page<8;page++)init.insert(init.end(),{0x9d,0x00,uint8_t(page)});
 init.push_back(0xe8);init.push_back(0xd0);init.push_back(uint8_t(loop-int(init.size())-1));
 // Prime the channel headers at time zero, without consuming a music tick.
 // Their tempo command sets the step used by subsequent real PLAY calls.
 init.insert(init.end(),{0xa9,0x0f,0x8d,0x15,0x40,0xa9,0x40,0x8d,0x17,0x40,0xa9,0xf0,0x20,0x03,0x80,0xa9,0x00,0x20,0x03,0x80,0xa9,0x00,0x85,0xc8,0x85,0xc9,0x85,0xca,0x20,0x00,0x80,0x60});int play=0xe000+int(init.size());init.insert(init.end(),{0x4c,0x00,0x80});
 r.nsf.assign(128+0x6000+init.size(),0);auto& b=r.nsf;b[0]='N';b[1]='E';b[2]='S';b[3]='M';b[4]=0x1a;b[5]=1;b[6]=1;b[7]=1;b[8]=0;b[9]=0x80;b[10]=0;b[11]=0xe0;b[12]=uint8_t(play);b[13]=uint8_t(play>>8);
 auto text=[&](int offset,const std::string& s){for(size_t i=0;i<std::min(size_t(31),s.size());i++)b[offset+i]=uint8_t(static_cast<unsigned char>(s[i])<128?s[i]:'?');};text(14,options.title);text(46,options.author);text(78,"Created with MM Studio");b[110]=0xff;b[111]=0x40;b[120]=0x1d;b[121]=0x4e;
 std::copy(r.driver.begin(),r.driver.end(),b.begin()+128);std::copy(r.song.begin(),r.song.end(),b.begin()+128+options.origin-0x8000);std::copy(init.begin(),init.end(),b.begin()+128+0x6000);
}
inline Result convert(const Bytes& midi,const uint8_t* driver,int size,const Options& options={}){
 Result r;try{if(!driver||size!=8192)throw std::runtime_error("Expected the embedded 8 KiB MM5 driver.");if(options.game!=0&&options.game!=3&&options.game!=4&&options.game!=5&&options.game!=6)throw std::runtime_error("Choose Mega Man 3, 4, 5, 6 or Omni.");if(options.title.size()>31||options.author.size()>31||std::any_of(options.title.begin(),options.title.end(),[](unsigned char c){return c>=128;})||std::any_of(options.author.begin(),options.author.end(),[](unsigned char c){return c>=128;}))r.issue("NSF header metadata is limited to 31 ASCII characters; longer/non-ASCII title or author text is shortened/replaced in that header.",-1,-1,false);auto events=parse(midi,r);if(r.ok())selectExportRange(events,r,options);if(r.ok())retime(events,r,options.quantize);if(r.ok()){
 // Across SMF tracks a note-on can precede another track's note-off at
 // the same tick. Release existing gates first, preserving order among
 // all other events (especially bank/program and CC sequences).
 std::stable_sort(events.begin(),events.end(),[](const Event& a,const Event& b){if(a.tick!=b.tick)return a.tick<b.tick;auto off=[](const Event& e){return e.type==0x80||(e.type==0x90&&!e.b);};return off(a)&&!off(b);});model(events,r,options,driver);}if(r.ok())compile(r,options,driver);if(r.ok())makeNsf(r,options);}catch(const std::exception& e){r.issue(e.what());}
 if(!r.ok()){r.song.clear();r.nsf.clear();r.driver.clear();r.assembly.clear();}
 std::ostringstream s;s<<"{\n  \"schema\": 1,\n  \"passed\": "<<(r.ok()?"true":"false")<<",\n  \"target\": "<<quoted(!options.game?"omni":("mm"+std::to_string(options.game)))<<",\n  \"origin\": "<<options.origin<<",\n  \"notes\": "<<r.notes.size()<<",\n  \"songBytes\": "<<r.song.size()<<",\n  \"loopStartTick\": "<<r.loopStart<<",\n  \"loopEndTick\": "<<r.loopEnd<<",\n";
 s<<"  \"ignoreUnusedChannels\": "<<(options.ignoreUnusedChannels?"true":"false")<<",\n  \"retriggerOverlaps\": "<<(options.retriggerOverlaps?"true":"false")<<",\n";
 s<<"  \"endAtLoopEnd\": "<<(options.endAtLoopEnd?"true":"false")<<",\n";
 s<<"  \"timebase\": "<<quoted("48 native ticks per quarter; original native tempo accumulator")<<",\n  \"ticksPerQuarter\": "<<48<<",\n  \"driverRequired\": "<<(!options.game?"true":"false")<<",\n  \"driverOrigin\": 32768,\n  \"dataLimitExclusive\": "<<(49152)<<",\n  \"playerImageScope\": \"Standalone one-song driver image; do not overwrite a complete game bank with it. Integrate music, directory and required patches deliberately.\",\n";
 s<<"  \"requiredDriverChanges\": "<<(!options.game?"[\"existing instant-legato-v1 at $8435/$8A1B\",\"226 internal instruments at $8ADB; source game programs mapped to shared IDs\"]":"[]")<<",\n  \"relocationsHighByteFirst\": [";
 for(size_t i=0;i<r.relocations.size();i++){if(i)s<<',';s<<"{\"offset\":"<<r.relocations[i].position<<",\"targetOffset\":"<<r.relocations[i].target<<'}';}s<<"],\n  \"issues\": [";
 for(size_t i=0;i<r.issues.size();i++){const auto& issue=r.issues[i];if(i)s<<',';s<<"\n    {\"error\": "<<(issue.error?"true":"false")<<", \"tick\": "<<issue.tick<<", \"channel\": "<<issue.channel+1<<", \"occurrences\": "<<issue.count<<", \"lastTick\": "<<issue.lastTick<<", \"message\": "<<quoted(issue.text)<<'}';}s<<"\n  ]\n}\n";r.report=s.str();return r;
}
}
