#include "engine/world_runtime_animation.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <utility>

namespace world_runtime_animation {
using namespace world_animation;
namespace {
std::atomic<std::uint64_t> next_manager_id{1};
bool finite(double v){return std::isfinite(v);}
bool valid_point(Point p){return finite(p.x)&&finite(p.y)&&finite(p.z)&&std::abs(p.x)<=1.e9&&std::abs(p.y)<=1.e9&&std::abs(p.z)<=1.e9;}
bool valid_vec(Vec3 p){return finite(p.x)&&finite(p.y)&&finite(p.z);}
bool placement_valid(const Placement &p){return valid_point(p.position)&&finite(p.yaw)&&std::abs(p.yaw)<=1.e6&&finite(p.scale)&&p.scale>0&&p.scale<=100;}
bool valid_speed(float v){return finite(v)&&v>=0&&v<=100;}
bool transform_valid(const Transform &t){
    const auto q=t.rotation;const double n=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
    return valid_vec(t.translation)&&valid_vec(t.scale)&&finite(n)&&n>1.e-12&&n<1.e12&&
           std::abs(t.scale.x)>1.e-8&&std::abs(t.scale.y)>1.e-8&&std::abs(t.scale.z)>1.e-8;
}
bool context_valid(const Context &c){
    if(!valid_point(c.origin)||c.volume_count>c.volumes.size())return false;
    for(unsigned i=0;i<c.volume_count;i++){
        const auto &v=c.volumes[i];if(!v.count||v.count>v.planes.size())return false;
        for(unsigned j=0;j<v.count;j++){const auto &p=v.planes[j];const double n=p.normal.x*p.normal.x+p.normal.y*p.normal.y+p.normal.z*p.normal.z;
            if(!valid_point(p.normal)||!finite(p.distance)||std::abs(p.distance)>1.e15||n<1.e-18)return false;}
    }
    return true;
}
double smooth(double t){t=std::clamp(t,0.,1.);return t*t*(3-2*t);}
Mat4 product(const Mat4 &a,const Mat4 &b){Mat4 out;out.m.fill(0);for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)for(unsigned k=0;k<4;k++)out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];return out;}
Mat4 transform_matrix(const Transform &t){
    auto q=t.rotation;const double n=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    q.x=float(q.x/n);q.y=float(q.y/n);q.z=float(q.z/n);q.w=float(q.w/n);
    const float x=q.x,y=q.y,z=q.z,w=q.w;Mat4 m;
    m.m={ (1-2*(y*y+z*z))*t.scale.x,2*(x*y+z*w)*t.scale.x,2*(x*z-y*w)*t.scale.x,0,
          2*(x*y-z*w)*t.scale.y,(1-2*(x*x+z*z))*t.scale.y,2*(y*z+x*w)*t.scale.y,0,
          2*(x*z+y*w)*t.scale.z,2*(y*z-x*w)*t.scale.z,(1-2*(x*x+y*y))*t.scale.z,0,
          t.translation.x,t.translation.y,t.translation.z,1};return m;
}
Quat mix_quat(Quat a,Quat b,float t){
    double dot=double(a.x)*b.x+double(a.y)*b.y+double(a.z)*b.z+double(a.w)*b.w;
    if(dot<0){b={-b.x,-b.y,-b.z,-b.w};dot=-dot;}
    float x=1-t,y=t;if(dot<.9995){const double angle=std::acos(std::clamp(dot,0.,1.)),s=std::sin(angle);x=float(std::sin((1-t)*angle)/s);y=float(std::sin(t*angle)/s);}
    Quat q{a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y,a.w*x+b.w*y};const double n=std::sqrt(double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w);
    q.x=float(q.x/n);q.y=float(q.y/n);q.z=float(q.z/n);q.w=float(q.w/n);return q;
}
void mix_local(Pose &a,const Pose &b,float t){
    auto mix=[&](Vec3 p,Vec3 q){return Vec3{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t};};
    for(unsigned i=0;i<a.nodes;i++){a.local[i].translation=mix(a.local[i].translation,b.local[i].translation);a.local[i].scale=mix(a.local[i].scale,b.local[i].scale);a.local[i].rotation=mix_quat(a.local[i].rotation,b.local[i].rotation,t);}
}
Point transformed(const Mat4 &m,Point p){return {m.m[0]*p.x+m.m[4]*p.y+m.m[8]*p.z+m.m[12],m.m[1]*p.x+m.m[5]*p.y+m.m[9]*p.z+m.m[13],m.m[2]*p.x+m.m[6]*p.y+m.m[10]*p.z+m.m[14]};}
Point placed(const Placement &p,Point v){const double c=std::cos(double(p.yaw)),s=std::sin(double(p.yaw));return {p.position.x+(c*v.x+s*v.z)*p.scale,p.position.y+v.y*p.scale,p.position.z+(-s*v.x+c*v.z)*p.scale};}
void grow(Bounds &b,Point p){b.minimum={std::min(b.minimum.x,p.x),std::min(b.minimum.y,p.y),std::min(b.minimum.z,p.z)};b.maximum={std::max(b.maximum.x,p.x),std::max(b.maximum.y,p.y),std::max(b.maximum.z,p.z)};}
Bounds empty_bounds(){const auto n=std::numeric_limits<double>::infinity();return {{n,n,n},{-n,-n,-n}};}
Point corner(const Bounds &b,unsigned bits){return {bits&1?b.maximum.x:b.minimum.x,bits&2?b.maximum.y:b.minimum.y,bits&4?b.maximum.z:b.minimum.z};}
bool visible(const Bounds &b,const Context &c){
    if(!c.volume_count)return true;
    for(unsigned i=0;i<c.volume_count;i++){bool inside=true;
        for(unsigned j=0;j<c.volumes[i].count;j++){const auto &p=c.volumes[i].planes[j];const Point support{p.normal.x>=0?b.maximum.x:b.minimum.x,p.normal.y>=0?b.maximum.y:b.minimum.y,p.normal.z>=0?b.maximum.z:b.minimum.z};
            if(p.normal.x*support.x+p.normal.y*support.y+p.normal.z*support.z+p.distance<0){inside=false;break;}}
        if(inside)return true;
    }return false;
}
struct Hash {
    std::uint64_t value=1469598103934665603ull;
    void byte(unsigned char c){value=(value^c)*1099511628211ull;}
    void integer(std::uint64_t n){for(unsigned i=0;i<8;i++){byte(static_cast<unsigned char>(n));n>>=8;}}
    void real(float f){std::uint32_t u;std::memcpy(&u,&f,sizeof u);integer(u);}
    void real(double f){std::uint64_t u;std::memcpy(&u,&f,sizeof u);integer(u);}
    void text(const std::string &s){integer(s.size());for(unsigned char c:s)byte(c);}
    void vec(Vec3 v){real(v.x);real(v.y);real(v.z);}
    void transform(const Transform &t){vec(t.translation);real(t.rotation.x);real(t.rotation.y);real(t.rotation.z);real(t.rotation.w);vec(t.scale);}
    void matrix(const Mat4 &m){for(float f:m.m)real(f);}
};
RigId asset_identity(const Asset &a){
    Hash h;h.integer(1);h.integer(a.skeleton.joints.size());h.integer(a.skeleton.mesh_node);
    for(const auto &j:a.skeleton.joints){h.text(j.name);h.integer(std::uint64_t(j.parent));h.transform(j.rest);}
    h.integer(a.skeleton.palette.size());for(unsigned i=0;i<a.skeleton.palette.size();i++){h.integer(a.skeleton.palette[i]);h.matrix(a.skeleton.inverse_bind[i]);}
    const auto rig=h.value;h.integer(a.clips.size());
    for(const auto &c:a.clips){h.text(c.name);h.real(c.duration);h.integer(c.channels.size());for(const auto &ch:c.channels){h.integer(ch.node);h.integer(unsigned(ch.path));h.integer(unsigned(ch.interpolation));h.integer(ch.times.size());for(float t:ch.times)h.real(t);h.integer(ch.values.size());for(const auto &v:ch.values)for(float f:v)h.real(f);}h.integer(c.events.size());for(const auto &e:c.events){h.text(e.name);h.real(e.time);}}
    h.integer(a.mesh.vertices.size());for(const auto &v:a.mesh.vertices){h.vec(v.position);h.vec(v.normal);for(float f:v.uv)h.real(f);for(unsigned j:v.joints)h.integer(j);for(float f:v.weights)h.real(f);for(float f:v.tangent)h.real(f);}
    h.integer(a.mesh.indices.size());for(auto index:a.mesh.indices)h.integer(index);
    h.integer(a.mesh.primitives.size());for(const auto &p:a.mesh.primitives){h.integer(p.first_index);h.integer(p.index_count);h.integer(std::uint64_t(p.material));h.integer(p.has_normals);h.integer(p.has_uv);h.integer(p.has_tangents);}
    h.integer(a.images.size());for(const auto &i:a.images){h.integer(i.encoded.size());for(auto b:i.encoded)h.byte(b);}
    h.integer(a.materials.size());for(const auto &m:a.materials){for(const auto *map:{&m.base,&m.normal,&m.metallic_roughness,&m.occlusion,&m.emissive}){h.integer(std::uint64_t(map->image));h.integer(map->min_filter);h.integer(map->mag_filter);h.integer(map->wrap_s);h.integer(map->wrap_t);}for(float f:m.base_factor)h.real(f);for(float f:m.emissive_factor)h.real(f);h.real(m.roughness);h.real(m.metallic);h.real(m.normal_scale);h.real(m.occlusion_strength);h.real(m.alpha_cutoff);h.integer(m.double_sided);}
    return {rig?rig:1,h.value?h.value:1};
}
std::uint64_t config_identity(const Config &c){
    Hash h;for(const auto *s:{&c.label,&c.walking,&c.running,&c.action,&c.idle,&c.planar_root,&c.attention_head,&c.attention_neck,&c.gesture_arm,&c.gesture_hand,&c.gesture_chest})h.text(*s);
    h.real(c.idle_source_seconds);for(float f:{c.walk_speed,c.run_speed,c.stop_speed,c.speed_response,c.action_fade_in,c.action_fade_out,c.action_cooldown,c.maximum_delta,c.bounds_padding,c.action_ik_weight})h.real(f);
    h.integer(c.plant_feet);h.integer(c.root_translation_axes);h.integer(c.socket_count);for(unsigned i=0;i<c.socket_count;i++){h.text(c.sockets[i].id);h.text(c.sockets[i].socket.joint);h.transform(c.sockets[i].socket.local);}
    h.integer(c.leg_count);for(unsigned i=0;i<c.leg_count;i++){const auto &l=c.legs[i];h.text(l.hip);h.text(l.knee);h.text(l.ankle);h.vec(l.pole_offset);for(float f:{l.weight,l.sole_height,l.max_rise,l.max_drop,l.contact_height,l.swing_fade})h.real(f);}
    return h.value?h.value:1;
}
int named_node(const Skeleton &s,const std::string &name){int found=-1;for(unsigned i=0;i<s.joints.size();i++)if(s.joints[i].name==name){if(found>=0)return -1;found=int(i);}return found;}
int named_clip(const Asset &a,const std::string &name){int found=-1;for(unsigned i=0;i<a.clips.size();i++)if(a.clips[i].name==name){if(found>=0)return -1;found=int(i);}return found;}
bool config_valid(const Config &c){
    for(const auto *name:{&c.label,&c.walking,&c.running,&c.action,&c.idle,&c.planar_root,&c.attention_head,&c.attention_neck,&c.gesture_arm,&c.gesture_hand,&c.gesture_chest})if(name->size()>128)return false;
    if(c.walking.empty()||c.running.empty()||c.action.empty()||c.walking==c.running||c.walking==c.action||c.running==c.action)return false;
    if(!finite(c.idle_source_seconds)||c.idle_source_seconds<0||c.idle_source_seconds>1.e6||!valid_speed(c.walk_speed)||c.walk_speed<=0||!valid_speed(c.run_speed)||c.run_speed<=c.walk_speed||!valid_speed(c.stop_speed)||c.stop_speed>=c.walk_speed)return false;
    for(float f:{c.speed_response,c.action_fade_in,c.action_fade_out,c.maximum_delta})if(!finite(f)||f<=0||f>10)return false;
    if(c.maximum_delta>1||!finite(c.action_cooldown)||c.action_cooldown<0||c.action_cooldown>60||!finite(c.bounds_padding)||c.bounds_padding<.001f||c.bounds_padding>10||!finite(c.action_ik_weight)||c.action_ik_weight<0||c.action_ik_weight>1||c.root_translation_axes>7||c.socket_count>socket_limit||c.leg_count>leg_limit)return false;
    for(unsigned i=0;i<c.socket_count;i++){const auto &s=c.sockets[i];if(s.id.empty()||s.id.size()>128||s.socket.joint.empty()||s.socket.joint.size()>128||!transform_valid(s.socket.local))return false;for(unsigned j=0;j<i;j++)if(c.sockets[j].id==s.id)return false;}
    for(unsigned i=0;i<c.leg_count;i++){const auto &l=c.legs[i];if(l.hip.empty()||l.hip.size()>128||l.knee.empty()||l.knee.size()>128||l.ankle.empty()||l.ankle.size()>128||!valid_vec(l.pole_offset)||!finite(l.weight)||l.weight<0||l.weight>1)return false;
        for(float f:{l.sole_height,l.max_rise,l.max_drop,l.contact_height,l.swing_fade})if(!finite(f)||f<0||f>10)return false;
        if(l.swing_fade<=0)return false;}
    return true;
}
bool state_valid(const ActorState &s,double action_end){
    for(float value:{s.attention_yaw,s.attention_pitch,s.target_attention_yaw,s.target_attention_pitch})if(!finite(value)||std::abs(value)>1)return false;
    for(float value:{s.attention_weight,s.target_attention_weight,s.talking,s.target_talking})if(!finite(value)||value<0||value>1)return false;
    if(!finite(s.conversation_time)||s.conversation_time<0||s.conversation_time>1.e6)return false;
    for(unsigned i=0;i<leg_limit;i++)if(!valid_point(s.planted[i])||!finite(s.plant_weight[i])||s.plant_weight[i]<0||s.plant_weight[i]>1)return false;
    return s.id&&placement_valid(s.placement)&&valid_speed(s.target_speed)&&valid_speed(s.smoothed_speed)&&finite(s.gait_phase)&&s.gait_phase>=0&&s.gait_phase<1&&finite(s.action_time)&&s.action_time>=0&&s.action_time<=action_end&&finite(s.cooldown)&&s.cooldown>=0&&s.cooldown<=60&&finite(s.idle_time)&&s.idle_time>=0&&s.idle_time<=1.e6&&(!s.action_active||s.cooldown==0);
}
}

