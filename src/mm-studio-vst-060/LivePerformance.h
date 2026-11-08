// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace mm5studio {
inline constexpr int kLiveTargets=32,kRawCCBase=0x10000;
inline constexpr int kLiveChannels[]={0,1,2,9};
inline constexpr int kLiveCCs[]={20,7,11,5,65,68,-1};
inline int liveKind(int target){return target>=28?7:target%7;}
inline int liveParam(int target){if(target>=28)return 3400+kLiveChannels[target-28];const int ch=kLiveChannels[target/7],kind=target%7;return kind==0?3000+ch:kind==1?3100+ch:kind==2?3200+ch:kind==3?2200+ch:kind==4?2300+ch:kind==5?3300+ch:2400+ch;}
inline double liveValue(int kind,int value){return kind==6?std::clamp(8192+std::clamp(int(std::lround(value*4./127))-2,-2,2)*4096,0,16383)/16383.:kind==0?(value/32)*32/127.:kind==4||kind==5?(value>=64?1.:0.):value/127.;}
struct LiveMappings {
 std::array<int,kLiveTargets> source,previous,targetValue;
 std::array<bool,kLiveTargets> acquired;
 int learning=-1;
 LiveMappings(){source.fill(-1);resetPickup();}
 void resetPickup(){previous.fill(-1);targetValue.fill(-1);acquired.fill(false);}
 void reset(int target){previous[target]=-1;targetValue[target]=-1;acquired[target]=false;}
 void bind(int target,int key,int physical){for(int i=0;i<kLiveTargets;i++)if(source[i]==key){source[i]=-1;reset(i);}source[target]=key;reset(target);previous[target]=physical;learning=-1;}
 int find(int key)const{for(int i=0;i<kLiveTargets;i++)if(source[i]==key)return i;return -1;}
 bool accept(int t,int physical,int current,bool pickup){
  if(targetValue[t]!=current)acquired[t]=false;
  targetValue[t]=current;const int old=previous[t];previous[t]=physical;
  if(!pickup||std::abs(physical-current)<=1||(old>=0&&(old-current)*(physical-current)<=0))acquired[t]=true;
  return acquired[t];
 }
 void applied(int t,int current){targetValue[t]=current;}
};
}
