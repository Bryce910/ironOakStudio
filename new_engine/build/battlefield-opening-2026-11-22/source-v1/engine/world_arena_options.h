#ifndef VALWOOD_WORLD_ARENA_OPTIONS_H
#define VALWOOD_WORLD_ARENA_OPTIONS_H
#include "world_combat_difficulty.h"
namespace world_arena_options {
using Difficulty=world_combat_difficulty::Difficulty;
// Arena labels deliberately retain the existing Normal timing as Easy.
// The separate story preferences keep their original four-tier semantics.
inline constexpr bool valid_difficulty(Difficulty d){return d==Difficulty::Normal||d==Difficulty::Hard||d==Difficulty::Expert;}
inline constexpr Difficulty difficulty(unsigned index){return index==1?Difficulty::Hard:index==2?Difficulty::Expert:Difficulty::Normal;}
inline constexpr unsigned difficulty_index(Difficulty d){return d==Difficulty::Hard?1u:d==Difficulty::Expert?2u:0u;}
inline constexpr const char *difficulty_name(Difficulty d){return d==Difficulty::Hard?"Medium":d==Difficulty::Expert?"Hard":"Easy";}
}
#endif
