#pragma once
#include <cstdint>
namespace mm5studio {
inline constexpr int kPresetCount=71,kNativeBank=125;
inline constexpr const char* kPresetNames[]={"MM5 $00","MM5 $01","MM5 $02","MM5 $03","MM5 $04","MM5 $05","MM5 $06","MM5 $07","MM5 $08","MM5 $09","MM5 $0A","MM5 $0B","MM5 $0C","MM5 $0D","MM5 $0E","MM5 $0F","MM5 $10","MM5 $11","MM5 $12","MM5 $13","MM5 $14","MM5 $15","MM5 $16","MM5 $17","MM5 $18","MM5 $19","MM5 $1A","MM5 $1B","MM5 $1C","MM5 $1D","MM5 $1E","MM5 $1F","MM5 $20","MM5 $21","MM5 $22","MM5 $23","MM5 $24","MM5 $25","MM5 $26","MM5 $27","MM5 $28","MM5 $29","MM5 $2A","MM5 $2B","MM5 $2C","MM5 $2D","MM5 $2E","MM5 $2F","MM5 $30","MM5 $31","MM5 $32","MM5 $33","MM5 $34","MM5 $35","MM5 $36","MM5 $37","MM5 $38","MM5 $39","MM5 $3A","MM5 $3B","MM5 $3C","MM5 $3D","MM5 $3E","MM5 $3F","MM5 $40","MM5 $41","MM5 $42","MM5 $43","MM5 $44","MM5 $45","MM5 $46 (silent entry)"};
struct Recipe { int instrument,voice,pitch,period,gate,volume,ticks; };
struct Drum { int note; Recipe recipe; const char* name; };
inline constexpr Drum kDrums[]={
{23,{21,1,7,0,220,0,24},"Triangle low kick L"},
{24,{25,0,0,6,90,9,12},"Electrical zap (custom)"},
{35,{20,0,0,13,100,10,12},"Kick H"},
{36,{20,0,0,12,200,9,12},"Kick A"},
{38,{20,0,0,7,46,9,12},"Snare C"},
{40,{20,0,0,8,46,9,12},"Alternate snare J"},
{41,{21,1,36,0,240,0,12},"Low Floor Tom"},
{42,{20,0,0,2,200,8,12},"Cymbal B *"},
{43,{21,1,40,0,240,0,12},"High Floor Tom"},
{45,{21,1,44,0,240,0,12},"Low Tom"},
{47,{21,1,48,0,240,0,12},"Low-Mid Tom"},
{48,{21,1,52,0,240,0,12},"Hi-Mid Tom"},
{49,{5,0,0,3,100,5,72},"Crash-like G *"},
{50,{21,1,56,0,240,0,12},"High Tom"},
{51,{20,0,0,4,200,8,12},"Ride-like E *"},
{77,{20,0,0,10,46,9,12},"Dry strike W *"},
};
inline constexpr Recipe kCymbals[]={{20,0,0,2,200,8,12},{20,0,0,3,200,8,12},{20,0,0,1,200,8,12},{20,0,0,1,100,9,12}};
inline constexpr int kChannelVoices[]={3,2,1,-1,-1,-1,-1,-1,-1,0,-1,-1,-1,-1,-1,-1};
inline constexpr int kDefaultPrograms[]={80,80,38,80,80,80,80,80,80,0,80,80,80,80,80,80};
inline constexpr int kGmInstruments[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
inline constexpr int kGmDuties[]={128,128,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
}
