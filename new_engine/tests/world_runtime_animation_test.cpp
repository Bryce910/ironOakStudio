#include "engine/world_runtime_animation.h"
#include "engine/world_motion_filter.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

namespace {
bool count_allocations=false;std::size_t allocation_count=0;
bool near(double a,double b,double epsilon=2.e-4){return std::abs(a-b)<=epsilon;}
}
[[gnu::noinline]] void *operator new(std::size_t size){if(count_allocations)allocation_count++;if(auto *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,std::size_t)noexcept{std::free(p);}

// CPU-only orchestration fixture. These mocks verify exact-rig ownership, one
// upload per published frame, identical three-pass data and fail-preservation.
// Actual GL shader/raster/texture verification belongs to the separate renderer
// fixture and is deliberately NOT run by this executable.
namespace world_animation_render {
struct Renderer {Stats statistics;std::uint64_t signature=0;};
struct Mock {unsigned creates=0,destroys=0,uploads=0,draws=0;bool fail_create=false,fail_upload=false;const world_animation::Pose *poses=nullptr;std::array<unsigned,8> programs={};} mock;
std::uint64_t signature(const world_animation::Pose *poses,std::size_t count){std::uint64_t h=0;for(std::size_t i=0;i<count;i++)for(unsigned j=0;j<poses[i].joints;j++)for(float f:poses[i].skin[j].m)h=h*131+std::uint64_t(std::llround((f+10000)*10000));return h;}
Renderer *create(const world_animation::Asset &a,unsigned capacity,std::string &error,std::uint64_t){error.clear();if(mock.fail_create){error="Injected loading failure";return nullptr;}auto *r=new Renderer;r->statistics.capacity=capacity;r->statistics.joints=unsigned(a.skeleton.palette.size());mock.creates++;return r;}
void destroy(Renderer *r){if(r){mock.destroys++;delete r;}}
bool warm(Renderer *,unsigned program,std::string &error){error.clear();if(!program){error="Invalid mock program";return false;}return true;}
bool upload(Renderer *r,const world_animation::Pose *poses,const Instance *,std::size_t count,const Bindings &,std::string &error){error.clear();if(mock.fail_upload){error="Injected upload failure";return false;}assert(count<=r->statistics.capacity);r->signature=signature(poses,count);r->statistics.instances=unsigned(count);mock.poses=poses;mock.uploads++;return true;}
bool draw(Renderer *,unsigned program,const Bindings &,std::string &error){error.clear();if(mock.draws<mock.programs.size())mock.programs[mock.draws]=program;mock.draws++;return true;}
}

using namespace world_animation;
using namespace world_runtime_animation;
namespace {
Quat rotate_x(float angle){return {std::sin(angle*.5f),0,0,std::cos(angle*.5f)};}
Quat rotate_z(float angle){return {0,0,std::sin(angle*.5f),std::cos(angle*.5f)};}
std::array<float,4> qvalue(Quat q){return {q.x,q.y,q.z,q.w};}
Asset fixture(){
    Asset asset;auto &s=asset.skeleton;
    s.joints={{"Root",-1,{}},{"Hip",0,{}},{"Knee",1,{}},{"Ankle",2,{}},{"Hand",0,{}},{"Mesh",0,{}}};
    s.joints[1].rest.translation={0,2,0};s.joints[2].rest.translation={0,-1,0};s.joints[3].rest.translation={0,-1,0};s.joints[4].rest.translation={.6f,1.4f,0};s.mesh_node=5;
    s.palette={1,2,3,4};s.inverse_bind.resize(4);s.inverse_bind[0].m[13]=-2;s.inverse_bind[1].m[13]=-1;s.inverse_bind[3].m[12]=-.6f;s.inverse_bind[3].m[13]=-1.4f;
    for(auto p:{Vec3{-.12f,0,0},Vec3{.12f,0,0},Vec3{0,.2f,.1f},Vec3{.6f,1.4f,0}}){SkinVertex v;v.position=p;v.joints={2,0,0,0};asset.mesh.vertices.push_back(v);}
    asset.mesh.vertices[2].joints={1,2,0,0};asset.mesh.vertices[2].weights={.4f,.6f,0,0};asset.mesh.vertices[3].joints={3,0,0,0};
    asset.mesh.indices={0,1,2};asset.mesh.primitives={{0,3,-1,true,false,false}};
    for(unsigned i=0;i<3;i++){
        Clip clip;clip.name=i==0?"Walking":i==1?"Running":"Skill_03";clip.duration=i==1?double(.6f):1.;
        Channel motion;motion.node=i==2?4:1;motion.path=Path::Translation;motion.times={0,float(clip.duration*.5),float(clip.duration)};
        motion.values=i==2?std::vector<std::array<float,4>>{{.6f,1.4f,0,0},{.6f,1.9f,0,0},{.6f,1.4f,0,0}}:std::vector<std::array<float,4>>{{0,2,0,0},{.4f,2.05f,.1f,0},{.8f,2,0,0}};
        Channel rotation;rotation.node=i==2?4:2;rotation.path=Path::Rotation;rotation.times=motion.times;const auto q=i==2?rotate_z(1):rotate_x(i==1?.6f:.2f);rotation.values={qvalue({}),qvalue(q),qvalue({})};
        clip.channels={motion,rotation};clip.events={{i==2?"strike":"foot_left",clip.duration*.25},{i==2?"recover":"foot_right",clip.duration*.75}};asset.clips.push_back(clip);
    }
    return asset;
}
Config configured(){Config c;c.planar_root="Hip";c.socket_count=1;c.sockets[0]={"weapon",{"Hand",{}}};c.sockets[0].socket.local.translation={.2f,0,0};c.leg_count=1;c.legs[0].hip="Hip";c.legs[0].knee="Knee";c.legs[0].ankle="Ankle";c.legs[0].weight=1;c.legs[0].sole_height=.04f;return c;}
std::unique_ptr<Manager> manager(unsigned capacity=4){std::string error;auto m=Manager::create(fixture(),configured(),capacity,error);if(!m)std::fprintf(stderr,"Manager creation: %s\n",error.c_str());assert(m);return m;}
const Pose &actor_pose(const Manager &manager,Handle actor){const auto f=manager.frame();for(unsigned i=0;i<f.count;i++)if(f.actors[i]==actor)return f.poses[i];assert(false);return f.poses[0];}
void same_pose(const Pose &a,const Pose &b){assert(a.nodes==b.nodes&&a.joints==b.joints);for(unsigned i=0;i<a.nodes;i++)for(unsigned k=0;k<16;k++)assert(near(a.global[i].m[k],b.global[i].m[k],1.e-5));}
struct Ground {double height=0;unsigned calls=0;bool missing=false,throwing=false;};
bool terrain(double,double,double &height,void *userdata){auto &g=*static_cast<Ground *>(userdata);g.calls++;if(g.throwing)throw 7;if(g.missing)return false;height=g.height;return true;}
void creation_identity_and_exact_mapping(){
    std::string error;auto a=manager(),b=manager();assert(a->identity()==b->identity()&&a->instance_id()!=b->instance_id());
    auto changed=fixture();changed.skeleton.joints[4].name="AnotherHand";auto config=configured();config.sockets[0].socket.joint="AnotherHand";auto other=Manager::create(changed,config,4,error);assert(other&&a->identity().rig!=other->identity().rig);
    changed=fixture();changed.clips[0].channels[0].values[1][1]+=.1f;other=Manager::create(changed,configured(),4,error);assert(other&&a->identity().rig==other->identity().rig&&a->identity().content!=other->identity().content);
    config=configured();config.walking="walk";assert(!Manager::create(fixture(),config,4,error));
    changed=fixture();changed.clips.push_back(changed.clips[0]);assert(!Manager::create(changed,configured(),4,error));
    config=configured();config.legs[0].knee="Hand";assert(!Manager::create(fixture(),config,4,error));
    config=configured();config.sockets[0].socket.joint="Missing";assert(!Manager::create(fixture(),config,4,error));
    config=configured();config.action_fade_in=2;assert(!Manager::create(fixture(),config,4,error));
    assert(!Manager::create(fixture(),configured(),0,error));assert(!Manager::create(fixture(),configured(),actor_limit+1,error));
    changed=fixture();changed.mesh={};assert(!Manager::create(changed,configured(),4,error));
    config=configured();config.walk_speed=config.run_speed;assert(!Manager::create(fixture(),config,4,error));
}
void lifecycle_speed_action_and_frozen_passes(){
    auto m=manager(2);std::string error;Context context;ActorDesc desc;desc.placement.position={50000,5,-40000};desc.placement.yaw=.5f;desc.gait_phase=.2;const auto a=m->spawn(desc,error);assert(a.id);
    Bounds bounds;assert(!m->bounds(a,bounds,error));assert(m->update(0,context,error));const auto zero_pose=actor_pose(*m,a);const auto first=m->frame();assert(first.count==1&&first.revision==1);
    ActorState state;assert(m->state(a,state,error)&&state.smoothed_speed==0);auto placement=desc.placement;
    assert(m->set_motion(a,placement,m->config().run_speed,error));assert(m->frame().revision==first.revision);same_pose(actor_pose(*m,a),zero_pose);
    assert(m->update(.016,context,error));assert(m->state(a,state,error)&&state.smoothed_speed>0&&state.smoothed_speed<m->config().run_speed);
    for(unsigned i=0;i<60;i++)assert(m->update(.016,context,error));
    assert(m->state(a,state,error)&&state.smoothed_speed>m->config().run_speed*.99f);
    const auto running=actor_pose(*m,a);assert(std::abs(running.local[2].rotation.x)>0);
    assert(m->start_action(a,error));assert(!m->start_action(a,error));const auto action_start=actor_pose(*m,a);
    assert(m->update(0,context,error));same_pose(actor_pose(*m,a),action_start);
    assert(m->update(.04,context,error));const float first_hand=actor_pose(*m,a).local[4].translation.y;assert(first_hand>1.4f&&first_hand<1.6f);
    for(unsigned i=0;i<3;i++)assert(m->update(.1,context,error));
    assert(actor_pose(*m,a).local[4].translation.y>1.6f);bool event=false;const auto action_frame=m->frame();for(unsigned i=0;i<action_frame.event_count;i++)event|=action_frame.events[i].clip==2;assert(event);
    while(true){assert(m->state(a,state,error));if(!state.action_active)break;assert(m->update(.1,context,error));}
    assert(state.cooldown>0&&!m->start_action(a,error));assert(m->update(.25,context,error));assert(m->start_action(a,error));
    // A frame is an immutable sampled packet; observing it for three passes has
    // no advancement or resampling side effects.
    const auto before=m->frame();for(unsigned pass=0;pass<3;pass++){const auto f=m->frame();assert(f.poses==before.poses&&f.instances==before.instances&&f.revision==before.revision);same_pose(f.poses[0],before.poses[0]);}
    const auto b=m->spawn({},error);assert(b.id);assert(!m->spawn({},error).id);assert(m->remove(b,error));const auto replacement=m->spawn({},error);assert(replacement.slot==b.slot&&replacement.id!=b.id);assert(!m->remove(b,error));
    placement.position.x=std::numeric_limits<double>::quiet_NaN();assert(!m->set_motion(a,placement,1,error));assert(!m->set_motion(a,desc.placement,-1,error));
    const auto revision=m->frame().revision;assert(!m->update(-1,context,error));assert(!m->update(std::numeric_limits<double>::infinity(),context,error));assert(!m->update(61,context,error));assert(m->frame().revision==revision);
    assert(m->update(2,context,error));assert(near(m->stats().accepted_delta,.25)&&near(m->stats().dropped_seconds,1.75));
    ActorState enabled;assert(m->state(a,enabled,error));assert(m->set_enabled(a,false,error)&&m->update(.1,context,error));assert(m->state(a,state,error)&&!state.render_enabled&&state.gait_phase==enabled.gait_phase&&state.action_time==enabled.action_time);
    assert(m->frame().count==1&&m->frame().actors[0]==replacement);Snapshot disabled;m->checkpoint(disabled);assert(m->set_enabled(a,true,error)&&m->update(.1,context,error));assert(m->frame().count==2);assert(m->restore(disabled,context,error)&&m->frame().count==1);assert(m->set_enabled(a,true,error)&&m->update(0,context,error)&&m->frame().count==2);
}
void conservative_bounds_origin_sockets_and_culling_union(){
    auto m=manager(3);std::string error;ActorDesc desc;desc.placement.position={50000,3,-40000};desc.placement.yaw=.7f;desc.placement.scale=1.3f;desc.speed=2.1f;desc.gait_phase=.3;
    const auto actor=m->spawn(desc,error);Context context;context.origin={49990,0,-40010};assert(m->update(.1,context,error));auto f=m->frame();assert(f.count==1&&near(f.instances[0].x,10)&&near(f.instances[0].z,10));
    Bounds b;assert(m->bounds(actor,b,error));const auto &pose=actor_pose(*m,actor);
    for(const auto &v:m->asset().mesh.vertices){Vec3 local,normal;assert(deform_vertex(v,pose,local,normal,error));const auto rig=transform_point(pose.global[m->asset().skeleton.mesh_node],local);const double c=std::cos(desc.placement.yaw),s=std::sin(desc.placement.yaw);const Point p{desc.placement.position.x+(c*rig.x+s*rig.z)*desc.placement.scale,desc.placement.position.y+rig.y*desc.placement.scale,desc.placement.position.z+(-s*rig.x+c*rig.z)*desc.placement.scale};assert(p.x>=b.minimum.x&&p.x<=b.maximum.x&&p.y>=b.minimum.y&&p.y<=b.maximum.y&&p.z>=b.minimum.z&&p.z<=b.maximum.z);}
    Mat4 socket;assert(m->socket(actor,"weapon",socket,error));const auto hand=transform_point(pose.global[4],{.2f,0,0});assert(near(socket.m[12],10+(std::cos(.7)*hand.x+std::sin(.7)*hand.z)*1.3,1.e-3));assert(near(socket.m[13],3+hand.y*1.3,1.e-3));assert(!m->socket(actor,"invented",socket,error));
    context.volume_count=1;context.volumes[0].count=1;context.volumes[0].planes[0]={{1,0,0},-60000};assert(m->update(.1,context,error));assert(m->frame().count==0&&m->stats().culled==1);assert(m->bounds(actor,b,error)&&m->socket(actor,"weapon",socket,error));
    context.volume_count=2;context.volumes[1].count=1;context.volumes[1].planes[0]={{-1,0,0},55000};assert(m->update(.1,context,error));assert(m->frame().count==1);
    const auto revision=m->frame().revision;context.volumes[0].planes[0].normal={};assert(!m->update(.1,context,error)&&m->frame().revision==revision);
}
void terrain_and_atomic_snapshots(){
    auto m=manager(3);std::string error;Ground ground;Context context;context.terrain=terrain;context.terrain_userdata=&ground;
    const auto actor=m->spawn({},error);assert(m->update(0,context,error));const auto &pose=actor_pose(*m,actor);const auto ankle=transform_point(pose.global[3],{});assert(near(ankle.y,.04,2.e-3)&&ground.calls==1);
    ground.missing=true;assert(m->update(.016,context,error));assert(m->stats().terrain_misses==1);
    ground.missing=false;ground.throwing=true;assert(m->update(.016,context,error)&&m->stats().terrain_misses==1);ground.throwing=false;
    assert(m->set_terrain_ik(actor,false,error));const auto calls=ground.calls;assert(m->update(.016,context,error)&&ground.calls==calls);
    assert(m->start_action(actor,error));assert(m->update(.2,context,error));Snapshot checkpoint;m->checkpoint(checkpoint);const auto saved_pose=actor_pose(*m,actor);ActorState saved;assert(m->state(actor,saved,error));
    const auto transient=m->spawn({},error);assert(transient.id);assert(m->set_motion(actor,{{5,0,3},1,1},3,error));assert(m->update(.1,context,error));
    assert(m->restore(checkpoint,context,error));ActorState restored;assert(m->state(actor,restored,error)&&near(restored.action_time,saved.action_time)&&near(restored.gait_phase,saved.gait_phase));same_pose(saved_pose,actor_pose(*m,actor));assert(!m->state(transient,restored,error));const auto fresh=m->spawn({},error);assert(fresh.id>transient.id);
    const auto revision=m->frame().revision;auto bad=checkpoint;bad.identity.rig++;assert(!m->restore(bad,context,error)&&m->frame().revision==revision);bad=checkpoint;bad.configuration++;assert(!m->restore(bad,context,error));bad=checkpoint;bad.actors[actor.slot].gait_phase=std::numeric_limits<double>::quiet_NaN();assert(!m->restore(bad,context,error));bad=checkpoint;bad.actors[2]=bad.actors[actor.slot];assert(!m->restore(bad,context,error));bad=checkpoint;bad.actors[actor.slot].placement.position.x=1.e9;context.origin.x=-1.e9;assert(!m->restore(bad,context,error)&&m->frame().revision==revision);context.origin={};
    auto another=manager(3);assert(another->restore(checkpoint,context,error));same_pose(saved_pose,actor_pose(*another,actor));
}
void planted_stance_and_reset(){
    std::string error;auto config=configured();config.plant_feet=true;config.legs[0].contact_height=.20f;auto m=Manager::create(fixture(),config,1,error);assert(m);
    Ground ground;Context context;context.terrain=terrain;context.terrain_userdata=&ground;
    ActorDesc desc;desc.placement.position={50000,0,-30000};desc.placement.yaw=.4f;const auto actor=m->spawn(desc,error);assert(actor.id);
    for(unsigned i=0;i<24;i++)assert(m->update(1./60.,context,error));
    ActorState state;assert(m->state(actor,state,error)&&state.plant_active[0]&&state.plant_weight[0]>.99f);const auto anchor=state.planted[0];
    auto placement=desc.placement;placement.position.x+=.06;assert(m->set_motion(actor,placement,0,error));assert(m->update(1./60.,context,error));
    const auto foot=transform_point(actor_pose(*m,actor).global[3],{});const double worldx=placement.position.x+std::cos(.4)*foot.x+std::sin(.4)*foot.z;
    assert(std::abs(worldx-anchor.x)<.015); // held sole instead of 6 cm of slide
    Snapshot snapshot;m->checkpoint(snapshot);const auto pose=actor_pose(*m,actor);assert(m->restore(snapshot,context,error));same_pose(pose,actor_pose(*m,actor));
    auto invalid=snapshot;invalid.actors[actor.slot].plant_weight[0]=2;assert(!m->restore(invalid,context,error));
    placement.position.x+=10;assert(m->set_motion(actor,placement,0,error));assert(m->state(actor,state,error)&&!state.plant_active[0]&&state.plant_weight[0]==0);
    assert(m->update(.016,context,error)&&m->state(actor,state,error)&&state.planted[0].x>anchor.x+9);
}
void gpu_orchestration_without_a_context(){
    using world_animation_render::mock;mock={};auto m=manager(),same=manager();std::string error;Context context;const auto h=m->spawn({},error);assert(h.id&&m->update(.016,context,error));same->spawn({},error);assert(same->update(.016,context,error));Gpu gpu;world_animation_render::Bindings bindings;
    assert(create_gpu(*m,gpu,error));assert(warm_gpu(*m,gpu,11,error)&&warm_gpu(*m,gpu,12,error)&&warm_gpu(*m,gpu,13,error));assert(!draw_pass(*m,gpu,11,bindings,error));assert(!upload_frame(*same,gpu,bindings,error));
    assert(upload_frame(*m,gpu,bindings,error));const auto uploads=mock.uploads;const auto sig=gpu.renderer->signature;assert(upload_frame(*m,gpu,bindings,error)&&mock.uploads==uploads);
    for(unsigned program:{11u,12u,13u})assert(draw_pass(*m,gpu,program,bindings,error));
    assert(mock.draws==3&&gpu.renderer->signature==sig&&mock.poses==m->frame().poses);
    assert(m->update(.016,context,error));assert(!draw_pass(*m,gpu,11,bindings,error));mock.fail_upload=true;const auto old=gpu.uploaded_revision;assert(!upload_frame(*m,gpu,bindings,error)&&gpu.uploaded_revision==old);mock.fail_upload=false;assert(upload_frame(*m,gpu,bindings,error));
    auto *previous=gpu.renderer;mock.fail_create=true;assert(!create_gpu(*m,gpu,error)&&gpu.renderer==previous);mock.fail_create=false;drop_gpu(gpu);assert(!gpu.renderer&&mock.creates==mock.destroys);
}
void bounded_allocation_free_frames_and_events(){
    auto m=manager(actor_limit);std::string error;error.reserve(256);Context context;
    for(unsigned i=0;i<actor_limit;i++){ActorDesc desc;desc.speed=1.35f;desc.gait_phase=double(i)/actor_limit;desc.placement.position={double(i),0,0};assert(m->spawn(desc,error).id);}
    assert(m->update(.016,context,error));allocation_count=0;count_allocations=true;
    for(unsigned i=0;i<24;i++)assert(m->update(.016,context,error));
    count_allocations=false;assert(allocation_count==0&&m->stats().actors==actor_limit&&m->stats().visible==actor_limit);
    // Authored dense event tracks can overrun an output packet, never storage.
    auto dense=fixture();for(auto &c:dense.clips){c.events.clear();for(unsigned i=0;i<64;i++)c.events.push_back({"step",c.duration*(double(i)+.5)/64});}
    m=Manager::create(std::move(dense),configured(),actor_limit,error);assert(m);for(unsigned i=0;i<actor_limit;i++){ActorDesc desc;desc.speed=1.35f;assert(m->spawn(desc,error).id);}assert(m->update(.25,context,error));assert(m->frame().event_count==frame_event_limit&&m->frame().dropped_events>0);
}
void actual_purchased_knight(){
    std::string error;auto m=Manager::load("projects/valwood_world/models/animations/white_knight_brown_armor.glb",knight_config(),4,error);
    if(!m)std::fprintf(stderr,"Purchased knight import: %s\n",error.c_str());
    assert(m);assert(m->asset().skeleton.palette.size()==24&&m->asset().clips.size()==3);
    ActorDesc desc;desc.speed=1.35f;desc.placement.position={1275,23,-401};const auto actor=m->spawn(desc,error);Context context;context.origin={1270,20,-400};Ground ground;ground.height=23;context.terrain=terrain;context.terrain_userdata=&ground;
    assert(m->update(.016,context,error));const auto first=actor_pose(*m,actor);for(unsigned i=0;i<12;i++)assert(m->update(.016,context,error));const auto walking=actor_pose(*m,actor);bool different=false;for(unsigned i=0;i<walking.joints;i++)different|=std::abs(first.skin[i].m[0]-walking.skin[i].m[0])>1.e-4;assert(different);
    assert(m->set_motion(actor,desc.placement,3.8f,error));for(unsigned i=0;i<12;i++)assert(m->update(.016,context,error));assert(m->start_action(actor,error));for(unsigned i=0;i<12;i++)assert(m->update(.016,context,error));
    Mat4 socket;assert(m->socket(actor,"weapon_right",socket,error)&&m->socket(actor,"shield_left",socket,error));Bounds b;assert(m->bounds(actor,b,error)&&b.maximum.y>b.minimum.y+.5&&b.maximum.y<b.minimum.y+5);
    std::printf("Purchased knight: %zu vertices, %zu joints, exact Walking / Running / Skill_03, hand sockets, authored leg IK, live world bounds passed\n",m->asset().mesh.vertices.size(),m->asset().skeleton.palette.size());
}
void measured_idle_and_motion(){
    std::string error;Asset original,edited;
    assert(load_glb("projects/valwood_world/models/animations/white_knight_brown_armor.glb",original,error));
    assert(load_glb("projects/valwood_world/models/animations/white_knight_brown_armor_demo.glb",edited,error));
    assert(edited.clips.size()==4&&edited.clips.back().name=="Idle");
    assert(edited.mesh.indices==original.mesh.indices&&edited.mesh.vertices.size()==original.mesh.vertices.size());
    assert(edited.images.size()==original.images.size());for(unsigned i=0;i<edited.images.size();i++)assert(edited.images[i].encoded==original.images[i].encoded);
    for(unsigned i=0;i<edited.mesh.vertices.size();i++){
        const auto &a=edited.mesh.vertices[i],&b=original.mesh.vertices[i];
        assert(a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z);
        assert(a.normal.x==b.normal.x&&a.normal.y==b.normal.y&&a.normal.z==b.normal.z);
        assert(a.uv==b.uv&&a.joints==b.joints&&a.weights==b.weights&&a.tangent==b.tangent);
    }
    for(unsigned clip=0;clip<3;clip++)for(double t:{0.,.2,.7}){
        Pose a,b;assert(sample(original.skeleton,original.clips[clip],t,true,a,error));
        assert(sample(edited.skeleton,edited.clips[clip],t,true,b,error));same_pose(a,b);
    }
    Pose start,end,middle;const auto &idle=edited.clips.back();
    assert(sample(edited.skeleton,idle,0,false,start,error)&&sample(edited.skeleton,idle,idle.duration,false,end,error));same_pose(start,end);
    assert(sample(edited.skeleton,idle,2,false,middle,error));bool breathing=false;
    for(unsigned i=0;i<start.joints;i++)for(unsigned j=0;j<16;j++)breathing|=std::abs(start.skin[i].m[j]-middle.skin[i].m[j])>1e-4;
    assert(breathing);
    double maximum_radius=0,minimum_idle_y=1e9,maximum_idle_y=-1e9;
    for(unsigned c=0;c<edited.clips.size();c++)for(unsigned step=0;step<=unsigned(std::ceil(edited.clips[c].duration*60));step++){
        Pose pose;assert(sample(edited.skeleton,edited.clips[c],std::min(edited.clips[c].duration,step/60.),false,pose,error));
        const auto config=knight_config(true);
        for(unsigned node=0;node<edited.skeleton.joints.size();node++)if(edited.skeleton.joints[node].name==config.planar_root){pose.local[node].translation.x=edited.skeleton.joints[node].rest.translation.x;pose.local[node].translation.z=edited.skeleton.joints[node].rest.translation.z;}
        assert(evaluate(edited.skeleton,pose,error));
        for(const auto &v:edited.mesh.vertices){Vec3 p{};for(unsigned j=0;j<4;j++){const auto q=transform_point(pose.skin[v.joints[j]],v.position);p.x+=q.x*v.weights[j];p.y+=q.y*v.weights[j];p.z+=q.z*v.weights[j];}
            p=transform_point(pose.global[edited.skeleton.mesh_node],p);
            assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z));maximum_radius=std::max(maximum_radius,std::sqrt(double(p.x)*p.x+double(p.y)*p.y+double(p.z)*p.z));
            if(c==3){minimum_idle_y=std::min(minimum_idle_y,double(p.y));maximum_idle_y=std::max(maximum_idle_y,double(p.y));}
        }
    }
    std::fprintf(stderr,"Demo skin sweep radius %.6f, idle Y[%.6f,%.6f]\n",maximum_radius,minimum_idle_y,maximum_idle_y);
    assert(maximum_radius<3&&minimum_idle_y>-.08&&maximum_idle_y<2.3);
    auto manager=Manager::create(edited,knight_config(true),2,error);assert(manager);
    ActorDesc desc;desc.gait_phase=.37;const auto h=manager->spawn(desc,error);ActorState state;
    assert(manager->state(h,state,error)&&near(state.idle_time,idle.duration*.37));
    world_motion::State motion;double yaw=0,distance=4;
    for(unsigned i=0;i<600;i++){
        const double old_yaw=yaw,old_speed=motion.speed;
        const auto s=world_motion::advance(motion,yaw,0,distance,.4,1.2,1./60);
        yaw=s.yaw;distance-=s.distance;
        assert(distance>=.4-1e-9&&s.distance>=0&&s.speed<=1.2+1e-9);
        assert(motion.speed-old_speed<=1.8/60+1e-9&&std::abs(yaw-old_yaw)<1e-9);
    }
    assert(distance<.411&&motion.speed==0);
    motion={};yaw=0;for(unsigned i=0;i<180;i++){
        const auto s=world_motion::advance(motion,yaw,3.141592653589793,10,.4,1.2,1./60);
        assert(s.yaw>=yaw-1e-10&&s.yaw<=3.141592653589793+1e-10&&std::abs(s.yaw-yaw)<=3.4/60+1e-9);
        if(3.141592653589793-yaw>.85)assert(s.distance==0);
        yaw=s.yaw;
    }
    assert(std::abs(yaw-3.141592653589793)<.001);
    assert(!world_motion::sweep_clear(-2,0,2,0,0,0,.7)); // full swept crossing, not just endpoints
    assert(world_motion::sweep_clear(-2,.71,2,.71,0,0,.7));
    assert(!world_motion::sweep_clear(.4,0,.2,0,0,0,.7)); // deeper contact rejected
    assert(world_motion::sweep_clear(.4,0,.6,0,0,0,.7)); // safe escape from initial overlap
    assert(!world_motion::sweep_clear(.4,0,-2,0,0,0,.7)); // cannot cross through the centre to escape
    assert(!world_motion::sweep_clear(1,0,.6,0,0,0,.7));
    assert(world_motion::sweep_clear(1,0,.71,0,0,0,.7));
    std::printf("Demo idle: exact source mesh/images/3clips; closed moving idle; 60Hz full skin sweep radius %.3fm, idle Y[%.3f,%.3f]; acceleration/braking/reversal and desynchronized idle passed\n",maximum_radius,minimum_idle_y,maximum_idle_y);
}
}
int main(){
    creation_identity_and_exact_mapping();lifecycle_speed_action_and_frozen_passes();conservative_bounds_origin_sockets_and_culling_union();terrain_and_atomic_snapshots();planted_stance_and_reset();gpu_orchestration_without_a_context();bounded_allocation_free_frames_and_events();actual_purchased_knight();measured_idle_and_motion();
    std::puts("world_runtime_animation: exact-rig live actors, continuous speed/action fades, frozen three-pass packet, conservative bounds/culling union/origin, sockets/terrain IK, rollback/stale handles, bounded events and allocation-free CPU updates passed (no GL context)");return 0;
}
