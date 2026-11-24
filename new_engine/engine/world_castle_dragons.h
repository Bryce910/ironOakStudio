#ifndef VALWOOD_WORLD_CASTLE_DRAGONS_H
#define VALWOOD_WORLD_CASTLE_DRAGONS_H
#include <cstddef>
#include <cstdint>
#include "engine/world_castle_dragons_layout.h"
struct WorldCastleDragons;
enum class WorldCastleDragonStage : unsigned { Flight,Approach,Landing,Idle,Breath,Takeoff,Departure,AerialBreath };
struct WorldCastleDragon {
    unsigned variant=0,pose_a=0,pose_b=0;
    WorldCastleDragonStage stage=WorldCastleDragonStage::Flight;
    double x=0,z=0,cycle_time=0;
    float y=0,yaw=0,blend=0,ground_y=0,breath=0;
    double mouth[3]={};
    float direction[3]={0,0,1},minimum[3]={},maximum[3]={};
    bool grounded=false,transition=false;
};
struct WorldCastleDragonsStats {
    bool active=false;
    unsigned actors=0,airborne=0,grounded=0,breathing=0,visible=0,reflection_visible=0,shadow_visible=0,pose_assets=0,blend_batches=0;
    std::uint64_t loads=0,unloads=0,failures=0,updates=0,uploads=0,draws=0;
};
// The atlas must be initialized. Pure bounded sampler for authoring/tests;
// runtime caches the surveyed terrain support at loading boundaries.
double world_castle_dragons_cycle_seconds();
bool world_castle_dragons_sample(unsigned variant,double time,WorldCastleDragon *out);
WorldCastleDragons *world_castle_dragons_create(const char *directory="models/castle_dragons");
bool world_castle_dragons_desired(const WorldCastleDragons *,double camera_x,double camera_z);
// Render thread, startup/destination loading only. All poses and matching
// adjacent/cross-clip VAOs are linked here, never during ready play.
bool world_castle_dragons_commit(WorldCastleDragons *,bool active,char *error=nullptr,std::size_t capacity=0);
// Absolute simulation time. CPU pose selection and at most two tiny instance
// uploads; no allocation, disk IO, source relinking or terrain scans.
void world_castle_dragons_update(WorldCastleDragons *,double time);
// Same committed geometry in main0/reflection1/shadow2. No uploads/allocation.
void world_castle_dragons_draw(WorldCastleDragons *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0);
WorldCastleDragonsStats world_castle_dragons_stats(const WorldCastleDragons *);
bool world_castle_dragons_actor(const WorldCastleDragons *,unsigned,WorldCastleDragon *);
void world_castle_dragons_free(WorldCastleDragons *);
#endif
