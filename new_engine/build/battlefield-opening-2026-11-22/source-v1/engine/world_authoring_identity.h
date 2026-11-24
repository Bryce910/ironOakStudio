#ifndef VALWOOD_WORLD_AUTHORING_IDENTITY_H
#define VALWOOD_WORLD_AUTHORING_IDENTITY_H
#include "engine/world_entity_graph.h"
#include "engine/world_studio.h"
#include <string>
#include <vector>

// Bridges the durable entity graph to the existing bounded scene arrays.
// CPU-only, owner-thread transactions; no rendering or filesystem work.
namespace world_authoring {
struct Source {
    std::string alias, file;
    double x=0,z=0,y=0,yaw=0,scale=1,burial=0,elevation=0,offset_y=0,pitch=0,roll=0,sx=1,sy=1,sz=1;
};
using Ground=double (*)(double,double,const void *);
// Full source metadata, not a lossy hash or nearest-position guess.
std::string source_key(const Source &);
// Rebinds unchanged source records after a source file reorder. Every existing
// referenced source needs a unique persisted key; legacy saves retain their
// signature guard. IDs, overrides and old-style groups commit atomically.
bool reconcile(world_studio::Document &,world_entity::Graph &,
               const std::vector<Source> &,const std::string &signature,
               Ground,const void *,std::string &error);
// Registers new authored records and reconciles numeric/gizmo edits. Existing
// parents stay connected; other child world poses remain unchanged.
bool synchronize(const world_studio::Document &,world_entity::Graph &,
                 const std::vector<Source> &,Ground,const void *,std::string &error);
// Call BEFORE compacting a scene array. Source hides retain their identities.
bool erased(world_entity::Graph &,world_entity::Binding,std::string &error);
// Stable runtime IDs come exclusively from the authoritative Sound bindings.
// Missing binding fails atomically; hidden ancestors disable their emitters.
bool sound_emitters(const world_studio::Document &,std::vector<world_soundscape::Emitter> &,std::string &error);
bool weather_areas(const world_studio::Document &,std::vector<world_weather_authoring::BoundArea> &,std::string &error);
// Removes an authored record and its binding in one transaction. Later array
// bindings and navigation references are compacted without changing their IDs.
bool erase_record(world_studio::Document &,world_entity::Binding,std::string &error);
}
#endif
