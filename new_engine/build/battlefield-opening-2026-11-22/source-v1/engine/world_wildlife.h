#ifndef VALWOOD_WORLD_WILDLIFE_H
#define VALWOOD_WORLD_WILDLIFE_H
#include <cstddef>
#include <cstdint>
struct WorldScenery;
struct WorldWildlife;
struct WildlifeSpeciesDef { const char *stem; float stride, idle_seconds, radius, height, bound_radius, foot_half_x, foot_half_z; bool airborne=false; };
struct WildlifeHabitatDef { const char *name; double center_x,center_z; float enter_radius,exit_radius,draw_radius; unsigned species_mask; };
struct WildlifeActorDef { int species,habitat; double x,z; float yaw,route_radius,speed,phase; };
#include "engine/world_wildlife_data.inc"
inline constexpr std::size_t world_wildlife_capacity=40;
struct WorldWildlifeStats {
    int habitat=-1;
    std::size_t actors=0,walking=0,idling=0,flying=0,visible=0,shadow_visible=0,reflection_visible=0;
    std::size_t species=0,pose_assets=0,draw_groups=0,peak_pose_assets=0;
    std::uint64_t updates=0,uploads=0,loads=0,unloads=0,load_failures=0,unsafe_roots=0;
};
struct WorldWildlifePreparationStats {
    std::uint64_t calls=0,main_passes=0,reflection_passes=0,shadow_passes=0,pose_draws=0,failures=0;
};
struct WorldWildlifePrepareFrame {
    unsigned program=0,pass=0; // Actual main MRT0, water-reflection1 or shadow2.
    double x=0,y=0,z=0;
    float yaw=0,pitch=0;
    bool loading=false;
    bool constrain_minimum_y=false; // Reflection water clip floor only.
    double minimum_y=0;
};
struct WorldWildlifeActor {
    int species=-1,habitat=-1; double x=0,z=0;
    float y=0,yaw=0,speed=0,cycle_phase=0,pose_blend=0;
    unsigned pose_a=0,pose_b=0; bool walking=false;
};
// CPU catalog only: no model/image IO or GL calls until a loading-screen commit.
WorldWildlife *world_wildlife_create(const char *models_directory="models");
// CPU-only private visual-project profile. Call before loading a habitat bank.
bool world_wildlife_expand_jungle(WorldWildlife *);
// Live environment-effects quality for the camp-only above-canopy flock.
// Existing farm/jungle wildlife is unchanged; no asset load or allocation.
void world_wildlife_set_camp_quality(WorldWildlife *,unsigned ambient_particles,float life_distance);
// Hysteresis uses current residency. -1 requests an empty bank; -2 invalid input.
int world_wildlife_desired(const WorldWildlife *,double camera_x,double camera_z);
// CPU-only startup/destination hint: prefer normal/current residency, then
// allow one nearby bank inside its existing exit envelope. Never call this to
// expand ordinary walking hysteresis; it performs no commit, IO or GL work.
int world_wildlife_loading_desired(const WorldWildlife *,double camera_x,double camera_z);
// Render thread ONLY, behind startup/destination loading. All borrowed blend
// VAOs free before source poses. Failure leaves an empty bank; never half loaded.
bool world_wildlife_commit(WorldWildlife *,int habitat,char *error=nullptr,std::size_t capacity=0);
// Absolute time, bounded terrain/collision queries, no IO/allocations/GL calls.
void world_wildlife_update(WorldWildlife *,const WorldScenery *,float time);
// Independent pass batches use the ACTUAL camera (including reflected height).
// pass0 main,1 reflection,2 shadow. Shadow retains offscreen nearby casters.
void world_wildlife_draw(WorldWildlife *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0);
// Loading boundary ONLY. Caller binds the real pass target and prepares its
// projection, lighting, samplers and enabled depth test/writes. This clears
// bound depth before each pose; clear/redraw the actual target afterward.
// A temporary one-instance batch borrows all
// resident adjacent pose pairs and draws near this camera, then is destroyed.
// No source/model IO, persistent pose/batch additions, actor changes or normal
// upload/visibility/update-counter changes. Never call during ready play.
bool world_wildlife_prepare(WorldWildlife *,const WorldWildlifePrepareFrame &);
WorldWildlifePreparationStats world_wildlife_preparation_stats(const WorldWildlife *);
WorldWildlifeStats world_wildlife_stats(const WorldWildlife *);
bool world_wildlife_actor(const WorldWildlife *,std::size_t,WorldWildlifeActor *);
void world_wildlife_free(WorldWildlife *);
#endif
