#ifndef VALWOOD_WORLD_TRAIL_CAMP_H
#define VALWOOD_WORLD_TRAIL_CAMP_H
#include <cstddef>
#include <cstdint>
#include "engine/world_demo_polish_layout.h"
struct WorldScenery;
struct WorldTrailCamp;
struct WorldTrailCampStats {
    bool active=false;
    unsigned actors=0,walking=0,seated=0,visible=0,pose_assets=0;
    std::uint64_t loads=0,unloads=0,failures=0,unsafe_roots=0,uploads=0,updates=0;
};
struct WorldTrailCamper {double x=0,z=0;float y=0,yaw=0,blend=0;unsigned pose_a=0,pose_b=0;bool walking=false,seated=false;};
struct WorldTrailCampPlacement {double x=0,y=0,z=0;float yaw=0;bool seated=false;};
WorldTrailCamp *world_trail_camp_create(const char *directory="models");
// Optional opening-only placement. Configure while unloaded; all three roots
// are authored in absolute world metres and stay fixed throughout their idles.
bool world_trail_camp_configure_arrival(WorldTrailCamp *,double x,double y,double z,
    const WorldTrailCampPlacement *,unsigned count);
// Optional arrival-only render heading. Actor roots and all authored story /
// physics positions remain fixed. A negative index returns to authored yaw.
bool world_trail_camp_focus(WorldTrailCamp *,int actor,double x,double z,float dt);
bool world_trail_camp_desired(const WorldTrailCamp *,double x,double z);
// Explicit startup/destination loading only. Pose geometry and textures come
// from existing purchased/retained village assets; no animation synthesis.
bool world_trail_camp_commit(WorldTrailCamp *,bool active,char *error=nullptr,std::size_t capacity=0);
void world_trail_camp_update(WorldTrailCamp *,const WorldScenery *,float time);
void world_trail_camp_draw(WorldTrailCamp *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0);
WorldTrailCampStats world_trail_camp_stats(const WorldTrailCamp *);
bool world_trail_camp_actor(const WorldTrailCamp *,unsigned,WorldTrailCamper *);
void world_trail_camp_free(WorldTrailCamp *);
#endif