struct Manager::Impl {
    Asset asset;Config config;unsigned capacity=0;RigId identity;std::uint64_t configuration=0,next_id=1,revision=0,instance_id=0;
    int walk=-1,run=-1,action=-1,idle=-1,root=-1;
    int attention_head=-1,attention_neck=-1,gesture_arm=-1,gesture_hand=-1,gesture_chest=-1;
    std::array<unsigned,socket_limit> socket_nodes={};std::array<Mat4,socket_limit> socket_local;
    std::array<std::array<unsigned,3>,leg_limit> leg_nodes={};
    std::array<Bounds,joint_limit> joint_bounds;std::array<bool,joint_limit> joint_used={};
    std::array<ActorState,actor_limit> actors;
    struct Slot {bool active=false;std::uint64_t id=0;Bounds bounds;std::array<Mat4,socket_limit> sockets;};
    struct Bank {
        std::array<Pose,actor_limit> poses;
        std::array<world_animation_render::Instance,actor_limit> instances;
        std::array<Handle,actor_limit> handles;
        std::array<Slot,actor_limit> slots;
        std::array<Event,frame_event_limit> events;
        unsigned count=0,event_count=0;std::uint64_t dropped_events=0;Point origin;Stats statistics;
    };
    std::array<Bank,2> banks;unsigned front=0;
    bool resolve(std::string &error){
        walk=named_clip(asset,config.walking);run=named_clip(asset,config.running);action=named_clip(asset,config.action);idle=config.idle.empty()?-1:named_clip(asset,config.idle);
        if(walk<0||run<0||action<0||(!config.idle.empty()&&idle<0)){error="Authored live animation clip name is missing or ambiguous";return false;}
        for(int c:{walk,run,action})if(asset.clips[unsigned(c)].duration<1.e-5){error="Live locomotion/action clip has no playable duration";return false;}
        if(config.action_fade_in>asset.clips[unsigned(action)].duration){error="Action fade-in exceeds the authored clip duration";return false;}
        if(config.idle_source_seconds>asset.clips[unsigned(walk)].duration){error="Held idle pose is outside the Walking source clip";return false;}
        if(!config.planar_root.empty()){root=named_node(asset.skeleton,config.planar_root);if(root<0){error="Authored root translation node is missing or ambiguous";return false;}}
        const std::string *attention_names[]={&config.attention_head,&config.attention_neck,&config.gesture_arm,&config.gesture_hand,&config.gesture_chest};
        int *attention_nodes[]={&attention_head,&attention_neck,&gesture_arm,&gesture_hand,&gesture_chest};
        for(unsigned i=0;i<5;i++)if(!attention_names[i]->empty()){*attention_nodes[i]=named_node(asset.skeleton,*attention_names[i]);if(*attention_nodes[i]<0){error="Authored conversation joint is missing or ambiguous";return false;}}
        for(unsigned i=0;i<config.socket_count;i++){const int n=named_node(asset.skeleton,config.sockets[i].socket.joint);if(n<0){error="Authored socket joint is missing or ambiguous";return false;}socket_nodes[i]=unsigned(n);socket_local[i]=transform_matrix(config.sockets[i].socket.local);}
        for(unsigned i=0;i<config.leg_count;i++){const auto &m=config.legs[i];const int h=named_node(asset.skeleton,m.hip),k=named_node(asset.skeleton,m.knee),a=named_node(asset.skeleton,m.ankle);
            if(h<0||k<0||a<0||asset.skeleton.joints[unsigned(k)].parent!=h||asset.skeleton.joints[unsigned(a)].parent!=k){error="Authored terrain IK chain is missing, ambiguous or not a direct two-bone chain";return false;}
            for(unsigned j=0;j<i;j++)for(unsigned n:leg_nodes[j])if(n==unsigned(h)||n==unsigned(k)||n==unsigned(a)){error="Terrain IK chains overlap";return false;}
            leg_nodes[i]={unsigned(h),unsigned(k),unsigned(a)};
            // The analytic solver cannot preserve lengths under sheared or
            // nonuniformly scaled ancestors. Reject once, before live update.
            for(int n=a;n>=0;n=asset.skeleton.joints[unsigned(n)].parent){const auto s=asset.skeleton.joints[unsigned(n)].rest.scale;if(s.x<=0||std::abs(s.x-s.y)>1.e-4||std::abs(s.x-s.z)>1.e-4){error="Authored terrain IK chain has nonuniform/nonpositive rest scale";return false;}}
        }
        for(auto &b:joint_bounds)b=empty_bounds();
        for(auto &v:asset.mesh.vertices){float sum=0;for(float w:v.weights)sum+=w;for(float &w:v.weights)w/=sum;
            for(unsigned k=0;k<4;k++)if(v.weights[k]>0){const unsigned j=v.joints[k];joint_used[j]=true;grow(joint_bounds[j],{v.position.x,v.position.y,v.position.z});}}
        identity=asset_identity(asset);configuration=config_identity(config);return true;
    }
    ActorState *actor(Handle h){return h.slot<capacity&&actors[h.slot].active&&actors[h.slot].id==h.id?&actors[h.slot]:nullptr;}
    const ActorState *actor(Handle h)const{return h.slot<capacity&&actors[h.slot].active&&actors[h.slot].id==h.id?&actors[h.slot]:nullptr;}
    bool skin_bounds(const Pose &p,const Placement &placement,Bounds &out,std::string &error)const{
        Bounds b=empty_bounds();for(unsigned i=0;i<asset.skeleton.palette.size();i++)if(joint_used[i]){
            const auto matrix=product(p.global[asset.skeleton.palette[i]],asset.skeleton.inverse_bind[i]);
            for(unsigned j=0;j<8;j++)grow(b,placed(placement,transformed(matrix,corner(joint_bounds[i],j))));}
        const double pad=config.bounds_padding*placement.scale;
        b.minimum={b.minimum.x-pad,b.minimum.y-pad,b.minimum.z-pad};b.maximum={b.maximum.x+pad,b.maximum.y+pad,b.maximum.z+pad};
        if(!valid_point(b.minimum)||!valid_point(b.maximum)){error="Live animation culling bounds overflow";return false;}out=b;return true;
    }
    double action_weight(const ActorState &s)const{
        if(!s.action_active)return 0;
        const double duration=asset.clips[unsigned(action)].duration;
        if(s.action_time<config.action_fade_in)return smooth(s.action_time/config.action_fade_in);
        if(s.action_time>duration)return 1-smooth((s.action_time-duration)/config.action_fade_out);
        return 1;
    }
    void append_events(Bank &bank,Handle actor,unsigned clip,double previous,double current,bool loop,std::string &error)const{
        EventBuffer events;if(!events_between(asset.clips[clip],previous,current,loop,events,error))return;
        bank.dropped_events=events.dropped>std::numeric_limits<std::uint64_t>::max()-bank.dropped_events?std::numeric_limits<std::uint64_t>::max():bank.dropped_events+events.dropped;
        for(unsigned i=0;i<events.count;i++){if(bank.event_count<frame_event_limit)bank.events[bank.event_count++]={actor,clip,events.hits[i].index,events.hits[i].absolute_time};else if(bank.dropped_events<std::numeric_limits<std::uint64_t>::max())bank.dropped_events++;}
    }
    bool build_actor(ActorState &s,Handle handle,double dt,const Context &context,Bank &bank,Pose &pose,std::string &error){
        const double duration=asset.clips[unsigned(action)].duration;
        const double previous_action=s.action_time,previous_phase=s.gait_phase;const bool had_action=s.action_active;
        const double response=-std::expm1(-dt/config.speed_response);s.smoothed_speed=float(s.smoothed_speed+(s.target_speed-s.smoothed_speed)*response);
        if(s.target_speed==0&&s.smoothed_speed<.0001f)s.smoothed_speed=0;
        const double run_weight=smooth((s.smoothed_speed-config.walk_speed)/(config.run_speed-config.walk_speed));
        const double active_weight=smooth((s.smoothed_speed-config.stop_speed)/(config.walk_speed-config.stop_speed));
        const double rated=config.walk_speed+(config.run_speed-config.walk_speed)*run_weight;
        const double cycle=asset.clips[unsigned(walk)].duration+(asset.clips[unsigned(run)].duration-asset.clips[unsigned(walk)].duration)*run_weight;
        const double phase_delta=dt*std::clamp(double(s.smoothed_speed)/rated,0.,3.)/cycle;
        s.gait_phase=std::fmod(previous_phase+phase_delta,1.);
        if(idle>=0&&asset.clips[unsigned(idle)].duration>0)s.idle_time=std::fmod(s.idle_time+dt,asset.clips[unsigned(idle)].duration);
        if(s.action_active){s.action_time=std::min(duration+config.action_fade_out,s.action_time+dt);if(s.action_time>=duration+config.action_fade_out){const double excess=std::max(0.,previous_action+dt-duration-config.action_fade_out);s.action_active=false;s.action_time=0;s.cooldown=std::max(0.,double(config.action_cooldown)-excess);}}
        else s.cooldown=std::max(0.,s.cooldown-dt);
        Blend sources[2]={{unsigned(walk),s.gait_phase*asset.clips[unsigned(walk)].duration,float(1-run_weight)},{unsigned(run),s.gait_phase*asset.clips[unsigned(run)].duration,float(run_weight)}};
        Pose other;
        if(active_weight>0){
            if(run_weight<=0){if(!sample(asset.skeleton,asset.clips[unsigned(walk)],sources[0].seconds,true,pose,error))return false;}
            else if(run_weight>=1){if(!sample(asset.skeleton,asset.clips[unsigned(run)],sources[1].seconds,true,pose,error))return false;}
            else if(!blend(asset,sources,2,true,pose,error))return false;
        }
        if(active_weight<1){const unsigned source=idle>=0?unsigned(idle):unsigned(walk);const double time=idle>=0?s.idle_time:config.idle_source_seconds;
            if(!sample(asset.skeleton,asset.clips[source],time,idle>=0,other,error))return false;
            if(active_weight>0){mix_local(other,pose,float(active_weight));}
            pose=other;}
        const double action_mix=action_weight(s);
        if(action_mix>0){if(!sample(asset.skeleton,asset.clips[unsigned(action)],s.action_time,false,other,error))return false;mix_local(pose,other,float(action_mix));}
        if(root>=0){const auto rest=asset.skeleton.joints[unsigned(root)].rest.translation;auto &p=pose.local[unsigned(root)].translation;
            if(config.root_translation_axes&1)p.x=rest.x;
            if(config.root_translation_axes&2)p.y=rest.y;
            if(config.root_translation_axes&4)p.z=rest.z;}
        const float attention_response=float(1-std::exp(-dt*7));
        s.attention_yaw+=(s.target_attention_yaw-s.attention_yaw)*attention_response;s.attention_pitch+=(s.target_attention_pitch-s.attention_pitch)*attention_response;
        s.attention_weight+=(s.target_attention_weight-s.attention_weight)*attention_response;s.talking+=(s.target_talking-s.talking)*attention_response;
        s.conversation_time=std::fmod(s.conversation_time+dt,10000.);
        const float social_weight=s.attention_weight*float(1-action_mix)*(1-float(active_weight));
        auto rotate_local=[&](int node,float x,float y){
            if(node<0||social_weight<=0)return;
            const float sx=std::sin(x*.5f*social_weight),cx=std::cos(x*.5f*social_weight),sy=std::sin(y*.5f*social_weight),cy=std::cos(y*.5f*social_weight);
            const Quat d{sx*cy,cx*sy,sx*sy,cx*cy};const auto q=pose.local[unsigned(node)].rotation;
            pose.local[unsigned(node)].rotation={q.w*d.x+q.x*d.w+q.y*d.z-q.z*d.y,q.w*d.y-q.x*d.z+q.y*d.w+q.z*d.x,q.w*d.z+q.x*d.y-q.y*d.x+q.z*d.w,q.w*d.w-q.x*d.x-q.y*d.y-q.z*d.z};
        };
        const float phrase=float(std::exp(-s.conversation_time*.22));
        const float nod=std::sin(float(s.conversation_time)*2.1f)*.026f*s.talking*(.3f+.7f*phrase);
        rotate_local(attention_head,s.attention_pitch*.65f+nod,s.attention_yaw*.65f);
        rotate_local(attention_neck,s.attention_pitch*.35f,s.attention_yaw*.35f);
        rotate_local(gesture_arm,(.025f+.085f*std::sin(float(s.conversation_time)*1.6f))*s.talking*phrase,0);
        rotate_local(gesture_hand,.065f*std::sin(float(s.conversation_time)*1.6f-.4f)*s.talking*phrase,0);
        rotate_local(gesture_chest,.012f*std::sin(float(s.conversation_time)*1.2f)*s.talking,.018f*std::sin(float(s.conversation_time)*.8f)*s.talking*phrase);
        if(!evaluate(asset.skeleton,pose,error))return false;
        if(context.terrain&&s.terrain_ik)for(unsigned i=0;i<config.leg_count;i++){
            const auto &map=config.legs[i];const auto nodes=leg_nodes[i];const auto foot=transform_point(pose.global[nodes[2]],{}),hip=transform_point(pose.global[nodes[0]],{});
            const Point world=placed(s.placement,{foot.x,foot.y,foot.z});double height=world.y;bool accepted=false;
            try{accepted=context.terrain(world.x,world.z,height,context.terrain_userdata);}catch(...){accepted=false;}
            if(!accepted||!finite(height)||std::abs(height)>1.e9){bank.statistics.terrain_misses++;continue;}
            double target=height+map.sole_height*s.placement.scale;const double delta=target-world.y;
            if(delta>map.max_rise*s.placement.scale||delta< -map.max_drop*s.placement.scale)continue;
            const double contact=1-smooth((std::max(0.,-delta)/s.placement.scale-map.contact_height)/map.swing_fade);
            const float weight=float(map.weight*contact*(1-action_mix*(1-config.action_ik_weight)));
            if(weight<=0)continue;
            double locked_x=world.x,locked_z=world.z;
            if(config.plant_feet){
                const bool grounded=world.y-target<map.contact_height*s.placement.scale&&action_mix<.1;
                const double separation=std::hypot(s.planted[i].x-world.x,s.planted[i].z-world.z)/s.placement.scale;
                if(!grounded||separation>.24)s.plant_active[i]=false;
                if(grounded&&!s.plant_active[i]&&s.plant_weight[i]<=.001f){s.planted[i]={world.x,height,world.z};s.plant_active[i]=true;}
                const float goal=s.plant_active[i]?1.f:0.f;
                if(dt>0)s.plant_weight[i]+=float((goal-s.plant_weight[i])*(1-std::exp(-dt/.045)));
                if(!s.plant_active[i]&&s.plant_weight[i]<.001f)s.plant_weight[i]=0;
                target+=(s.planted[i].y+map.sole_height*s.placement.scale-target)*s.plant_weight[i];
                locked_x+=(s.planted[i].x-world.x)*s.plant_weight[i];locked_z+=(s.planted[i].z-world.z)*s.plant_weight[i];
            }
            const double c=std::cos(double(s.placement.yaw)),sn=std::sin(double(s.placement.yaw));
            const double dx=(locked_x-s.placement.position.x)/s.placement.scale,dz=(locked_z-s.placement.position.z)/s.placement.scale;
            LegIK ik;ik.hip=nodes[0];ik.knee=nodes[1];ik.ankle=nodes[2];ik.target={float(c*dx-sn*dz),float((target-s.placement.position.y)/s.placement.scale),float(sn*dx+c*dz)};
            ik.pole={hip.x+map.pole_offset.x,hip.y+map.pole_offset.y,hip.z+map.pole_offset.z};ik.weight=weight;
            // Streaming terrain misses are soft. A solved pose is committed only
            // on success, so malformed animated scales cannot corrupt this frame.
            if(!two_bone_ik(asset.skeleton,pose,ik,error)){bank.statistics.terrain_misses++;error.clear();}
        }
        if(dt>0){if(had_action){const double end=s.action_active?s.action_time:duration;
                append_events(bank,handle,unsigned(action),std::min(previous_action,duration),std::min(end,duration),false,error);if(!error.empty())return false;}
            else if(active_weight>.05&&phase_delta>0){const unsigned clip=run_weight>=.5?unsigned(run):unsigned(walk);append_events(bank,handle,clip,previous_phase*asset.clips[clip].duration,(previous_phase+phase_delta)*asset.clips[clip].duration,true,error);if(!error.empty())return false;}}
        return true;
    }
    bool build(std::array<ActorState,actor_limit> &next,double dt,const Context &context,Bank &bank,std::string &error){
        bank.count=bank.event_count=0;bank.dropped_events=0;bank.origin=context.origin;bank.statistics={};bank.statistics.capacity=capacity;bank.statistics.accepted_delta=dt;
        for(auto &slot:bank.slots)slot.active=false;
        Pose pose;
        for(unsigned i=0;i<capacity;i++)if(next[i].active){auto &s=next[i];const Handle handle{i,s.id};bank.statistics.actors++;
            if(!s.render_enabled)continue;
            if(!build_actor(s,handle,dt,context,bank,pose,error))return false;
            bank.statistics.pose_builds++;
            auto &slot=bank.slots[i];slot.active=true;slot.id=s.id;
            if(!skin_bounds(pose,s.placement,slot.bounds,error))return false;
            Transform placement;placement.translation={float(s.placement.position.x-context.origin.x),float(s.placement.position.y-context.origin.y),float(s.placement.position.z-context.origin.z)};
            if(std::abs(placement.translation.x)>1.e8||std::abs(placement.translation.y)>1.e8||std::abs(placement.translation.z)>1.e8){error="Live actor is too far from the render origin";return false;}
            placement.rotation={0,std::sin(s.placement.yaw*.5f),0,std::cos(s.placement.yaw*.5f)};placement.scale={s.placement.scale,s.placement.scale,s.placement.scale};const auto instance=transform_matrix(placement);
            for(unsigned j=0;j<config.socket_count;j++)slot.sockets[j]=product(instance,product(pose.global[socket_nodes[j]],socket_local[j]));
            if(visible(slot.bounds,context)){const auto index=bank.count++;bank.poses[index]=pose;bank.instances[index]={placement.translation.x,placement.translation.y,placement.translation.z,s.placement.scale,s.placement.yaw};bank.handles[index]=handle;}
        }
        bank.statistics.visible=bank.count;bank.statistics.culled=bank.statistics.actors-bank.count;return true;
    }
};

