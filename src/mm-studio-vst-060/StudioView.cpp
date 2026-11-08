// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include <commctrl.h>
#include "StudioPlugin.h"
#include "SettingsDrag.h"
#include "PadLabels.h"
#include "CollectionBrowser.h"
#include "ProgramNames.h"
#include "NativeExportWindow.h"
#include <iomanip>
#include "public.sdk/source/common/pluginview.h"
#include <mutex>
#include <string>
#include <sstream>
#include <set>
extern HINSTANCE ghInst;
namespace mm5studio {
static std::wstring wide(const std::string& s){return {s.begin(),s.end()};}
class StudioView final:public CPluginView {
public:
 explicit StudioView(Controller* c):controller(c){c->addRef();rect={0,0,1040,790};}
 ~StudioView()override{close();controller->release();}
 tresult PLUGIN_API isPlatformTypeSupported(FIDString t)override{return t&&!std::strcmp(t,kPlatformTypeHWND)?kResultTrue:kResultFalse;}
 tresult PLUGIN_API attached(void* parent,FIDString type)override{
  if(isPlatformTypeSupported(type)!=kResultTrue)return kResultFalse;
  static std::once_flag once;std::call_once(once,[]{WNDCLASSW wc{};wc.style=CS_DBLCLKS;wc.lpfnWndProc=proc;wc.hInstance=ghInst;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);wc.lpszClassName=L"KSA_MM5_Live_060";RegisterClassW(&wc);});
  CPluginView::attached(parent,type);window=CreateWindowExW(WS_EX_CONTROLPARENT,L"KSA_MM5_Live_060",L"MM Studio 0.6.1",WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_TABSTOP,0,0,1040,790,static_cast<HWND>(parent),nullptr,ghInst,this);if(!window)return kResultFalse;
  font=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  titleFont=CreateFontW(-26,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  auto title=child(L"STATIC",L"MM STUDIO / LIVE PERFORMANCE",0,22,14,700,34);SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
  child(L"BUTTON",L"All sound off",BS_PUSHBUTTON,866,16,150,32,10);
  child(L"BUTTON",L"NSF / data...",BS_PUSHBUTTON,744,16,112,32,80);
  child(L"STATIC",L"Voice / MIDI channel",0,22,60,215,22);voice=combo(22,84,218,11);for(auto s:{L"Pulse 1 / channel 1",L"Pulse 2 / channel 2",L"Triangle / channel 3",L"Percussion / channel 10"})add(voice,s);
  child(L"STATIC",L"Native program",0,260,60,445,22);program=combo(260,84,450,12);
  child(L"BUTTON",L"Send current settings",BS_PUSHBUTTON,744,81,272,30,13);
  auto dragButton=child(L"BUTTON",L"Drag settings MIDI",BS_PUSHBUTTON,744,113,272,26,14);SetWindowSubclass(dragButton,dragButtonProc,1,reinterpret_cast<DWORD_PTR>(this));
  const wchar_t* labels[]={L"Duty / CC20",L"Volume / CC7",L"Expr / CC11",L"Glide time / CC5",L"Pitch bend",L"Fine tune CC21"};
  for(int k=0;k<6;k++){const int x=22+k*119;child(L"STATIC",labels[k],0,x,127,115,22);knobValue[k]=child(L"STATIC",L"",SS_CENTER,x,231,112,22);learn[k]=child(L"BUTTON",L"Learn",BS_PUSHBUTTON,x+4,260,105,27,k==5?47:40+k);binding[k]=child(L"STATIC",L"",SS_CENTER,x,291,115,35);}
  porta=child(L"BUTTON",L"Portamento",BS_AUTOCHECKBOX,756,144,230,28,20);legato=child(L"BUTTON",L"Legato",BS_AUTOCHECKBOX,756,195,230,28,21);
  learn[6]=child(L"BUTTON",L"Learn porta switch",BS_PUSHBUTTON,756,235,245,26,45);binding[6]=child(L"STATIC",L"",0,756,265,245,22);
  learn[7]=child(L"BUTTON",L"Learn legato switch",BS_PUSHBUTTON,756,291,245,26,46);binding[7]=child(L"STATIC",L"",0,756,320,245,22);
  pickup=child(L"BUTTON",L"Controller pickup",BS_AUTOCHECKBOX,22,338,214,26,22);
  emitStart=child(L"BUTTON",L"Emit settings on recording start",BS_AUTOCHECKBOX,258,338,340,26,23);
  child(L"BUTTON",L"Clear selected binding",BS_PUSHBUTTON,756,351,245,27,24);
  child(L"STATIC",L"PRESETS",0,22,393,300,24);
  padMode=child(L"BUTTON",L"Play pads from MIDI",BS_AUTOCHECKBOX,340,393,200,24,70);padDynamics=child(L"BUTTON",L"Velocity dynamics",BS_AUTOCHECKBOX,550,393,185,24,72);padReset=child(L"BUTTON",L"Reset pad note bindings",BS_PUSHBUTTON,756,391,245,27,71);
  padRepeat=child(L"BUTTON",L"Repeat pad program changes",BS_AUTOCHECKBOX,22,366,340,24,73);
  game=combo(22,423,160,74);for(auto name:{L"Mega Man 3",L"Mega Man 4",L"Mega Man 5",L"Mega Man 6"})add(game,name);
  child(L"STATIC",L"Collection",0,195,427,85,22);song=combo(280,423,452,30);
  reference=child(L"BUTTON",L"Source reference",BS_AUTOCHECKBOX,770,422,240,26,31);
  sounds=child(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_HSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,22,462,550,214,32);SendMessageW(sounds,LB_SETHORIZONTALEXTENT,850,0);
  for(int i=0;i<8;i++){pads[i]=child(L"BUTTON",L"",BS_PUSHBUTTON|BS_MULTILINE,22+(i%4)*139,462+(i/4)*85,133,79,60+i);SetWindowSubclass(pads[i],padButtonProc,i,reinterpret_cast<DWORD_PTR>(this));}
  padPrevious=child(L"BUTTON",L"Previous",BS_PUSHBUTTON,22,638,95,32,68);padPageText=child(L"STATIC",L"",SS_CENTER,125,644,337,26);padNext=child(L"BUTTON",L"Next",BS_PUSHBUTTON,470,638,95,32,69);
  details=child(L"STATIC",L"",0,595,463,420,214);
  loadButton=child(L"BUTTON",L"Load preset + send MIDI",BS_PUSHBUTTON,22,687,270,34,33);
  child(L"BUTTON",L"Audition source",BS_PUSHBUTTON,310,687,180,34,34);child(L"BUTTON",L"Stop preview",BS_PUSHBUTTON,505,687,150,34,35);
  mode=combo(755,690,245,36);add(mode,L"Strict NES hardware");add(mode,L"Free arranging");
  status=child(L"STATIC",L"",0,22,742,995,38);
  refresh();SetTimer(window,1,80,nullptr);return kResultTrue;
 }
 tresult PLUGIN_API removed()override{close();return CPluginView::removed();}
private:
 HWND padRepeat=nullptr,padDynamics=nullptr,padMode=nullptr,padReset=nullptr,pads[8]{},padPrevious=nullptr,padNext=nullptr,padPageText=nullptr,loadButton=nullptr;std::vector<int> padRecipes;int padPage=0,padSelected=-1;
 int padSlot()const{for(int i=0;i<int(padRecipes.size());i++)if(padRecipes[i]==padSelected)return i;return -1;}
 int padKey(int slot)const{return param(600+slot,2048)-1;}
 void showPads(){const bool drums=part()==3;ShowWindow(sounds,drums?SW_HIDE:SW_SHOW);for(auto h:pads)ShowWindow(h,drums?SW_SHOW:SW_HIDE);for(auto h:{padPrevious,padNext,padPageText,padMode,padDynamics,padReset,padRepeat})ShowWindow(h,drums?SW_SHOW:SW_HIDE);SetWindowTextW(loadButton,drums?L"Load selected pad settings":L"Load preset + send MIDI");EnableWindow(reference,!drums);if(!drums)return;
  SendMessageW(padRepeat,BM_SETCHECK,param(503,1)?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(padDynamics,BM_SETCHECK,param(502,1)?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(padMode,BM_SETCHECK,param(500,1)?BST_CHECKED:BST_UNCHECKED,0);
  const int pages=std::max(1,int((padRecipes.size()+7)/8));padPage=std::clamp(padPage,0,pages-1);
  for(int j=0;j<8;j++){const int slot=padPage*8+j;EnableWindow(pads[j],slot<int(padRecipes.size()));std::wstring label=L"";if(slot<int(padRecipes.size())){const int i=padRecipes[slot];const auto& r=kSongSounds[i];const int key=padKey(slot);label=wide(kPadNames[i]);}SetWindowTextW(pads[j],label.c_str());}
  const auto text=padRecipes.empty()?L"No percussion in this collection":L"Click / drag / right-click: learn   |   "+std::to_wstring(padPage+1)+L" / "+std::to_wstring(pages);SetWindowTextW(padPageText,text.c_str());EnableWindow(padPrevious,padPage>0);EnableWindow(padNext,padPage+1<pages);
 }

 Controller* controller;HWND window=nullptr,voice=nullptr,game=nullptr,program=nullptr,porta=nullptr,legato=nullptr,pickup=nullptr,emitStart=nullptr,song=nullptr,sounds=nullptr,details=nullptr,reference=nullptr,status=nullptr,mode=nullptr,learn[8]{},binding[8]{},knobValue[6]{};HFONT font=nullptr,titleFont=nullptr;
 std::wstring dragNotice;ULONGLONG dragNoticeUntil=0;
 int shownVoice=-1,shownSong=-1,shownSound=-1,drag=-1,dragY=0,focused=0;double dragValue=0;bool showReference=false;
 int param(int id,int max=127)const{return std::clamp(int(std::lround(controller->getParamNormalized(id)*max)),0,max);}
 int part()const{return param(401,3);}int channel()const{return kLiveChannels[part()];}
 HWND child(const wchar_t* type,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0){if(id)style|=WS_TABSTOP;auto c=CreateWindowExW(0,type,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,reinterpret_cast<HMENU>(INT_PTR(id)),ghInst,nullptr);SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);return c;}
 HWND combo(int x,int y,int w,int id){return child(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,430,id);}
 static void add(HWND h,const wchar_t* s){SendMessageW(h,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));}
 int selected()const{if(part()==3)return padSelected;const auto i=SendMessageW(sounds,LB_GETCURSEL,0,0);return i==LB_ERR?-1:int(SendMessageW(sounds,LB_GETITEMDATA,i,0));}
 bool enabled(int kind)const{return kind==0?part()<2:kind==1||kind==2?true:part()!=3;}
 int target(int ui)const{const int kinds[]={0,1,2,3,6,7,4,5};return kinds[ui]==7?28+part():part()*7+kinds[ui];}
 void editKnob(int k,double value){if(!enabled(liveKind(target(k))))return;const int kind=liveKind(target(k));double v=std::clamp(value,0.,1.);if(kind==0)v=std::min(3,int(v*4))*32/127.;else if(kind==6)v=(8192+std::clamp(int(std::lround(v*4))-2,-2,2)*4096)/16383.;else v=std::lround(v*127)/127.;controller->performanceEdit(liveParam(target(k)),std::clamp(v,0.,1.));refresh();}
 bool compatible(const SongSound& r)const{return part()<2?r.voice>=2&&r.gmNote<0:part()==2?r.voice==1&&r.gmNote<0:r.voice==0||r.gmNote>=0;}
 void rebuild(){shownVoice=part();shownSong=visibleCollection(param(301,kSongCount-1));if(shownSong!=param(301,kSongCount-1))controller->edit(301,shownSong/double(kSongCount-1));SendMessageW(voice,CB_SETCURSEL,shownVoice,0);const int g=kCollectionGames[shownSong]-3;SendMessageW(game,CB_SETCURSEL,g,0);SendMessageW(song,CB_RESETCONTENT,0,0);
  for(int i:browserCollections(g)){const int row=int(SendMessageW(song,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(kSongs[i].name).c_str())));SendMessageW(song,CB_SETITEMDATA,row,i);if(i==shownSong)SendMessageW(song,CB_SETCURSEL,row,0);}
  SendMessageW(program,CB_RESETCONTENT,0,0);if(part()==3)add(program,L"GM percussion kit / bank 0, program 0");
  for(const auto& bank:kNativeBanks)for(int i=0;i<bank.count;i++){wchar_t text[140];std::swprintf(text,140,L"MM%d / %s",bank.game,wide(nativeProgramName(bank.bank,part()<2?3:part()==2?1:0,i)).c_str());const int row=int(SendMessageW(program,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text)));SendMessageW(program,CB_SETITEMDATA,row,bank.bank*128+i);}
  add(program,L"External MIDI bank/program - choose a native instrument");SendMessageW(program,CB_SETITEMDATA,SendMessageW(program,CB_GETCOUNT,0,0)-1,-1);
  SendMessageW(sounds,LB_RESETCONTENT,0,0);int chosen=-1;const int wanted=param(303,kSoundCount-1);const auto& collection=kSongs[shownSong];
  std::set<std::string> seen;
  for(int i=collection.first;i<collection.first+collection.count;i++)if(compatible(kSongSounds[i])){const auto& r=kSongSounds[i];
   const std::string key=std::to_string(recipeTimbre(i));
   if(!showReference&&!seen.insert(key).second)continue;
   const std::wstring text=showReference?wide(r.name):wide(recipeName(i));const int row=int(SendMessageW(sounds,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str())));SendMessageW(sounds,LB_SETITEMDATA,row,i);if(i==wanted||(!showReference&&recipeTimbre(i)==recipeTimbre(wanted)))chosen=row;}
  if(chosen<0&&SendMessageW(sounds,LB_GETCOUNT,0,0)>0)chosen=0;SendMessageW(sounds,LB_SETCURSEL,chosen,0);padRecipes=percussionPads(shownSong);padSelected=padRecipes.empty()?-1:padRecipes[0];for(int j=0;j<int(padRecipes.size());j++)if(padRecipes[j]==wanted){padSelected=wanted;padPage=j/8;break;}showPads();shownSound=selected();setDetails();
 }
 void setDetails(){const int i=selected();if(i<0){SetWindowTextW(details,L"No presets in this collection for this voice.\n\nChoose another collection.");return;}const auto& r=kSongSounds[i];std::wostringstream t;
  if(part()==3){const int slot=padSlot(),key=slot>=0?padKey(slot):-1;t<<wide(kPadNames[i])<<L"\n"<<L"Input: "<<(key<0?L"unbound":L"channel "+std::to_wstring(key/128+1)+L", note "+std::to_wstring(key%128))<<L"\nOutput: ch10 / bank "<<recipeBank(r)<<L" / program "<<(r.gmNote>=0?0:r.preset)<<L" / note "<<padNote(r)<<L"\n"<<std::fixed<<std::setprecision(2)<<padBeats(r)<<L" beats / "<<r.ticks<<L" ticks; source level "<<r.volume<<L" / 15.\nSource tempo: "<<r.bpm<<L" BPM.\n\n"<<wide(kPadEvidence[i]);SetWindowTextW(details,t.str().c_str());return;}

  t<<wide(recipeName(i))<<L"\n";if(r.gmNote>=0)t<<L"GM percussion kit / play MIDI note "<<r.gmNote<<L"\n";else{wchar_t hex[8];std::swprintf(hex,8,L"%02X",r.preset);t<<L"Native program $"<<hex<<L" / bank "<<recipeBank(r)<<L"\n";}
  if(r.voice>=2)t<<L"Duty: "<<(r.duty==0?L"12.5%":r.duty==64?L"25%":r.duty==128?L"50%":L"75%")<<L"\n";
  t<<L"Volume and expression stay unchanged.\n"<<L"Portamento: "<<((r.porta&127)?L"on":L"off")<<L" / legato: off\n";
  if(!r.voice&&r.gmNote<0)t<<L"Raw noise: MIDI notes 0..15 select period.\n";
  if(part()==3)t<<L"\nPad: MIDI note "<<padNote(r)<<L" / channel 10\nClick plays and outputs setup + note.\nLength: "<<padBeats(r)<<L" beats at host tempo.\nVolume-only duplicates are combined.";else t<<L"\nYour played notes control pitch and length.";
  t<<L"\nFine tune: "<<int(int8_t(r.detune))<<L" native units / CC21.";
  if(showReference)t<<L"\nSource volume: "<<r.volume<<L" / 15 (reference only)\nSource only: "<<r.ticks<<L" ticks, gate "<<r.gate<<L" / 255\n"<<L"Preview tempo: "<<int(r.bpm+.5)<<L" BPM / detune "<<int(int8_t(r.detune));
  SetWindowTextW(details,t.str().c_str());
 }
 void load(){const int i=selected();if(i>=0){controller->preset(i,part());controller->setDirty(true);}}
 void refresh(){if(!window)return;if(shownVoice!=part()||shownSong!=param(301,kSongCount-1))rebuild();const int ch=channel(),base=1000+ch*3,bank=controller->liveBanks[ch].load()/128,p=param(base);int selectedProgram=int(SendMessageW(program,CB_GETCOUNT,0,0))-1;const int address=bank*128+p;for(int row=0;row<selectedProgram;row++)if(int(SendMessageW(program,CB_GETITEMDATA,row,0))==address){selectedProgram=row;break;}SendMessageW(program,CB_SETCURSEL,selectedProgram,0);
  SendMessageW(porta,BM_SETCHECK,param(2300+ch)>=64?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(legato,BM_SETCHECK,param(3300+ch)>=64?BST_CHECKED:BST_UNCHECKED,0);EnableWindow(porta,part()!=3);EnableWindow(legato,part()!=3);
  SendMessageW(pickup,BM_SETCHECK,param(402,1)?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(emitStart,BM_SETCHECK,param(400,1)?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(mode,CB_SETCURSEL,param(kMode,1),0);
  for(int ui=0;ui<8;ui++){const int t=target(ui),kind=liveKind(t),key=controller->learnedSources[t].load();EnableWindow(learn[ui],enabled(kind));std::wstring label=controller->learning.load()==t?L"Learning...":ui<6?L"Learn":ui==6?L"Learn porta switch":L"Learn legato switch";SetWindowTextW(learn[ui],label.c_str());
   const std::wstring mapping=key<0?L"Unbound":L"Ch "+std::to_wstring(key/128+1)+L" CC"+std::to_wstring(key%128)+(controller->learnedPickup[t].load()?L"":L" / pickup");SetWindowTextW(binding[ui],mapping.c_str());
   if(ui<6){std::wstring value;if(!enabled(kind))value=L"Unavailable";else if(kind==0){const wchar_t* duty[]={L"12.5%",L"25%",L"50%",L"75%"};value=duty[param(liveParam(t))/32];}else if(kind==6)value=std::to_wstring(std::clamp(int(std::lround((controller->getParamNormalized(liveParam(t))*16383-8192)/4096)),-2,2))+L" semitones";else if(kind==7)value=std::to_wstring(param(liveParam(t))-64)+L" native units";else value=std::to_wstring(param(liveParam(t)))+L" / 127";SetWindowTextW(knobValue[ui],value.c_str());}
  }
  showPads();
  const auto text=controller->padLearning.load()>=0?L"Pad Learn: strike one controller key/pad. The learning note is silent; its channel and note are saved.":controller->learning.load()>=0?L"MIDI Learn: move a CC control. First message binds without changing the sound. Click Learn again to cancel.":controller->previewActive.load()?L"Source preview playing independently. Stop preview before performing.":L"Drag knobs vertically; double-click resets. Right-click a control to learn. MIDI output must be routed to a recording track.";SetWindowTextW(status,GetTickCount64()<dragNoticeUntil?dragNotice.c_str():text);
  RECT area{20,149,735,230};InvalidateRect(window,&area,FALSE);
 }
 void paint(){PAINTSTRUCT ps;auto dc=BeginPaint(window,&ps);SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
  for(int k=0;k<6;k++){const bool on=enabled(liveKind(target(k)));const int x=22+k*119+23,y=155,r=30;const auto brush=CreateSolidBrush(on?RGB(239,243,247):RGB(244,244,244));auto old=SelectObject(dc,brush);const auto pen=CreatePen(PS_SOLID,2,on?RGB(50,80,103):RGB(185,185,185));auto oldPen=SelectObject(dc,pen);Ellipse(dc,x,y,x+r*2,y+r*2);
   double value=controller->getParamNormalized(liveParam(target(k)));if(k==0)value=(param(liveParam(target(k)))/32)/3.;if(k==4)value=(std::clamp(int(std::lround((value*16383-8192)/4096)),-2,2)+2)/4.;const double angle=(-225+value*270)*3.141592653589793/180;
   MoveToEx(dc,x+r,y+r,nullptr);LineTo(dc,x+r+int(std::cos(angle)*(r-7)),y+r+int(std::sin(angle)*(r-7)));SelectObject(dc,old);SelectObject(dc,oldPen);DeleteObject(brush);DeleteObject(pen);
  }EndPaint(window,&ps);
 }
 int hit(int x,int y)const{if(y<151||y>228)return -1;for(int k=0;k<6;k++)if(x>=22+k*119&&x<137+k*119)return k;return -1;}
 void close(){if(window){KillTimer(window,1);DestroyWindow(window);window=nullptr;}if(font)DeleteObject(font);if(titleFont)DeleteObject(titleFont);font=titleFont=nullptr;}
 static LRESULT CALLBACK padButtonProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
  auto* s=reinterpret_cast<StudioView*>(data);const int slot=s->padPage*8+int(id);const int index=slot<int(s->padRecipes.size())?s->padRecipes[slot]:-1;
  if(msg==WM_LBUTTONDOWN&&index>=0){POINT point{int(short(LOWORD(l))),int(short(HIWORD(l)))};ClientToScreen(h,&point);SetFocus(h);
   if(DragDetect(h,point)){ReleaseCapture();const auto result=dragSettingsFile(s->controller->currentSettings(3),index);s->dragNotice=result==DRAGDROP_S_DROP?L"Pad MIDI copied: settings, note and duration.":result==DRAGDROP_S_CANCEL?L"Pad drag cancelled. Your sound is unchanged.":L"Could not drag pad MIDI.";s->dragNoticeUntil=GetTickCount64()+6000;s->refresh();}
   else SendMessageW(s->window,WM_COMMAND,MAKEWPARAM(60+id,BN_CLICKED),reinterpret_cast<LPARAM>(h));return 0;
  }
  if(msg==WM_CONTEXTMENU&&index>=0){s->padSelected=index;s->setDetails();POINT point{int(short(LOWORD(l))),int(short(HIWORD(l)))};if(point.x==-1){RECT r;GetWindowRect(h,&r);point={r.left,r.bottom};}auto menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING,1,L"Learn MIDI note for this pad");AppendMenuW(menu,MF_STRING,2,L"Unbind MIDI note");AppendMenuW(menu,MF_STRING,3,L"Restore default MIDI note");const int action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,0,h,nullptr);DestroyMenu(menu);
   if(action==1){s->controller->performanceEdit(501,s->shownSong/double(kSongCount-1));s->controller->performanceEdit(500,1);s->controller->command("PadLearn",s->controller->padLearning.load()==slot?-1:slot);}if(action==2)s->controller->performanceEdit(600+slot,0);if(action==3)s->controller->performanceEdit(600+slot,(9*128+36+slot+1)/2048.);s->refresh();return 0;
  }
  if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,padButtonProc,id);return DefSubclassProc(h,msg,w,l);
 }
 static LRESULT CALLBACK dragButtonProc(HWND h,UINT msg,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR data){
  auto* s=reinterpret_cast<StudioView*>(data);
  if(msg==WM_LBUTTONDOWN){POINT point{int(short(LOWORD(l))),int(short(HIWORD(l)))};ClientToScreen(h,&point);
   if(DragDetect(h,point)){ReleaseCapture();const auto snapshot=s->controller->currentSettings(s->part());const HRESULT result=dragSettingsFile(snapshot);
    s->dragNotice=result==DRAGDROP_S_DROP?L"Settings MIDI copied. Use Bank/Program Select to view its program.":result==DRAGDROP_S_CANCEL?L"Settings drag cancelled. Your sound is unchanged.":L"Could not drag settings MIDI. Check that the temporary folder is writable.";s->dragNoticeUntil=GetTickCount64()+6000;s->refresh();}return 0;
  }
  if(msg==WM_NCDESTROY)RemoveWindowSubclass(h,dragButtonProc,id);
  return DefSubclassProc(h,msg,w,l);
 }
 static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){auto* s=reinterpret_cast<StudioView*>(GetWindowLongPtrW(h,GWLP_USERDATA));if(msg==WM_NCCREATE){s=static_cast<StudioView*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));}if(!s)return DefWindowProcW(h,msg,w,l);
  if(msg==WM_TIMER){s->controller->command("StatusRequest");s->refresh();return 0;}if(msg==WM_PAINT){s->paint();return 0;}
  if(msg==WM_LBUTTONDOWN||msg==WM_LBUTTONDBLCLK||msg==WM_RBUTTONDOWN){const int k=s->hit(LOWORD(l),HIWORD(l));if(k>=0){s->focused=k;if(msg==WM_RBUTTONDOWN){s->controller->command("Learn",s->target(k));s->controller->setDirty(true);}else if(msg==WM_LBUTTONDBLCLK){s->editKnob(k,k==1||k==2?1:k==3?0:.5);}else if(s->enabled(liveKind(s->target(k)))){s->drag=k;s->dragY=HIWORD(l);s->dragValue=k==0?(s->param(liveParam(s->target(k)))/32)/3.:k==4?(s->controller->getParamNormalized(liveParam(s->target(k)))*16383-8192)/16384+.5:s->controller->getParamNormalized(liveParam(s->target(k)));SetFocus(h);SetCapture(h);}return 0;}}
  if(msg==WM_MOUSEMOVE&&s->drag>=0){s->editKnob(s->drag,s->dragValue+(s->dragY-int(short(HIWORD(l))))/180.);return 0;}
  if(msg==WM_LBUTTONUP&&s->drag>=0){s->drag=-1;ReleaseCapture();return 0;}
  if(msg==WM_CAPTURECHANGED){s->drag=-1;return 0;}
  if(msg==WM_KEYDOWN&&(w==VK_UP||w==VK_DOWN||w==VK_LEFT||w==VK_RIGHT)){const int k=s->focused;if(k>=6)return 0;const double amount=k==0?1./3:k==4?.25:1./127;s->editKnob(k,(k==0?(s->param(liveParam(s->target(k)))/32)/3.:s->controller->getParamNormalized(liveParam(s->target(k))))+((w==VK_UP||w==VK_RIGHT)?amount:-amount));return 0;}
  if(msg==WM_COMMAND){const int id=LOWORD(w),code=HIWORD(w);if(id==80&&code==BN_CLICKED){showNativeExport(h,ghInst);return 0;}
   if(code==BN_CLICKED){if(id==73)s->controller->performanceEdit(503,SendMessageW(s->padRepeat,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==72)s->controller->performanceEdit(502,SendMessageW(s->padDynamics,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==70){s->controller->performanceEdit(501,s->param(301,kSongCount-1)/double(kSongCount-1));s->controller->performanceEdit(500,SendMessageW(s->padMode,BM_GETCHECK,0,0)==BST_CHECKED?1:0);}if(id==71){s->controller->command("PadLearn",-1);for(int i=0;i<71;i++)s->controller->performanceEdit(600+i,(9*128+36+i+1)/2048.);}if(id>=60&&id<68){const int slot=s->padPage*8+id-60;if(slot<int(s->padRecipes.size())){s->padSelected=s->padRecipes[slot];s->controller->edit(303,s->padSelected/double(kSoundCount-1));s->controller->command("PercussionHit",s->padSelected);s->controller->setDirty(true);s->setDetails();}}if(id==68||id==69){s->padPage+=id==68?-1:1;s->showPads();}if(id==10)s->controller->command("Panic");if(id==13)s->controller->command("Send",s->part());if(id==20)s->controller->performanceEdit(2300+s->channel(),SendMessageW(s->porta,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==21)s->controller->performanceEdit(3300+s->channel(),SendMessageW(s->legato,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==22)s->controller->edit(402,SendMessageW(s->pickup,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==23)s->controller->edit(400,SendMessageW(s->emitStart,BM_GETCHECK,0,0)==BST_CHECKED?1:0);if(id==24){s->controller->command("Forget",s->target(s->focused));s->controller->setDirty(true);}if(id==31){s->showReference=SendMessageW(s->reference,BM_GETCHECK,0,0)==BST_CHECKED;s->rebuild();}if(id==33)s->load();if(id==34&&s->selected()>=0)s->controller->command("LibraryPlay",s->selected());if(id==35)s->controller->command("LibraryPlay",-1);if(id>=40&&id<=47){s->focused=id==47?5:id==45?6:id==46?7:id-40;const int t=s->target(s->focused);s->controller->command("Learn",s->controller->learning.load()==t?-1:t);s->controller->setDirty(true);}s->refresh();return 0;}
   if(id==32&&code==LBN_DBLCLK){s->load();return 0;}if(id==32&&code==LBN_SELCHANGE){s->shownSound=s->selected();if(s->shownSound>=0)s->controller->edit(303,s->shownSound/double(kSoundCount-1));s->setDetails();return 0;}
   if(code==CBN_SELCHANGE){const int value=int(SendMessageW(reinterpret_cast<HWND>(l),CB_GETCURSEL,0,0));if(value<0)return 0;if(id==11){s->controller->edit(401,value/3.);if(value==3)s->controller->performanceEdit(501,s->param(301,kSongCount-1)/double(kSongCount-1));}if(id==74){const int collection=browserFirstCollection(value);s->controller->edit(301,collection/double(kSongCount-1));if(s->part()==3)s->controller->performanceEdit(501,collection/double(kSongCount-1));}if(id==30){const int collection=int(SendMessageW(s->song,CB_GETITEMDATA,value,0));s->controller->edit(301,collection/double(kSongCount-1));if(s->part()==3)s->controller->performanceEdit(501,collection/double(kSongCount-1));}if(id==36)s->controller->edit(kMode,value);
    if(id==12){const int address=int(SendMessageW(s->program,CB_GETITEMDATA,value,0));if(address>=0){const int ch=s->channel();s->controller->performanceEdit(1001+ch*3,(address/128)/127.);s->controller->performanceEdit(1002+ch*3,0);s->controller->performanceEdit(1000+ch*3,(address%128)/127.);}}s->refresh();return 0;}
  }return DefWindowProcW(h,msg,w,l);
 }
};
IPlugView* PLUGIN_API Controller::createView(FIDString t){return t&&!std::strcmp(t,ViewType::kEditor)?new StudioView(this):nullptr;}
}
