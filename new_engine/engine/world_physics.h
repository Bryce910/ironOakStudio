#ifndef VALWOOD_WORLD_PHYSICS_H
#define VALWOOD_WORLD_PHYSICS_H
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Jolt CPU component adapter for the custom Valwood engine. No renderer/audio/
// SDL dependency. All World operations, including destruction, use its owner
// thread. Reconfigure/restore/codec are loading/editor boundaries, never frames.
namespace world_physics {
using Id=std::uint64_t;
constexpr unsigned body_limit=512,door_limit=32,sensor_limit=32,event_limit=2048;
constexpr unsigned vertex_limit=147456,triangle_limit=294912,layer_count=8;
constexpr double fixed_step=1./60.;
constexpr std::size_t document_byte_limit=16*1024*1024;
enum class Shape : unsigned { Box,Sphere,Capsule,Mesh };
enum class Motion : unsigned { Static,Dynamic,Kinematic };
struct Vec3 {double x=0,y=0,z=0;};
struct Quaternion {float x=0,y=0,z=0,w=1;};
struct Pose {Vec3 position;Quaternion rotation;};
struct Body {
    Id id=0;std::string name="Body";Shape shape=Shape::Box;Motion motion=Motion::Static;
    bool sensor=false,allow_sleep=true;unsigned layer=0;Pose pose;
    // Box: XYZ half extents. Sphere: X radius. Capsule: X radius/Y cylinder
    // half height (overall height=2*(X+Y)); its axis is local Y.
    Vec3 size={.5,.5,.5},linear_velocity,angular_velocity;
    float friction=.6f,restitution=.05f,density=1000,mass=0,gravity_factor=1;
    // Static mesh: local vertices, CCW triangles; world translation stays double.
    std::vector<Vec3> vertices;std::vector<std::array<std::uint32_t,3>> triangles;
};
struct Door {
    Id body=0;Vec3 pivot,axis={0,1,0};
    float minimum=0,maximum=1.570796327f,speed=1; // radians/radians per second
};
struct Document {
    Vec3 gravity={0,-9.81,0};
    std::array<unsigned,layer_count> layer_masks={255,255,255,255,255,255,255,255};
    std::vector<Body> bodies;std::vector<Door> doors;
};
bool validate(const Document &,std::string &error);
bool save(const char *,const Document &,std::string &error);
bool load(const char *,Document &,std::string &error);
struct BodyState {Id id=0;Pose pose;Vec3 linear_velocity,angular_velocity;bool active=false,enabled=true;};
enum class EventKind : unsigned { ContactBegin,ContactEnd,TriggerEnter,TriggerExit };
struct Event {EventKind kind=EventKind::ContactBegin;Id a=0,b=0;Vec3 position,normal;std::uint64_t tick=0;};
struct Hit {bool hit=false;Id body=0;float fraction=1;Vec3 position,normal;};
struct DoorState {Id body=0;float angle=0,target=0;};
struct Snapshot {
    std::uint64_t signature=0;unsigned count=0,door_count=0;
    std::array<BodyState,body_limit> bodies={};std::array<DoorState,door_limit> doors={};
    double accumulator=0;
};
struct Stats {
    unsigned bodies=0,active_bodies=0,sensors=0,substeps=0,pending_events=0;
    std::uint64_t ticks=0,event_overflows=0,pair_overflows=0,update_errors=0;
    std::uint64_t last_update_error=0,frame_allocations=0,total_frame_allocations=0,query_allocations=0;
    // Heap fallback requests versus fixed scratch-slot allocations. Counts are
    // cumulative except frame_allocations (last step). A nonzero heap fallback
    // must be surfaced; the pool does not silently hide budget overruns.
    std::uint64_t pooled_frame_allocations=0,pooled_query_allocations=0;
    double accumulator=0,unprocessed_debt=0,dropped_seconds=0;
    std::size_t temporary_capacity=32*1024*1024;
    std::size_t scratch_capacity=32*64*1024;
};
class World {
public:
    World();~World();World(const World &)=delete;World &operator=(const World &)=delete;
    // Creates a new Jolt scene; invalid/failed scenes leave the old one active.
    bool configure(const Document &,std::string &error);
    // At most four fixed steps. Remaining whole-step debt is explicitly dropped
    // and counted; fractional debt remains for interpolation. No hidden catchup.
    bool step(double frame_seconds);
    bool body_state(Id,BodyState &)const;
    // Retains its authored ID, pose and velocity while removed from collision
    // broadphase. Re-enable restores velocity and wakes nonstatic bodies.
    bool set_enabled(Id,bool);
    bool impulse(Id,Vec3 impulse);
    bool teleport(Id,const Pose &,bool activate=true);
    bool door_target(Id,float radians);
    bool raycast(Vec3 origin,Vec3 displacement,Hit &,Id ignore=0,unsigned query_layer=0)const;
    bool cast_capsule(Vec3 center,float radius,float half_height,Vec3 displacement,Hit &,Id ignore=0,unsigned query_layer=0)const;
    // Locomotion sweep: tangent or separating contacts do not block movement.
    // They are discarded before closest-hit selection so a later wall remains
    // visible. Normal-dot-displacement tolerance is one part per million of
    // displacement, with a one-micrometer minimum. Generic queries unchanged.
    bool cast_capsule_movement(Vec3 center,float radius,float half_height,Vec3 displacement,Hit &,Id ignore=0,unsigned query_layer=0)const;
    bool sweep_body(Id,Vec3 displacement,Hit &,unsigned query_layer=0)const;
    unsigned drain_events(Event *,unsigned capacity);
    bool snapshot(Snapshot &)const;
    // Rebuilds and restores poses/velocities/door state atomically. Contact cache
    // and notifications reset; this is Editor Stop state, not a rollback-netcode
    // snapshot of Jolt's internal warm-start/contact cache.
    bool restore(const Snapshot &,std::string &error);
    Stats stats()const;
    bool stop();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
#endif
