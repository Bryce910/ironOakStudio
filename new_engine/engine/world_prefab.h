#ifndef VALWOOD_WORLD_PREFAB_H
#define VALWOOD_WORLD_PREFAB_H
#include "engine/world_studio.h"
#include "engine/world_entity_graph.h"
namespace world_prefab {
using world_entity::Binding;
using world_entity::Transform;
using world_entity::Id;
// Called during explicit authoring operations only. Must return the ground
// support height before burial/elevation/offset_y, in metres. Flat zero is the
// default for CPU authoring/tests. Supply the actual support sampler in Studio.
using GroundHeight=double(*)(double,double,const void *);
struct Ground {GroundHeight height=nullptr;const void *context=nullptr;};
// Source objects are materialized into added placements in the prefab; source
// indices and resident/emitter override identities never escape the bundle.
struct SourceModel {unsigned index=0;world_studio::Placement placement;double root_y=0;bool hidden=false;};
struct Selection {std::vector<Binding> objects;std::vector<SourceModel> source_models;Ground ground;};
struct Prefab {std::string name;Transform origin;world_studio::Document content;
    world_entity::Graph graph;Id root=0;std::vector<double> model_heights;};
struct Instance {Id root=0;std::vector<Binding> objects;std::vector<std::pair<Id,Id>> ids;};
bool validate(const Prefab &,std::string &error);
// Capture includes referenced material/asset/shader graph and navigation
// definitions. Global weather, terrain and source overrides are not captured.
// Only local water(source=3) is portable; fixed sea/moat overrides are rejected.
bool capture(const world_studio::Document &,const Selection &,const Transform &origin,
             const std::string &name,Prefab &,std::string &error);
// Atomic over destination document, graph and result. Conflicting shared
// aliases fail rather than changing existing instances. Every instantiate gets
// fresh persistent IDs and actor/emitter overrides become independent actors.
bool instantiate(const Prefab &,world_studio::Document &,world_entity::Graph &,
                 const Transform &target,Id parent,Instance &,std::string &error,Ground ground={});
// Move a previously instantiated bundle, including NPC routes and emitters,
// using its graph root. No runtime uploads or gameplay file access occur here.
bool transform_instance(world_studio::Document &,world_entity::Graph &,Id root,
                        const Transform &target,std::string &error,Ground ground={});
// One atomic wrapper file; content uses Studio's actual versioned serializer.
bool save(const char *path,const Prefab &,std::string &error);
bool load(const char *path,Prefab &,std::string &error);
}
#endif
