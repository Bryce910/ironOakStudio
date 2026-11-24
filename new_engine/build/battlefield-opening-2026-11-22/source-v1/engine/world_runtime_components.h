#ifndef VALWOOD_WORLD_RUNTIME_COMPONENTS_H
#define VALWOOD_WORLD_RUNTIME_COMPONENTS_H
#include "engine/world_physics.h"
#include "engine/world_town.h"
#include "engine/world_gameplay.h"
#include "engine/world_living.h"
#include "engine/world_npc_minds.h"
#include "engine/world_entity_graph.h"
#include <array>
#include <memory>
#include <string>
#include <vector>

// Actual scene bridge for the three CPU systems. IDs in these systems occupy
// different namespaces: every association below is explicit. Owner-thread only;
// configure/load/save/snapshot/restore are editor/loading boundaries, not frames.
namespace world_runtime {
using Id=std::uint64_t;
// Reserve built-in journey actors and markers beyond a full authoring graph.
constexpr unsigned entity_limit=world_entity::node_limit+64;
using Vec=world_physics::Vec3;
using Pose=world_physics::Pose;
struct Entity {Id id=0;Pose pose;double scale=1;bool active=true;};
struct PhysicsBinding {
    Id entity=0;world_physics::Id body=0;
    // Rigid transform from the collider to the render entity. Render scale is
    // preserved from the scene; collision dimensions are authored independently.
    Pose body_to_entity;
};
struct LivingBinding {
    Id entity=0;world_living::Id actor=0;
    Vec home,work,leisure,guard;
    double speed=1.35,arrival_radius=.4;
};
struct Document {
    std::string physics_asset,gameplay_asset,living_asset;
    std::vector<PhysicsBinding> physics;
    std::vector<LivingBinding> living;
};
// The scene table comes from the committed persistent entity graph. Documents
// can come directly from Studio previews or be loaded using load_project().
struct Configuration {
    Document components;
    std::vector<Entity> entities;
    bool physics_enabled=false,gameplay_enabled=false,living_enabled=false;
    world_physics::Document physics;
    world_gameplay::Document gameplay;
    world_living::World living;
    // Optional Studio-owned authoritative world. It must outlive this Runtime.
    // Set drive_living_clock=false when Studio already observes/processes it;
    // motion targets and snapshots then read that same world without a second
    // independent simulation. Discard Play restores it atomically as well.
    world_living::World *external_living=nullptr;
    bool drive_living_clock=true;
    world_gameplay::Budget gameplay_budget;
    std::vector<world_npc_minds::Binding> minds;
};
bool validate(const Document &,std::string &error);
bool save(const char *path,const Document &,std::string &error);
bool load(const char *path,Document &,std::string &error);
// Every named asset must be a regular file under project_root. Absolute paths,
// traversal and symlinks are rejected. A living asset supplies its initial
// definition; offline player progress is deliberately a separate save concern.
bool load_project(const std::string &project_root,const Document &,
                  const std::vector<Entity> &,double unix_seconds,
                  Configuration &,std::string &error);
struct MotionTarget {Id entity=0;world_living::Id actor=0;
    world_living::Activity activity=world_living::Activity::Leisure;
    Vec destination;double speed=0,arrival_radius=0;};
// Typed host work. The bridge applies entity activation itself. Training and
// encounter requests must be consumed by the engine's combat system; dialogue
// text borrows immutable configured data until the next configure/restore.
using HostRequest=world_gameplay::Command;
struct Frame {
    std::array<Entity,entity_limit> entities={};unsigned entity_count=0;
    std::array<MotionTarget,world_living::actor_limit> motion={};unsigned motion_count=0;
    world_gameplay::Commands requests;
    world_gameplay::Result gameplay_result=world_gameplay::Result::NotConfigured;
    unsigned physics_events=0,living_steps=0;
    // Borrows the Runtime's fixed buffer until its next tick/configure/restore.
    const world_physics::Event *physics_event_data=nullptr;
};
struct Stats {std::uint64_t frames=0,physics_events=0,failed_gameplay_ticks=0;
    world_physics::Stats physics;world_living::Stats living;
    std::uint64_t living_pending_steps=0;};
struct SnapshotData;
struct Snapshot {std::shared_ptr<const SnapshotData> data;};
class Runtime {
public:
    Runtime();~Runtime();Runtime(const Runtime &)=delete;Runtime &operator=(const Runtime &)=delete;
    // Atomic loading boundary: a rejected scene preserves the running world.
    bool configure(const Configuration &,std::string &error);
    // Loading-only graph refresh: keeps component definitions, all system state
    // and motion-owned entity poses. Missing referenced IDs or changed collider
    // render scales reject atomically. A successful refresh invalidates prior
    // Play snapshots; the host must defer it while Play/Stop is pending.
    bool rebind_scene(const std::vector<Entity> &,std::string &error);
    bool can_rebind_scene(const std::vector<Entity> &,std::string &error)const;
    bool configured()const;
    // No allocation, IO or inference. dt<=.25 avoids unexpected long catch-up;
    // Jolt itself caps to four steps and reports dropped debt. A failed gameplay
    // transaction is reported in Frame and cannot drop its accepted input queue.
    bool tick(double dt,double unix_seconds,Vec player,Frame &);
    // Initial/rollback render data without advancing clocks, physics or rules.
    // Host requests and physics notifications are empty in this read-only view.
    bool read_frame(Frame &)const;
    bool entity(Id,Entity &)const;
    // Feed actual navigator/NPC positions back before the next gameplay tick.
    // Physics-owned entities reject this; use teleport_body explicitly instead.
    bool set_scene_pose(Id,const Pose &);
    bool enqueue(world_gameplay::Event);
    bool interact_nearest(Vec player,double maximum_distance,Id &selected);
    bool choose(unsigned choice_index);bool close_dialogue();
    const world_gameplay::Line *current_line()const;
    world_gameplay::Choices choices()const;
    bool impulse_body(world_physics::Id,Vec);
    bool teleport_body(world_physics::Id,const Pose &);
    bool door_target(world_physics::Id,float radians);
    Id scene_entity_for_body(world_physics::Id)const;
    bool raycast(Vec origin,Vec displacement,world_physics::Hit &,Id &scene_entity,
                 world_physics::Id ignore=0,unsigned layer=0)const;
    bool cast_capsule(Vec center,float radius,float half_height,Vec displacement,
                      world_physics::Hit &,Id &scene_entity,
                      world_physics::Id ignore=0,unsigned layer=0)const;
    bool snapshot(Snapshot &,std::string &error)const;
    // Atomic all-system restore, rebasing living time to exclude discarded Play.
    bool restore(const Snapshot &,double unix_seconds,std::string &error);
    bool can_restore(const Snapshot &,double unix_seconds,std::string &error)const;
    Stats stats()const;
    world_npc_minds::Recollection recollection()const;
    const world_npc_minds::State *experiences()const;
    world_gameplay::Progress quest(Id)const;
    int counter(Id)const;
    const world_gameplay::Document *gameplay_document()const;
    // Owner-thread, explicit loading boundaries. Player progress is separate
    // from authored world files and refuses incompatible/corrupt snapshots.
    struct Player {Vec feet;double yaw=0,pitch=0;bool third_person=true;world_town::State town;};
    bool save_progress(const char *,const Player &,std::string &error)const;
    bool load_progress(const char *,Player &,double unix_seconds,std::string &error);
    bool bind_memory(Id,const world_npc_memory::Document &,std::string &error);
    bool encode_minds(std::string &,std::string &error)const;
    bool save_minds(const char *,std::string &error)const;
    bool load_minds(const char *,std::string &error);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
#endif
