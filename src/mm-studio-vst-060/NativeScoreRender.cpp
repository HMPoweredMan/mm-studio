// SPDX-License-Identifier: GPL-3.0-only
#include "DriverCore.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
int main(int argc,char** argv){
 if(argc!=5)return 1;std::ifstream bankFile(argv[1],std::ios::binary),scoreFile(argv[2],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(bankFile)),{}),score((std::istreambuf_iterator<char>(scoreFile)),{});
 mm5studio::DriverCore driver;if(!driver.setup(bank.data(),int(bank.size()),48000)||!driver.playScore(score.data(),int(score.size())))return 2;
 std::ofstream audio(argv[3],std::ios::binary),trace(argv[4]);trace<<"seconds,pulse1Duty,pulse1Level,pulse1Phase,pulse1Period,trianglePhase\n";
 for(int n=0;n<48000*8;n++){const float sample=driver.render();audio.write(reinterpret_cast<const char*>(&sample),sizeof(sample));
  if(n%400==0)trace<<n/48000.<<','<<(driver.reg(0x70c+0x2b)&0xc0)<<','<<(driver.reg(0x70c+0x2b)&15)<<','<<(driver.reg(0x704+0x2b)&7)<<','<<(driver.reg(0x720+0x2b)|(driver.reg(0x724+0x2b)<<8))<<','<<(driver.reg(0x704+0x29)&7)<<'\n';
  if(driver.failed())return 3;
 }std::cout<<"PASS: compiled native payload executes for two performance-demo cycles\n";
}
