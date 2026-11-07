// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Nes_Cpu.h"
#include "Nes_Apu.h"
#include "Blip_Buffer.h"
#include "PerformanceDriverPatch.h"
#include "NativeBanks.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace mm5studio {
// Executes MM5 sound data with the guarded instant-legato dispatch fix.
class DriverCore final:public Nes_Cpu {
public:
    struct Write { int frame,cycle,address,value; };
    using Observer=void(*)(void*,const Write&);
    Observer observer=nullptr;void* observerData=nullptr;
    bool setup(const uint8_t* original,int size,int rate) {
        if(size!=8192||rate<8000||rate>384000)return false;
        std::copy(original,original+8192,rom.begin());if(!applyPerformanceDriverPatch(rom.data(),size))return false;std::copy(std::begin(kAdditionalInstruments),std::end(kAdditionalInstruments),rom.begin()+0xadb+71*8);sampleRate=rate;
        if(buffer.set_sample_rate(rate,100))return false;
        buffer.clock_rate(1789773);buffer.bass_freq(20);apu.output(&buffer);apu.volume(.65);
        silence();return true;
    }
    void silence() {
        bad=false;held.fill(false);awaitingAttackTick.fill(false);pendingRelease.fill(false);releaseAge.fill(0);samplesToFrame=sampleRate/60.098811862348406;frameIndex=0;
        Nes_Cpu::reset(unmapped.data());std::fill(std::begin(low_mem),std::end(low_mem),0);
        map_code(0x8000,8192,rom.data());map_code(0xa000,8192,streams.data());
        sentinel.fill(0xf2);map_code(0x6000,0x800,sentinel.data());
        buffer.clear();apu.reset(false,0);call(0x8003,0xf0);
        apu.write_register(0,0x4015,15);apu.write_register(0,0x4017,0x40);
    }
    int read(unsigned address) {if(address<0x2000)return low_mem[address&2047];return *get_code(address);}
    bool playScore(const uint8_t* data,int size){
        if(size<9||size>8192)return false;silence();std::copy(data,data+size,streams.begin());
        for(int voice=3;voice>=0;voice--){const int header=1+(3-voice)*2,address=(data[header]<<8)|data[header+1],x=voice+0x28;if(address<0xa009||address>=0xa000+size)return false;
            low_mem[0x728+x]=uint8_t(address);low_mem[0x72c+x]=uint8_t(address>>8);low_mem[0x738+x]=low_mem[0x730+x]=low_mem[0x734+x]=0;call(0x8393,0,voice);
        }return !bad;
    }
    void write(unsigned address,int value,int time) {
        if(address<0x2000){low_mem[address&2047]=uint8_t(value);return;}
        if(address>=0x4000&&address<=0x4017&&address!=0x4014&&address!=0x4016){
            // Calls execute atomically at the event/frame boundary. Instruction timing is retained in trace evidence.
            if(observer)observer(observerData,{frameIndex,time,int(address),value});
            apu.write_register(0,address,value);
        }
    }
    bool call(int address,int accumulator=0,int index=-1,int y=0) {
        r.pc=uint16_t(address);r.a=uint8_t(accumulator);r.y=uint8_t(y);if(index>=0)r.x=uint8_t(index);
        r.sp=0xdd;low_mem[0x1de]=0xff;low_mem[0x1df]=0x5f;r.status=4;
        set_time(0);const bool stopped=run(24000);
        if(!stopped||r.pc!=0x6000||error_count()){bad=true;return false;}return true;
    }
    void noteOn(int voice,int preset,int pitch,int volume,int duty,int gate=255,int duration=0,int portamento=0,int detune=0) {
        if(bad||voice<0||voice>3||preset<0||preset>=kInternalInstrumentCount)return;
        const int x=voice+0x28,addr=0xa000+voice*256;
        auto* data=streams.data()+voice*256;int i=0;
        data[i++]=8;data[i++]=uint8_t(preset);data[i++]=7;data[i++]=uint8_t(volume);
        data[i++]=6;data[i++]=uint8_t(gate);
        data[i++]=0x0d;data[i++]=uint8_t(portamento);
        data[i++]=0x0c;data[i++]=uint8_t(detune); // Restore zero too when leaving a named detuned patch.
        if(voice>=2){data[i++]=0x18;data[i++]=uint8_t(duty);}
        if(voice){data[i++]=9;data[i++]=uint8_t(pitch/12);data[i++]=uint8_t(0xe0+pitch%12+1);}
        else data[i++]=uint8_t(0xe0+16-pitch); // noise: caller passes hardware period 0..15
        data[i++]=0x17;data[i++]=0;
        low_mem[0x728+x]=uint8_t(addr);low_mem[0x72c+x]=uint8_t(addr>>8);
        low_mem[0x738+x]=0;low_mem[0x730+x]=0;low_mem[0x734+x]=0;
        low_mem[0x714+x]=0;low_mem[0x718+x]=0;low_mem[0xc7]=0;
        // MIDI can enter this voice between native frame updates. Re-select
        // its instrument context even when opcode 08 sees the same preset;
        // that opcode otherwise keeps the last voice's shared $C5/$C6 pointer.
        call(0x8684,0,x,preset+1);
        call(0x8393,0,voice);
        awaitingAttackTick[voice]=true;pendingRelease[voice]=false;
        held[voice]=duration==0;releaseAge[voice]=0;
        if(duration>0){low_mem[0x738+x]=uint8_t(duration);low_mem[0x740+x]=uint8_t(std::max(1,duration*gate/256));}
    }
    void noteOff(int voice,bool immediate=false) {
        if(bad||voice<0||voice>3)return;held[voice]=false;releaseAge[voice]=0;
        // A short MIDI gate may end before this native frame has processed
        // the attack. Native sequencer gates cannot cancel their own first
        // envelope update. Release immediately after that update instead.
        if(!immediate&&awaitingAttackTick[voice]){pendingRelease[voice]=true;return;}
        pendingRelease[voice]=false;awaitingAttackTick[voice]=false;
        call(0x85a3,0,voice+0x28);
    }
    void detune(int voice,int value){if(voice>0&&voice<4)low_mem[0x714+0x28+voice]=uint8_t(value);}
    void portamento(int voice,int speed){if(voice>0&&voice<4)low_mem[0x718+0x28+voice]=uint8_t(speed);}
    void levelDuty(int voice,int volume,int duty){
        if(voice<0||voice>3)return;const int x=voice+0x28;
        low_mem[0x70c+x]=uint8_t((voice>=2?duty:low_mem[0x70c+x]&0xc0)|0x30|std::clamp(volume,0,15));
    }
    void kill(int voice){
        if(voice<0||voice>3)return;const int x=voice+0x28;
        held[voice]=awaitingAttackTick[voice]=pendingRelease[voice]=false;
        low_mem[0x728+x]=low_mem[0x72c+x]=0;call(0x80d8,0,x);
    }
    void retune(int voice,int pitch,int speed){
        if(bad||voice<1||voice>3||pitch<0||pitch>95)return;
        const int x=voice+0x28,preset=low_mem[0x700+x];if(!preset)return;
        // Native tied-note pitch handling: no L85AE key-on and no envelope reset.
        call(0x8684,0,x,preset);low_mem[0x718+x]=uint8_t(speed);
        const int flags=low_mem[0x730+x];low_mem[0x730+x]|=128;
        call(0x85de,pitch,x);low_mem[0x730+x]=uint8_t(flags);
    }
    void setTempo(double bpm) {
        const int step=std::clamp(int(48*bpm/60/60.098811862348406*256+.5),1,65535);
        low_mem[0xc9]=uint8_t(step>>8);low_mem[0xca]=uint8_t(step);
    }
    void tick() {
        for(int voice=0;voice<4;voice++)if(held[voice]){
            low_mem[0x760+voice]=200;low_mem[0x768+voice]=200;
        }
        frameIndex++;call(0x8000);
        for(int voice=0;voice<4;voice++){
            awaitingAttackTick[voice]=false;
            if(pendingRelease[voice]){pendingRelease[voice]=false;call(0x85a3,0,voice+0x28);}
        }
    }
    float render() {
        if(bad)return 0;
        // Use the resampler's actual fixed-point clock ratio, so generated
        // samples cannot accumulate behind the host and add growing latency.
        const int clocks=buffer.count_clocks(1);
        apu.end_frame(clocks);buffer.end_frame(clocks);
        // Driver updates follow host time independently of ratio rounding.
        if(--samplesToFrame<=0){tick();samplesToFrame+=sampleRate/60.098811862348406;}
        short output=0;buffer.read_samples(&output,1);return output/32768.f;
    }
    bool failed()const{return bad;}
    int outputLatency()const{return buffer.output_latency();}
    int queuedSamples()const{return int(buffer.samples_avail());}
    uint8_t reg(int address)const{return low_mem[address&2047];}
private:
    std::array<uint8_t,8192> rom{},streams{};
    std::array<uint8_t,2048> unmapped{},sentinel{};
    Nes_Apu apu;Blip_Buffer buffer;
    std::array<bool,4> held{};std::array<int,4> releaseAge{};
    std::array<bool,4> awaitingAttackTick{},pendingRelease{};
    double samplesToFrame=1;int sampleRate=48000,frameIndex=0;bool bad=false;
};
}
