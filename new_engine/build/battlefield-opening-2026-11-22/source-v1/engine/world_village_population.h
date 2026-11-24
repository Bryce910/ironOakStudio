#ifndef VALWOOD_WORLD_VILLAGE_POPULATION_H
#define VALWOOD_WORLD_VILLAGE_POPULATION_H
#include <cstddef>
#include <cstdint>
#include <array>
#include "engine/world_navigation.h"
#include "engine/world_models.h"
struct WorldVillagePopulation;
void world_village_population_graphs(WorldVillagePopulation *,const char *const *,std::size_t);
struct WorldVillagePoseArchetype {
    const char *name;
    unsigned walk_count,idle_count,walk_group,idle_group;
    const char *file_prefix="village_character";
};
/* Fixed on-disk aliases and contiguous pose groups, shared with asset tests. */
inline constexpr WorldVillagePoseArchetype world_village_pose_archetypes[]={
    {"knight",8,4,0,8},{"archer",0,4,0,12},
    {"trader",0,4,0,16,"purchased_character"},{"traveler",8,4,20,28,"purchased_character"},
    {"traderwoman",0,4,0,32},{"travelerwoman",8,4,36,44},
    {"merchant",0,4,0,48,"purchased_character"},
    {"craftsman",8,4,52,60,"purchased_character"},
    {"wayfarer",8,4,64,72,"purchased_character"},
    {"seated",0,4,0,76}
};
/* Original source0..75 and blend0..90 indices are stable. Genuine source
 * Sitting1 poses and their cyclic blend batches append to those registries. */
inline constexpr std::size_t world_village_original_pose_group_count=76;
inline constexpr std::size_t world_village_original_blend_group_count=91;
inline constexpr std::size_t world_village_pose_group_count=80;
inline constexpr std::size_t world_village_pose_blend_group_count=95;
/* Full two-step source strides, measured from source skeleton foot excursion.
 * Stationary appearances have zero; route legs retime to a planted half cycle. */
inline constexpr float world_village_walk_stride_m[]={1.45832f,0,0,.9565f,0,1.1464363f,0,.9596f,.9811f,0};
inline constexpr std::size_t world_village_original_resident_count=128;
inline constexpr std::size_t world_village_resident_count=140;
constexpr unsigned WORLD_EDITED_ACTOR_CAPACITY=32;
struct WorldVillageActorConfig {int source=-1;unsigned appearance=3;bool enabled=true;std::array<char,65> name={};float speed=1.35f,dwell=1.2f,yaw=0;unsigned count=0;std::array<world_navigation::Point,128> points={};std::uint64_t identity=0;bool source_floor=false;std::array<bool,128> pauses={};};
// Atomic owner-thread configuration; reuses loaded pose batches. No asset IO.
bool world_village_population_configuration_valid(const WorldVillageActorConfig *,std::size_t);
bool world_village_population_configure(WorldVillagePopulation *,const WorldVillageActorConfig *,std::size_t);
using WorldVillageSourceFloor=bool(*)(double,double,double,double &,const void *);
void world_village_population_source_floor(WorldVillagePopulation *,WorldVillageSourceFloor,const void *context=nullptr);
struct WorldVillagePopulationStats {
    std::size_t residents=0,knights=0,archers=0,traders=0,travelers=0;
    std::size_t walking=0,idling=0,visible=0,loaded_pose_assets=0,draw_groups=0;
    std::uint64_t uploads=0,updates=0;
    std::size_t source_floor_failures=0;
    std::size_t sheltered_residents=0,tavern_residents=0,street_residents=0,seated=0;
    // Cumulative color-pass batch decisions; legacy/shadow draws do not count.
    std::uint64_t frustum_draw_calls=0,frustum_culled_batches=0,frustum_submitted_batches=0;
};
struct WorldVillageResident {
    const char *name=nullptr,*role=nullptr; // static strings, valid until free
    const char *archetype=nullptr; // fixed source appearance alias
    double x=0,z=0;float y=0,yaw=0;
    bool walking=false;
    // Continuous locomotion/render diagnostics, independent of update dt.
    float speed=0,cycle_phase=0,pose_blend=0;
    // Render-only sole support; y remains the authored navigation floor.
    float visual_y_offset=0;
    float appearance_scale=1,body_width=1;
    unsigned pose_a=0,pose_b=0;
    bool transitioning=false;
    bool seated=false;
    const char *district=nullptr;
};
/* GL3.3 render thread. Loads bounded pre-baked walk/idle character poses.
 * Same world_model material/texture cache and
 * foliage shader contract as scenery; no legacy scene/main.c dependencies.
 * Paths are fixed aliases under models_directory. Partial failure is atomic. */
