#ifndef VALWOOD_WORLD_ARENA_RENDER_H
#define VALWOOD_WORLD_ARENA_RENDER_H
#include <cstddef>
#include <cstdint>
struct WorldArenaRender;
struct WorldArenaRenderConfig {double x=0,ground_y=0,z=0;float half_x=12,half_z=10;};
// x/y/z is the camera eye for first person, player feet for third person.
// yaw/pitch use the ordinary camera convention: yaw0 faces -Z.
struct WorldArenaBowPose {bool visible=false,first_person=false;double x=0,y=0,z=0;float yaw=0,pitch=0,draw=0;};
struct WorldArenaRenderArrow {double base[3]={},tip[3]={};bool visible=false;};
struct WorldArenaRenderStats {std::uint64_t draws=0,bow_draws=0,arrow_draws=0,uploads=0;unsigned vertices=0,active_arrows=0,bow_pose_assets=0,failures=0;};
// Loading-only allocation. Uses the caller's existing world-foliage program,
// view/projection, lighting and shadow samplers; no new shaders or pose banks.
WorldArenaRender *world_arena_render_create(const WorldArenaRenderConfig &,char *error=nullptr,std::size_t capacity=0);
// sockets[kind body/arms][draw pose0/.5/1][left grip/right string][xyz],
// measured from the exact baked source geometry in actor-local metres.
bool world_arena_render_load_bow(WorldArenaRender *,const char *directory,const float sockets[2][3][2][3],char *error=nullptr,std::size_t capacity=0);
bool world_arena_render_update(WorldArenaRender *,const WorldArenaBowPose &,const WorldArenaRenderArrow *,unsigned count);
void world_arena_render_draw(WorldArenaRender *,unsigned program,double eye_x,double eye_y,double eye_z,bool shadow=false);
void world_arena_render_draw_bow(WorldArenaRender *,unsigned program,double eye_x,double eye_y,double eye_z,bool shadow=false);
void world_arena_render_draw_arrows(WorldArenaRender *,unsigned program,double eye_x,double eye_y,double eye_z,bool shadow=false);
// The original sword player must be omitted while these owned bow poses draw.
void world_arena_render_draw_bow_actor(WorldArenaRender *,unsigned program,double eye_x,double eye_y,double eye_z,bool first_person,bool shadow=false);
void world_arena_render_equipment(WorldArenaRender *,unsigned mask);
unsigned world_arena_render_equipment(const WorldArenaRender *);
WorldArenaRenderStats world_arena_render_stats(const WorldArenaRender *);
void world_arena_render_free(WorldArenaRender *);
#endif
