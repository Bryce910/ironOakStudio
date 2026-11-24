#ifndef VALWOOD_WORLD_VILLAGE_PETS_H
#define VALWOOD_WORLD_VILLAGE_PETS_H
#include <cstddef>
#include <cstdint>
struct WorldVillagePets;
inline constexpr std::size_t world_village_pet_count=12;
inline constexpr std::size_t world_village_pet_pose_count=32;
inline constexpr std::size_t world_village_pet_blend_count=38;
struct WorldVillagePetsStats {
    std::size_t pets=0,cats=0,dogs=0,walking=0,idling=0,visible=0,loaded_pose_assets=0,draw_groups=0;
    std::uint64_t uploads=0,updates=0;
};
struct WorldVillagePet {
    const char *name=nullptr,*species=nullptr;
    double x=0,z=0;float y=0,yaw=0,speed=0,cycle_phase=0,pose_blend=0;
    unsigned pose_a=0,pose_b=0;
    bool walking=false,seated=false,transitioning=false;
};
/* Fixed, offline cooked meshes; same world_model texture cache and foliage
 * material/pass contract as residents. Atomic on any partial-load failure. */
WorldVillagePets *world_village_pets_load(const char *models_directory="models",char *error=nullptr,std::size_t capacity=0);
/* Absolute-time deterministic gentle patrols and idle poses. Fixed storage;
 * no per-frame mesh deformation, allocations, files or online requests. */
void world_village_pets_update(WorldVillagePets *,double camera_x,double camera_y,double camera_z,float time,float dt);
void world_village_pets_draw(WorldVillagePets *,unsigned program,double camera_x,double camera_y,double camera_z,bool shadow=false);
WorldVillagePetsStats world_village_pets_stats(const WorldVillagePets *);
bool world_village_pets_pet(const WorldVillagePets *,std::size_t,WorldVillagePet *);
void world_village_pets_free(WorldVillagePets *);
#endif
