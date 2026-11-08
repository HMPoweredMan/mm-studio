// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include "StudioPlugin.h"
#include "SettingsDrag.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include <iostream>
#include <fstream>
#include <stdexcept>
HINSTANCE ghInst=GetModuleHandleW(nullptr);
using namespace mm5studio;using namespace Steinberg;using namespace Steinberg::Vst;
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
 check(argc==2,"output directory required");const auto root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
 const auto cache=root/L"cache-\u03a9";SetEnvironmentVariableW(L"MM5_STUDIO_MIDI_CACHE",cache.c_str());
 HostApplication host;auto* c=new Controller;check(c->initialize(&host)==kResultOk,"controller initialize");
 const int programs[]={6,17,21,0},duties[]={32,96,64,64},volumes[]={80,0,90,91},expressions[]={40,110,127,70},bends[]={12288,0,16383,8192};
 for(int v=0;v<4;v++){const int ch=kLiveChannels[v],bank=v==3?0:125;
  c->liveBanks[ch].store(bank*128);c->setParamNormalized(1000+ch*3,programs[v]/127.);c->setParamNormalized(3000+ch,duties[v]/127.);c->setParamNormalized(3100+ch,volumes[v]/127.);c->setParamNormalized(3200+ch,expressions[v]/127.);c->setParamNormalized(3300+ch,v==0?1:0);c->setParamNormalized(2200+ch,42/127.);c->setParamNormalized(2300+ch,v==0?1:0);c->setParamNormalized(2400+ch,bends[v]/16383.);
  // Pending bank bytes do not replace the bank actually latched with Program Change.
  c->setParamNormalized(1001+ch*3,124/127.);c->setParamNormalized(1002+ch*3,3/127.);
  const auto s=c->currentSettings(v);check(s.channel==ch&&s.bankMSB==bank&&s.bankLSB==0,"committed address and selected channel");check(s.program==programs[v]&&s.volume==volumes[v]&&s.expression==expressions[v]&&s.bend==bends[v],"current settings exact including mute and bend endpoints");
  const auto bytes=settingsMidi(s);const auto file=root/("voice-"+std::to_string(v)+".mid");std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();
  const auto path=cacheSettingsMidi(s);check(path==cacheSettingsMidi(s),"identical snapshots reuse immutable file");auto changed=s;changed.expression=(s.expression+1)%128;const auto second=cacheSettingsMidi(changed);check(path!=second,"new settings get new immutable file");std::ifstream existing(path,std::ios::binary);check(std::vector<uint8_t>((std::istreambuf_iterator<char>(existing)),{})==bytes,"previous drag file remains intact");
  auto* data=new SettingsFileData(path.wstring());FORMATETC format{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};check(data->QueryGetData(&format)==S_OK,"file drop available");STGMEDIUM medium{};check(data->GetData(&format,&medium)==S_OK,"file drop payload");wchar_t name[32768];check(DragQueryFileW(static_cast<HDROP>(medium.hGlobal),0,name,32768)>0&&std::filesystem::path(name)==path,"Unicode file drop path exact");check(DragQueryFileW(static_cast<HDROP>(medium.hGlobal),0xffffffff,nullptr,0)==1,"one file per drag");ReleaseStgMedium(&medium);
  IEnumFORMATETC* formats=nullptr;check(data->EnumFormatEtc(DATADIR_GET,&formats)==S_OK,"format enumeration");FORMATETC got{};ULONG fetched=0;check(formats->Next(1,&got,&fetched)==S_OK&&got.cfFormat==CF_HDROP,"enumerated file format");formats->Release();format.cfFormat=CF_TEXT;check(data->QueryGetData(&format)==DV_E_FORMATETC,"unsupported format rejected");data->Release();
 }

 auto snapshot=c->currentSettings(3);snapshot.volume=80;snapshot.expression=40;
 int count=0;
 for(int collection=0;collection<27;collection++)for(int index:percussionPads(collection)){const auto bytes=padMidi(snapshot,index);const auto path=root/("pad-"+std::to_string(index)+".mid");std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());f.close();const auto cached=cachePadMidi(snapshot,index);check(cached==cachePadMidi(snapshot,index),"immutable source pad cache");check(!bytes.empty(),"source MIDI bytes");count++;}check(count==298,"all exact source pads exported");
 int mm6Count=0;
 for(int collection=29;collection<kMM3FirstCollection;collection++)for(int index:percussionPads(collection)){const auto bytes=padMidi(snapshot,index);const auto& r=kSongSounds[index];check(padSettings(snapshot,r).bankMSB==126,"MM6 drag bank");const auto path=root/("mm6-pad-"+std::to_string(index)+".mid");std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());mm6Count++;}check(mm6Count==351,"71 MM6 native programs and 280 source pads exported");
 int addedCount=0;
 for(int collection=kMM3FirstCollection;collection<kSongCount;collection++)for(int index:percussionPads(collection)){const auto bytes=padMidi(snapshot,index);const auto path=root/("added-pad-"+std::to_string(index)+".mid");std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());addedCount++;}check(addedCount==611,"all MM3/MM4 pads exported");
 c->setParamNormalized(3400,61/127.);auto fine=c->currentSettings(0);check(fine.detune==61,"fine tune snapshot");const auto fineBytes=settingsMidi(fine);std::ofstream fineOut(root/"fine-tune-settings.mid",std::ios::binary);fineOut.write(reinterpret_cast<const char*>(fineBytes.data()),fineBytes.size());fineOut.close();
 check(padMidi(snapshot,-1).empty(),"invalid pad cannot create MIDI");
 SettingsDropSource source;check(source.QueryContinueDrag(TRUE,MK_LBUTTON)==DRAGDROP_S_CANCEL,"escape cancels");check(source.QueryContinueDrag(FALSE,0)==DRAGDROP_S_DROP,"release drops");check(source.QueryContinueDrag(FALSE,MK_LBUTTON)==S_OK,"held continues");
 check(c->getParamNormalized(3100)==80/127.,"export does not change controller state");c->terminate();c->release();
 std::cout<<"PASS: stopped controller snapshots, all four voice channels, committed bank/program, CC7/11 mute preservation, bend endpoints, immutable Unicode cache, CF_HDROP/format enumeration, cancel/drop semantics; standard settings and 298 exact articulation MIDI files written\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
