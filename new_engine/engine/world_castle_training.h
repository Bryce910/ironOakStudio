#ifndef VALWOOD_WORLD_CASTLE_TRAINING_H
#define VALWOOD_WORLD_CASTLE_TRAINING_H
#include <cstddef>
#include <cstdint>
struct WorldCastleTraining;
inline constexpr std::size_t world_castle_training_actor_count=7;
inline constexpr std::size_t world_castle_training_drill_pose_count=24;
inline constexpr std::size_t world_castle_training_pose_count=28;
inline constexpr std::size_t world_castle_training_blend_count=28;
struct WorldCastleTrainingStation {double x,z;float yaw;double cycle_seconds,phase_offset;bool instructor;};
// Local east/south metres. Static roots keep all practice inside the reserved
// west court; source foot/hip/hand motion is baked into the actual posed mesh.
inline constexpr WorldCastleTrainingStation world_castle_training_stations[]={
    {-47,-73,-1.5707963268f,4.0,0.00,false},
    {-51,-73, 1.5707963268f,4.0,0.50,false},
    {-47,-84,-1.5707963268f,4.4,0.17,false},
    {-51,-84, 1.5707963268f,4.4,0.67,false},
    {-55,-72,-1.5707963268f,4.8,0.31,false},
    {-55,-83,-1.5707963268f,4.8,0.76,false},
    {-42.5,-68,-1.5707963268f,2.4,0.13,true}
};
struct WorldCastleTrainingStats {
    std::size_t actors=0,trainees=0,instructors=0,visible=0,loaded_pose_assets=0,draw_groups=0;
    std::uint64_t uploads=0,updates=0;
};
struct WorldCastleTrainingActor {
    const char *name=nullptr,*role=nullptr;
    double x=0,z=0;float y=24,yaw=0,cycle_phase=0,pose_blend=0;
    unsigned pose_a=0,pose_b=0;bool instructor=false;
};
// GL3.3 render thread; atomic partial failure. Only24 new offline source poses
// plus4 existing resident idle poses. All images share existing texture keys.
WorldCastleTraining *world_castle_training_load(const char *models_directory="models",char *error=nullptr,std::size_t capacity=0);
// Absolute visual time; deterministic across frame rates. Fixed storage and
// no runtime skeleton, file IO, allocations, physics solver or network calls.
void world_castle_training_update(WorldCastleTraining *,double camera_x,double camera_y,double camera_z,float time,float dt);
// Call alongside village_population in color/reflection/shadow. The same
// uploaded pose and weights render in every pass; no wind or texture jitter.
void world_castle_training_draw(WorldCastleTraining *,unsigned program,double camera_x,double camera_y,double camera_z,bool shadow=false);
// Interactive override reuses the existing first sparring station and pose bank.
// Its partner steps out while the player occupies that marked lane.
void world_castle_training_sparring(WorldCastleTraining *,bool active,float normalized_pose);
void world_castle_training_replace_opponent(WorldCastleTraining *,bool active);
void world_castle_training_replace_instructor(WorldCastleTraining *,bool active);
WorldCastleTrainingStats world_castle_training_stats(const WorldCastleTraining *);
bool world_castle_training_actor(const WorldCastleTraining *,std::size_t,WorldCastleTrainingActor *);
void world_castle_training_free(WorldCastleTraining *);
// A separate one-instance player batch borrows loaded poses; no hot-path VAO allocation.
bool world_castle_training_player_update(WorldCastleTraining *,double x,double feet,double z,float yaw,float phase,bool walking);
void world_castle_training_player_draw(WorldCastleTraining *,unsigned program,double x,double y,double z,bool shadow=false);
#endif
