#ifndef VALWOOD_WORLD_BANDIT_CAMP_H
#define VALWOOD_WORLD_BANDIT_CAMP_H
#include <cstddef>
#include <cstdint>
struct WorldScenery;
struct WorldBanditCamp;
struct BanditCampPathPoint {double x,z;float distance;};
struct BanditCampPathDef {unsigned first,count;float length;};
struct BanditCampActorDef {int mode,path;double x,z;float yaw,phase;};
struct BanditCampClipDef {unsigned base,count;float seconds,stride,height,foot_x,foot_z,bound;};
// Generated absolute paths, source pose/contact dimensions and clip periods.
#include "engine/world_bandit_camp_data.inc"
inline constexpr std::size_t world_bandit_camp_capacity=10;
struct WorldBanditCampStats {
    bool active=false;
    std::size_t actors=0,walking=0,running=0,sentries=0,visible=0,reflection_visible=0,shadow_visible=0;
    std::size_t pose_assets=0,draw_groups=0,peak_pose_assets=0,peak_visible=0;
    std::uint64_t updates=0,uploads=0,loads=0,unloads=0,load_failures=0,unsafe_roots=0;
};
struct WorldBanditCampActor {
    double x=0,z=0;float y=0,yaw=0,speed=0,cycle_phase=0,pose_blend=0,ground_correction=0;
    unsigned mode=0,pose_a=0,pose_b=0;bool patrolling=false;
};
// CPU-only catalog until an explicit render-thread loading-screen commit.
WorldBanditCamp *world_bandit_camp_create(const char *models_directory="models");
// 0 requests resident camp, -1 empty bank, -2 invalid input. Uses hysteresis.
int world_bandit_camp_desired(const WorldBanditCamp *,double camera_x,double camera_z);
bool world_bandit_camp_commit(WorldBanditCamp *,bool active,char *error=nullptr,std::size_t capacity=0);
// Absolute animation/path time; no IO, allocation or GL calls.
void world_bandit_camp_update(WorldBanditCamp *,const WorldScenery *,float time);
// Independent actual-camera batches: main0, reflection1, offscreen casters2.
void world_bandit_camp_draw(WorldBanditCamp *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0);
WorldBanditCampStats world_bandit_camp_stats(const WorldBanditCamp *);
bool world_bandit_camp_actor(const WorldBanditCamp *,std::size_t,WorldBanditCampActor *);
void world_bandit_camp_free(WorldBanditCamp *);
#endif
