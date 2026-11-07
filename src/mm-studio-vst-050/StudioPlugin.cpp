// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include "StudioPlugin.h"
#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/base/futils.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
#include <string>
extern HINSTANCE ghInst;
namespace mm5studio {
static bool readState(IBStream* in,float* v,int* stateVersion=nullptr){if(!in)return false;IBStreamer s(in,kLittleEndian);int32 magic,version;
    if(!s.readInt32(magic)||magic!=0x4D355354||!s.readInt32(version)||version<9||version>12)return false;
    for(int i=0;i<kCount;i++)v[i]=defaultValue(i);
    const int oldCount=version<12?kLegacyCount:kCount;
    for(int i=0;i<oldCount;i++){if(version==9&&i==245){v[i]=0;continue;}if(!s.readFloat(v[i])||!std::isfinite(v[i])||v[i]<0||v[i]>1)return false;}
    for(int i=0;i<32;i++)if(!s.readFloat(v[kCount+i])||!std::isfinite(v[kCount+i])||v[kCount+i]<0||v[kCount+i]>1)return false;
    if(version<12){const int oldSongs=version<11?29:66,oldSounds=version<11?1391:2151;v[101]=std::lround(v[101]*(oldSongs-1))/(double)(kSongCount-1);v[103]=std::lround(v[103]*(oldSounds-1))/(double)(kSoundCount-1);v[172]=std::lround(v[172]*(oldSongs-1))/(double)(kSongCount-1);}
    if(stateVersion)*stateVersion=version;return true;}
static bool writeState(IBStream* out,const float* v){if(!out)return false;IBStreamer s(out,kLittleEndian);
    if(!s.writeInt32(0x4D355354)||!s.writeInt32(12))return false;for(int i=0;i<kStored;i++)if(!s.writeFloat(v[i]))return false;return true;}
Processor::Processor(){for(int i=0;i<kLiveTargets;i++){savedBindings[i].store(-1);learnedSources[i].store(-1);learnedPickup[i].store(0);}for(int ch=0;ch<16;ch++)committedMSB[ch].store(kPerformanceBanks[ch]);setControllerClass(kControllerUID);for(int i=0;i<kCount;i++)values[i].store(defaultValue(i));}
tresult PLUGIN_API Processor::initialize(FUnknown* context){
    const auto result=AudioEffect::initialize(context);if(result!=kResultOk)return result;
    FUnknownPtr<IHostApplication> host(context);String128 hostName{};char name[128]{};
    if(host&&host->getName(hostName)==kResultOk){UString(hostName,128).toAscii(name,128);reaperHost=std::string(name).find("REAPER")!=std::string::npos;}
    for(int c=0;c<kSongCount;c++){const auto kit=percussionPads(c);padKitCounts[c]=int(kit.size());std::copy(kit.begin(),kit.end(),padKits[c].begin());}
    addAudioOutput(STR16("Stereo"),SpeakerArr::kStereo);addEventInput(STR16("MM5 MIDI"),16);addEventOutput(STR16("Performance MIDI"),16);
    const auto resource=FindResourceW(ghInst,MAKEINTRESOURCEW(102),RT_RCDATA);if(!resource)return kInternalError;
    bank=static_cast<const uint8_t*>(LockResource(LoadResource(ghInst,resource)));bankSize=SizeofResource(ghInst,resource);
    if(!songPreview.setup(bank,bankSize,48000)||!core.setup(bank,bankSize,48000))return kInternalError;synchronize();return kResultOk;
}
void Processor::synchronize(){padPatchSent=false;padConsumed.fill(false);padLearning.store(-1);soundingPad=-1;padRemaining=0;core.reset();core.setStrict(values[1].load()<.5);
    for(int ch=0;ch<16;ch++){const int i=kMain+ch*3;core.bank(ch,false,committedMSB[ch].load());core.bank(ch,true,committedLSB[ch].load());core.program(ch,int(std::lround(values[i].load()*127)));core.bank(ch,false,int(std::lround(values[i+1].load()*127)));core.bank(ch,true,int(std::lround(values[i+2].load()*127)));core.portamento(ch,values[53+ch*2].load()>=64/127.f,int(std::lround(values[52+ch*2].load()*127)));core.bend(ch,int(std::lround(values[84+ch].load()*16383))-8192);core.control(ch,21,int(std::lround(values[246+ch].load()*127)));for(int group=0;group<4;group++){const int cc[]={20,7,11,68};core.control(ch,cc[group],int(std::lround(values[104+group*16+ch].load()*127)));}}}
tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup){if(!bank||!songPreview.setup(bank,bankSize,int(std::lround(setup.sampleRate)))||!core.setup(bank,bankSize,int(std::lround(setup.sampleRate))))return kResultFalse;synchronize();return AudioEffect::setupProcessing(setup);}
tresult PLUGIN_API Processor::setActive(TBool active){core.panic();return AudioEffect::setActive(active);}
tresult PLUGIN_API Processor::setProcessing(TBool active){if(!active){padPatchSent=false;soundingPad=-1;padRemaining=0;percussionRequest.store(-1);core.panic();songPreview.stop();preview.store(-2);}return AudioEffect::setProcessing(active);}
tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement*,int32 inputs,SpeakerArrangement* outs,int32 count){if(inputs||count!=1||!outs||outs[0]!=SpeakerArr::kStereo)return kResultFalse;getAudioOutput(0)->setArrangement(SpeakerArr::kStereo);return kResultTrue;}
void Processor::change(ParamID id,double v,bool emit){
    if(id>=kRawCCBase&&id<kRawCCBase+2048){incomingCC((id-kRawCCBase)/128,(id-kRawCCBase)%128,int(std::lround(v*127)));return;}
    if(id>=0x11000&&id<0x11010){change(1000+(id-0x11000)*3,v,false);return;}
    if(id>=0x11100&&id<0x11110){change(2400+id-0x11100,v,false);return;}
    if(!std::isfinite(v))return;
    if(id==kReset){if(v>.5){core.panic();songPreview.stop();}return;}
    if(id>=2000&&id<2016){core.control(int(id)-2000,123,0);return;}
    if(id>=2500&&id<2516){core.control(int(id)-2500,120,0);return;}
    if(id>=2600&&id<2616){const int ch=int(id)-2600;core.control(ch,121,0);values[246+ch].store(64/127.f);values[104+ch].store(64/127.f);values[136+ch].store(1);values[152+ch].store(0);values[52+ch*2].store(0);values[53+ch*2].store(0);values[84+ch].store(8192/16383.f);if(feedback)for(int index:{246+ch,104+ch,136+ch,152+ch,52+ch*2,53+ch*2,84+ch}){int32 queueIndex=0,pointIndex=0;if(auto* queue=feedback->addParameterData(paramID(index),queueIndex))queue->addPoint(feedbackOffset,values[index].load(),pointIndex);}return;}
    if(id>=2100&&id<2116){core.warn(UnsupportedEvent,int(id)-2100);return;}
    const int i=paramIndex(id);if(i<0)return;v=std::clamp(v,0.,1.);if(i>=173&&i<244){const int key=int(std::lround(v*2048));if(key)for(int other=173;other<244;other++)if(other!=i&&int(std::lround(values[other].load()*2048))==key)change(paramID(other),0);}values[i].store(float(v),std::memory_order_relaxed);
    if(emit)emitParam(id,v);
    if(feedback&&((i>=4&&i<100)||(i>=104&&i<168)||i>=171)){int32 queueIndex=0,pointIndex=0;if(auto* queue=feedback->addParameterData(id,queueIndex))queue->addPoint(feedbackOffset,v,pointIndex);}
    if(i==1)core.setStrict(v<.5);
    if(i>=246&&i<262)core.control(i-246,21,int(std::lround(v*127)));
    if(i>=104&&i<168){const int cc[]={20,7,11,68};core.control((i-104)%16,cc[(i-104)/16],int(std::lround(v*127)));}
    if(i>=84&&i<100)core.bend((i-84),int(std::lround(v*16383))-8192);
    if(i>=52&&i<84){const int ch=(i-52)/2;core.portamento(ch,values[53+ch*2].load()>=64/127.f,int(std::lround(values[52+ch*2].load()*127)));}
    if(i>=kMain&&i<52){if((i-kMain)/3==9)padPatchSent=false;const int ch=(i-kMain)/3,kind=(i-kMain)%3,value=int(std::lround(v*127));
        if(kind==0){core.program(ch,value);committedMSB[ch].store(core.patch(ch).msb);committedLSB[ch].store(core.patch(ch).lsb);}else core.bank(ch,kind==2,value);}
}
void Processor::event(const Event& e){
    if(e.busIndex)return;
    if(routePadEvent(e))return;
    if(e.type==Event::kNoteOnEvent&&e.noteOn.channel==9&&e.noteOn.velocity>0)stopPercussionPad();
    if(midiOutput&&(e.type==Event::kNoteOnEvent||e.type==Event::kNoteOffEvent)){auto copy=e;copy.busIndex=0;midiOutput->addEvent(copy);}
    if(e.type==Event::kNoteOnEvent){if(e.noteOn.velocity<=0)core.off(e.noteOn.channel,e.noteOn.pitch,e.noteOn.noteId);else core.on(e.noteOn.channel,e.noteOn.pitch,e.noteOn.velocity,e.noteOn.noteId);}
    else if(e.type==Event::kNoteOffEvent)core.off(e.noteOff.channel,e.noteOff.pitch,e.noteOff.noteId);
    else if(e.type==Event::kLegacyMIDICCOutEvent){const auto& m=e.midiCCOut;const int ch=m.channel;
        if(m.controlNumber==kCtrlProgramChange)change(1000+ch*3,m.value/127.,false);
        else if(m.controlNumber==0||m.controlNumber==32)incomingCC(ch,m.controlNumber,m.value);
        else if(m.controlNumber==5||m.controlNumber==65||m.controlNumber==7||m.controlNumber==11||m.controlNumber==20||m.controlNumber==68||m.controlNumber==120||m.controlNumber==121||m.controlNumber==123)incomingCC(ch,m.controlNumber,m.value);
        else if(m.controlNumber==kPitchBend)change(2400+ch,((int(m.value)&127)|((int(m.value2)&127)<<7))/16383.,false);
        
        else if(m.controlNumber<128)incomingCC(ch,m.controlNumber,m.value);else core.warn(UnsupportedEvent,ch);
    }else if(e.type==Event::kPolyPressureEvent)core.warn(UnsupportedEvent,e.polyPressure.channel);
    else if(e.type==Event::kDataEvent)core.warn(UnsupportedEvent,0);
}
bool Processor::routePadEvent(const Event& e){
 const bool on=e.type==Event::kNoteOnEvent&&e.noteOn.velocity>0;
 if(!on&&e.type!=Event::kNoteOffEvent&&e.type!=Event::kNoteOnEvent)return false;
 const int ch=e.type==Event::kNoteOffEvent?e.noteOff.channel:e.noteOn.channel,note=e.type==Event::kNoteOffEvent?e.noteOff.pitch:e.noteOn.pitch;
 if(ch<0||ch>15||note<0||note>127)return false;const int key=ch*128+note;
 if(!on){if(padConsumed[key]){padConsumed[key]=false;return true;}return false;}
 const int learn=padLearning.load();
 if(learn>=0){
  for(int i=0;i<71;i++)if(i!=learn&&int(std::lround(values[173+i].load()*2048))==key+1)change(600+i,0);
  change(600+learn,(key+1)/2048.);padLearning.store(-1);padConsumed[key]=true;return true;
 }
 if(values[171].load()>=.5){const int collection=std::clamp(int(std::lround(values[172].load()*(kSongCount-1))),0,kSongCount-1);
  for(int i=0;i<padKitCounts[collection];i++)if(int(std::lround(values[173+i].load()*2048))==key+1){padConsumed[key]=true;startPercussionPad(padKits[collection][i],e.noteOn.velocity);return true;}
 }
 padConsumed[key]=false;return false;
}
void Processor::emitCC(int ch,int cc,int value,int value2){
 if(!midiOutput||ch<0||ch>15)return;Event e{};e.busIndex=0;e.sampleOffset=feedbackOffset;e.flags=Event::kIsLive;e.type=Event::kLegacyMIDICCOutEvent;
 e.midiCCOut.channel=int8(ch);e.midiCCOut.controlNumber=uint8(cc);e.midiCCOut.value=int8(value);e.midiCCOut.value2=int8(value2);midiOutput->addEvent(e);
}
void Processor::emitParam(ParamID id,double v){
 const int i=paramIndex(id);if(i>=4&&i<52){const int ch=(i-4)/3,kind=(i-4)%3;emitCC(ch,kind==0?kCtrlProgramChange:kind==1?0:32,int(std::lround(v*127)));}
 else if(i>=52&&i<84)emitCC((i-52)/2,(i-52)%2?65:5,int(std::lround(v*127)));
 else if(i>=84&&i<100){const int value=int(std::lround(v*16383));emitCC(i-84,kPitchBend,value&127,value>>7);}
 else if(i>=246&&i<262)emitCC(i-246,21,int(std::lround(v*127)));
 else if(i>=104&&i<168){const int cc[]={20,7,11,68};emitCC((i-104)%16,cc[(i-104)/16],int(std::lround(v*127)));}
}
void Processor::snapshot(int ch){
 emitCC(ch,0,committedMSB[ch].load());emitCC(ch,32,committedLSB[ch].load());emitCC(ch,kCtrlProgramChange,int(std::lround(values[4+ch*3].load()*127)));
 emitParam(3400+ch,values[246+ch].load());
 for(int group=0;group<4;group++)emitParam(3000+group*100+ch,values[104+group*16+ch].load());
 emitParam(2200+ch,values[52+ch*2].load());emitParam(2300+ch,values[53+ch*2].load());emitParam(2400+ch,values[84+ch].load());
}
void Processor::loadRecipe(int index,int voice,bool padHit){
 if(index<0||index>=kSoundCount||voice<0||voice>3)return;const auto& r=kSongSounds[index];
 if((voice<2&&r.voice<2)||(voice==2&&r.voice!=1)||(voice==3&&r.voice!=0&&r.gmNote<0))return;
 const int ch=kLiveChannels[voice],speed=r.porta&127;
 const int msb=recipeBank(r),program=r.gmNote>=0?0:r.preset;
 const bool repeat=!padHit||values[245].load()>=.5||!padPatchSent||padPatchMSB!=msb||padPatchLSB!=0||padPatchProgram!=program;
 change(1001+ch*3,msb/127.,repeat);change(1002+ch*3,0,repeat);change(1000+ch*3,program/127.,repeat);
 if(padHit){padPatchSent=true;padPatchMSB=msb;padPatchLSB=0;padPatchProgram=program;}
 change(3400+ch,(voice==3?64:64+int(int8_t(r.detune)))/127.);
 change(3000+ch,(r.voice>=2?r.duty/2:64)/127.);change(3100+ch,voice==3&&sourcePad(r)?1:values[120+ch].load());change(3200+ch,voice==3&&sourcePad(r)?1:values[136+ch].load());change(3300+ch,0);
 change(2200+ch,(speed?128-speed:0)/127.);change(2300+ch,speed?1:0);change(2400+ch,8192/16383.);
 mappings.reset(28+voice);
 for(int i=voice*7;i<voice*7+7;i++)if(i%7!=1&&i%7!=2)mappings.reset(i);
}
void Processor::stopPercussionPad(){
 if(soundingPad<0)return;core.off(9,soundingPad);if(midiOutput){Event e{};e.busIndex=0;e.sampleOffset=feedbackOffset;e.flags=Event::kIsLive;e.type=Event::kNoteOffEvent;e.noteOff.channel=9;e.noteOff.pitch=int16(soundingPad);e.noteOff.noteId=-1;midiOutput->addEvent(e);}soundingPad=-1;padRemaining=0;
}
void Processor::startPercussionPad(int index,float velocity){
 if(index<0||index>=kSoundCount)return;const auto& r=kSongSounds[index];if(r.voice!=0&&r.gmNote<0)return;
 stopPercussionPad();loadRecipe(index,3,true);
 const float dynamics=values[244].load()>=.5?std::clamp(velocity,0.f,1.f):1.f;velocity=std::clamp(int(std::lround(sourceVelocity(r)*dynamics)),1,127)/127.f;
 soundingPad=padNote(r);padRemaining=padBeats(r);core.on(9,soundingPad,velocity);
 if(midiOutput){Event e{};e.busIndex=0;e.sampleOffset=feedbackOffset;e.flags=Event::kIsLive;e.type=Event::kNoteOnEvent;e.noteOn.channel=9;e.noteOn.pitch=int16(soundingPad);e.noteOn.velocity=velocity;e.noteOn.noteId=-1;midiOutput->addEvent(e);}
}
void Processor::incomingCC(int ch,int cc,int physical){
 if(ch<0||ch>15||cc<0||cc>127)return;const int key=ch*128+cc;
 if(mappings.learning>=0){mappings.bind(mappings.learning,key,physical);return;}
 const int target=mappings.find(key);
 if(target>=0){const int id=liveParam(target),kind=liveKind(target),index=paramIndex(id);const double current=values[index].load();
  const int physicalTarget=kind==6?std::clamp(int(std::lround((current*16383-8192)/16384*127+64)),0,127):int(std::lround(current*127));
  if(mappings.accept(target,physical,physicalTarget,values[170].load()>=.5)){
   const double next=liveValue(kind,physical);change(id,next,true);
   mappings.applied(target,kind==6?std::clamp(int(std::lround((next*16383-8192)/16384*127+64)),0,127):int(std::lround(next*127)));
  }return;
 }
 if(cc==0||cc==32)change(1001+ch*3+(cc==32),physical/127.,false);
 else if(cc==21||cc==5||cc==65||cc==7||cc==11||cc==20||cc==68||cc==120||cc==121||cc==123)change(controlID(ch,cc),physical/127.,false);
 else core.warn(UnsupportedEvent,ch);
}