WorldVillagePopulation *world_village_population_load(const char *models_directory="models",
                                                      char *error=nullptr,std::size_t capacity=0);
/* Deterministic scripted street/patrol loops, not online AI/economy. `time` is
 * absolute seconds (same visual time as the scene); dt is finite frame seconds.
 * Newly configured/changed authored patrols start at their first point on their
 * first update. Unchanged durable identities retain their patrol phase across Apply.
 * Fixed storage, bounded140 residents; no runtime file IO or CPU allocations. */
void world_village_population_update(WorldVillagePopulation *,double camera_x,double camera_y,double camera_z,
                                     float time,float dt);
// Stock crowds are independent from the authored-character pose library.
// Hiding stock residents leaves explicitly authored additions/overrides active.
void world_village_population_stock_enabled(WorldVillagePopulation *,bool);
// A bound live skeletal actor replaces this base resident's baked drawing.
// The next update applies this to all passes, preserving authored identity/routes.
bool world_village_population_runtime_active(WorldVillagePopulation *,std::size_t resident,bool);
struct WorldVillageActorRuntime {
    double x=0,y=0,z=0; // world root; y is the exact navigation feet height
    float yaw=0,speed=0;bool active=true;
};
// Edited slot is the original configure/Studio.actors index (0..31), including
// both edited source residents and added actors. Null clears one override.
// Configuration/rebinding clears all overrides. Idle-only appearances reject
// nonzero speed. Fixed storage; no IO/GL/allocations. Apply before update once.
bool world_village_population_actor_runtime(WorldVillagePopulation *,unsigned edited_index,const WorldVillageActorRuntime *);
void world_village_population_clear_runtime(WorldVillagePopulation *);
/* Caller sets view/light/material textures/raster state. Uses existing color,
 * reflection or shadow foliage program. Sets rigid vegetation/no wind, actor
 * reactive alpha .85 (reset to0 afterward), and camera-relative uniforms.
 * Same current poses are reused in every pass, so shadows/reflections animate.
 */
void world_village_population_draw(WorldVillagePopulation *,unsigned program,double camera_x,double camera_y,
                                   double camera_z,bool shadow=false);
/* Color/reflection only. Column-major camera-relative view_projection must be
 * the actual matrix used by this pass. Conservatively culls whole committed
 * pose batches using both source poses, without changing uploads or shadows.
 * Null, non-finite or degenerate matrices draw every batch. No runtime bounds
 * queries, uniform queries beyond the existing program cache, or allocations. */
void world_village_population_draw_frustum(WorldVillagePopulation *,unsigned program,double camera_x,double camera_y,
                                          double camera_z,const float *view_projection);
WorldVillagePopulationStats world_village_population_stats(const WorldVillagePopulation *);
bool world_village_population_resident(const WorldVillagePopulation *,std::size_t,WorldVillageResident *);
struct WorldVillageAppearanceInfo {float neutral_height=0;unsigned capabilities=0;};
bool world_village_population_appearance_info(const WorldVillagePopulation *,std::size_t,WorldVillageAppearanceInfo *);
// Explicit visual slot binding, independent of living actor IDs. Null clears.
// Bounded CPU update; following normal update commits identical all-pass poses.
bool world_village_population_appearance(WorldVillagePopulation *,std::size_t,float scale,const WorldModelCharacterLook *);
struct WorldVillageAppearanceChange {std::size_t resident=0;float scale=1;WorldModelCharacterLook look={1,0,1,0,{1,1,1},0};bool clear=false;};
// Owner-thread atomic multi-target reload; any rejected setter restores all.
bool world_village_population_apply_appearances(WorldVillagePopulation *,const WorldVillageAppearanceChange *,std::size_t);
bool world_village_population_closest(const WorldVillagePopulation *,double x,double z,double radius,
                                       WorldVillageResident *);
void world_village_population_free(WorldVillagePopulation *);
// A separate one-instance player batch borrows loaded poses; no hot-path VAO allocation.
bool world_village_population_player_update(WorldVillagePopulation *,double x,double feet,double z,float yaw,float phase,bool walking);
void world_village_population_player_draw(WorldVillagePopulation *,unsigned program,double x,double y,double z,bool shadow=false);
#endif
