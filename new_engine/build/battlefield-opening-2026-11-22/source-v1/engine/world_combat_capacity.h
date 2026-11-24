#ifndef VALWOOD_WORLD_COMBAT_CAPACITY_H
#define VALWOOD_WORLD_COMBAT_CAPACITY_H
namespace world_combat_capacity {
inline constexpr unsigned opponents=24,teammates=6,npcs=opponents+teammates,actors=npcs+1;
inline constexpr unsigned actor_mask=(1u<<actors)-1u;
}
#endif
