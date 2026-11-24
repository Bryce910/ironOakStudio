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
    std::uint64_t attention_frames=0,speaking_frames=0,listening_frames=0,acknowledgments=0,head_uploads=0;
    std::uint64_t presentation_cpu_bytes=0,presentation_gpu_bytes=0;
    unsigned skeletal_actors=0;
    std::uint64_t skeletal_uploads=0,skeletal_draws=0,motion_draws=0;
};
struct WorldTrailCamper {double x=0,z=0;float y=0,yaw=0,blend=0;unsigned pose_a=0,pose_b=0;bool walking=false,seated=false;float attention=0;bool speaking=false;};
struct WorldTrailCampPlacement {double x=0,y=0,z=0;float yaw=0;bool seated=false;};
WorldTrailCamp *world_trail_camp_create(const char *directory="models");
// Optional opening-only placement. Configure while unloaded; all three roots
// are authored in absolute world metres and stay fixed throughout their idles.
bool world_trail_camp_configure_arrival(WorldTrailCamp *,double x,double y,double z,
    const WorldTrailCampPlacement *,unsigned count);
// Legacy arrival focus entry point. Retimes the actor's genuine idle without
// rotating planted feet, seats, or the authored collision/story transforms.
bool world_trail_camp_focus(WorldTrailCamp *,int actor,double x,double z,float dt);
// One call per visual update, after the final dialogue frame is resolved.
// Observer is the logical player's eye, not the cinematic render camera.
// Body roots/yaw, collision, dialogue credit and story state never change.
// actor -1 means ambient life; npc_speaking distinguishes the NPC's line from
// the player's line, and beat identifies one bounded acknowledgment cadence.
bool world_trail_camp_present(WorldTrailCamp *,int actor,bool npc_speaking,unsigned beat,
    double player_x,double player_eye_y,double player_z,float dt);
bool world_trail_camp_desired(const WorldTrailCamp *,double x,double z);
// Explicit startup/destination loading only. Existing purchased village
// appearances remain intact. Optional source-rig joint acting is authored
// offline; retained four-pose assets remain resident for atomic fallback.
bool world_trail_camp_commit(WorldTrailCamp *,bool active,char *error=nullptr,std::size_t capacity=0);
void world_trail_camp_update(WorldTrailCamp *,const WorldScenery *,float time);
void world_trail_camp_draw(WorldTrailCamp *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0,unsigned skeletal_program=0);
// Optional continuous purchased-rig camp actors. Legacy baked poses remain
// resident as an atomic fallback; callers prepare the matching skeletal shader.
bool world_trail_camp_skeletal(const WorldTrailCamp *);
bool world_trail_camp_warm(WorldTrailCamp *,unsigned program);
// Root host sets current/previous VP and camera delta on the shared motion
// program. This redraw uses exactly the committed color/depth pose; fixed
// camp roots need no fabricated root motion. Returns actual submitted actors.
unsigned world_trail_camp_draw_motion(WorldTrailCamp *,unsigned program,
    double camera_x,double camera_y,double camera_z,bool reset=false);
WorldTrailCampStats world_trail_camp_stats(const WorldTrailCamp *);
bool world_trail_camp_actor(const WorldTrailCamp *,unsigned,WorldTrailCamper *);
void world_trail_camp_free(WorldTrailCamp *);
#endif
