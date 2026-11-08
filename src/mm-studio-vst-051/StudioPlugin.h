// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "StudioCore.h"
#include "SongPreview.h"
#include "PresetCatalog.h"
#include "LivePerformance.h"
#include "SettingsMidi.h"
#include "PercussionPads.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstevents.h"
#include <atomic>
#include "pluginterfaces/vst/ivstparameterchanges.h"
namespace mm5studio {
using namespace Steinberg;using namespace Steinberg::Vst;
inline const FUID kProcessorUID(0x02F03462,0xC9574B5F,0xA826AD5B,0x44BE1331);
inline const FUID kControllerUID(0x02F03462,0xC9574B5F,0xA826AD5B,0x44BE1332);
inline constexpr int kMain=4,kLegacyCount=kMain+48+32+16+4+64+3+75,kCount=kLegacyCount+16,kStored=kCount+32;
inline constexpr ParamID kOutput=100,kMode=101,kPreviewChannel=102,kPreviewNote=103,kReset=200;
inline ParamID paramID(int i){return i>=246?3400+i-246:i==245?503:i==244?502:i>=173?600+i-173:i==172?501:i==171?500:i>=168?400+i-168:i>=104?3000+(i-104)/16*100+(i-104)%16:i>=100?300+i-100:i<kMain?100+i:i<52?1000+i-kMain:i>=84?2400+i-84:2200+(i-52)/2+((i-52)%2)*100;}
inline int paramIndex(ParamID id){if(id>=3400&&id<3416)return 246+int(id)-3400;if(id==503)return 245;if(id==502)return 244;if(id==500||id==501)return 171+int(id)-500;if(id>=600&&id<671)return 173+int(id)-600;if(id>=400&&id<403)return 168+int(id)-400;for(int group=0;group<4;group++)if(id>=3000+group*100&&id<3016+group*100)return 104+group*16+int(id)-(3000+group*100);return id>=300&&id<304?100+int(id)-300:id>=100&&id<104?int(id)-100:id>=1000&&id<1048?int(id)-1000+kMain:id>=2200&&id<2216?52+(int(id)-2200)*2:id>=2300&&id<2316?53+(int(id)-2300)*2:id>=2400&&id<2416?84+int(id)-2400:-1;}
inline float defaultValue(int i){return i>=246?64/127.f:i>=244?0.f:i>=173?(9*128+36+i-173+1)/2048.f:i>=171?0.f:i>=168?(i==169?0.f:1.f):i>=104?(i<120?64/127.f:i<152?1.f:0.f):i>=100?(i==100?2.f/3:0.f):i>=84?8192/16383.f:i==0?.5f:i==1?0.f:i==2?0.f:i==3?60/127.f:i<52&&(i-kMain)%3==0?kPerformancePrograms[(i-kMain)/3]/127.f:i<52&&(i-kMain)%3==1?kPerformanceBanks[(i-kMain)/3]/127.f:0.f;}
inline ParamID controlID(int ch,int cc){return cc==21?3400+ch:cc==20?3000+ch:cc==7?3100+ch:cc==11?3200+ch:cc==68?3300+ch:cc==5?2200+ch:cc==65?2300+ch:cc==120?2500+ch:cc==121?2600+ch:cc==123?2000+ch:2100+ch;}
class Processor final:public AudioEffect {
public:
    Processor();static FUnknown* createInstance(void*){return static_cast<IAudioProcessor*>(new Processor);}
    tresult PLUGIN_API initialize(FUnknown*)override;
    tresult PLUGIN_API setupProcessing(ProcessSetup&)override;
    tresult PLUGIN_API setActive(TBool)override;
    tresult PLUGIN_API setProcessing(TBool)override;
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32)override;
    tresult PLUGIN_API canProcessSampleSize(int32 size)override{return size==kSample32||size==kSample64?kResultTrue:kResultFalse;}
    uint32 PLUGIN_API getProcessContextRequirements()override{return IProcessContextRequirements::kNeedTransportState|IProcessContextRequirements::kNeedTempo;}
    uint32 PLUGIN_API getLatencySamples()override{return core.latency();}
    uint32 PLUGIN_API getTailSamples()override{return uint32(processSetup.sampleRate*4);}
    tresult PLUGIN_API process(ProcessData&)override;
    tresult PLUGIN_API getState(IBStream*)override;
    tresult PLUGIN_API setState(IBStream*)override;
    tresult PLUGIN_API notify(IMessage*)override;
private:
    LiveMappings mappings;
    std::atomic<int> learnRequest{-2},forgetRequest{-1},sendRequest{-1},recipeRequest{-1};
    std::atomic<bool> mapRestore{false};
    std::atomic<int> savedBindings[kLiveTargets];
    IEventList* midiOutput=nullptr;bool recording=false,snapshotPending=false;
    std::atomic<int> learnedSources[kLiveTargets],learnedPickup[kLiveTargets],learning{-1};
    void incomingCC(int,int,int);void emitParam(ParamID,double);void snapshot(int);void loadRecipe(int,int,bool=false);
    void emitCC(int,int,int,int=0);
    std::atomic<int> percussionRequest{-1};int soundingPad=-1;double padRemaining=0,hostTempo=120;
    void stopPercussionPad();void startPercussionPad(int,float=1);
    std::atomic<int> padLearnRequest{-2},padLearning{-1};
    std::array<std::array<int,71>,kSongCount> padKits{};std::array<int,kSongCount> padKitCounts{};std::array<bool,2048> padConsumed{};
    bool routePadEvent(const Event&);
    bool padPatchSent=false;int padPatchMSB=-1,padPatchLSB=-1,padPatchProgram=-1;
    SongPreview songPreview;std::atomic<int> libraryRequest{-2},settingsRequest{-1};
    IParameterChanges* feedback=nullptr;int feedbackOffset=0;
    StudioCore core;const uint8_t* bank=nullptr;int bankSize=0;bool reaperHost=false;
    std::atomic<float> values[kCount];std::atomic<bool> restore{false},reset{false},clear{false};
    std::atomic<int> preview{-2},pad{-1};std::atomic<uint64_t> patchPreview{~0ull};float gain=1;
    std::atomic<uint64_t> liveVoices[4]{};
    std::atomic<int> liveControls[16]{};
    std::atomic<bool> previewActive{false};
    std::atomic<int> statusWarnings{0},statusReason{0},statusChannel{0},statusVoices{0};
    std::atomic<int> committedMSB[16]{},committedLSB[16]{};
    void change(ParamID,double,bool emit=true);void event(const Event&);void synchronize();
    template<class T>void render(ProcessData&);
};
class Controller final:public EditControllerEx1,public IMidiMapping {
public:
    static FUnknown* createInstance(void*){return static_cast<IEditController*>(new Controller);}
    tresult PLUGIN_API initialize(FUnknown*)override;
    tresult PLUGIN_API setComponentState(IBStream*)override;
    tresult PLUGIN_API setState(IBStream*)override;
    tresult PLUGIN_API getState(IBStream*)override;
    tresult PLUGIN_API notify(IMessage*)override;
    IPlugView* PLUGIN_API createView(FIDString)override;
    tresult PLUGIN_API getMidiControllerAssignment(int32,int16,CtrlNumber,ParamID&)override;
    tresult PLUGIN_API getUnitByBus(MediaType,BusDirection,int32,int32,UnitID&)override;
    tresult PLUGIN_API queryInterface(const TUID,void**)override;
    std::atomic<int> padLearning{-1};
    SettingsSnapshot currentSettings(int voice);
    void edit(ParamID,double);void performanceEdit(ParamID,double);void preset(int,int);void command(const char*,int value=0);
    std::atomic<int> liveBanks[16]{};
    std::atomic<uint64_t> liveVoices[4]{};
    std::atomic<int> liveControls[16]{};
    std::atomic<bool> previewActive{false};
    std::atomic<int> warnings{0},reason{0},channel{0},voices{0};
    std::atomic<int> learnedSources[kLiveTargets],learnedPickup[kLiveTargets],learning{-1};
    REFCOUNT_METHODS(EditControllerEx1)
};
}
