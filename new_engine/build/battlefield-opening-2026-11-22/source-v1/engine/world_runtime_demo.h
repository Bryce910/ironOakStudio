#ifndef VALWOOD_WORLD_RUNTIME_DEMO_H
#define VALWOOD_WORLD_RUNTIME_DEMO_H
#include "engine/world_runtime_components.h"
#include "engine/world_entity_graph.h"
namespace world_runtime_demo {
constexpr world_gameplay::Id journey_id=760001,mastery_counter=721001,camp_counter=721002,return_counter=721003,coin_counter=721004,renown_counter=721005;
using Id=world_runtime::Id;
inline constexpr Id id_base=0x56414c5700000000ull;
inline constexpr Id arena_id=id_base+1,instructor_id=id_base+2,opponent_id=id_base+4;
inline constexpr Id castle_actor_first=id_base+0x10,jungle_actor_first=id_base+0x20;
inline constexpr Id castle_crate_id=id_base+0x30,jungle_crate_id=id_base+0x31;
inline constexpr Id training_encounter_id=710001;
struct Sites {double castle_x=0,castle_z=0,castle_floor=24,jungle_x=0,jungle_z=0,jungle_floor=24;};
enum class VisualKind {CastleKnight,JungleSteward,JungleScout,Crate};
struct Visual {Id entity=0;VisualKind kind=VisualKind::CastleKnight;std::string name,asset;world_living::Id living_actor=0;};
struct Seed {
    // Separate fixed registry: no nodes are inserted into the user's scene.
    world_entity::Graph markers;
    world_runtime::Configuration configuration;
    std::vector<Visual> visuals;
    bool initialized_living=false;
    std::string status;
};
// Built-in loading fallback when world.components is absent. Existing scene
// entities are retained; conflicting reserved IDs fail explicitly. An existing
// living definition/resources are preserved. With allow_initial_living=true a
// previously unconfigured empty world receives the six-actor demo definition.
bool build(const Sites &,const std::vector<world_runtime::Entity> &authored,
           world_living::World &authoritative_living,bool allow_initial_living,
           double unix_seconds,Seed &,std::string &error);
// Opt-in authored preview. Does not change the ordinary demo's document/signature.
bool presentation(const Sites &,Seed &,double crossing_floor,std::string &error);
}
#endif
