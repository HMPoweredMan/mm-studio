// SPDX-License-Identifier: GPL-3.0-only
#include "MidiExport.h"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace mm5studio::exporter;
namespace fs=std::filesystem;
static void check(bool b,const char* text){if(!b)throw std::runtime_error(text);}
static Bytes read(const fs::path& path){std::ifstream f(path,std::ios::binary);check(bool(f),"Input open failed");return Bytes(std::istreambuf_iterator<char>(f),{});}
static void write(const fs::path& path,const Bytes& data){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.data()),data.size());check(bool(f),"Fixture write failed");}
static void write(const fs::path& path,const std::string& data){write(path,Bytes(data.begin(),data.end()));}
static Bytes midi(const Bytes& events){Bytes b={'M','T','h','d',0,0,0,6,0,0,0,1,0,48,'M','T','r','k',0,0,uint8_t((events.size()+4)>>8),uint8_t(events.size()+4)};b.insert(b.end(),events.begin(),events.end());b.insert(b.end(),{0,255,47,0});return b;}
int main(int argc,char** argv){try{
 check(argc==4,"driver bank, examples root and output directory required");auto bank=read(argv[1]);check(bank.size()==8192,"Driver size");fs::path out=argv[3];fs::create_directories(out);
 Options strict;strict.quantize=false;strict.ignoreUnusedChannels=false;strict.retriggerOverlaps=false;strict.endAtLoopEnd=false;
 check(Options{}.quantize&&Options{}.ignoreUnusedChannels&&Options{}.retriggerOverlaps&&Options{}.endAtLoopEnd,"DAW-friendly defaults must be enabled");
 const Bytes setup={0,0xb0,0,125,0,0xb0,32,0,0,0xc0,2};
 auto song=[&](Bytes tail){Bytes b=setup;b.insert(b.end(),tail.begin(),tail.end());return midi(b);};
 const Bytes notes={0,0x90,60,100,24,0x80,60,0};auto basic=convert(song(notes),bank.data(),8192,strict);check(basic.ok()&&basic.song.size()>9&&basic.nsf.size()>128,"Basic export");
 auto missing=convert(midi(notes),bank.data(),8192,strict);check(!missing.ok()&&missing.song.empty()&&missing.nsf.empty(),"Missing setup must block all outputs");
 auto broken=song(notes);broken.pop_back();check(!convert(broken,bank.data(),8192,strict).ok(),"Truncated track accepted");
 auto pressure=song({0,0x90,60,100,12,0xd0,90,12,0x80,60,0});check(!convert(pressure,bank.data(),8192,strict).ok(),"Unsupported pressure accepted");
 auto running=song({0,0x90,60,100,0,255,1,1,'x',24,60,0});check(convert(running,bank.data(),8192,strict).ok(),"Running status across metadata");
 auto overlap=song({0,0x90,60,100,6,0x90,64,100,6,0x80,60,0,6,0x80,64,0});check(!convert(overlap,bank.data(),8192,strict).ok(),"Hardware overlap accepted");
 Options retrigger;retrigger.retriggerOverlaps=true;auto retriggered=convert(overlap,bank.data(),8192,retrigger);check(retriggered.ok()&&retriggered.notes.size()==2&&retriggered.notes[0].end==6&&retriggered.notes[1].end==18,"Explicit retrigger must cut old gate without held-key return");
 auto touching=song({0,0x90,60,100,12,0x90,64,100,0,0x80,60,0,12,0x80,64,0});auto ordered=convert(touching,bank.data(),8192,strict);check(ordered.ok()&&ordered.notes.size()==2&&ordered.notes[0].end==12,"Same-tick release must precede next attack");
 auto panSetup=song({0,0xb0,10,0,0,0xb0,10,127,0,0x90,60,100,24,0x80,60,0});auto pan=convert(panSetup,bank.data(),8192,strict);check(pan.ok()&&pan.song==basic.song,"Mono pan must not change native output");
 auto rpn=song({0,0xb0,101,0,0,0xb0,100,0,0,0xb0,6,12,0,0xb0,38,0,0,0xe0,0,64,0,0x90,60,100,24,0x80,60,0});check(convert(rpn,bank.data(),8192,strict).ok(),"Centered bends with DAW range setup must import");
 auto badRange=song({0,0xb0,101,0,0,0xb0,100,0,0,0xb0,6,12,0,0x90,60,100,12,0xe0,0,96,12,0x80,60,0});check(!convert(badRange,bank.data(),8192,strict).ok(),"Non-default RPN bend range must not silently change pitch");
 auto dataEntry=song({0,0xb0,6,12,0,0x90,60,100,24,0x80,60,0});check(!convert(dataEntry,bank.data(),8192,strict).ok(),"Unselected RPN Data Entry must not be discarded");
 auto otherChannel=song({0,0x93,72,100,0,0x90,60,100,24,0x83,72,0,0,0x80,60,0});check(!convert(otherChannel,bank.data(),8192,strict).ok(),"Unused channels must require opt-in exclusion");Options ignore;ignore.ignoreUnusedChannels=true;auto excluded=convert(otherChannel,bank.data(),8192,ignore);check(excluded.ok()&&excluded.notes.size()==1&&excluded.song==basic.song,"Explicit unused channel exclusion must preserve supported voices");
 auto held=song({0,0x90,60,100,6,0xc0,11,6,0xb0,11,64,6,0xe0,0,96,6,0x80,60,0});auto h=convert(held,bank.data(),8192,strict);check(h.ok()&&h.notes.size()==1&&h.notes[0].changes.size()==3,"Held controls and instrument changes");
 auto legato=song({0,0xb0,68,127,0,0x90,60,100,6,0x90,64,100,6,0x80,64,0,6,0x80,60,0});auto l=convert(legato,bank.data(),8192,strict);check(l.ok()&&l.notes.size()==1&&l.notes[0].changes.size()==2,"Legato and held-key return");
 Options stock;stock.game=5;check(convert(song(notes),bank.data(),8192,stock).ok(),"Stock simple export");
 auto extended=song(notes);extended[25]=126; // CC0 value at byte 25
 auto e=convert(extended,bank.data(),8192,strict);check(e.ok()&&e.notes[0].value.instrument==73,"MM6 selection");
 auto gm=midi({0,0xb9,0,0,0,0xb9,32,0,0,0xc9,0,0,0x99,49,100,96,0x89,49,0});auto g=convert(gm,bank.data(),8192,strict);check(g.ok()&&g.notes.size()==1,"Fixed GM crash gate");
 auto tiny=song({0,0x90,60,100,1,0x80,60,0});auto t=convert(tiny,bank.data(),8192,strict);if(!t.ok())std::cout<<t.summary();check(t.ok(),"Native one-tick gate");check(convert(tiny,bank.data(),8192,stock).ok(),"Stock one-tick gate");check(!convert(legato,bank.data(),8192,stock).ok(),"Stock instant held retune accepted without its patch");
 Options relocated;relocated.origin=0xb000;auto rebased=convert(song(notes),bank.data(),8192,relocated);check(rebased.ok()&&rebased.song[1]==0xb0,"Relocation origin");
 auto half=tiny;half[13]=96;check(!convert(half,bank.data(),8192,strict).ok(),"Off-grid timing accepted silently");Options quantized;quantized.quantize=true;check(convert(half,bank.data(),8192,quantized).ok(),"Explicit quantization failed");
 bool unavailable=false;for(int i=0;i<71;i++)if(targetInstrument(71+i,bank.data(),5)<0){auto m=song(notes);m[25]=126;m[32]=uint8_t(i);check(!convert(m,bank.data(),8192,stock).ok(),"Unavailable sound silently approximated");unavailable=true;break;}check(unavailable,"No incompatible MM6 fixture");
 for(auto pair:{std::pair<const char*,Bytes>{"held-controls",held},{"legato-return",legato},{"one-tick",tiny},{"simple",song(notes)}}){auto r=convert(pair.second,bank.data(),8192,strict);write(out/(std::string(pair.first)+".mid"),pair.second);write(out/(std::string(pair.first)+".nsf"),r.nsf);write(out/(std::string(pair.first)+".bin"),r.song);write(out/(std::string(pair.first)+".asm"),r.assembly);write(out/(std::string(pair.first)+".json"),r.report);}

 auto loopSong=[&](bool tail,bool crossing){Bytes b=setup;auto add=[&](Bytes more){b.insert(b.end(),more.begin(),more.end());};add({0,255,6,10,'L','O','O','P','_','S','T','A','R','T',0,0x90,60,100});if(!crossing)add({12,0x80,60,0});add({uint8_t(crossing?24:12),255,6,8,'L','O','O','P','_','E','N','D'});if(crossing)add({12,0x80,60,0});if(tail)add({12,0x90,64,100,12,0x80,64,0});return midi(b);};
 Options range;range.endAtLoopEnd=true;
 auto tailMidi=loopSong(true,false);check(!convert(tailMidi,bank.data(),8192,strict).ok(),"Full-file policy must report notes after LOOP_END");auto tailSource=tailMidi;auto bounded=convert(tailMidi,bank.data(),8192,range);check(bounded.ok()&&bounded.notes.size()==1&&bounded.end==24&&tailMidi==tailSource,"Range policy must exclude tail without modifying source MIDI");
 auto crossingLoop=loopSong(false,true);check(!convert(crossingLoop,bank.data(),8192,strict).ok(),"Crossing gate needs explicit trim policy");auto clipped=convert(crossingLoop,bank.data(),8192,range);check(clipped.ok()&&clipped.notes[0].end==24,"Range policy must close held notes at LOOP_END");
 auto tailSysex=loopSong(false,false);tailSysex.insert(tailSysex.end()-4,{12,0xf0,1,0xf7});tailSysex[21]+=4;check(!convert(tailSysex,bank.data(),8192,strict).ok()&&convert(tailSysex,bank.data(),8192,range).ok(),"Unsupported events outside selected range must not block export");
 check(convert(song(notes),bank.data(),8192,range).ok(),"Default loop-range policy must export the full song when no markers exist");
 auto partialLoop=loopSong(false,false);std::string markerText="LOOP_END";auto marker=std::search(partialLoop.begin(),partialLoop.end(),markerText.begin(),markerText.end());check(marker!=partialLoop.end(),"Loop marker fixture");*marker='X';check(!convert(partialLoop,bank.data(),8192,range).ok(),"Incomplete loop marker pair must remain an error");
 auto missingOff=loopSong(false,true);missingOff.erase(missingOff.end()-8,missingOff.end()-4);missingOff[21]-=4;check(!convert(missingOff,bank.data(),8192,range).ok(),"Range selection must not hide missing final note-off");
 Result diagnostic;for(int i=0;i<250;i++)diagnostic.issue("Repeated issue",i,0);check(diagnostic.failed&&diagnostic.issues.size()==1&&diagnostic.issues[0].count==250&&diagnostic.issues[0].lastTick==249,"Repeated diagnostics must retain occurrence counts");
 int passed=0,blocked=0;const std::map<std::string,int> games={{"Dark Man demo",5},{"Dust Man demo",4},{"Flame Man demo",6},{"Gravity Man demo",5},{"Knight Man demo",6},{"Mega Man 3 Intro demo",3},{"Napalm Man demo",5},{"Skull Man demo",4},{"Snake Man demo",3},{"Spark Man demo",3},{"Tomahawk Man demo",6},{"Wave Man demo",5}};
 for(const auto& folder:fs::directory_iterator(argv[2]))if(folder.is_directory())for(const auto& file:fs::directory_iterator(folder.path()))if(file.path().extension()==".mid"){
  for(int game:{0,games.at(folder.path().filename().string())}){Options o;o.game=game;o.title=file.path().stem().string();auto r=convert(read(file.path()),bank.data(),8192,o);auto dest=out/(game?"MM"+std::to_string(game)+" - "+folder.path().filename().string():folder.path().filename().string());fs::create_directories(dest);write(dest/"report.json",r.report);
  std::cout<<folder.path().filename().string()<<": "<<(r.ok()?"PASS":"BLOCKED")<<"; "<<r.notes.size()<<" notes, "<<r.song.size()<<" bytes\n";
  if(r.ok()){passed++;check(r.song.size()<=8192,"Native bank overflow");if(game){auto original=targetDriver(game,bank.data());for(int i=0;i<8192;i++)if(i!=0xa40&&i!=0xa43&&i!=0xa44)check(r.driver[i]==original[i],"Game target altered original driver code/data");}write(dest/"song.nsf",r.nsf);write(dest/"song.bin",r.song);write(dest/"song.asm",r.assembly);write(dest/"driver.bin",r.driver);
   std::ofstream expected(dest/"expected.csv");for(const auto& n:r.notes){auto line=[&](int tick,const Values& v,char kind){expected<<tick<<','<<n.voice<<','<<kind<<','<<v.instrument<<','<<v.pitch<<','<<v.period<<','<<v.level<<','<<v.duty<<','<<v.detune<<','<<v.porta<<'\n';};line(n.start,n.value,'A');for(auto c:n.changes)line(c.tick,c.value,'C');line(n.end,n.value,'R');}
  }else{blocked++;for(auto name:{"song.nsf","song.bin","song.asm","driver.bin","expected.csv"})fs::remove(dest/name);std::cout<<r.summary();}}
 }
 check(passed==24&&blocked==0,"Every supplied example must export for its game and Omni");
 std::cout<<"PASS: parser/negative cases, explicit setup, held programs/controls, legato return, GM gate, target restrictions and relocation. Examples: "<<passed<<" exported, "<<blocked<<" blocked with diagnostics.\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
