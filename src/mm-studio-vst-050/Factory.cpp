// SPDX-License-Identifier: GPL-3.0-only
#include "StudioPlugin.h"
#include "public.sdk/source/main/pluginfactory.h"
using namespace Steinberg;using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("KSA Tools","","")
DEF_CLASS2(INLINE_UID_FROM_FUID(mm5studio::kProcessorUID),PClassInfo::kManyInstances,kVstAudioEffectClass,"MM Studio 0.5.0",Vst::kDistributable,"Instrument|Synth|Drum","0.5.0",kVstVersionString,mm5studio::Processor::createInstance)
DEF_CLASS2(INLINE_UID_FROM_FUID(mm5studio::kControllerUID),PClassInfo::kManyInstances,kVstComponentControllerClass,"MM Studio Controller",0,"","0.5.0",kVstVersionString,mm5studio::Controller::createInstance)
END_FACTORY