template<class T>void Processor::render(ProcessData& data){
    T** output;if constexpr(std::is_same_v<T,float>)output=data.outputs[0].channelBuffers32;else output=data.outputs[0].channelBuffers64;
    if(!output)return;
    struct Queue { IParamValueQueue* queue=nullptr;int32 point=0,offset=0;ParamValue value=0;bool valid=false; };
    std::array<Queue,2304> queues{};int count=0;
    if(data.inputParameterChanges){count=std::min(2304,int(data.inputParameterChanges->getParameterCount()));
        for(int i=0;i<count;i++){auto& q=queues[i];q.queue=data.inputParameterChanges->getParameterData(i);q.valid=q.queue&&q.queue->getPoint(0,q.offset,q.value)==kResultOk;}}
    std::array<int,16> bankOffset;bankOffset.fill(-1);
    if(reaperHost)for(int i=0;i<count;i++){auto& q=queues[i];if(!q.queue)continue;const auto id=q.queue->getParameterId();const int p=paramIndex(id);const bool rawBank=id>=kRawCCBase&&id<kRawCCBase+2048&&((id-kRawCCBase)%128==0||(id-kRawCCBase)%128==32);
        if(rawBank||(p>=kMain&&p<52&&(p-kMain)%3!=0)){for(int j=0;j<q.queue->getPointCount();j++){int32 offset=0;ParamValue value=0;
            if(q.queue->getPoint(j,offset,value)==kResultOk&&offset>0){auto& earliest=bankOffset[rawBank?(id-kRawCCBase)/128:(p-kMain)/3];if(earliest<0||offset<earliest)earliest=offset;}}}}
    auto alignProgram=[&](Queue& q){if(!reaperHost||!q.valid||q.offset!=0)return;const int p=paramIndex(q.queue->getParameterId());
        const auto id=q.queue->getParameterId();if(id>=0x11000&&id<0x11010&&bankOffset[id-0x11000]>0)q.offset=bankOffset[id-0x11000];else if(p>=kMain&&p<52&&(p-kMain)%3==0&&bankOffset[(p-kMain)/3]>0)q.offset=bankOffset[(p-kMain)/3];};
    // REAPER delivers Program Change at block start, but related bank CCs retain their true offset.
    // Associate that program with its bank bytes in this block, before the note at that same offset.
    for(int i=0;i<count;i++)alignProgram(queues[i]);
    int32 index=0;Event e{};bool has=data.inputEvents&&data.inputEvents->getEventCount()>0&&data.inputEvents->getEvent(0,e)==kResultOk;bool audible=false;
    for(int frame=0;frame<data.numSamples;frame++){
        feedbackOffset=frame;
        // Bank bytes precede Program Change at the same sample. All points are consumed, not only the last in a block.
        for(int pass=0;pass<3;pass++)for(int i=0;i<count;i++){auto& q=queues[i];if(!q.valid)continue;
            const auto id=q.queue->getParameterId();const int p=paramIndex(id);const bool program=(id>=0x11000&&id<0x11010)||(p>=kMain&&p<52&&(p-kMain)%3==0);
            if((id>=2600&&id<2616?0:program?2:1)!=pass)continue;
            while(q.valid&&q.offset<=frame){change(id,q.value);q.valid=q.queue->getPoint(++q.point,q.offset,q.value)==kResultOk;alignProgram(q);}}
        if(frame==0&&snapshotPending){for(int ch:kLiveChannels)snapshot(ch);snapshotPending=false;}
        if(frame==0){const int hit=percussionRequest.exchange(-1);if(hit>=0)startPercussionPad(hit);}
        if(soundingPad>=0){if(padRemaining<=0)stopPercussionPad();else padRemaining-=hostTempo/(60.*processSetup.sampleRate);}
        while(has&&e.sampleOffset<=frame){event(e);has=++index<data.inputEvents->getEventCount()&&data.inputEvents->getEvent(index,e)==kResultOk;}
        gain+=(values[0].load(std::memory_order_relaxed)*2-gain)*.005f;
        const T sample=T((core.render()+songPreview.render())*gain);if(sample!=0)audible=true;
        for(int ch=0;ch<data.outputs[0].numChannels;ch++)if(output[ch])output[ch][frame]=sample;
    }
    data.outputs[0].silenceFlags=audible?0:3;
}
tresult PLUGIN_API Processor::process(ProcessData& data){
    feedback=data.outputParameterChanges;feedbackOffset=0;midiOutput=data.outputEvents;
    if(mapRestore.exchange(false)){for(int i=0;i<kLiveTargets;i++)mappings.source[i]=savedBindings[i].load();mappings.resetPickup();mappings.learning=-1;}
    const int padLearn=padLearnRequest.exchange(-2);if(padLearn>=-1&&padLearn<71)padLearning.store(padLearn);
    const int learn=learnRequest.exchange(-2);if(learn>=-1&&learn<kLiveTargets)mappings.learning=learn;
    const int forget=forgetRequest.exchange(-1);if(forget>=0&&forget<kLiveTargets){mappings.source[forget]=-1;mappings.reset(forget);}
    const int recipe=recipeRequest.exchange(-1);if(recipe>=0)loadRecipe(recipe&0xffff,(recipe>>16)&3);
    const int send=sendRequest.exchange(-1);if(send>=0&&send<4)snapshot(kLiveChannels[send]);
    const bool nowRecording=data.processContext&&(data.processContext->state&ProcessContext::kRecording);
    if(nowRecording&&!recording){padPatchSent=false;if(values[168].load()>=.5)snapshotPending=true;}
    recording=nowRecording;
    if(restore.exchange(false))synchronize();if(reset.exchange(false)){stopPercussionPad();percussionRequest.store(-1);core.panic();songPreview.stop();for(int ch:kLiveChannels)emitCC(ch,120,0);}if(clear.exchange(false))core.clearWarnings();
    const int library=libraryRequest.exchange(-2);if(library==-1)songPreview.stop();else if(library>=0)songPreview.play(library);
    const uint64_t patch=patchPreview.exchange(~0ull);if(patch!=~0ull){const int channel=int(patch&15),p=int((patch>>4)&127),lsb=int((patch>>11)&127),msb=int((patch>>18)&127);
        change(1001+channel*3,msb/127.);change(1002+channel*3,lsb/127.);change(1000+channel*3,p/127.);}
    if(data.processContext&&(data.processContext->state&ProcessContext::kTempoValid)){hostTempo=data.processContext->tempo;core.setTempo(hostTempo);}
    const int settings=settingsRequest.exchange(-1);if(settings>=0&&settings<kSoundCount){const auto& r=kSongSounds[settings];loadRecipe(settings,r.gmNote>=0?3:r.voice==3?0:r.voice==2?1:r.voice==1?2:3);}
    const int action=preview.exchange(-2),drum=pad.exchange(-1);
    const int ch=(action>>8)&15,note=(action>>1)&127;
    if(action>=0){if(action&1)core.on(ch,note,1);else core.off(ch,note);}
    if(drum>=0)core.on(9,drum,1);
    if(data.numSamples<=0&&data.inputParameterChanges){for(int pass=0;pass<3;pass++)for(int i=0;i<data.inputParameterChanges->getParameterCount();i++){
        auto* q=data.inputParameterChanges->getParameterData(i);if(!q)continue;const auto id=q->getParameterId();const int p=paramIndex(id);const bool program=(id>=0x11000&&id<0x11010)||(p>=kMain&&p<52&&(p-kMain)%3==0);if((id>=2600&&id<2616?0:program?2:1)!=pass)continue;
        for(int j=0;j<q->getPointCount();j++){int32 offset=0;ParamValue value=0;if(q->getPoint(j,offset,value)==kResultOk)change(id,value);}}}
    if(data.numOutputs&&data.outputs&&data.numSamples>0){if(data.symbolicSampleSize==kSample32)render<float>(data);else if(data.symbolicSampleSize==kSample64)render<double>(data);}
    for(int v=0;v<4;v++)liveVoices[v].store(core.voiceSnapshot(v));
    for(int ch=0;ch<16;ch++)liveControls[ch].store(core.controlValue(ch,20)|(core.controlValue(ch,7)<<7)|(core.controlValue(ch,11)<<14)|((core.controlValue(ch,68)>=64)<<21));
    previewActive.store(songPreview.active());statusWarnings.store(int(core.warnings));statusReason.store(core.reason);statusChannel.store(core.lastChannel);statusVoices.store(core.activeCount());
    for(int t=0;t<kLiveTargets;t++){savedBindings[t].store(mappings.source[t]);learnedSources[t].store(mappings.source[t]);learnedPickup[t].store(mappings.acquired[t]?1:0);}learning.store(mappings.learning);
    feedback=nullptr;midiOutput=nullptr;return kResultOk;
}
tresult PLUGIN_API Processor::getState(IBStream* out){float v[kStored];for(int i=0;i<kCount;i++)v[i]=values[i].load();for(int ch=0;ch<16;ch++){v[kCount+ch*2]=committedMSB[ch].load()/127.f;v[kCount+ch*2+1]=committedLSB[ch].load()/127.f;}if(!writeState(out,v))return kResultFalse;IBStreamer stream(out,kLittleEndian);for(auto& binding:savedBindings)if(!stream.writeInt32(binding.load()))return kResultFalse;return kResultOk;}
tresult PLUGIN_API Processor::setState(IBStream* in){float v[kStored];int version=0;if(!readState(in,v,&version))return kResultFalse;IBStreamer stream(in,kLittleEndian);int32 sources[kLiveTargets];std::fill(std::begin(sources),std::end(sources),-1);for(int i=0;i<(version<12?28:kLiveTargets);i++)if(!stream.readInt32(sources[i])||sources[i]< -1||sources[i]>=2048)return kResultFalse;for(int t=0;t<kLiveTargets;t++)savedBindings[t].store(sources[t]);mapRestore.store(true);for(int i=0;i<kCount;i++)values[i].store(v[i]);for(int ch=0;ch<16;ch++){committedMSB[ch].store(int(std::lround(v[kCount+ch*2]*127)));committedLSB[ch].store(int(std::lround(v[kCount+ch*2+1]*127)));}restore.store(true);return kResultOk;}
tresult PLUGIN_API Processor::notify(IMessage* m){if(!m)return kInvalidArgument;const auto* id=m->getMessageID();int64 value=0;
    if(!std::strcmp(id,"PadLearn")){m->getAttributes()->getInt("value",value);padLearnRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"PercussionHit")){m->getAttributes()->getInt("value",value);if(value>=0&&value<kSoundCount)percussionRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Recipe")){m->getAttributes()->getInt("value",value);recipeRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Send")){m->getAttributes()->getInt("value",value);sendRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Learn")){m->getAttributes()->getInt("value",value);learnRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Forget")){m->getAttributes()->getInt("value",value);forgetRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Settings")){m->getAttributes()->getInt("value",value);settingsRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"LibraryPlay")){m->getAttributes()->getInt("value",value);libraryRequest.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Preview")){m->getAttributes()->getInt("value",value);preview.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Pad")){m->getAttributes()->getInt("value",value);pad.store(int(value));return kResultOk;}
    if(!std::strcmp(id,"Patch")){m->getAttributes()->getInt("value",value);patchPreview.store(uint64_t(value));return kResultOk;}
    if(!std::strcmp(id,"StatusRequest")){if(auto* reply=allocateMessage()){reply->setMessageID("Status");auto* a=reply->getAttributes();a->setInt("padLearning",padLearning.load());a->setInt("learning",learning.load());for(int ch=0;ch<16;ch++)a->setInt(("bank"+std::to_string(ch)).c_str(),committedMSB[ch].load()*128+committedLSB[ch].load());for(int t=0;t<kLiveTargets;t++){a->setInt(("binding"+std::to_string(t)).c_str(),learnedSources[t].load());a->setInt(("pickup"+std::to_string(t)).c_str(),learnedPickup[t].load());}for(int i=0;i<kCount;i++)a->setFloat(("param"+std::to_string(i)).c_str(),values[i].load());for(int ch=0;ch<16;ch++){const auto key="controls"+std::to_string(ch);a->setInt(key.c_str(),liveControls[ch].load());}a->setInt("preview",previewActive.load());a->setInt("warnings",statusWarnings.load());a->setInt("reason",statusReason.load());a->setInt("channel",statusChannel.load());a->setInt("voices",statusVoices.load());for(int i=0;i<4;i++){const char* key[]={"voice0","voice1","voice2","voice3"};a->setInt(key[i],int64(liveVoices[i].load()));}sendMessage(reply);reply->release();}return kResultOk;}
    if(!std::strcmp(id,"Panic")){reset.store(true);return kResultOk;}if(!std::strcmp(id,"Clear")){clear.store(true);return kResultOk;}
    return AudioEffect::notify(m);}
