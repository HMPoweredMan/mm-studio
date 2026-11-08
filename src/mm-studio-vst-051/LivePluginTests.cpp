// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include "StudioPlugin.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/common/memorystream.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ustring.h"
#include <iostream>
#include <stdexcept>
#include <vector>
HINSTANCE ghInst=GetModuleHandleW(nullptr);
using namespace Steinberg;using namespace Steinberg::Vst;using namespace mm5studio;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Host:HostApplication{tresult PLUGIN_API getName(String128 name)override{UString(name,128).fromAscii("REAPER QA");return kResultOk;}};
struct Fixture{
 Host host;Processor* p=new Processor;ParameterChanges in{2304},feedback{2304};EventList notes{128},out{1024};ProcessContext context{};float left[256]{},right[256]{};float* channels[2]{left,right};AudioBusBuffers bus{};ProcessData data{};
 Fixture(){check(p->initialize(&host)==kResultOk,"initialize");ProcessSetup setup{};setup.processMode=kRealtime;setup.symbolicSampleSize=kSample32;setup.maxSamplesPerBlock=256;setup.sampleRate=48000;check(p->setupProcessing(setup)==kResultOk,"setup");p->setActive(true);p->setProcessing(true);bus.numChannels=2;bus.channelBuffers32=channels;data.numOutputs=1;data.outputs=&bus;data.numSamples=256;data.symbolicSampleSize=kSample32;data.inputParameterChanges=&in;data.outputParameterChanges=&feedback;data.inputEvents=&notes;data.outputEvents=&out;data.processContext=&context;context.sampleRate=48000;}
 ~Fixture(){p->setProcessing(false);p->setActive(false);p->terminate();p->release();}
 void param(int id,double v,int offset=0){int32 qi,pi;in.addParameterData(id,qi)->addPoint(offset,v,pi);}
 void cc(int ch,int cc,int value,int offset=0){param(kRawCCBase+ch*128+cc,value/127.,offset);}
 void command(const char* name,int value){HostMessage m;m.setMessageID(name);m.getAttributes()->setInt("value",value);check(p->notify(&m)==kResultOk,"command");}
 void run(){out.clear();feedback.clearQueue();check(p->process(data)==kResultOk,"process");in.clearQueue();notes.clear();context.projectTimeSamples+=256;}
 std::vector<Event> events(){std::vector<Event> result;for(int i=0;i<out.getEventCount();i++){Event e;out.getEvent(i,e);result.push_back(e);}return result;}
 int count(int ch,int cc,int value=-1){int n=0;for(const auto& e:events())if(e.type==Event::kLegacyMIDICCOutEvent&&e.midiCCOut.channel==ch&&e.midiCCOut.controlNumber==cc&&(value<0||e.midiCCOut.value==value))n++;return n;}
};
int main(){try{
 Fixture f;
 f.param(3000,32/127.,71);f.run();check(f.count(0,20,32)==1,"GUI duty emits one CC20");check(f.events()[0].sampleOffset==71,"sample offset preserved");f.run();check(f.out.getEventCount()==0,"no idle output storm");
 f.cc(0,20,96);f.cc(0,7,80);f.param(0x11000,6/127.);f.run();check(f.out.getEventCount()==0,"native incoming CC/PC must not echo");
 f.command("Learn",2);f.cc(4,74,0);f.run();check(f.out.getEventCount()==0,"learn must not jump");f.cc(4,74,40);f.run();check(f.out.getEventCount()==0,"pickup waits below expression127");f.cc(4,74,127);f.run();check(f.count(0,11,127)==1,"pickup acquired at target");f.cc(4,74,64);f.run();check(f.count(0,11,64)==1,"mapped knob emits canonical target channel");
 f.param(3200,100/127.);f.run();f.cc(4,74,65);f.run();check(f.out.getEventCount()==0,"external target change rearms pickup");f.cc(4,74,105);f.run();check(f.count(0,11,105)==1,"crossing target acquires pickup");
 MemoryStream state;check(f.p->getState(&state)==kResultOk,"save state");Fixture restored;state.seek(0,IBStream::kIBSeekSet,nullptr);check(restored.p->setState(&state)==kResultOk,"restore state");restored.run();restored.cc(4,74,0);restored.run();check(restored.out.getEventCount()==0,"restored mapping requires pickup");restored.cc(4,74,127);restored.run();check(restored.count(0,11,127)==1,"restored mapping still works");
 int recipe=-1;for(int i=0;i<kSoundCount;i++)if(kSongSounds[i].voice>=2&&kSongSounds[i].gmNote<0&&kSongSounds[i].preset==6){recipe=i;break;}check(recipe>=0,"recipe exists");f.param(3101,43/127.);f.param(3201,71/127.);f.run();f.command("Recipe",recipe|(1<<16));f.run();check(f.out.getEventCount()==11,"recipe emits complete eleven-message setup");check(f.count(1,0,125)==1&&f.count(1,32,0)==1&&f.count(1,kCtrlProgramChange,6)==1,"recipe targets chosen pulse2");check(f.count(0,0)==0,"recipe channel isolated");check(f.count(1,7,43)==1&&f.count(1,11,71)==1,"preset preserves current volume and expression in emitted MIDI");
 int variants=0;for(int i=0;i<kSoundCount;i++)if(recipeTimbre(i)==recipeTimbre(recipe)){f.command("Recipe",i|(1<<16));f.run();check(f.count(1,7,43)==1&&f.count(1,11,71)==1,"all merged volume/reference variants preserve levels");variants++;}check(variants>1,"covered merged variants");
 for(int i=0;i<kSoundCount;i++){check(recipeTimbre(i)>=0&&recipeTimbre(i)<kTimbreCount,"valid timbre identity");check(std::strlen(recipeName(i))>0,"every source recipe named");}

 f.command("Send",1);f.run();check(f.out.getEventCount()==11,"send snapshot complete");auto es=f.events();check(es[0].midiCCOut.controlNumber==0&&es[1].midiCCOut.controlNumber==32&&es[2].midiCCOut.controlNumber==kCtrlProgramChange,"bank precedes program");
 Event note{};note.type=Event::kNoteOnEvent;note.noteOn.channel=0;note.noteOn.pitch=72;note.noteOn.velocity=1;note.noteOn.noteId=-1;f.notes.addEvent(note);f.context.state=ProcessContext::kRecording|ProcessContext::kPlaying;f.run();check(f.out.getEventCount()==45,"record edge emits four voices plus note");check(f.events().back().type==Event::kNoteOnEvent,"setup precedes first note");f.run();check(f.out.getEventCount()==0,"record snapshot only once");f.context.state=0;f.run();f.param(400,0);f.run();f.context.state=ProcessContext::kRecording;f.run();check(f.out.getEventCount()==0,"record-start option can disable snapshot");
 LiveMappings m;check(liveValue(6,127)<=1&&liveValue(6,0)>=0,"bend endpoint range");for(int value=0;value<128;value++)check(int(std::lround(liveValue(0,value)*127))==value/32*32,"duty detents");



 // Default compact output retains program transitions, session starts and externally changed patches.
 const auto compactKit=percussionPads(8);int exception=-1;
 for(int i=0;i<kSoundCount;i++)if(sourcePad(kSongSounds[i])&&kSongSounds[i].preset==25){exception=i;break;}
 check(exception>=0,"real non-$14 noise exception exists");
 Fixture compact;compact.context.state=ProcessContext::kTempoValid;compact.context.tempo=120;
 auto strike=[&](int index){compact.command("PercussionHit",index);compact.run();};
 strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)==1,"first hit initializes program even after preview");
 strike(compactKit[1]);check(compact.count(9,kCtrlProgramChange)==0&&compact.count(9,0)==0&&compact.count(9,32)==0,"same instrument omits bank/program");check(compact.events().back().type==Event::kNoteOnEvent,"compact hit still emits note");
 strike(exception);check(compact.count(9,kCtrlProgramChange,25)==1,"real instrument change retained");
 strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)==1,"return to $14 retained");
 compact.param(0x11009,5/127.);compact.run();strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)==1,"incoming program invalidates compact output cache");
 compact.context.state|=ProcessContext::kRecording;strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)>=1,"new recording reinitializes even after prior pad preview");
 compact.param(503,1);compact.run();strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)==1,"repeat option on");strike(compactKit[0]);check(compact.count(9,kCtrlProgramChange,20)==1,"repeat option every hit");
 MemoryStream compactState;check(compact.p->getState(&compactState)==kResultOk,"save repeat flag");Fixture restoredCompact;compactState.seek(0,IBStream::kIBSeekSet,nullptr);check(restoredCompact.p->setState(&compactState)==kResultOk,"restore repeat flag");restoredCompact.run();restoredCompact.command("PercussionHit",compactKit[0]);restoredCompact.run();restoredCompact.command("PercussionHit",compactKit[0]);restoredCompact.run();check(restoredCompact.count(9,kCtrlProgramChange,20)==1,"repeat flag survives state reload");
 // Migrate the old 245-value state without shifting its 32 latched bank bytes.
 MemoryStream oldState;IBStreamer oldWriter(&oldState,kLittleEndian);oldWriter.writeInt32(0x4D355354);oldWriter.writeInt32(9);
 for(int i=0;i<245;i++)oldWriter.writeFloat(defaultValue(i));
 for(int ch=0;ch<16;ch++){oldWriter.writeFloat(kPerformanceBanks[ch]/127.f);oldWriter.writeFloat(0);}
 for(int i=0;i<kLiveTargets;i++)oldWriter.writeInt32(-1);
 Fixture migrated;oldState.seek(0,IBStream::kIBSeekSet,nullptr);check(migrated.p->setState(&oldState)==kResultOk,"0.4.5 state migrates");migrated.run();migrated.command("PercussionHit",compactKit[0]);migrated.run();migrated.command("PercussionHit",compactKit[0]);migrated.run();check(migrated.count(9,kCtrlProgramChange)==0,"legacy state defaults repeat off");
 // Full articulation regression uses the explicit repeat mode.
 f.param(503,1);f.run();
 f.context.state=ProcessContext::kTempoValid;f.context.tempo=120;int tested=0;
 // Exhaust every source articulation, including all quiet accents and non-modal lengths.
 for(int collection=0;collection<27;collection++)for(int index:percussionPads(collection)){
  const auto& r=kSongSounds[index];check(sourcePad(r),"song pads come from exact source rows");
  f.param(3109,0);f.param(3209,40/127.);f.run();f.command("PercussionHit",index);f.run();auto hit=f.events();
  check(hit.size()==12&&hit.back().type==Event::kNoteOnEvent,"exact pad emits eleven settings then note");
  check(hit.back().noteOn.channel==9&&hit.back().noteOn.pitch==padNote(r),"exact source pitch");
  check(int(std::lround(hit.back().noteOn.velocity*127))==sourceVelocity(r),"source intensity encoded in velocity");
  check(int(std::lround(hit.back().noteOn.velocity*15))==r.volume,"native source level reproduced");
  check(f.count(9,0,125)==1&&f.count(9,kCtrlProgramChange,r.preset)==1&&f.count(9,7,127)==1&&f.count(9,11,127)==1,"canonical setup neutralizes prior gain");
  int off=-1,blocks=1;float peak=0;for(float v:f.left)peak=std::max(peak,std::abs(v));
  while(off<0&&blocks<12000){f.run();for(float v:f.left)peak=std::max(peak,std::abs(v));for(const auto& e:f.events())if(e.type==Event::kNoteOffEvent){check(e.noteOff.channel==9&&e.noteOff.pitch==padNote(r),"matching release");off=blocks*256+e.sampleOffset;}blocks++;}
  check(off>=0&&std::abs(off-r.ticks*500.)<=1.01,"already-gated source ticks used once, exact host release sample");check(peak>0.00001f,"actual source pad produces audio");f.run();check(f.events().empty(),"no repeated note-offs");tested++;
 }
 check(tested==298,"all 298 source articulations exercised");const auto kit=percussionPads(8);check(kit.size()==8,"Dark Man accents and fills retained");
 f.param(500,1);f.param(501,8/double(kSongCount-1));f.run();
 auto keyOn=[&](int ch,int pitch,float velocity,int offset){Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=int16(ch);e.noteOn.pitch=int16(pitch);e.noteOn.velocity=velocity;e.noteOn.noteId=-1;e.sampleOffset=offset;f.notes.addEvent(e);};
 auto keyOff=[&](int ch,int pitch){Event e{};e.type=Event::kNoteOffEvent;e.noteOff.channel=int16(ch);e.noteOff.pitch=int16(pitch);e.noteOff.noteId=-1;f.notes.addEvent(e);};
 keyOn(9,36,.5f,71);f.run();es=f.events();check(es.size()==12&&es.back().noteOn.pitch==padNote(kSongSounds[kit[0]])&&int(std::lround(es.back().noteOn.velocity*127))==sourceVelocity(kSongSounds[kit[0]]),"default controller strike keeps exact reference intensity");check(es.back().sampleOffset==71,"mapped hit offset");keyOff(9,36);f.run();f.command("Panic",0);f.run();
 f.param(502,1);keyOn(9,36,.5f,0);f.run();check(int(std::lround(f.events().back().noteOn.velocity*127))==int(std::lround(sourceVelocity(kSongSounds[kit[0]])*.5)),"optional dynamics scales source velocity");keyOff(9,36);f.run();f.command("Panic",0);f.run();f.param(502,0);f.run();
 f.command("PadLearn",1);keyOn(4,60,1,0);f.run();check(f.events().empty(),"learn silent");keyOff(4,60);f.run();check(f.events().empty(),"learn release consumed");
 MemoryStream padState;check(f.p->getState(&padState)==kResultOk,"save exact kit/mapping/dynamics");Fixture padRestored;padState.seek(0,IBStream::kIBSeekSet,nullptr);check(padRestored.p->setState(&padState)==kResultOk,"restore state");padRestored.run();Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=4;e.noteOn.pitch=60;e.noteOn.velocity=.75;e.noteOn.noteId=-1;padRestored.notes.addEvent(e);padRestored.run();check(padRestored.events().back().noteOn.pitch==padNote(kSongSounds[kit[1]]),"learned key and source kit survive reload");check(int(std::lround(padRestored.events().back().noteOn.velocity*127))==sourceVelocity(kSongSounds[kit[1]]),"fixed intensity policy survives reload");
 f.param(500,0);keyOn(9,3,1,0);f.run();check(f.events().size()==1&&f.events()[0].noteOn.pitch==3,"native playback bypass");keyOff(9,3);f.run();
 f.param(500,1);f.command("PadLearn",2);keyOn(9,36,1,0);f.run();keyOff(9,36);f.run();keyOn(9,36,1,0);f.run();check(f.events().back().noteOn.pitch==padNote(kSongSounds[kit[2]]),"duplicate key moves binding");keyOff(9,36);f.run();f.command("Panic",0);f.run();f.run();check(f.events().empty(),"panic clears timer");

 // A separate MM6 bank retains all IDs and uses the same controls/output contract.
 Fixture mm6;mm6.context.state=ProcessContext::kTempoValid;mm6.context.tempo=120;
 for(int ch:{0,1,2,9})for(int program=0;program<71;program++){
  mm6.param(1001+ch*3,126/127.);mm6.param(1002+ch*3,0);mm6.param(1000+ch*3,program/127.);mm6.run();
  check(mm6.count(ch,0,126)==1&&mm6.count(ch,kCtrlProgramChange,program)==1,"MM6 bank/program canonical output");
  Event e{};e.type=Event::kNoteOnEvent;e.noteOn.channel=ch;e.noteOn.pitch=ch==9?3:60;e.noteOn.velocity=1;e.noteOn.noteId=-1;mm6.notes.addEvent(e);mm6.run();check(mm6.events().back().type==Event::kNoteOnEvent,"all MM6 program notes pass output");mm6.command("Panic",0);mm6.run();
 }
 mm6.param(503,1);mm6.run();int mm6Pads=0;
 for(int collection=30;collection<kMM3FirstCollection;collection++)for(int index:percussionPads(collection)){
  mm6.command("PercussionHit",index);mm6.run();auto out=mm6.events();check(out.back().type==Event::kNoteOnEvent&&out.back().noteOn.channel==9,"MM6 source pad note");check(mm6.count(9,0,126)==1,"MM6 pad selects its own bank");check(out.back().noteOn.pitch==padNote(kSongSounds[index]),"MM6 exact source period");check(int(std::lround(out.back().noteOn.velocity*127))==sourceVelocity(kSongSounds[index]),"MM6 exact source intensity");mm6.command("Panic",0);mm6.run();mm6Pads++;
 }
 check(mm6Pads==280,"all MM6 source articulations covered");
 mm6.param(1001,126/127.);mm6.param(1000,70/127.);mm6.run();MemoryStream gameState;check(mm6.p->getState(&gameState)==kResultOk,"save MM6 patch");Fixture restoredGame;gameState.seek(0,IBStream::kIBSeekSet,nullptr);check(restoredGame.p->setState(&gameState)==kResultOk,"restore MM6 patch");restoredGame.run();restoredGame.command("Send",0);restoredGame.run();check(restoredGame.count(0,0,126)==1&&restoredGame.count(0,kCtrlProgramChange,70)==1,"saved MM6 bank/program survive restore and snapshot");
 // New banks and fine tune use the actual processor/output path.
 Fixture added;added.context.state=ProcessContext::kTempoValid;added.context.tempo=120;
 for(const auto& bank:kNativeBanks)if(bank.game==3||bank.game==4)for(int ch:{0,1,2,9})for(int program=0;program<bank.count;program++){
  added.param(1001+ch*3,bank.bank/127.);added.param(1002+ch*3,0);added.param(1000+ch*3,program/127.);added.run();
  check(added.count(ch,0,bank.bank)==1&&added.count(ch,kCtrlProgramChange,program)==1,"new native bank/program output");
 }
 added.param(3400,61/127.,71);added.run();check(added.count(0,21,61)==1&&added.events()[0].sampleOffset==71,"fine tune GUI emits correct CC with sample offset");
 added.cc(0,21,66);added.run();check(added.events().empty(),"incoming fine tune does not echo");
 added.command("Learn",28);added.cc(4,74,0);added.run();check(added.events().empty(),"fine tune learn silent");added.cc(4,74,67);added.run();check(added.count(0,21,67)==1,"fine tune pickup and canonical CC output");
 MemoryStream fineState;check(added.p->getState(&fineState)==kResultOk,"save fine tune");Fixture fineRestored;fineState.seek(0,IBStream::kIBSeekSet,nullptr);check(fineRestored.p->setState(&fineState)==kResultOk,"restore fine tune");fineRestored.run();fineRestored.command("Send",0);fineRestored.run();check(fineRestored.count(0,21,67)==1,"fine tune restored and included in snapshot");
 int newPads=0;
 added.param(503,1);added.run();
 for(int collection=kMM3FirstCollection;collection<kSongCount;collection++)for(int index:percussionPads(collection)){
  added.command("PercussionHit",index);added.run();const auto& r=kSongSounds[index];check(added.count(9,0,recipeBank(r))==1&&added.count(9,kCtrlProgramChange,r.preset)==1,"new game pad address");check(added.count(9,21,64)==1,"noise fine tune neutral");check(added.events().back().noteOn.pitch==padNote(r),"new pad source pitch");added.command("Panic",0);added.run();newPads++;
 }
 check(newPads==611,"132 native plus 479 observed new-game source pads");
 int detuned=0;for(int i=0;i<kSoundCount;i++)if(kSongSounds[i].voice>0&&kSongSounds[i].detune){const auto& r=kSongSounds[i];const int v=r.voice>=2?0:2;added.command("Recipe",i|(v<<16));added.run();check(added.count(kLiveChannels[v],21,64+int(int8_t(r.detune)))==1,"every detuned recipe emits its source tuning");detuned++;}check(detuned>0,"detuned source recipes exercised");
 std::cout<<"PASS: actual processor MIDI output, no incoming CC/PC echo, offsets, 28-target learn/pickup/rearm and state, recipe routing/setup order, volume/expression preserved across merged variants, catalog coverage, four-voice record-edge snapshot, notes through, duty/bend bounds, all 298 exact source pads across 27 songs, native intensity, source duration applied once, source pitch, audible output and timed releases, optional velocity dynamics, MIDI mapping/learning/state, panic and native playback bypass\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
