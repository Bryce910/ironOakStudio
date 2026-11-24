#ifndef VALWOOD_WORLD_COMBAT_MOTION_H
#define VALWOOD_WORLD_COMBAT_MOTION_H
#include <cstddef>
#include "engine/world_training_combat.h"
struct WorldCombatMotion;
namespace world_combat_motion {
inline constexpr float reaction_duration=.32f;
inline constexpr float death_duration=1.8f;
}
struct WorldCombatBlade {double base[3]={},tip[3]={};};
struct WorldCombatPose {unsigned slot=0,guard_direction=2;float guard_weight=0,blend=0;bool active=false;unsigned reaction_direction=2;bool defeated=false;float defeat_phase=0;bool combo_transition=false;unsigned combo_from=2,combo_to=2;};
// Read the pose committed by the renderer, rather than the requested input.
WorldCombatPose world_combat_motion_pose(const WorldCombatMotion *,unsigned actor);
bool world_combat_motion_blade(const WorldCombatMotion *,unsigned actor,WorldCombatBlade &,bool viewmodel=false);
struct WorldCombatMotionStats{unsigned pose_assets=0,actors=0,crowd_instances=0;unsigned long long uploads=0;bool polished=false;unsigned reaction_assets=0,death_assets=0,fallen_instances=0,peak_fallen_instances=0;unsigned long long directional_reactions=0,death_starts=0;unsigned active_enemies=0,peak_actors=0,combo_batches=0;unsigned long long combo_transitions=0;};
// All pose/transition batches are allocated at loading time. Shared texture keys
// are the purchased shield knight's existing original material images.
// Polished banks require172 poses plus validated sockets; their visual sampling
// adds attack weight while retaining the same gameplay contact boundaries.
WorldCombatMotion *world_combat_motion_load(const char *directory,char *error,std::size_t capacity,bool polished=false);
void world_combat_motion_arena(WorldCombatMotion *,double origin_x,double origin_z,float feet);
// Explicit arena-only behavior; ordinary worlds retain their existing poses.
void world_combat_motion_arena_enemy_walk(WorldCombatMotion *,bool walking);
struct WorldCombatCrowdInstance {double x=0,y=0,z=0;float yaw=0;bool yielded=false;float defeat_phase=0;};
// Loading-boundary allocation, then bounded uploads sharing the already loaded
// idle/collapse pose VBOs and texture references. At most8 additional knights.
bool world_combat_motion_prepare_crowd(WorldCombatMotion *);
bool world_combat_motion_crowd(WorldCombatMotion *,const WorldCombatCrowdInstance *,unsigned count);
void world_combat_motion_draw_crowd(WorldCombatMotion *,unsigned program,double eye_x,double eye_y,double eye_z,bool shadow=false);
// A newly published body/weapon contact commits once before its hit-stop;
// subsequent stopped frames retain the committed pose and upload buffers.
void world_combat_motion_update(WorldCombatMotion *,const WorldTrainingCombat &,double x,double feet,double z,float yaw,float dt,bool show_enemy,bool paused=false);
// Stable slots1..8 belong to arena opponents0..7. These are read-only pose
// snapshots; only the arena owns simulation and the shared player clock.
struct WorldCombatArenaActor {WorldTrainingCombat combat;double x=0,y=0,z=0;bool visible=false,walking=false;};
void world_combat_motion_update_arena(WorldCombatMotion *,const WorldTrainingCombat &,double x,double feet,double z,float yaw,float dt,const WorldCombatArenaActor *,unsigned count,bool paused=false);
void world_combat_motion_draw(WorldCombatMotion *,unsigned program,double x,double y,double z,bool shadow,bool show_player=true);
void world_combat_motion_draw_arms(WorldCombatMotion *,unsigned program);
WorldCombatMotionStats world_combat_motion_stats(const WorldCombatMotion *);
void world_combat_motion_free(WorldCombatMotion *);
#endif
