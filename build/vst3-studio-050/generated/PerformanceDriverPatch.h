#pragma once
#include <algorithm>
namespace mm5studio {
inline bool applyPerformanceDriverPatch(unsigned char* data,int size){if(size!=8192)return false;
constexpr unsigned char before0[]={189,24,7};
constexpr unsigned char after0[]={32,27,138};
constexpr unsigned char before1[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
constexpr unsigned char after1[]={189,24,7,201,128,208,7,169,0,157,24,7,169,1,96};
if(std::equal(after0,after0+3,data+1077)&&std::equal(after1,after1+15,data+2587))return true;
if(!(std::equal(before0,before0+3,data+1077)&&std::equal(before1,before1+15,data+2587)))return false;
std::copy(after0,after0+3,data+1077);
std::copy(after1,after1+15,data+2587);
return true;}
}
