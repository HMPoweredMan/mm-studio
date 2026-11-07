// SPDX-License-Identifier: GPL-3.0-only
#include "CollectionBrowser.h"
#include "PresetCatalog.h"
#include "ProgramNames.h"
#include <set>
#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>
using namespace mm5studio;
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
 std::set<int> visible;
 for(int game=0;game<4;game++){
  const auto entries=browserCollections(game);check(!entries.empty()&&entries[0]==browserFirstCollection(game),"native collection first");
  for(int i:entries){check(kCollectionGames[i]==game+3&&i!=28,"correct game and no GM review collection");check(visible.insert(i).second,"unique collection membership");}
 }
 check(visible.size()==kSongCount-1,"all other collections retained");check(browserCollections(2)[0]==27,"MM5 native first");check(visibleCollection(28)==27,"hidden legacy selection has native fallback");
 for(int i=0;i<kSongCount;i++)if(i!=28)check(visibleCollection(i)==i,"visible serialized IDs unchanged");
 for(int i=0;i<kSoundCount;i++)check(recipeTimbre(i)>=0&&recipeTimbre(i)<kTimbreCount&&std::strlen(recipeName(i))>0,"every recipe has name");
 check(std::size(kProgramLabels)==822,"all program/voice combinations named");
 for(const auto& l:kProgramLabels)check(std::string(nativeProgramName(l.bank,l.voice,l.program))==l.name,"selector labels resolve");
 std::cout<<"PASS: four native-first game menus, hidden GM collection fallback, 121 visible collections, stable IDs and 822 program/voice labels\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
