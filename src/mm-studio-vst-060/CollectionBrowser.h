// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "SongLibrary.h"
#include <vector>
#include <algorithm>
namespace mm5studio {
// Presentation order only: serialized collection/recipe IDs and MIDI banks stay stable.
inline int browserFirstCollection(int gameIndex){const int first[]={kMM3FirstCollection,kMM4FirstCollection,27,kMM6FirstCollection};return first[std::clamp(gameIndex,0,3)];}
inline int visibleCollection(int index){return index==28?27:std::clamp(index,0,kSongCount-1);}
inline std::vector<int> browserCollections(int gameIndex){
 std::vector<int> result;const int first=browserFirstCollection(gameIndex);result.push_back(first);
 for(int i=0;i<kSongCount;i++)if(i!=first&&i!=28&&kCollectionGames[i]==gameIndex+3)result.push_back(i);
 return result;
}
}