tresult PLUGIN_API Controller::initialize(FUnknown* context){auto result=EditControllerEx1::initialize(context);if(result!=kResultOk)return result;
    parameters.addParameter(new RangeParameter(STR16("Output"),kOutput,STR16("x"),0,2,1));
    auto* mode=new StringListParameter(STR16("Compatibility"),kMode);mode->appendString(STR16("Strict MM5"));mode->appendString(STR16("Free arranging"));parameters.addParameter(mode);
    parameters.addParameter(new RangeParameter(STR16("Preview channel"),kPreviewChannel,nullptr,1,16,1,15));
    parameters.addParameter(new RangeParameter(STR16("Preview note"),kPreviewNote,nullptr,0,127,60,127));
    addUnit(new Unit(STR16("MM Studio"),kRootUnitId,kNoParentUnitId,kNoProgramListId));
    for(int ch=0;ch<16;ch++){
        const int i=kMain+ch*3,id=int(paramID(i));String128 title{};UString(title,128).fromAscii(("MIDI "+std::to_string(ch+1)).c_str());
        addUnit(new Unit(title,ch+1,kRootUnitId,id));
        auto* list=new ProgramListWithPitchNames(title,id,ch+1);
        for(int p=0;p<128;p++){String128 name{};const std::string label=p<kPresetCount?"Instrument "+std::to_string(p):std::to_string(p)+" (unmapped)";UString(name,128).fromAscii(label.c_str());list->addProgram(name);}
        if(ch==9)for(auto& drum:kDrums){String128 name{};UString(name,128).fromAscii(drum.name);list->setPitchName(0,int16(drum.note),name);}
        addProgramList(list);
        auto* program=new RangeParameter(STR16("Program"),id,nullptr,0,127,kPerformancePrograms[ch],127,ParameterInfo::kCanAutomate|ParameterInfo::kIsProgramChange|ParameterInfo::kIsList,ch+1);
        parameters.addParameter(program);
        parameters.addParameter(new RangeParameter(STR16("Bank MSB"),id+1,nullptr,0,127,kPerformanceBanks[ch],127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Bank LSB"),id+2,nullptr,0,127,0,127,ParameterInfo::kCanAutomate,ch+1));
    }
    for(int ch=0;ch<16;ch++){
        parameters.addParameter(new RangeParameter(STR16("Portamento time / CC5"),2200+ch,nullptr,0,127,0,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Portamento switch / CC65"),2300+ch,nullptr,0,127,0,127,ParameterInfo::kCanAutomate,ch+1));
    }
    for(int ch=0;ch<16;ch++){
        parameters.addParameter(STR16("Channel notes off"),nullptr,1,0,ParameterInfo::kIsHidden,2000+ch);
        parameters.addParameter(STR16("Unsupported controller"),nullptr,0,0,ParameterInfo::kIsHidden,2100+ch);
    }
    for(int ch=0;ch<16;ch++)parameters.addParameter(new RangeParameter(STR16("Native pitch target / Pitch Bend"),2400+ch,nullptr,-8192,8191,0,16383,ParameterInfo::kCanAutomate,ch+1));
    parameters.addParameter(new RangeParameter(STR16("Browser game"),300,nullptr,0,3,2,3,ParameterInfo::kIsHidden));
    parameters.addParameter(new RangeParameter(STR16("Browser song"),301,nullptr,0,kSongCount-1,0,kSongCount-1,ParameterInfo::kIsHidden));
    parameters.addParameter(new RangeParameter(STR16("Browser voice"),302,nullptr,0,4,0,4,ParameterInfo::kIsHidden));
    parameters.addParameter(new RangeParameter(STR16("Browser sound"),303,nullptr,0,kSoundCount-1,0,kSoundCount-1,ParameterInfo::kIsHidden));
    for(int ch=0;ch<16;ch++){
        parameters.addParameter(new RangeParameter(STR16("Native fine tune / CC21"),3400+ch,nullptr,0,127,64,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Pulse duty / CC20"),3000+ch,nullptr,0,127,64,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Channel volume / CC7"),3100+ch,nullptr,0,127,127,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Expression / CC11"),3200+ch,nullptr,0,127,127,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(new RangeParameter(STR16("Legato / CC68"),3300+ch,nullptr,0,127,0,127,ParameterInfo::kCanAutomate,ch+1));
        parameters.addParameter(STR16("All sound off / CC120"),nullptr,1,0,ParameterInfo::kIsHidden,2500+ch);
        parameters.addParameter(STR16("Reset controllers / CC121"),nullptr,1,0,ParameterInfo::kIsHidden,2600+ch);
    }
    parameters.addParameter(STR16("Emit settings on recording start"),nullptr,1,1,ParameterInfo::kCanAutomate,400);
    auto* liveVoice=new StringListParameter(STR16("Performance voice"),401);for(auto name:{STR16("Pulse 1"),STR16("Pulse 2"),STR16("Triangle"),STR16("Percussion")})liveVoice->appendString(name);parameters.addParameter(liveVoice);
    parameters.addParameter(STR16("MIDI Learn pickup"),nullptr,1,1,ParameterInfo::kCanAutomate,402);
    parameters.addParameter(STR16("Play pads from MIDI"),nullptr,1,0,ParameterInfo::kCanAutomate,500);
    parameters.addParameter(new RangeParameter(STR16("Pad kit collection"),501,nullptr,0,kSongCount-1,0,kSongCount-1,ParameterInfo::kCanAutomate));
    parameters.addParameter(STR16("Pad velocity dynamics"),nullptr,1,0,ParameterInfo::kCanAutomate,502);
    parameters.addParameter(STR16("Repeat pad program changes"),nullptr,1,0,ParameterInfo::kCanAutomate,503);
    for(int i=0;i<71;i++){String128 name{};UString(name,128).fromAscii(("Pad "+std::to_string(i+1)+" trigger (channel/note key)").c_str());parameters.addParameter(new RangeParameter(name,600+i,nullptr,0,2048,9*128+36+i+1,2048,ParameterInfo::kCanAutomate));}
    for(int ch=0;ch<16;ch++){
      for(int cc=0;cc<128;cc++)parameters.addParameter(STR16("Controller input"),nullptr,127,0,ParameterInfo::kIsHidden,kRawCCBase+ch*128+cc);
      parameters.addParameter(STR16("Program input"),nullptr,127,0,ParameterInfo::kIsHidden,0x11000+ch);
      parameters.addParameter(STR16("Pitch input"),nullptr,16383,8192/16383.,ParameterInfo::kIsHidden,0x11100+ch);
    }
    for(int ch=0;ch<16;ch++)liveBanks[ch].store(kPerformanceBanks[ch]*128);
    for(int t=0;t<kLiveTargets;t++){learnedSources[t].store(-1);learnedPickup[t].store(0);}
    parameters.addParameter(STR16("Panic"),nullptr,1,0,ParameterInfo::kIsHidden,kReset);return kResultOk;
}
tresult PLUGIN_API Controller::getState(IBStream* out){if(!out)return kResultFalse;IBStreamer s(out,kLittleEndian);if(!s.writeInt32(0x4d354255)||!s.writeInt32(1))return kResultFalse;for(int id=300;id<304;id++)if(!s.writeFloat(float(getParamNormalized(id))))return kResultFalse;return kResultOk;}
tresult PLUGIN_API Controller::setState(IBStream* in){if(!in)return kResultFalse;IBStreamer s(in,kLittleEndian);int32 magic=0,version=0;if(!s.readInt32(magic)||magic!=0x4d354255||!s.readInt32(version)||version!=1)return kResultFalse;float values[4];for(auto& value:values)if(!s.readFloat(value)||!std::isfinite(value)||value<0||value>1)return kResultFalse;for(int i=0;i<4;i++)setParamNormalized(300+i,values[i]);return kResultOk;}
// Only browser selection belongs to editor state; audio/controller state stays authoritative in the processor.
tresult PLUGIN_API Controller::setComponentState(IBStream* in){float v[kStored];if(!readState(in,v))return kResultFalse;for(int i=0;i<kCount;i++)setParamNormalized(paramID(i),v[i]);for(int ch=0;ch<16;ch++)liveBanks[ch].store(int(std::lround(v[kCount+ch*2]*127))*128+int(std::lround(v[kCount+ch*2+1]*127)));return kResultOk;}
tresult PLUGIN_API Controller::notify(IMessage* m){if(m&&!std::strcmp(m->getMessageID(),"Status")){int64 v=0;auto* a=m->getAttributes();a->getInt("padLearning",v);const int oldPadLearning=padLearning.exchange(int(v));if(oldPadLearning>=0&&v<0)setDirty(true);a->getInt("learning",v);learning.store(int(v));for(int ch=0;ch<16;ch++){a->getInt(("bank"+std::to_string(ch)).c_str(),v);liveBanks[ch].store(int(v));}for(int t=0;t<kLiveTargets;t++){a->getInt(("binding"+std::to_string(t)).c_str(),v);learnedSources[t].store(int(v));a->getInt(("pickup"+std::to_string(t)).c_str(),v);learnedPickup[t].store(int(v));}for(int i=0;i<kCount;i++){double value=0;if(a->getFloat(("param"+std::to_string(i)).c_str(),value)==kResultOk&&i!=100&&i!=101&&i!=102&&i!=103)setParamNormalized(paramID(i),value);}for(int ch=0;ch<16;ch++){const auto key="controls"+std::to_string(ch);a->getInt(key.c_str(),v);liveControls[ch].store(int(v));}a->getInt("preview",v);previewActive.store(v!=0);a->getInt("warnings",v);warnings.store(int(v));a->getInt("reason",v);reason.store(int(v));a->getInt("channel",v);channel.store(int(v));a->getInt("voices",v);voices.store(int(v));for(int i=0;i<4;i++){const char* key[]={"voice0","voice1","voice2","voice3"};a->getInt(key[i],v);liveVoices[i].store(uint64_t(v));}return kResultOk;}return EditControllerEx1::notify(m);}
tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 bus,int16 ch,CtrlNumber cc,ParamID& id){
    if(bus||ch<0||ch>15)return kResultFalse;const int base=1000+ch*3;
    if(cc>=0&&cc<128)id=kRawCCBase+ch*128+cc;else if(cc==kCtrlProgramChange)id=0x11000+ch;else if(cc==kPitchBend)id=0x11100+ch;else return kResultFalse;return kResultTrue;}
tresult PLUGIN_API Controller::getUnitByBus(MediaType t,BusDirection d,int32 bus,int32 ch,UnitID& id){if(t==kEvent&&d==kInput&&bus==0&&ch>=0&&ch<16){id=ch+1;return kResultTrue;}return kResultFalse;}
tresult PLUGIN_API Controller::queryInterface(const TUID iid,void** obj){QUERY_INTERFACE(iid,obj,IMidiMapping::iid,IMidiMapping);return EditControllerEx1::queryInterface(iid,obj);}
SettingsSnapshot Controller::currentSettings(int voice){
 SettingsSnapshot s;voice=std::clamp(voice,0,3);s.channel=kLiveChannels[voice];const int ch=s.channel,bank=liveBanks[ch].load();
 auto value=[&](int id,int max=127){return std::clamp(int(std::lround(getParamNormalized(id)*max)),0,max);};
 s.bankMSB=bank/128;s.bankLSB=bank%128;s.program=value(1000+ch*3);s.duty=value(3000+ch);s.volume=value(3100+ch);s.expression=value(3200+ch);s.legato=value(3300+ch);s.glide=value(2200+ch);s.portamento=value(2300+ch);s.bend=value(2400+ch,16383);s.detune=value(3400+ch);return s;
}
void Controller::edit(ParamID id,double v){beginEdit(id);setParamNormalized(id,v);performEdit(id,v);endEdit(id);}
void Controller::performanceEdit(ParamID id,double v){edit(id,v);setDirty(true);}
void Controller::preset(int index,int voice){command("Recipe",index|(voice<<16));setDirty(true);}
void Controller::command(const char* id,int v){auto* m=allocateMessage();if(!m)return;m->setMessageID(id);m->getAttributes()->setInt("value",v);sendMessage(m);m->release();}
}
