#ifndef VALWOOD_WORLD_ROAD_TRAFFIC_H
#define VALWOOD_WORLD_ROAD_TRAFFIC_H
#include <array>
#include <cstddef>
#include <cstdint>
struct WorldRoadTraffic;
inline constexpr unsigned WORLD_ROAD_TRAFFIC_ACTORS=31;
enum class WorldRoadTrafficKind : unsigned {Walker,Rider,Pullcart};
struct WorldRoadTrafficPoint {double x=0,y=0,z=0;};
struct WorldRoadTrafficConfig {
    std::array<WorldRoadTrafficPoint,6> points={{{-11988,11.8,5172},{-11943,12.26025486,5252},{-11928,12.57140255,5342},{-11948,12.53000259,5432},{-11968,11.8,5522},{-11968,11.8,5582}}};
    // Endpoints are trimmed away from the actual camp/town conversation roots.
    double start_trim=23,end_trim=12,lane_offset=1.30,road_half_width=2.75;
    WorldRoadTrafficPoint camp={-11988,11.8,5172},town={-11968,11.8,5582};
    double camp_radius=12,town_radius=6;
    WorldRoadTrafficPoint encounter={-11933,12.50280952,5312},satchel={-11930.5,12.52490234,5320};
    double encounter_radius=10,satchel_radius=5;
};
using WorldRoadTrafficGround=bool(*)(double x,double z,double reference_y,double &floor_y,const void *);
using WorldRoadTrafficClear=bool(*)(double from_x,double from_z,double to_x,double to_z,double feet_y,double height,double radius,const void *);
struct WorldRoadTrafficQuery {WorldRoadTrafficGround ground=nullptr;WorldRoadTrafficClear clear=nullptr;const void *context=nullptr;};
struct WorldRoadTrafficFrame {
    double dt=0,player_x=0,player_feet_y=0,player_z=0;
    bool enabled=true,paused=false,encounter_cleared=false;
    WorldRoadTrafficQuery query;
};
struct WorldRoadTrafficActor {
    unsigned id=0,appearance=0;const char *name=nullptr;WorldRoadTrafficKind kind=WorldRoadTrafficKind::Walker;
    double x=0,y=0,z=0,yaw=0,pitch=0,speed=0,travelled=0,cycle_phase=0,pose_weight=0,wheel_angle=0;
    std::array<double,2> wheel_angles={};
    double radius=0,front=0,rear=0,height=0;
    unsigned pose_a=0,pose_b=0;bool active=false,moving=false,yielding=false,barrier_wait=false,transitioning=false;
};
struct WorldRoadTrafficStats {
    unsigned actors=0,walkers=0,riders=0,pullcarts=0,moving=0,yielding=0,barrier_waiting=0,visible=0,pose_assets=0,draw_groups=0;
    std::uint64_t updates=0,simulation_steps=0,uploads=0,draws=0,failures=0,unsupported=0,blocked=0,player_yields=0,actor_yields=0;
    std::uint64_t rigid_uploads=0,rigid_uploads_skipped=0;
    std::uint64_t source_geometry_bytes=0;double travelled=0,minimum_player_clearance=1e9,maximum_ground_error=0;
};
bool world_road_traffic_config_load(const char *,WorldRoadTrafficConfig &,char *error=nullptr,std::size_t capacity=0);
// GL-owner loading boundary only. Original models_directory supplies the four
// retained civilian walking/idle banks. Manifest cfg paths are relative to its
// own directory. Unknown/malformed assets fail atomically; no hot-frame IO.
WorldRoadTraffic *world_road_traffic_load(const char *models_directory,const char *manifest_cfg,const WorldRoadTrafficConfig &,char *error=nullptr,std::size_t capacity=0);
// Samples the entire actual lane/turn swept envelope with the host's ground
// and collision policy. Must succeed before enabling this optional road life.
// CPU only, bounded; does not move the player, quest NPCs or traffic actors.
bool world_road_traffic_validate_route(WorldRoadTraffic *,const WorldRoadTrafficQuery &,char *error=nullptr,std::size_t capacity=0);
// Fixed-capacity simulation and instance upload, no allocation or file IO.
// player_feet_y is the logical player (never a detached cinematic camera).
void world_road_traffic_update(WorldRoadTraffic *,const WorldRoadTrafficFrame &);
// Actual main/reflection/shadow camera; pass0/1/2. Bounded per-pose instancing.
void world_road_traffic_draw(WorldRoadTraffic *,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass=0,const float *view_projection=nullptr);
bool world_road_traffic_blocks_segment(const WorldRoadTraffic *,double from_x,double from_z,double to_x,double to_z,double feet_y,double height,double radius);
// Strict swept-lens clearance through the existing conservative source body
// envelopes, including full rider/horse height. No actor movement or GL work.
bool world_road_traffic_camera_clear(const WorldRoadTraffic *,double ax,double ay,double az,double bx,double by,double bz,float radius,float half_height=.02f);
WorldRoadTrafficStats world_road_traffic_stats(const WorldRoadTraffic *);
unsigned world_road_traffic_count(const WorldRoadTraffic *);
bool world_road_traffic_actor(const WorldRoadTraffic *,unsigned,WorldRoadTrafficActor *);
void world_road_traffic_free(WorldRoadTraffic *);
#endif
