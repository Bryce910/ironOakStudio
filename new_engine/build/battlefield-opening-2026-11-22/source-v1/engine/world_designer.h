#ifndef VALWOOD_WORLD_DESIGNER_H
#define VALWOOD_WORLD_DESIGNER_H
#include "engine/world_authoring_identity.h"
#include "engine/world_prefab.h"
#include "engine/world_studio_tools.h"
#include <unordered_map>

// CPU-only designer commands. These never touch the renderer, user files or
// live documents. The caller publishes a validated candidate at its Apply fence.
namespace world_designer {
using Id=world_entity::Id;
enum class Kind {None,Entity,Source};
struct Ref {
    Kind kind=Kind::None;Id id=0;unsigned source=0;std::string source_key;
    static Ref entity(Id);
    static Ref source_model(unsigned);
    bool empty()const{return kind==Kind::None;}
};
bool operator==(const Ref &,const Ref &);
bool operator!=(const Ref &,const Ref &);
// Immutable, owned identity snapshot. The index cannot be paired accidentally
// with a different mutable source vector: Context only uses the fast path when
// its identity/key pointers refer to this catalogue's own vectors. Rebuild the
// catalogue after a source reload, including same-size reorder/replacement.
class SourceCatalog {
public:
    SourceCatalog()=default;
    explicit SourceCatalog(std::vector<world_authoring::Source>);
    const std::vector<world_authoring::Source> &identities()const{return identities_;}
    const std::vector<std::string> &keys()const{return keys_;}
    bool unique_index(const std::string &,unsigned &)const;
private:
    std::vector<world_authoring::Source> identities_;
    std::vector<std::string> keys_;
    std::unordered_map<std::string,unsigned> indices_;
};
struct Context {
    const std::vector<world_studio::Pose> *source_poses=nullptr;
    const std::vector<world_authoring::Source> *source_identities=nullptr;
    // Only catalogue-owned keys are trusted as a cache. A foreign/stale raw
    // vector falls back to canonical identity comparison rather than rebinding
    // a selected source incorrectly after a same-size source reload.
    const std::vector<std::string> *source_keys=nullptr;
    const SourceCatalog *source_catalog=nullptr;
    world_prefab::Ground ground;
    std::string source_signature;
    std::vector<Id> protected_entities; // live references that deletion must retain
    bool (*validate_candidate)(const world_studio::Document &,const void *,std::string &)=nullptr;
    const void *validation_context=nullptr;
};
struct Selection {
    std::vector<Ref> items;
    Ref active;
    void clear();
};
// Selecting a source does not allocate a graph node. Registered sources resolve
// to persistent IDs; unregistered sources retain canonical source metadata.
bool resolve(const world_studio::Document &,const Context &,const Ref &,Ref &,world_entity::Binding &,std::string &);
bool select(const world_studio::Document &,const Context &,Selection &,Ref,bool toggle,std::string &);
void reconcile(const world_studio::Document &,const Context &,Selection &);
Ref from_binding(const world_studio::Document &,world_entity::Binding);
struct LegacySelection {
    std::vector<unsigned> keys;Id entity=0;
    int selected=-1,selected_added=-1,light=-1,actor=-1,area=-1,particle=-1,water=-1,sound=-1,weather=-1;
    int tool=-1;
};
LegacySelection project(const world_studio::Document &,const Context &,const Selection &);
bool world_pose(const world_studio::Document &,const Context &,Ref,world_entity::Transform &,std::string &);
std::string name(const world_studio::Document &,const Context &,Ref);

enum class Operation {Rename,SetFlags,CreateGroup,Reparent,Duplicate,Erase,Transform,SetWind};
struct Command {
    Operation operation=Operation::Rename;
    // Empty subject uses Selection.active. Duplicate/Erase use every selected
    // reference unless a subject is supplied; overlapping subtrees deduplicate.
    Ref subject;
    std::string name;
    Id parent=0;
    bool change_visible=false,visible=true,change_locked=false,locked=false;
    // Wind changes apply once to selected model bindings and group descendants.
    bool change_wind_enabled=false,wind_enabled=true,change_wind_sway=false,change_wind_flutter=false;
    float wind_sway=1,wind_flutter=1;
    world_entity::ChildPolicy children=world_entity::ChildPolicy::EraseSubtree;
    world_entity::Transform transform;
    world_studio::Vec offset={2,0,0};
};
struct Result {
    bool changed=false,geometry=false;
    Id created=0;
    Selection selection;
};
// Both outputs remain unchanged on failure. No-op success returns changed=false
// and preserves the input document. One accepted command is one undo operation.
bool execute(const world_studio::Document &,const Selection &,const Context &,const Command &,
             world_studio::Document &candidate,Result &,std::string &);

struct ModelBounds {
    bool (*get)(const std::string &,world_studio::Vec &,world_studio::Vec &,const void *)=nullptr;
    const void *context=nullptr;
    // Runtime's normal pivot with offset_y=0, including its asset-specific
    // support/burial/elevation rules. CPU hosts default to terrain-burial+elevation.
    bool (*grounded_pivot)(const std::string &,const world_entity::Transform &,double burial,double elevation,
                           world_prefab::Ground,double &,const void *)=nullptr;
};
// Seat transformed model bounds on the terrain. Models in a selected group
// move together; separate selections ground independently. Both outputs are
// unchanged on failure; one accepted call is one undo operation.
bool snap_to_terrain(const world_studio::Document &,const Selection &,const Context &,ModelBounds,
                     world_studio::Document &,Result &,std::string &);
struct PasteOptions {
    bool snap=true;
    // Relative to the copied object's yaw: right, left, forward, back.
    int direction=0;
    double gap=0;
};
// Owned scene clipboard; copying never changes the document or history. Paste
// publishes atomically, including its repeat anchor. Undo removes an anchor
// from consideration, so the next paste fills that slot again.
class Clipboard {
public:
    bool copy(const world_studio::Document &,const Selection &,const Context &,ModelBounds,std::string &);
    bool paste(const world_studio::Document &,const Context &,const PasteOptions &,
               world_studio::Document &,Result &,std::string &);
    bool ready()const{return prefab_.root!=0;}
    const std::string &label()const{return prefab_.name;}
private:
    world_prefab::Prefab prefab_;
    std::vector<world_studio::Vec> footprint_;
    std::vector<Ref> anchors_;
    bool exact_bounds_=false,single_model_=false;
};
// Repair pre-existing exact model overlaps without deleting the user's objects.
// The caller records one undo checkpoint for the complete accepted repair.
bool separate_overlapping_models(const world_studio::Document &,const Context &,ModelBounds,
                                 world_studio::Document &,Result &,std::string &);

// Repeated updates evaluate one command against the gesture's baseline. Commit
// records one checkpoint only for a real final change; Cancel records none.
class EditScope {
public:
    bool begin(const world_studio::Document &,const Selection &);
    bool update(const Context &,const Command &,world_studio::Document &,Selection &,Result &,std::string &);
    bool commit(world_studio::History &);
    bool cancel(world_studio::Document &,Selection &);
    bool active()const{return active_;}
private:
    bool active_=false,changed_=false,baseline_checked_=false,baseline_valid_=false;
    std::string baseline_error_;
    world_studio::Document before_;
    Selection selection_;
    Id issued_=1;
};
}
#endif
