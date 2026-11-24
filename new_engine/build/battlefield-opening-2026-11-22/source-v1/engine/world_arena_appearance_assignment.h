#ifndef VALWOOD_WORLD_ARENA_APPEARANCE_ASSIGNMENT_H
#define VALWOOD_WORLD_ARENA_APPEARANCE_ASSIGNMENT_H
#include <algorithm>
#include "world_combat_capacity.h"
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace world_arena_appearance_assignment {
// A run owns its shuffled deck. The stable opponent ID keeps this entry while
// queued, fighting, dying and lying on the ground; render frames never reroll.
inline bool make(const std::vector<std::string>&roster,unsigned count,std::uint64_t seed,
                 std::vector<std::string>&out){
 if(roster.empty()||roster.size()>32||!count||count>world_combat_capacity::npcs)return false;
 auto unique=roster;std::sort(unique.begin(),unique.end());
 if(unique.front().empty()||std::adjacent_find(unique.begin(),unique.end())!=unique.end())return false;
 std::mt19937_64 rng(seed);std::vector<std::string> next;next.reserve(count);
 while(next.size()<count){auto deck=roster;std::shuffle(deck.begin(),deck.end(),rng);
  for(const auto&id:deck){if(next.size()==count)break;next.push_back(id);}
 }
 out=std::move(next);return true;
}
}
#endif
