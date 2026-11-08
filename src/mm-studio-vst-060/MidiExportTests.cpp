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
 const Bytes setup={0,0xb0,0,125,0,0xb0,32,0,0,0xc0,2};
 auto song=[&](Bytes tail){Bytes b=setup;b.insert(b.end(),tail.begin(),tail.end());return midi(b);};
 const Bytes notes={0,0x90,60,100,24,0x80,60,0};auto basic=convert(song(notes),bank.data(),8192);check(basic.ok()&&basic.song.size()>9&&basic.nsf.size()>128,"Basic export");
 auto missing=convert(midi(notes),bank.data(),8192);check(!missing.ok()&&missing.song.empty()&&missing.nsf.empty(),"Missing setup must block all outputs");
 auto broken=song(notes);broken.pop_back();check(!convert(broken,bank.data(),8192).ok(),"Truncated track accepted");
 auto pressure=song({0,0x90,60,100,12,0xd0,90,12,0x80,60,0});check(!convert(pressure,bank.data(),8192).ok(),"Unsupported pressure accepted");
 auto running=song({0,0x90,60,100,0,255,1,1,'x',24,60,0});check(convert(running,bank.data(),8192).ok(),"Running status across metadata");
 auto overlap=song({0,0x90,60,100,6,0x90,64,100,6,0x80,60,0,6,0x80,64,0});check(!convert(overlap,bank.data(),8192).ok(),"Hardware overlap accepted");
 auto held=song({0,0x90,60,100,6,0xc0,11,6,0xb0,11,64,6,0xe0,0,96,6,0x80,60,0});auto h=convert(held,bank.data(),8192);check(h.ok()&&h.notes.size()==1&&h.notes[0].changes.size()==3,"Held controls and instrument changes");
 auto legato=song({0,0xb0,68,127,0,0x90,60,100,6,0x90,64,100,6,0x80,64,0,6,0x80,60,0});auto l=convert(legato,bank.data(),8192);check(l.ok()&&l.notes.size()==1&&l.notes[0].changes.size()==2,"Legato and held-key return");
 Options stock;stock.game=5;check(convert(song(notes),bank.data(),8192,stock).ok(),"Stock simple export");
 auto extended=song(notes);extended[25]=126; // CC0 value at byte 25
 auto e=convert(extended,bank.data(),8192);check(e.ok()&&e.notes[0].value.instrument==73,"MM6 selection");
 auto gm=midi({0,0xb9,0,0,0,0xb9,32,0,0,0xc9,0,0,0x99,49,100,96,0x89,49,0});auto g=convert(gm,bank.data(),8192);check(g.ok()&&g.notes.size()==1,"Fixed GM crash gate");
 auto tiny=song({0,0x90,60,100,1,0x80,60,0});auto t=convert(tiny,bank.data(),8192);if(!t.ok())std::cout<<t.summary();check(t.ok(),"Native one-tick gate");check(convert(tiny,bank.data(),8192,stock).ok(),"Stock one-tick gate");check(!convert(legato,bank.data(),8192,stock).ok(),"Stock instant held retune accepted without its patch");
 Options relocated;relocated.origin=0xb000;auto rebased=convert(song(notes),bank.data(),8192,relocated);check(rebased.ok()&&rebased.song[1]==0xb0,"Relocation origin");
 auto half=tiny;half[13]=96;check(!convert(half,bank.data(),8192).ok(),"Off-grid timing accepted silently");Options quantized;quantized.quantize=true;check(convert(half,bank.data(),8192,quantized).ok(),"Explicit quantization failed");
 bool unavailable=false;for(int i=0;i<71;i++)if(targetInstrument(71+i,bank.data(),5)<0){auto m=song(notes);m[25]=126;m[32]=uint8_t(i);check(!convert(m,bank.data(),8192,stock).ok(),"Unavailable sound silently approximated");unavailable=true;break;}check(unavailable,"No incompatible MM6 fixture");
 for(auto pair:{std::pair<const char*,Bytes>{"held-controls",held},{"legato-return",legato},{"one-tick",tiny},{"simple",song(notes)}}){auto r=convert(pair.second,bank.data(),8192);write(out/(std::string(pair.first)+".mid"),pair.second);write(out/(std::string(pair.first)+".nsf"),r.nsf);write(out/(std::string(pair.first)+".bin"),r.song);write(out/(std::string(pair.first)+".asm"),r.assembly);write(out/(std::string(pair.first)+".json"),r.report);}
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
