#ifndef VALWOOD_WORLD_VILLAGE_WATCH_H
#define VALWOOD_WORLD_VILLAGE_WATCH_H
#include <cstddef>
#include <cstdint>

struct WorldVillageWatch;
inline constexpr std::size_t world_village_watch_actor_count = 6;
inline constexpr std::size_t world_village_watch_pose_count = 8;
struct WorldVillageWatchStation { double x, z; float yaw, phase; };
#include "engine/world_village_watch.inc"

struct WorldVillageWatchStats {
    std::size_t actors=0, visible=0, loaded_pose_assets=0, draw_groups=0;
    std::uint64_t uploads=0, updates=0;
};
struct WorldVillageWatchActor {
    double x=0, z=0;
    float y=24, yaw=0, cycle_phase=0, pose_blend=0;
    unsigned pose_a=0, pose_b=0;
};
// Actual source guarded-idle poses; only the adjacent mesh interpolation runs
// at runtime. Fixed roots, fixed storage, shared materials, no skeleton solver.
WorldVillageWatch *world_village_watch_load(const char *directory="models", char *error=nullptr, std::size_t capacity=0);
void world_village_watch_update(WorldVillageWatch *, double camera_x, double camera_y, double camera_z, float time, float dt);
// One uploaded pose/weight set is shared by color, reflection and shadow passes.
void world_village_watch_draw(WorldVillageWatch *, unsigned program, double camera_x, double camera_y, double camera_z, bool shadow=false);
WorldVillageWatchStats world_village_watch_stats(const WorldVillageWatch *);
bool world_village_watch_actor(const WorldVillageWatch *, std::size_t index, WorldVillageWatchActor *out);
void world_village_watch_free(WorldVillageWatch *);
#endif
