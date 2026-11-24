#ifndef VALWOOD_WORLD_RUNTIME_ANIMATION_H
#define VALWOOD_WORLD_RUNTIME_ANIMATION_H
#include "engine/world_animation.h"
#include "engine/world_animation_render.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

// Live, bounded actor scheduling for one exact imported rig. CPU update happens
// once, before the render passes. GL ownership remains at the host's explicit
// loading/shutdown boundaries; this module never creates a context.
namespace world_runtime_animation {
constexpr unsigned actor_limit=64,socket_limit=8,leg_limit=2,frame_event_limit=256;
struct Point {double x=0,y=0,z=0;};
struct Bounds {Point minimum,maximum;};
struct RigId {
    std::uint64_t rig=0,content=0;
    friend bool operator==(RigId a,RigId b){return a.rig==b.rig&&a.content==b.content;}
    friend bool operator!=(RigId a,RigId b){return !(a==b);}
};
struct Handle {
    unsigned slot=actor_limit;std::uint64_t id=0;
    friend bool operator==(Handle a,Handle b){return a.slot==b.slot&&a.id==b.id;}
    friend bool operator!=(Handle a,Handle b){return !(a==b);}
};
struct SocketMap {std::string id;world_animation::Socket socket;};
struct LegMap {
    std::string hip,knee,ankle;
    // Pole offset is in the imported rig's global coordinates, relative to hip.
    world_animation::Vec3 pole_offset{0,0,1};
    float weight=.8f,sole_height=.025f,max_rise=.20f,max_drop=.25f;
    float contact_height=.08f,swing_fade=.12f;
};
struct Config {
    std::string label="Valwood knight",walking="Walking",running="Running",action="Skill_03",idle;
    // With no authored idle clip, hold an explicit Walking source pose. The
    // module does not invent an idle animation or silently retarget another rig.
    double idle_source_seconds=0;
    float walk_speed=1.35f,run_speed=3.8f,stop_speed=.04f,speed_response=.12f;
    float action_fade_in=.12f,action_fade_out=.18f,action_cooldown=.25f;
    float maximum_delta=.25f,bounds_padding=.03f;
    std::string planar_root;
    // Local translation axes only: bit0 X, bit1 Y, bit2 Z. Rotation and vertical
    // body motion survive. Empty planar_root disables stripping.
    unsigned root_translation_axes=5;
    std::array<SocketMap,socket_limit> sockets;unsigned socket_count=0;
    std::array<LegMap,leg_limit> legs;unsigned leg_count=0;
    float action_ik_weight=.25f;
    bool plant_feet=false; // bounded stance anchoring for explicitly mapped rigs
    std::string attention_head,attention_neck,gesture_arm,gesture_hand,gesture_chest;
};
// Exact mappings for the purchased, already cooked white_knight_brown_armor GLB.
// Root/hand/leg names and clips are validated against that loaded asset.
Config knight_config(bool breathing_idle=false);
struct Placement {Point position;float yaw=0,scale=1;};
struct ActorDesc {Placement placement;float speed=0;double gait_phase=0;bool terrain_ik=true;};
struct Plane {Point normal;double distance=0;}; // dot(normal,world)+distance >= 0 inside
struct Volume {std::array<Plane,6> planes;unsigned count=0;};
struct Context {
    Point origin;
    // Union of main/reflection/shadow visibility volumes. Empty means all actors.
    // Using their union prevents a different actor/pose set in the three passes.
    std::array<Volume,3> volumes;unsigned volume_count=0;
    world_animation::TerrainHeight terrain=nullptr;void *terrain_userdata=nullptr;
};
struct Event {Handle actor;unsigned clip=0,index=0;double time=0;};
struct Frame {
    RigId identity;std::uint64_t revision=0;Point origin;
    const world_animation::Pose *poses=nullptr;
    const world_animation_render::Instance *instances=nullptr;
    const Handle *actors=nullptr;std::size_t count=0;
    const Event *events=nullptr;unsigned event_count=0;std::uint64_t dropped_events=0;
};
struct Stats {
    unsigned capacity=0,actors=0,visible=0,culled=0,pose_builds=0,terrain_misses=0;
    double accepted_delta=0,dropped_seconds=0;std::uint64_t revision=0;
};
struct ActorState {
    bool active=false,terrain_ik=true,action_active=false;
    std::uint64_t id=0;Placement placement;
    float target_speed=0,smoothed_speed=0;
    double gait_phase=0,action_time=0,cooldown=0,idle_time=0;
    bool render_enabled=true;
    std::array<Point,leg_limit> planted={};
    std::array<float,leg_limit> plant_weight={};
    std::array<bool,leg_limit> plant_active={};
    float attention_yaw=0,attention_pitch=0,attention_weight=0,talking=0;
    float target_attention_yaw=0,target_attention_pitch=0,target_attention_weight=0,target_talking=0;
    double conversation_time=0;
};
struct Snapshot {
    unsigned version=1,capacity=0;RigId identity;std::uint64_t configuration=0,next_id=1;
    std::array<ActorState,actor_limit> actors;
};
class Manager {
public:
    static std::unique_ptr<Manager> create(world_animation::Asset,Config,unsigned capacity,std::string &error);
    static std::unique_ptr<Manager> load(const char *glb,Config,unsigned capacity,std::string &error);
    ~Manager();Manager(const Manager &)=delete;Manager &operator=(const Manager &)=delete;
    RigId identity() const;const Config &config() const;const world_animation::Asset &asset() const;
    std::uint64_t instance_id() const;
    unsigned capacity() const;
    Handle spawn(const ActorDesc &,std::string &error);
    bool remove(Handle,std::string &error);
    bool set_motion(Handle,const Placement &,float speed,std::string &error);
    bool set_terrain_ik(Handle,bool,std::string &error);
    // Disabled actors keep their handle and frozen animation state, and are
    // absent from every render pass until enabled + update publishes them again.
    bool set_enabled(Handle,bool,std::string &error);
    bool set_attention(Handle,float yaw,float pitch,bool enabled,bool talking,std::string &error);
    bool start_action(Handle,std::string &error);
    bool state(Handle,ActorState &,std::string &error) const;
    // Inputs and actors are staged separately from the committed render frame.
    // update validates/stages every actor before publishing a new frame. Large
    // visual deltas are capped explicitly, with dropped_seconds in Stats.
    bool update(double delta,const Context &,std::string &error);
    Frame frame() const;Stats stats() const;
    bool bounds(Handle,Bounds &,std::string &error) const;
    // Socket matrix is relative to Frame.origin, suitable for an attached prop
    // drawn with the same origin. Query requires a successful update after spawn.
    bool socket(Handle,std::string_view id,world_animation::Mat4 &,std::string &error) const;
    void checkpoint(Snapshot &) const;
    // Exact rig + configuration required. Entire restore is validated before
    // commit; post-checkpoint handle IDs are never recycled after rollback.
    bool restore(const Snapshot &,const Context &,std::string &error);
private:
    struct Impl;std::unique_ptr<Impl> impl_;explicit Manager(std::unique_ptr<Impl>);
};

// Explicit GL-owner hooks. A binding is created from this exact immutable asset,
// so same-sized but semantically different skeletons cannot be paired by guess.
// No destructor calls GL: host MUST drop_gpu before destroying its GL context.
struct Gpu {
    world_animation_render::Renderer *renderer=nullptr;RigId identity;
    std::uint64_t manager_instance=0,uploaded_revision=0;
    Gpu()=default;Gpu(const Gpu &)=delete;Gpu &operator=(const Gpu &)=delete;
};
inline void drop_gpu(Gpu &gpu){world_animation_render::destroy(gpu.renderer);gpu.renderer=nullptr;gpu.identity={};gpu.manager_instance=0;gpu.uploaded_revision=0;}
inline bool create_gpu(const Manager &manager,Gpu &gpu,std::string &error,std::uint64_t texture_budget=256ull*1024*1024){
    auto *next=world_animation_render::create(manager.asset(),manager.capacity(),error,texture_budget);
    if(!next)return false;
    drop_gpu(gpu);gpu.renderer=next;gpu.identity=manager.identity();gpu.manager_instance=manager.instance_id();return true;
}
inline bool warm_gpu(const Manager &manager,Gpu &gpu,unsigned program,std::string &error){
    if(!gpu.renderer||gpu.identity!=manager.identity()||gpu.manager_instance!=manager.instance_id()){error="Live animation GPU rig identity differs";return false;}
    return world_animation_render::warm(gpu.renderer,program,error);
}
inline bool upload_frame(const Manager &manager,Gpu &gpu,const world_animation_render::Bindings &bindings,std::string &error){
    const auto frame=manager.frame();
    if(!gpu.renderer||gpu.identity!=frame.identity||gpu.manager_instance!=manager.instance_id()||!frame.revision){error="No matching committed live animation frame";return false;}
    if(gpu.uploaded_revision==frame.revision){error.clear();return true;}
    if(!world_animation_render::upload(gpu.renderer,frame.poses,frame.instances,frame.count,bindings,error))return false;
    gpu.uploaded_revision=frame.revision;return true;
}
inline bool draw_pass(const Manager &manager,Gpu &gpu,unsigned program,const world_animation_render::Bindings &bindings,std::string &error){
    const auto frame=manager.frame();
    if(!gpu.renderer||gpu.identity!=frame.identity||gpu.manager_instance!=manager.instance_id()||!frame.revision||gpu.uploaded_revision!=frame.revision){error="Live animation frame has not been uploaded";return false;}
    return world_animation_render::draw(gpu.renderer,program,bindings,error);
}
}
#endif
