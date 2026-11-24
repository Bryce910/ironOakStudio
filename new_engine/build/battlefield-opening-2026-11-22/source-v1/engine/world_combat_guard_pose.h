#ifndef VALWOOD_WORLD_COMBAT_GUARD_POSE_H
#define VALWOOD_WORLD_COMBAT_GUARD_POSE_H
// The Orc's authored side guard actually covers its legs; the labelled low
// guard leaves the blade above an incoming knee cut. Select the usable source
// stance for BOTH visible motion and geometric prediction, without changing
// body/weapon surfaces or declaring semantic blocks.
inline unsigned world_combat_guard_pose(unsigned direction,bool orc){
    return orc&&direction==3?0:direction;
}
#endif