Config knight_config(bool breathing_idle){
    Config c;c.plant_feet=true;c.planar_root="Hips";if(breathing_idle)c.idle="Idle";
    c.socket_count=3;c.sockets[0]={"weapon_right",{"RightHand",{}}};c.sockets[1]={"shield_left",{"LeftHand",{}}};c.sockets[2]={"head",{"Head",{}}};
    c.leg_count=2;c.legs[0].hip="LeftUpLeg";c.legs[0].knee="LeftLeg";c.legs[0].ankle="LeftFoot";
    c.legs[1].hip="RightUpLeg";c.legs[1].knee="RightLeg";c.legs[1].ankle="RightFoot";return c;
}
Manager::Manager(std::unique_ptr<Impl> impl):impl_(std::move(impl)){}
Manager::~Manager()=default;
std::unique_ptr<Manager> Manager::create(Asset asset,Config config,unsigned capacity,std::string &error){
    error.clear();if(!capacity||capacity>actor_limit||!config_valid(config)){error="Invalid live animation capacity/configuration";return {};}
    if(!validate(asset,error))return {};
    if(asset.mesh.vertices.empty()||asset.mesh.indices.empty()||asset.mesh.primitives.empty()){error="Live animation requires an actual skinned mesh";return {};}
    try{auto impl=std::make_unique<Impl>();impl->asset=std::move(asset);impl->config=std::move(config);impl->capacity=capacity;
        if(!impl->resolve(error))return {};
        impl->instance_id=next_manager_id.fetch_add(1,std::memory_order_relaxed);
        if(!impl->instance_id||impl->instance_id==std::numeric_limits<std::uint64_t>::max()){error="Live animation manager IDs exhausted";return {};}
        return std::unique_ptr<Manager>(new Manager(std::move(impl)));
    }catch(const std::exception &){error="Cannot allocate bounded live animation manager";return {};}
}
std::unique_ptr<Manager> Manager::load(const char *path,Config config,unsigned capacity,std::string &error){Asset asset;if(!load_glb(path,asset,error))return {};return create(std::move(asset),std::move(config),capacity,error);}
RigId Manager::identity()const{return impl_->identity;}
std::uint64_t Manager::instance_id()const{return impl_->instance_id;}
const Config &Manager::config()const{return impl_->config;}
const Asset &Manager::asset()const{return impl_->asset;}
unsigned Manager::capacity()const{return impl_->capacity;}
Handle Manager::spawn(const ActorDesc &desc,std::string &error){
    error.clear();if(!placement_valid(desc.placement)||!valid_speed(desc.speed)||!finite(desc.gait_phase)||desc.gait_phase<0||desc.gait_phase>=1){error="Invalid live actor placement, speed or phase";return {};}
    auto &p=*impl_;if(p.next_id==std::numeric_limits<std::uint64_t>::max()){error="Live actor handle IDs exhausted";return {};}
    for(unsigned i=0;i<p.capacity;i++)if(!p.actors[i].active){auto &s=p.actors[i];s={};s.active=true;s.terrain_ik=desc.terrain_ik;s.id=p.next_id++;s.placement=desc.placement;s.target_speed=desc.speed;s.smoothed_speed=desc.speed;s.gait_phase=desc.gait_phase;s.idle_time=p.idle>=0?desc.gait_phase*p.asset.clips[unsigned(p.idle)].duration:0;return {i,s.id};}
    error="Live animation actor capacity reached";return {};
}
bool Manager::remove(Handle handle,std::string &error){error.clear();auto *s=impl_->actor(handle);if(!s){error="Stale live actor handle";return false;}s->active=false;return true;}
bool Manager::set_motion(Handle handle,const Placement &placement,float speed,std::string &error){error.clear();auto *s=impl_->actor(handle);if(!s||!placement_valid(placement)||!valid_speed(speed)){error="Invalid live actor motion/handle";return false;}if(std::hypot(placement.position.x-s->placement.position.x,placement.position.z-s->placement.position.z)>.5||placement.scale!=s->placement.scale||std::abs(std::remainder(double(placement.yaw-s->placement.yaw),6.28318530718))>.8){s->plant_active={};s->plant_weight={};}s->placement=placement;s->target_speed=speed;return true;}
bool Manager::set_terrain_ik(Handle handle,bool enabled,std::string &error){error.clear();auto *s=impl_->actor(handle);if(!s){error="Stale live actor handle";return false;}s->terrain_ik=enabled;return true;}
bool Manager::set_enabled(Handle handle,bool enabled,std::string &error){error.clear();auto *s=impl_->actor(handle);if(!s){error="Stale live actor handle";return false;}s->render_enabled=enabled;return true;}
bool Manager::set_attention(Handle handle,float yaw,float pitch,bool enabled,bool talking,std::string &error){
    error.clear();auto *s=impl_->actor(handle);
    if(!s||!finite(yaw)||!finite(pitch)||std::abs(yaw)>.7f||std::abs(pitch)>.4f||(enabled&&impl_->attention_head<0)){error="Invalid conversation attention or unmapped head joint";return false;}
    if(enabled&&talking&&!s->target_talking)s->conversation_time=0;
    s->target_attention_yaw=yaw;s->target_attention_pitch=pitch;s->target_attention_weight=enabled?1.f:0.f;s->target_talking=talking?1.f:0.f;return true;
}
bool Manager::start_action(Handle handle,std::string &error){error.clear();auto *s=impl_->actor(handle);if(!s||s->action_active||s->cooldown>0){error="Live action is unavailable or cooling down";return false;}s->action_active=true;s->action_time=0;return true;}
bool Manager::state(Handle handle,ActorState &out,std::string &error)const{error.clear();const auto *s=impl_->actor(handle);if(!s){error="Stale live actor handle";return false;}out=*s;return true;}
bool Manager::update(double delta,const Context &context,std::string &error){
    error.clear();auto &p=*impl_;if(!finite(delta)||delta<0||delta>60||!context_valid(context)||p.revision==std::numeric_limits<std::uint64_t>::max()){error="Invalid live animation frame interval/context/revision";return false;}
    auto next=p.actors;const double dt=std::min(delta,double(p.config.maximum_delta));const unsigned back=1-p.front;
    if(!p.build(next,dt,context,p.banks[back],error))return false;
    auto &bank=p.banks[back];bank.statistics.dropped_seconds=delta-dt;bank.statistics.revision=++p.revision;p.actors=next;p.front=back;return true;
}
Frame Manager::frame()const{const auto &p=*impl_;const auto &b=p.banks[p.front];return {p.identity,p.revision,b.origin,b.poses.data(),b.instances.data(),b.handles.data(),b.count,b.events.data(),b.event_count,b.dropped_events};}
Stats Manager::stats()const{return impl_->banks[impl_->front].statistics;}
bool Manager::bounds(Handle handle,Bounds &out,std::string &error)const{
    error.clear();const auto &p=*impl_;if(!p.actor(handle)||!p.revision){error="Live actor has no committed bounds";return false;}const auto &s=p.banks[p.front].slots[handle.slot];if(!s.active||s.id!=handle.id){error="Update required before querying live actor bounds";return false;}out=s.bounds;return true;
}
bool Manager::socket(Handle handle,std::string_view id,Mat4 &out,std::string &error)const{
    error.clear();const auto &p=*impl_;if(!p.actor(handle)||!p.revision){error="Live actor has no committed sockets";return false;}const auto &s=p.banks[p.front].slots[handle.slot];if(!s.active||s.id!=handle.id){error="Update required before querying live actor sockets";return false;}
    for(unsigned i=0;i<p.config.socket_count;i++)if(p.config.sockets[i].id==id){out=s.sockets[i];return true;}
    error="Socket ID was not authored for this rig";return false;
}
void Manager::checkpoint(Snapshot &out)const{const auto &p=*impl_;out.version=1;out.capacity=p.capacity;out.identity=p.identity;out.configuration=p.configuration;out.next_id=p.next_id;out.actors=p.actors;}
bool Manager::restore(const Snapshot &snapshot,const Context &context,std::string &error){
    error.clear();auto &p=*impl_;if(snapshot.version!=1||snapshot.capacity!=p.capacity||snapshot.identity!=p.identity||snapshot.configuration!=p.configuration||!snapshot.next_id||!context_valid(context)||p.revision==std::numeric_limits<std::uint64_t>::max()){error="Live animation snapshot identity/configuration differs";return false;}
    const double end=p.asset.clips[unsigned(p.action)].duration+p.config.action_fade_out;
    for(unsigned i=0;i<actor_limit;i++){const auto &s=snapshot.actors[i];if(!s.active)continue;
        const double idle_duration=p.idle>=0?p.asset.clips[unsigned(p.idle)].duration:0;
        if(i>=p.capacity||!state_valid(s,end)||s.id>=snapshot.next_id||s.cooldown>p.config.action_cooldown||
           (s.action_active&&s.action_time>=end)||(!s.action_active&&s.action_time!=0)||
           (idle_duration>0?s.idle_time>=idle_duration:s.idle_time!=0)){error="Invalid live animation snapshot actor";return false;}
        for(unsigned j=0;j<i;j++)if(snapshot.actors[j].active&&snapshot.actors[j].id==s.id){error="Duplicate live animation snapshot actor ID";return false;}}
    auto next=snapshot.actors;const unsigned back=1-p.front;if(!p.build(next,0,context,p.banks[back],error))return false;
    p.banks[back].statistics.revision=++p.revision;p.actors=next;p.next_id=std::max(p.next_id,snapshot.next_id);p.front=back;return true;
}
}
