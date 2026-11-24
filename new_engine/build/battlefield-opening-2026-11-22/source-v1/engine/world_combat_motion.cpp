#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_combat_motion.h"
#include "engine/world_combat_capacity.h"
#include "engine/world_arena_render.h"
#include "engine/world_combat_presentation.h"
#include "engine/world_combat_attack_entry.h"
#include "engine/world_combat_continuity.h"
#include "engine/world_combat_transition.h"
#include "engine/world_combat_death_transition.h"
#include "engine/world_combat_locomotion.h"
#include "engine/world_rigid_weapon_pose.h"
#include "engine/world_combat_physical_grips.h"
#include "engine/world_combat_opponent_metadata.h"
#include "engine/world_combat_guard_pose.h"
#include "engine/world_arm_grip_stream.h"
#include "engine/world_aquatic_animation.h"
#include "engine/world_combat_fatality.h"
#include "engine/world_models.h"
#include "engine/world_model_material_batch.h"
#include "engine/world_model_texture_budget.h"
#include "engine/world_combat_pose_packet.h"
#include "engine/world_combat_weapon_surface.h"
#include "engine/world_model_variants.h"
#include "engine/world_atlas.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>
namespace {
struct Clip{unsigned base,count;bool loop;};
constexpr Clip clips[]={{0,4,false},{4,8,true},{12,12,false},{24,12,false},{36,12,false},{48,12,false},{60,12,false},{72,4,false},{76,4,false},{80,4,false},{84,4,false},{88,4,false},{92,8,false},{100,8,false},{108,8,false},{116,8,false},{124,8,false},{132,8,false},{140,8,false},{148,24,false}};
constexpr unsigned legacy_poses=108,poses=172,combo_batches=25,entry_batches=5,settle_batches=12,batches=poses+3+combo_batches+entry_batches+settle_batches,death_base=148,death_frames=24,actor_capacity=world_combat_capacity::actors,npc_capacity=world_combat_capacity::npcs;
constexpr unsigned settle_offset=3+combo_batches+entry_batches;
unsigned settle_endpoint(unsigned index){return index<5?23+index*12:index==5?99:index==6?107:115+(index-7)*8;}
#include "engine/world_combat_weapon_sockets.inc"
struct Upload {std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};unsigned count=0;};
struct Bank{std::array<WorldModelAsset*,poses> assets={};std::array<WorldModelPoseBlend*,batches> blend={};std::array<Upload,batches> uploaded={};std::array<WorldModelRigidWeaponPose,batches> rigid={};};
constexpr unsigned aquatic_poses=world_aquatic_animation::pose_count,aquatic_batches=aquatic_poses*(1+world_aquatic_animation::modes);
std::array<unsigned,2> pose_pair(unsigned slot,unsigned count){
    if(slot<count){for(const auto &c:clips)if(slot>=c.base&&slot<c.base+c.count)return {slot,c.base+(c.loop?(slot-c.base+1)%c.count:std::min(slot-c.base+1,c.count-1))};}
    else if(slot<count+3)return {slot==count?0u:slot==count+1?4u:8u,slot==count?4u:0u};
    else if(slot<count+3+combo_batches){const unsigned pair=slot-count-3;return {12+12*(pair/5)+8,12+12*(pair%5)};}
    else if(slot<count+3+combo_batches+entry_batches)return {0,12+12*(slot-count-3-combo_batches)};
    else if(slot<count+settle_offset+settle_batches)return {settle_endpoint(slot-count-settle_offset),0};
    return {slot,slot};
}
unsigned rigid_bank(Bank &bank,const float sockets[][2][3],unsigned count,unsigned slots,bool arms,unsigned weapon=0,const float (*pivots)[3]=nullptr){
    unsigned accepted=0;
    for(unsigned slot=0;slot<slots;++slot){if(!bank.blend[slot])continue;
        const auto pair=pose_pair(slot,count);float a[3],b[3];
        const auto hand_a=pivots?world_rigid_weapon_pose::Point{pivots[pair[0]][0],pivots[pair[0]][1],pivots[pair[0]][2]}:world_combat_physical_grips::wrist(weapon,pair[0],sockets[pair[0]]);
        const auto hand_b=pivots?world_rigid_weapon_pose::Point{pivots[pair[1]][0],pivots[pair[1]][1],pivots[pair[1]][2]}:world_combat_physical_grips::wrist(weapon,pair[1],sockets[pair[1]]);
        std::copy_n(hand_a.begin(),3,a);std::copy_n(hand_b.begin(),3,b);
        // The first-person bake uses this same translation for all arm/weapon
        // vertices. It must be present in the grip, never added to rotations.
        if(arms){a[0]+=.25f;b[0]+=.25f;a[2]+=.65f;b[2]+=.65f;}
        accepted+=world_model_pose_blend_rigid_weapon(bank.blend[slot],a,b,&bank.rigid[slot]);
    }
    return accepted;
}
void release_rigid_bank_source(Bank &bank){for(auto *asset:bank.assets)if(asset)world_model_pose_release_rigid_source(asset);}
struct AquaticBank {std::array<WorldModelAsset*,aquatic_poses> assets={};std::array<WorldModelPoseBlend*,aquatic_batches> blends={};};
struct Aquatics {AquaticBank body,arms;world_aquatic_animation::State state;unsigned slot=0;};
void free_aquatics(Aquatics *p){if(!p)return;for(auto *b:{&p->body,&p->arms}){for(auto *v:b->blends)if(v)world_model_pose_blend_free(v);for(auto *a:b->assets)if(a)world_model_free(a);}delete p;}
#include "engine/world_combat_fatality_geometry.inc"
#include "engine/world_neck_blood_geometry.inc"
struct Fatalities {
 std::array<WorldModelAsset*,48> assets={};
 std::array<WorldModelPoseBlend*,24> body={},crowd={},heads={};
 std::array<unsigned,24> body_counts={},crowd_counts={},head_counts={};
 world_combat_fatality::System state;
};
void free_fatalities(Fatalities *g){if(!g)return;for(auto *list:{&g->body,&g->crowd,&g->heads})for(auto *b:*list)if(b)world_model_pose_blend_free(b);for(auto *a:g->assets)if(a)world_model_free(a);delete g;}
struct Actor{
    world_combat_transition::Origin reaction_origin;
    bool reaction_bridge=false;double reaction_started=0;
    bool reaction_entry=false,reaction_guard=false,reaction_idle=false;unsigned reaction_guard_direction=0;float reaction_from_phase=0;
    unsigned hits=0,enemy_hits=0,parries=0,blocks=0,guarded=0;
    std::uint64_t steel_serial=0;double steel_started=0;float steel_phase=0,steel_turn=0,steel_retreat=0;unsigned steel_direction=0,steel_pose_slot=~0u;float steel_pose_weight=0;bool steel_rebound=false;
    std::uint64_t attack_serial=0,resolved_serial=0;unsigned attack_direction=2,combo_from=2,combo_to=2;bool combo=false,root_ready=false,idle_entry=false;unsigned idle_entry_from=0;float idle_entry_phase=0;
    world_combat_continuity::Combo combo_path=world_combat_continuity::Combo::Bridge;
    float combo_phase=world_combat_continuity::combo_key,guard_entry_phase=0,idle_elapsed=.14f;bool guard_entry=false,guard_entry_counted=false,late_combo_counted=false,guard_recoil=false;
    world_combat_transition::Origin entry_origin;float parry_duration=.20f;bool entry_counted=false,guard_recoil_counted=false;
    bool guard_idle_entry=false;float guard_idle_from=0,guard_idle_to=0,guard_idle_time=0;
    float guarded_gait_phase=0,guarded_gait_mix=0;
    double last_x=0,last_z=0,last_clock=0;
    unsigned slot=0;bool active=false,attacking=false,facing_ready=false,defeated=false,walking=false;float facing_yaw=0,combo_turn=0;float attack_leadin=0,attack_guard=0;unsigned attack_guard_direction=2,reaction_direction=2;float guard=0,guard_direction=2,phase=0,transition=0,settle_from=0,settle_to=0;int walk=0;unsigned half=0;float hit=0,parry=0,turn=0,death_time=0;};
struct Uniforms {unsigned long long epoch=0;unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1,blood_wear=-1;};
float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
// Shared, preloaded gait transitions. The same selector is used by rendering
// and contact prediction, including the support-foot return on stopping.
void walk_sample(Actor &a,unsigned count,bool polished,float &weight,unsigned &clip,float &phase,bool &direct,
                 bool moving,float dt,double travel,const world_combat_locomotion::State *locomotion,double stride=1.3){
            if(moving){
                if(a.walk==0){a.walk=1;a.transition=0;a.phase=0;
                    if(polished&&a.slot<4){a.settle_from=std::clamp((float(a.slot)+weight)/3.f,0.f,1.f);if(a.settle_from>0)a.walk=6;}}
                if(a.walk==3)a.walk=2;
                if(a.walk==4)a.walk=5;
                if(a.walk==6){a.transition=std::min(1.f,a.transition+dt/.04f);clip=0;phase=a.settle_from*(1-smooth(a.transition));if(a.transition>=1){a.walk=1;a.transition=0;}}
                else if(a.walk==1){a.transition=std::min(1.f,a.transition+dt/.18f);a.slot=count;weight=smooth(a.transition);direct=true;if(a.transition>=1){a.walk=2;a.phase=0;}}
                else if(a.walk==5){a.transition=std::max(0.f,a.transition-dt/.12f);a.slot=count+1+a.half;weight=smooth(a.transition);direct=true;if(a.transition<=0){a.walk=2;a.phase=a.half*.5f;}}
                else{if(locomotion)a.phase=world_combat_locomotion::follow_phase(a.phase,locomotion->phase,locomotion->phase_step,dt);else a.phase=world_combat_locomotion::wrapped(a.phase+float(travel/stride));clip=1;phase=a.phase;}
            }else if(a.walk!=0){
                if(a.walk==6){a.transition=std::min(1.f,a.transition+dt/.04f);clip=0;phase=a.settle_from*(1-smooth(a.transition));if(a.transition>=1){a.walk=0;a.phase=0;}}
                else if(a.walk==1){a.slot=count;a.transition=std::max(0.f,a.transition-dt/.14f);weight=smooth(a.transition);direct=true;if(a.transition<=0)a.walk=0;}
                else{
                    if(a.walk==5)a.walk=4;
                    if(a.walk==2){a.walk=3;a.transition=0;a.settle_from=a.phase;const float support=locomotion?locomotion->support_phase:world_combat_locomotion::wrapped(std::round(a.phase*2)*.5f);a.settle_to=a.phase+std::remainder(support-a.phase,1.f);}
                    if(a.walk==3){a.transition=std::min(1.f,a.transition+dt/.10f);phase=world_combat_locomotion::wrapped(a.settle_from+(a.settle_to-a.settle_from)*smooth(a.transition));a.phase=phase;clip=1;if(a.transition>=1){a.walk=4;a.transition=0;a.half=(int(std::lround(a.settle_to*2))&1);}}
                    else{a.transition=std::min(1.f,a.transition+dt/.18f);a.slot=count+1+a.half;weight=smooth(a.transition);direct=true;if(a.transition>=1)a.walk=0;}
                }
            }
}
bool guard_sample(unsigned slot,float weight,unsigned &direction,float &phase){
    for(unsigned d=0;d<5;++d){const auto &q=clips[7+d];if(slot>=q.base&&slot<q.base+q.count){direction=d;phase=std::clamp((float(slot-q.base)+weight)/float(q.count-1),0.f,1.f);return true;}}
    return false;
}
world_combat_transition::Origin entry_origin(unsigned slot,float weight,unsigned count,bool shield){
    using Kind=world_combat_transition::OriginKind;weight=std::clamp(weight,0.f,1.f);
    if(slot>=4&&slot<12)return {Kind::Walk,1,0,(float(slot-4)+weight)/8.f};
    if(slot==count)return {Kind::WalkStart,0,0,weight};
    if(slot==count+1||slot==count+2)return {Kind::WalkReturn,0,slot-count-1,weight};
    if(slot>=count+3+combo_batches&&slot<count+3+combo_batches+entry_batches)return {Kind::Entry,0,slot-count-3-combo_batches,weight};
    if(slot>=count+settle_offset&&slot<count+settle_offset+settle_batches){const unsigned index=slot-count-settle_offset;if(!shield||index<7)return {Kind::Return,0,index,weight};}
    for(unsigned clip=2;clip<19;++clip){const auto &q=clips[clip];if(clip>=7&&clip<=11)continue;if(slot<q.base||slot>=q.base+q.count)continue;
        const unsigned index=clip<7?clip-2:clip==12?5:clip==13?6:7+clip-14;
        if(!shield||index<7)return {Kind::Clip,clip,index,(float(slot-q.base)+weight)/float(q.count-1)};
    }
    return {};
}
// Keep an interrupted Orc on its exact committed pose, then use the same
// certified return pairs as attack entry. Body/cap rendering, blade queries
// and anatomical prediction all select this identical bounded route.
constexpr float opponent_reaction_entry_seconds=.09f;
// Render-only:120ms to the fixed authored entry,240ms source reaction,80ms return.
constexpr float npc_reaction_entry_seconds=.12f;
constexpr float npc_reaction_seconds=.44f;
bool npc_reaction_window(const WorldTrainingCombat&c){
 // A body hit does not necessarily interrupt the AI. Only a real Stagger
 // with enough remaining time plus its shortest following Measure can own
 // a full-body visual reaction without concealing a committed live cut.
 if(c.state!=TrainingState::Fighting||c.enemy_phase!=TrainingEnemyPhase::Stagger)return false;
 const double tempo=world_combat_difficulty::profile(c.difficulty).attack_tempo_scale;
 return std::max(0.,c.enemy_duration-c.enemy_time)+(c.arena_mode?.20:.35)/tempo>=npc_reaction_seconds+1.e-5;
}
bool opponent_reaction_entry(const Actor &a,float elapsed,unsigned count,unsigned &slot,float &weight,unsigned &clip,float &phase,bool &direct){
    if(!a.reaction_entry||elapsed>=opponent_reaction_entry_seconds)return false;
    const float f=smooth(elapsed/opponent_reaction_entry_seconds);
    if(a.reaction_guard){clip=7+a.reaction_guard_direction;phase=a.reaction_from_phase*(1-f);}
    else if(a.reaction_idle){clip=0;phase=a.reaction_from_phase*(1-f);}
    else{const auto s=world_combat_transition::entry(a.reaction_origin,elapsed,opponent_reaction_entry_seconds,0);
        using Kind=world_combat_transition::SampleKind;
        if(s.kind==Kind::Clip){clip=s.index;phase=s.phase;if(clips[clip].loop)phase-=std::floor(phase);}
        else{slot=s.kind==Kind::WalkStart?count:s.kind==Kind::WalkReturn?count+1+s.index:s.kind==Kind::Return?count+settle_offset+s.index:count+3+combo_batches+s.index;weight=s.phase;direct=true;}}
    return true;
}
float attack_phase(TrainingPlayerPhase phase,double t,double duration,double leadin,bool polished){
    float f=phase==TrainingPlayerPhase::Windup?world_combat_attack_entry::windup_fraction(t,duration,leadin):float(std::clamp(t/std::max(.001,duration),0.,1.));
    // Only visual sampling changes: preparation accelerates into contact,
    // follow-through sheds speed, and the return settles into the stance.
    // Keep the gameplay's exact .36 contact/.60 recovery/1 neutral anchors.
    if(polished)f=phase==TrainingPlayerPhase::Windup?std::pow(f,1.6f):phase==TrainingPlayerPhase::Strike?1-std::pow(1-f,1.4f):smooth(f);
    return phase==TrainingPlayerPhase::Windup?f*.36f:phase==TrainingPlayerPhase::Strike?.36f+f*.24f:.60f+f*.40f;
}
// A real clash stops at its measured source phase. A short, measured retreat
// precedes an exact rigid frozen-pose return, avoiding the old preparation arc.
// A missed swing never enters this route.
constexpr double opponent_steel_seconds=.44;
float opponent_guard_recoil(float guard_phase,double elapsed,double duration){
    const float t=float(std::clamp(elapsed/std::max(.000001,duration),0.,1.));const float pulse=16*t*t*(1-t)*(1-t);
    return std::clamp(guard_phase,0.f,1.f)*(1-.12f*pulse);
}

}
struct Unarmed {std::array<WorldModelAsset*,32> assets={};std::array<WorldModelPoseBlend*,96> blends={};float phase=0,transition=1,weight=0,finish_from=0,finish_to=0,finish_time=0;unsigned mode=0,from=0,slot=0;bool active=false,finishing=false;};
struct Loadouts {struct Entry {Bank body,arms;float sockets[poses][2][3]={};world_combat_presentation::Bank presentation;};std::array<Entry,3> entries={};};
struct LocomotionBank {
    WorldModelVertexStream *stream=nullptr;
    std::vector<unsigned> indices;
    std::vector<float> masks;
    std::vector<std::array<float,9>> templates;
    std::vector<WorldModelVertexOverride> staging;
};
struct Locomotion {std::array<LocomotionBank,5> banks;std::array<float,4> strides={};world_combat_locomotion::State state;};
void free_locomotion(Locomotion *p){if(p){for(auto &b:p->banks)world_model_vertex_stream_free(b.stream);delete p;}}
void free_loadouts(Loadouts *p){if(!p)return;for(auto &entry:p->entries)for(auto *bank:{&entry.body,&entry.arms}){for(auto *b:bank->blend)if(b)world_model_pose_blend_free(b);for(auto *a:bank->assets)if(a)world_model_free(a);}delete p;}
struct GripViewStream {
    WorldModelVertexStream *stream=nullptr;
    std::vector<WorldModelVertexOverride> staging;
};
struct ArmGrips {
    world_arm_grip_stream::Database database;
    std::array<GripViewStream,3> arms;
    bool healthy=true,body_active=false,arms_active=false;
};
void free_arm_grips(ArmGrips *p){if(p){for(auto &v:p->arms)world_model_vertex_stream_free(v.stream);delete p;}}
#include "engine/world_combat_anatomy_runtime.inc"
struct DeathActorBridge {
    WorldModelPoseBlend *body=nullptr,*cap=nullptr;
    WorldModelVertexStream *stream=nullptr,*cap_stream=nullptr;
    std::vector<WorldModelVertexOverride> frozen,staging,cap_frozen,cap_staging;
    AnatomyPose frozen_pose;anatomy_math::Edge frozen_edge;std::array<float,3> frozen_pivot={};std::uint64_t frozen_generation=0;
    WorldCombatDeathPose pose;AnatomySnapshot source_snapshot;float elapsed=0;bool mirrored=false,uploaded=false,crowd_override=false,clash=false;
};
struct DeathBridges {std::array<DeathActorBridge,actor_capacity> actors;WorldCombatDeathStats stats;};
void free_deaths(DeathBridges *p){if(p){for(auto &v:p->actors){world_model_pose_blend_free(v.body);world_model_pose_blend_free(v.cap);world_model_vertex_stream_free(v.stream);world_model_vertex_stream_free(v.cap_stream);}delete p;}}
#include "engine/world_armor_wear.h"
#include "engine/world_combat_compact_runtime.inc"
struct WorldCombatMotion{
    // Ordinary worlds retain their existing allocation budget. Arena compact
    // banks provide explicit31-bit ownership at the loading boundary.
    std::uint32_t blood_seed=0;
    CompactMotion *compact=nullptr;unsigned actor_mask=(1u<<9)-1u;

    bool opponents_only=false;unsigned measured_pivots=0;
    double opponent_stride_metres=1.3,opponent_body_radius_metres=.34,opponent_body_height_metres=1.8;
    WorldCombatVariantStats variant_stats;
    DeathBridges *deaths=nullptr;
    CombatAnatomy *anatomy=nullptr;
    Locomotion *locomotion=nullptr;ArmGrips *arm_grips=nullptr;WorldCombatGripStats grip_stats;bool grip_weapon_allowed=true;
    Loadouts *loadouts=nullptr;unsigned loadout=0,equipment=340;
    Unarmed *unarmed=nullptr;
    Fatalities *fatalities=nullptr;
    double (*fatality_floor)(double,double,void*)=nullptr;void *fatality_floor_context=nullptr;
    Aquatics *aquatics=nullptr;
    Bank body,arms;std::array<WorldModelPoseBlend*,batches> enemy_only={};std::array<Upload,batches> enemy_uploaded={};std::array<Actor,actor_capacity> actors={};std::array<Uniforms,4> uniforms={};unsigned next_uniform=0;
    WorldCombatContinuityStats continuity;
    std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};
    double x=0,z=0,last_x=0,last_z=0;bool initialized=false;bool weapon_sockets=false;WorldModelInstance arms_instance={};float clock=0;WorldCombatMotionStats stats;
    double arena_x=0,arena_z=0;float arena_feet=24;bool custom_arena=false;
    unsigned pose_count=legacy_poses,batch_count=legacy_poses+3+combo_batches+entry_batches;std::array<unsigned,4> idle_entry_witnesses={};
    float sockets[poses][2][3]={};
    float viewmodel_sockets[poses][2][3]={};bool separate_viewmodel_sockets=false;
    world_combat_presentation::Bank presentation;
    bool arena_enemy_walk=false,crowd_ready=false;std::array<WorldModelPoseBlend*,death_frames+1> arena_crowd={};std::array<unsigned,death_frames+1> crowd_counts={};double crowd_x=0,crowd_z=0;
    Uniforms &locations(unsigned program){for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");u.prop=glGetUniformLocation(program,"uPropLodRange");u.blood_wear=glGetUniformLocation(program,"uBloodWear");return u;}
};
namespace {
void actor_blood_wear(WorldCombatMotion*p,unsigned program,unsigned actor){
 auto &u=p->locations(program);if(u.blood_wear<0)return;
 const auto wear=actor&&!p->opponents_only&&(p->compact||p->variant_stats.loaded)?world_armor_wear::opponent(actor,p->blood_seed):std::array<float,3>{};
 glUniform3fv(u.blood_wear,1,wear.data());
}
}
#include "engine/world_combat_locomotion_runtime.inc"
#include "engine/world_combat_arm_grips_runtime.inc"
namespace {unsigned anatomy_caps_mask(unsigned);}
#include "engine/world_combat_compact_actions.inc"
#include "engine/world_combat_clash_runtime.inc"
#include "engine/world_combat_death_runtime.inc"
#include "engine/world_combat_anatomy_actions.inc"
#include "engine/world_combat_compact_draw.inc"
namespace {
bool load_presentation(world_combat_presentation::Bank &out,const char *directory,double *required_stride=nullptr,double *required_radius=nullptr,double *required_height=nullptr,bool require_aimed=true){
    char path[1200];const int n=std::snprintf(path,sizeof path,"%s/combat_presentation.cfg",directory);
    if(n<0||std::size_t(n)>=sizeof path)return false;
    auto *file=std::fopen(path,"rb");if(!file)return !required_stride&&!required_radius&&!required_height&&errno==ENOENT; // legacy banks retain their exact sampling
    unsigned version=0,aimed=0;float contact[5]={};char end[64]={};double stride=1.3,radius=.34,height=1.8;bool measured_stride=false,measured_radius=false,measured_height=false;
    const int parsed=std::fscanf(file,"VALWOOD_COMBAT_PRESENTATION %u upper_body_aimed %u contact_frames %f %f %f %f %f %63s",&version,&aimed,&contact[0],&contact[1],&contact[2],&contact[3],&contact[4],end);
    bool okay=parsed==8&&version==1&&aimed<=1&&(!require_aimed||aimed==1);
    while(okay&&std::strcmp(end,"end")){
        double *value=nullptr;
        if(!std::strcmp(end,"walk_stride_m")&&!measured_stride){value=&stride;measured_stride=true;}
        else if(!std::strcmp(end,"body_radius_m")&&!measured_radius){value=&radius;measured_radius=true;}
        else if(!std::strcmp(end,"body_height_m")&&!measured_height){value=&height;measured_height=true;}
        else{okay=false;break;}
        okay=std::fscanf(file,"%lf %63s",value,end)==2;
    }
    okay=okay&&!std::strcmp(end,"end")&&(!required_stride||measured_stride)&&(!required_radius||measured_radius)&&(!required_height||measured_height)&&
        std::isfinite(stride)&&stride>=.25&&stride<=4.&&std::isfinite(radius)&&radius>=.15&&radius<=.75&&std::isfinite(height)&&height>=1.&&height<=3.5&&height>=2*radius;
    for(float frame:contact)okay=okay&&std::isfinite(frame)&&frame>=.88f&&frame<=6.38f;
    int tail=0;while((tail=std::fgetc(file))!=EOF)if(tail!=' '&&tail!='\t'&&tail!='\r'&&tail!='\n')okay=false;
    std::fclose(file);if(!okay)return false;
    out.upper_body_aimed=aimed!=0;std::copy_n(contact,5,out.contact_frames.begin());
    if(required_stride)*required_stride=stride;
    if(required_radius)*required_radius=radius;
    if(required_height)*required_height=height;
    return true;
}
bool load_sockets(WorldCombatMotion *p,const char *directory,bool original=true,const char *filename="combat_weapon_sockets.bin"){
    char path[1200];const int n=std::snprintf(path,sizeof path,"%s/%s",directory,filename);
    if(n<0||std::size_t(n)>=sizeof path)return false;
    auto *file=std::fopen(path,"rb");if(!file)return false;
    auto word=[&](std::uint32_t &out){unsigned char b[4];if(std::fread(b,1,4,file)!=4)return false;out=std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);return true;};
    char magic[8];std::uint32_t count=0;bool okay=std::fread(magic,1,8,file)==8&&!std::memcmp(magic,"VWMOT172",8)&&word(count)&&count==poses;
    for(unsigned i=0;okay&&i<poses;i++)for(unsigned j=0;okay&&j<2;j++)for(unsigned k=0;okay&&k<3;k++){
        std::uint32_t bits=0;okay=word(bits);float f=0;std::memcpy(&f,&bits,4);
        okay=okay&&std::isfinite(f)&&std::abs(f)<=16&&(!original||i>=legacy_poses||std::abs(f-combat_weapon_sockets[i][j][k])<=1.e-5f);p->sockets[i][j][k]=f;
    }
    okay=okay&&std::fgetc(file)==EOF;std::fclose(file);return okay;
}
bool load_viewmodel_sockets(WorldCombatMotion *p,const char *directory){
    constexpr const char *filename="combat_viewmodel_weapon_sockets.bin";
    char path[1200];const int n=std::snprintf(path,sizeof path,"%s/%s",directory,filename);
    if(n<0||std::size_t(n)>=sizeof path)return false;
    auto *probe=std::fopen(path,"rb");if(!probe)return errno==ENOENT;std::fclose(probe);
    // A present but malformed explicit viewmodel must fail loading; it cannot
    // silently borrow the other view's new guard pivots or contact endpoints.
    WorldCombatMotion temporary;if(!load_sockets(&temporary,directory,false,filename))return false;
    std::memcpy(p->viewmodel_sockets,temporary.sockets,sizeof p->viewmodel_sockets);p->separate_viewmodel_sockets=true;
    std::printf("[Combat motion] Separate authored first-person weapon sockets loaded\n");return true;
}
}
namespace {
WorldCombatMotion *load_motion(const char *directory,char *error,std::size_t capacity,bool polished,bool opponents_only){
    WorldModelMaterialBatch material_batch_scope;
    if(error&&capacity)error[0]=0;
    if(!directory||!*directory)return nullptr;
    auto *p=new(std::nothrow)WorldCombatMotion;if(!p)return nullptr;
    p->opponents_only=opponents_only;
    world_combat_opponent_metadata::Pivots measured={};
    std::memcpy(p->sockets,combat_weapon_sockets,sizeof combat_weapon_sockets);
    if(!load_presentation(p->presentation,directory,opponents_only?&p->opponent_stride_metres:nullptr,opponents_only?&p->opponent_body_radius_metres:nullptr,opponents_only?&p->opponent_body_height_metres:nullptr))goto fail;
    if(opponents_only){
        // New species require their own explicit presentation and pivot data.
        // Optional legacy metadata and human wrist tables are not a fallback.
        char path[1200];const int n=std::snprintf(path,sizeof path,"%s/combat_weapon_pivots.bin",directory);
        if(!p->presentation.upper_body_aimed||n<0||std::size_t(n)>=sizeof path)goto fail;
        FILE *file=std::fopen(path,"rb");const bool ok=world_combat_opponent_metadata::read_pivots(file,measured);if(file)std::fclose(file);
        if(!ok)goto fail;
        p->measured_pivots=poses;
    }
    if(polished){p->pose_count=poses;p->batch_count=batches;p->stats.polished=true;if(!load_sockets(p,directory,!opponents_only&&!p->presentation.upper_body_aimed))goto fail;}
    if(opponents_only){for(const auto &edge:p->sockets)if(!world_combat_opponent_metadata::valid_edge(edge[0],edge[1]))goto fail;}
    else if(!load_viewmodel_sockets(p,directory))goto fail;
    for(unsigned kind=0;kind<(opponents_only?1u:2u);kind++){auto &b=kind?p->arms:p->body;
        for(unsigned i=0;i<p->pose_count;i++){char path[1200];const int n=std::snprintf(path,sizeof path,"%s/combat_%s_%03u.glb",directory,kind?"arms":"body",i);
            if(n<0||std::size_t(n)>=sizeof path||!(b.assets[i]=kind?world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION):polished?world_model_load_pose_support_pnt(path,WORLD_MODEL_POSE_BLEND_VERSION,opponents_only||i>=legacy_poses):world_model_load_pose_support(path,WORLD_MODEL_POSE_BLEND_VERSION,i>=legacy_poses)))goto fail;}
        for(const auto &c:clips)if(c.base<p->pose_count)for(unsigned i=0;i<c.count;i++){const unsigned slot=c.base+i,next=c.base+(c.loop?(i+1)%c.count:std::min(i+1,c.count-1));
            b.blend[slot]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);if(!b.blend[slot]||!world_model_pose_blend_set_targets(b.blend[slot],b.assets[slot],b.assets[next]))goto fail;}
        for(unsigned i=0;i<3;i++){b.blend[p->pose_count+i]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);const unsigned from=i==0?0:(i==1?4:8),to=i==0?4:0;
            if(!b.blend[p->pose_count+i]||!world_model_pose_blend_set_targets(b.blend[p->pose_count+i],b.assets[from],b.assets[to]))goto fail;}
        for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to){
            const unsigned at=p->pose_count+3+from*5+to;
            b.blend[at]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.blend[at]||!world_model_pose_blend_set_targets(b.blend[at],b.assets[12+12*from+8],b.assets[12+12*to]))goto fail;
        }
        for(unsigned direction=0;direction<entry_batches;++direction){
            const unsigned at=p->pose_count+3+combo_batches+direction;
            b.blend[at]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.blend[at]||!world_model_pose_blend_set_targets(b.blend[at],b.assets[0],b.assets[12+12*direction]))goto fail;
        }
        if(polished)for(unsigned i=0;i<settle_batches;++i){const unsigned at=p->pose_count+settle_offset+i;
            b.blend[at]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.blend[at]||!world_model_pose_blend_set_targets(b.blend[at],b.assets[settle_endpoint(i)],b.assets[0]))goto fail;
        }
    }
    for(const auto &c:clips)if(c.base<p->pose_count)for(unsigned i=0;i<c.count;i++){
        const unsigned slot=c.base+i,next=c.base+(c.loop?(i+1)%c.count:std::min(i+1,c.count-1));
        p->enemy_only[slot]=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[slot]||!world_model_pose_blend_set_targets(p->enemy_only[slot],p->body.assets[slot],p->body.assets[next]))goto fail;
    }
    for(unsigned i=0;i<3;i++){
        const unsigned from=i==0?0:(i==1?4:8),to=i==0?4:0;
        p->enemy_only[p->pose_count+i]=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[p->pose_count+i]||!world_model_pose_blend_set_targets(p->enemy_only[p->pose_count+i],p->body.assets[from],p->body.assets[to]))goto fail;
    }
    for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to){
        const unsigned at=p->pose_count+3+from*5+to;
        p->enemy_only[at]=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[at]||!world_model_pose_blend_set_targets(p->enemy_only[at],p->body.assets[12+12*from+8],p->body.assets[12+12*to]))goto fail;
    }
    for(unsigned direction=0;direction<entry_batches;++direction){
        const unsigned at=p->pose_count+3+combo_batches+direction;
        p->enemy_only[at]=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[at]||!world_model_pose_blend_set_targets(p->enemy_only[at],p->body.assets[0],p->body.assets[12+12*direction]))goto fail;
    }
    if(polished)for(unsigned i=0;i<settle_batches;++i){const unsigned at=p->pose_count+settle_offset+i;
        p->enemy_only[at]=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[at]||!world_model_pose_blend_set_targets(p->enemy_only[at],p->body.assets[settle_endpoint(i)],p->body.assets[0]))goto fail;
    }
    if(opponents_only){
        float pivots[poses][3];for(unsigned i=0;i<poses;++i)std::copy_n(measured[i].begin(),3,pivots[i]);
        p->stats.rigid_weapon_pairs=rigid_bank(p->body,p->sockets,p->pose_count,p->batch_count,false,0,pivots);
        if(p->stats.rigid_weapon_pairs!=p->batch_count)goto fail;
    }else p->stats.rigid_weapon_pairs=rigid_bank(p->body,p->sockets,p->pose_count,p->batch_count,false)+rigid_bank(p->arms,p->separate_viewmodel_sockets?p->viewmodel_sockets:p->sockets,p->pose_count,p->batch_count,true);
    for(unsigned slot=0;slot<p->batch_count;++slot)if(p->body.rigid[slot].enabled)world_model_pose_blend_share_rigid(p->enemy_only[slot],p->body.blend[slot]);
    release_rigid_bank_source(p->body);release_rigid_bank_source(p->arms);
    std::printf("[Combat motion] Rigid weapon interpolation: %u certified body/arm pairs\n",p->stats.rigid_weapon_pairs);
    p->stats.combo_batches=combo_batches;
    p->weapon_sockets=polished||std::string(directory).find("longsword_mocap")!=std::string::npos;
    p->variant_stats.available_mask=world_model_pose_variant_mask(p->body.assets[0]);
    if(p->variant_stats.available_mask&&p->variant_stats.available_mask!=world_model_variants::all)goto fail;
    p->variant_stats.loaded=p->variant_stats.available_mask!=0;
    p->stats.pose_assets=p->pose_count*(opponents_only?1u:2u);p->stats.reaction_assets=polished?40:0;p->stats.death_assets=polished?24:0;return p;
fail:
    if(error&&capacity)std::snprintf(error,capacity,opponents_only?"Opponent bank requires its own 172 body poses, presentation, measured weapon pivots and matching rigid weapon sockets":"Owned combat pose bank missing or incompatible");
    world_combat_motion_free(p);return nullptr;
}
}
WorldCombatMotion *world_combat_motion_load(const char *directory,char *error,std::size_t capacity,bool polished){return load_motion(directory,error,capacity,polished,false);}
WorldCombatMotion *world_combat_motion_load_opponents(const char *directory,const char *anatomy_directory,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!directory||!*directory||!anatomy_directory||!*anatomy_directory){if(error&&capacity)std::snprintf(error,capacity,"Opponent body and anatomy directories are both required");return nullptr;}
    auto *p=load_motion(directory,error,capacity,true,true);if(!p)return nullptr;
    if(!world_combat_motion_load_anatomy(p,anatomy_directory)||!world_combat_motion_prepare_crowd(p)||!world_combat_motion_opponents_ready(p)){
        if(error&&capacity)std::snprintf(error,capacity,"Opponent anatomy, cut caps or crowd do not match this body bank");
        world_combat_motion_free(p);return nullptr;
    }
    return p;
}
namespace {
bool compact_surface_prepare(CompactMotion &c,const char *file,const world_combat_opponent_metadata::Pivots &pivots){
 if(!world_combat_weapon_surface::roles(file,c.packet.meshes().size(),c.surface_roles))c.surface_roles.assign(c.packet.meshes().size(),world_combat_weapon_surface::Unknown);
 for(unsigned mesh=0;mesh<c.surface_roles.size();++mesh)if(c.surface_roles[mesh]==world_combat_weapon_surface::Structural&&(c.packet.meshes()[mesh].flags&1)){world_combat_weapon_surface::Tree tree;if(!tree.build(c.packet,mesh))return false;c.surface_trees.push_back(std::move(tree));}
 if(!c.surface_trees.empty())for(unsigned pose=0;pose<poses;++pose)if(!c.packet.rigid(0,pose,pivots[0].data(),pivots[pose].data(),c.surface_rigid[pose]))return false;return true;
}
}
WorldCombatMotion *world_combat_motion_load_compact(const char *directory,const char *anatomy_directory,const char *packet_file,unsigned actor_mask,bool opponent_policy,char *error,std::size_t capacity,unsigned max_texture_edge){
 if(error&&capacity)error[0]=0;if(!directory||!*directory||!anatomy_directory||!*anatomy_directory||!packet_file||!*packet_file||!world_model_texture_budget::valid(max_texture_edge)||!actor_mask||(actor_mask&~world_combat_capacity::actor_mask)||(opponent_policy&&(actor_mask&1)))return nullptr;
 auto *p=new(std::nothrow)WorldCombatMotion;if(!p)return nullptr;p->compact=new(std::nothrow)CompactMotion;
 bool ok=p->compact&&p->compact->packet.load(packet_file)&&(p->compact->packet.pose_count()==poses||(!opponent_policy&&(p->compact->packet.pose_count()==204||p->compact->packet.pose_count()==210)));
 p->blood_seed=world_armor_wear::bank_seed(directory);p->actor_mask=actor_mask;p->opponents_only=opponent_policy;p->pose_count=poses;p->batch_count=batches;p->stats.polished=true;
 world_combat_opponent_metadata::Pivots pivots={};
 if(ok)ok=load_presentation(p->presentation,directory,opponent_policy?&p->opponent_stride_metres:nullptr,&p->opponent_body_radius_metres,&p->opponent_body_height_metres,opponent_policy)&&load_sockets(p,directory,false);
 char path[1600];if(ok){std::snprintf(path,sizeof path,"%s/combat_weapon_pivots.bin",directory);FILE*f=std::fopen(path,"rb");if(f){ok=world_combat_opponent_metadata::read_pivots(f,pivots);std::fclose(f);}else if(opponent_policy)ok=false;else for(unsigned i=0;i<poses;++i)pivots[i]=world_combat_physical_grips::wrist(0,i,p->sockets[i]);}
 if(ok){std::snprintf(path,sizeof path,"%s/combat_body_000.glb",directory);p->body.assets[0]=world_model_load_pose_support_pnt_budgeted(path,WORLD_MODEL_POSE_BLEND_VERSION,true,max_texture_edge);ok=p->body.assets[0]&&world_model_pose_mesh_count(p->body.assets[0])==p->compact->packet.meshes().size()&&world_model_pose_vertices(p->body.assets[0])==p->compact->packet.vertices();}
 if(ok){std::snprintf(path,sizeof path,"%s/combat_body_000.glb",directory);ok=compact_surface_prepare(*p->compact,path,pivots);}
 if(ok){p->body.blend[0]=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);ok=p->body.blend[0]&&world_model_pose_blend_set_targets(p->body.blend[0],p->body.assets[0],p->body.assets[0]);}
 for(unsigned slot=0;ok&&slot<p->batch_count;++slot){const auto pair=pose_pair(slot,poses);ok=p->compact->packet.rigid(pair[0],pair[1],pivots[pair[0]].data(),pivots[pair[1]].data(),p->compact->rigid[slot],&p->body.rigid[slot]);if(ok)++p->stats.rigid_weapon_pairs;}
 if(ok){p->measured_pivots=poses;p->stats.pose_assets=1;p->stats.reaction_assets=40;p->stats.death_assets=24;p->stats.combo_batches=25;p->weapon_sockets=true;ok=world_combat_motion_load_anatomy(p,anatomy_directory)&&world_combat_motion_prepare_crowd(p);}
 if(!ok){if(error&&capacity)std::snprintf(error,capacity,"Exact compact pose packet, physical weapon, original anatomy or actor stream contract rejected");world_combat_motion_free(p);return nullptr;}
 if(actor_mask&1){auto &c=*p->compact;c.arms=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);std::snprintf(path,sizeof path,"%s/first_person_indices.bin",directory);const WorldModelInstance seed={0,0,0,0,1};const float zero=0;
  if(!c.arms||!world_model_pose_blend_set_targets(c.arms,p->body.assets[0],p->body.assets[0])||!world_model_pose_blend_attach_pose_streams(c.arms,c.actors[0].stream,c.actors[0].next_stream)||!world_model_pose_blend_load_index_subset(c.arms,path)||!world_model_pose_blend_upload(c.arms,&seed,&zero,1)){if(error&&capacity)std::snprintf(error,capacity,"Selected player's exact source forearm/hand triangle subset is missing or invalid");world_combat_motion_free(p);return nullptr;}c.stats.first_person_ready=true;
 }
 if((actor_mask&1)&&p->compact->packet.pose_count()>=204){p->compact->unarmed_rigid.resize(p->compact->packet.meshes().size());p->unarmed=new(std::nothrow)Unarmed;if(!p->unarmed){world_combat_motion_free(p);return nullptr;}}
 auto &c=*p->compact;c.stats.loaded=true;c.stats.actor_mask=actor_mask;c.stats.logical_poses=poses;c.stats.packet_poses=c.packet.pose_count();c.stats.resident_body_assets=1;c.stats.rigid_pairs=p->stats.rigid_weapon_pairs;c.stats.mapped_bytes=c.packet.mapped_bytes();
 c.stats.base_geometry_bytes=world_model_geometry_bytes(p->body.assets[0]);for(auto *cap:p->anatomy->cap_assets)c.stats.cap_geometry_bytes+=world_model_geometry_bytes(cap);
 c.stats.anatomy_gpu_bytes=std::uint64_t(c.packet.vertices())*8*sizeof(float);c.stats.death_gpu_bytes=p->deaths?p->deaths->stats.extra_gpu_bytes:0;
 c.stats.gpu_bytes+=c.stats.base_geometry_bytes+c.stats.cap_geometry_bytes+c.stats.anatomy_gpu_bytes+c.stats.death_gpu_bytes;
 c.stats.staging_bytes+=p->deaths?p->deaths->stats.staging_cpu_bytes:0;
 c.stats.staging_bytes+=c.surface_roles.capacity()*sizeof(world_combat_weapon_surface::Role)+c.surface_trees.capacity()*sizeof(world_combat_weapon_surface::Tree);for(const auto&tree:c.surface_trees)c.stats.staging_bytes+=tree.order.capacity()*sizeof(unsigned)+tree.nodes.capacity()*sizeof(world_combat_weapon_surface::Node);for(const auto&row:c.surface_rigid)c.stats.staging_bytes+=row.capacity()*sizeof(WorldModelRigidWeaponPose);
 std::printf("[Compact combat] Ready: %u exact poses, 1 body, actor mask%u, mapped%lluB, actor/piece stream%lluB\n",poses,actor_mask,c.stats.mapped_bytes,c.stats.gpu_bytes);return p;
}
WorldCombatCompactStats world_combat_motion_compact_stats(const WorldCombatMotion*p){return p&&p->compact?p->compact->stats:WorldCombatCompactStats{};}
bool world_combat_motion_set_actor_mask(WorldCombatMotion*p,unsigned mask){if(!p||(mask&~world_combat_capacity::actor_mask)||(p->compact&&mask!=p->actor_mask))return false;p->actor_mask=mask;for(unsigned who=0;who<actor_capacity;++who)if(!(mask&(1u<<who)))p->actors[who].active=false;return true;}
unsigned world_combat_motion_actor_mask(const WorldCombatMotion*p){return p?p->actor_mask:0;}
bool world_combat_motion_opponents_ready(const WorldCombatMotion *p){return p&&(p->opponents_only||p->compact)&&p->stats.polished&&p->pose_count==poses&&(p->compact?p->compact->stats.logical_poses==poses:p->stats.pose_assets==poses)&&p->measured_pivots==poses&&p->stats.rigid_weapon_pairs==(p->compact?batches:batches)&&p->anatomy&&p->anatomy->stats.loaded&&p->crowd_ready;}
WorldCombatOpponentBankStats world_combat_motion_opponents_stats(const WorldCombatMotion *p){
    if(!p)return {};
    return {p->opponents_only||bool(p->compact),world_combat_motion_opponents_ready(p),p->pose_count,p->opponents_only||p->compact?0u:p->pose_count,p->measured_pivots,p->stats.rigid_weapon_pairs,p->opponent_stride_metres,p->opponent_body_radius_metres,p->opponent_body_height_metres};
}
bool world_combat_motion_set_opponent_stride(WorldCombatMotion *p,double metres){
    if(!p||!p->opponents_only||!std::isfinite(metres)||metres<.25||metres>4.)return false;
    p->opponent_stride_metres=metres;return true;
}
void world_combat_motion_reset_arena_opponents(WorldCombatMotion *p){
    if(!p)return;
    if(p->compact)p->compact->bow={};
    // The original human bank also owns the player. Clear only NPC slots so a
    // roster handoff never edits the player's weapon/animation ownership.
    for(unsigned who=1;who<actor_capacity;++who){p->actors[who]={};p->instances[who]={};p->weights[who]=0;}
    if(p->opponents_only){p->actors[0]={};p->instances[0]={};p->weights[0]=0;p->initialized=false;p->clock=0;}
    p->crowd_counts={};p->stats.actors=p->actors[0].active?1:0;p->stats.active_enemies=p->stats.crowd_instances=0;
    death_reset(p);
    if(p->fatalities){p->fatalities->state.reset();p->fatalities->body_counts={};p->fatalities->crowd_counts={};p->fatalities->head_counts={};}
    if(p->anatomy){auto &g=*p->anatomy;g.masks={};g.mirrored={};g.pieces={};g.contact={};g.crowd={};g.crowd_count=0;g.support_pool.clear();
        g.stats.active={};g.stats.settled={};g.stats.stumps=0;g.stats.support_points_active=0;}
}
bool world_combat_motion_load_loadouts(WorldCombatMotion *p,const char *directory){
    WorldModelMaterialBatch material_batch_scope;
 if(!p||p->opponents_only||!p->stats.polished||!directory||p->loadouts)return false;
 auto *out=new(std::nothrow)Loadouts;if(!out)return false;
 const char *names[]={"sword_shield","great_axe","staff"};
 for(unsigned w=0;w<3;++w){auto &entry=out->entries[w];char dir[1200];std::snprintf(dir,sizeof dir,"%s/%s",directory,names[w]);
  if(!load_presentation(entry.presentation,dir)){free_loadouts(out);return false;}
  WorldCombatMotion socket_reader;if(!load_sockets(&socket_reader,dir,false)){free_loadouts(out);return false;}std::memcpy(entry.sockets,socket_reader.sockets,sizeof entry.sockets);
  for(unsigned kind=0;kind<2;++kind){auto &b=kind?entry.arms:entry.body;
   for(unsigned i=0;i<poses;++i){char path[1400];std::snprintf(path,sizeof path,"%s/combat_%s_%03u.glb",dir,kind?"arms":"body",i);b.assets[i]=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION);if(!b.assets[i]){free_loadouts(out);return false;}}
   auto link=[&](unsigned slot,unsigned from,unsigned to){auto *&batch=b.blend[slot];batch=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);return batch&&world_model_pose_blend_set_targets(batch,b.assets[from],b.assets[to]);};
   for(const auto &c:clips)for(unsigned i=0;i<c.count;++i)if(!link(c.base+i,c.base+i,c.base+(c.loop?(i+1)%c.count:std::min(i+1,c.count-1)))){free_loadouts(out);return false;}
   for(unsigned i=0;i<3;++i)if(!link(poses+i,i==0?0:i==1?4:8,i==0?4:0)){free_loadouts(out);return false;}
   for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to)if(!link(poses+3+from*5+to,12+12*from+8,12+12*to)){free_loadouts(out);return false;}
   for(unsigned direction=0;direction<entry_batches;++direction)if(!link(poses+3+combo_batches+direction,0,12+12*direction)){free_loadouts(out);return false;}
   // The shield's authored directional-hit returns do not admit a safe
   // vertex-morph bridge to neutral: retain their original curve rather than
   // folding the hand/gauntlet triangles. Generic/parry returns remain safe.
   for(unsigned i=0;i<settle_batches;++i)if(!(w==0&&i>=7)&&!link(poses+settle_offset+i,settle_endpoint(i),0)){free_loadouts(out);return false;}
  }
 }
 for(unsigned weapon=0;weapon<out->entries.size();++weapon){auto &entry=out->entries[weapon];
  p->stats.rigid_weapon_pairs+=rigid_bank(entry.body,entry.sockets,poses,batches,false,weapon+1)+rigid_bank(entry.arms,entry.sockets,poses,batches,true,weapon+1);
  release_rigid_bank_source(entry.body);release_rigid_bank_source(entry.arms);
 }
 std::printf("[Combat motion] Rigid weapon interpolation including loadouts: %u certified body/arm pairs\n",p->stats.rigid_weapon_pairs);
 p->loadouts=out;return true;
}
bool world_combat_motion_loadouts_ready(const WorldCombatMotion *p){return p&&(p->loadouts||(p->compact&&!p->opponents_only));}
bool world_combat_motion_bow_pose(WorldCombatMotion *p,const WorldArenaBowPose &bow){
 if(!p||!p->compact||!(p->actor_mask&1))return !bow.visible;
 auto &current=p->compact->bow;current={};if(!bow.visible)return true;
 if(p->compact->packet.pose_count()!=210||!std::isfinite(bow.x)||!std::isfinite(bow.y)||!std::isfinite(bow.z)||!std::isfinite(bow.yaw)||!std::isfinite(bow.pitch)||!std::isfinite(bow.draw))return false;
 current=bow;current.draw=std::clamp(current.draw,0.f,1.f);return true;
}
void world_combat_motion_draw_bow_actor(WorldCombatMotion *p,unsigned program,double eye_x,double eye_y,double eye_z,bool first_person,bool shadow){
 if(!p||!p->compact||!program||!(p->actor_mask&1)||p->actors[0].defeated||!p->actors[0].active)return;
 auto &c=*p->compact;const auto &bow=c.bow;if(!bow.visible||first_person!=bow.first_person||(shadow&&first_person))return;
 auto &v=c.actors[0];if(!compact_bow_commit(p,v,first_person))return;
 compact_uniforms(p,program,eye_x,eye_y,eye_z,p->x,p->z,shadow);
 const float cy=std::cos(bow.yaw),sy=std::sin(bow.yaw);
 WorldModelInstance instance={float(bow.x-p->x),float(bow.y),float(bow.z-p->z),3.14159265359f-bow.yaw,first_person?.8f:1.f};
 if(first_person){instance.x+=-.30f*cy+.35f*sy;instance.y-=1.40f;instance.z+=-.30f*sy-.35f*cy;
  if(!c.arms||!world_model_pose_blend_share_rigid(c.arms,v.body)||!world_model_pose_blend_upload(c.arms,&instance,&v.weight,1)){++c.stats.failures;return;}
  world_model_pose_blend_draw_equipped(c.arms,program,p->equipment,1);
 }else{WorldModelAnatomyDraw body={};body.x=instance.x;body.y=instance.y;body.z=instance.z;body.yaw=instance.yaw;body.scale=instance.scale;body.weight=v.weight;body.hide_weapon=true;world_model_pose_blend_draw_anatomy(v.body,program,&body);}
 glUniform1f(p->locations(program).reactive,0);
}
unsigned world_combat_motion_loadout(const WorldCombatMotion *p){return p?p->loadout:0;}
bool world_combat_motion_load_fatalities(WorldCombatMotion *p,const char *dir){
    WorldModelMaterialBatch material_batch_scope;
 if(!p||p->opponents_only||!p->stats.polished||!dir||p->fatalities)return false;
 auto *g=new(std::nothrow)Fatalities;if(!g)return false;
 for(unsigned i=0;i<48;++i){char path[1200];std::snprintf(path,sizeof path,"%s/fatality_%s_%03u.glb",dir,i<24?"body":"head",i%24);
  g->assets[i]=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION);if(!g->assets[i]){free_fatalities(g);return false;}}
 for(unsigned i=0;i<24;++i)for(unsigned kind=0;kind<3;++kind){auto &batch=kind==0?g->body[i]:kind==1?g->crowd[i]:g->heads[i];
  batch=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);const unsigned a=kind==2?24+i:i,b=kind==2?24+(i+1)%24:std::min(i+1,23u);
  if(!batch||!world_model_pose_blend_set_targets(batch,g->assets[a],g->assets[b])){free_fatalities(g);return false;}}
 p->fatalities=g;return true;
}
WorldCombatFatalityStats world_combat_motion_fatality_stats(const WorldCombatMotion *p){
 if(!p||!p->fatalities)return {};const auto &s=p->fatalities->state;unsigned severed=0;for(bool v:s.severed)severed+=v;unsigned settled=0;for(const auto &h:s.heads)settled+=h.active&&h.settled;return {s.launches,s.active(),severed,s.peak,settled};
}
void world_combat_motion_fatality_ground(WorldCombatMotion *p,double (*fn)(double,double,void*),void *context){if(p){p->fatality_floor=fn;p->fatality_floor_context=context;}}
bool world_combat_motion_neck_emitter(const WorldCombatMotion *p,unsigned actor,double point[3],double axis[3],float collapse,float yaw){
 if(!p||!p->fatalities||!point||!axis||!actor||actor>=actor_capacity||!std::isfinite(collapse)||!std::isfinite(yaw))return false;
 const auto &a=p->actors[actor];const auto &v=p->instances[actor];unsigned frame=0;double w=0;
 if(collapse>=0){const double f=std::clamp(double(collapse),0.,1.)*23;frame=unsigned(f);w=f-frame;}
 else{frame=a.slot>=death_base&&a.slot<poses?a.slot-death_base:0;w=a.slot>=death_base&&a.slot<poses?p->weights[actor]:0;yaw=v.yaw;}
 const double cs=std::cos(yaw),sn=std::sin(yaw);double local[3],up[3];
 for(unsigned k=0;k<3;++k){local[k]=fatality_necks[frame][k]+(fatality_necks[std::min(frame+1,23u)][k]-fatality_necks[frame][k])*w;up[k]=fatality_neck_axes[frame][k]+(fatality_neck_axes[std::min(frame+1,23u)][k]-fatality_neck_axes[frame][k])*w;}
 point[0]=p->x+v.x+cs*local[0]+sn*local[2];point[1]=v.y+local[1];point[2]=p->z+v.z-sn*local[0]+cs*local[2];
 const double n=std::max(1e-8,std::hypot(up[0],up[1],up[2]));axis[0]=(cs*up[0]+sn*up[2])/n;axis[1]=up[1]/n;axis[2]=(-sn*up[0]+cs*up[2])/n;return true;
}
bool world_combat_motion_neck(const WorldCombatMotion *p,unsigned actor,double point[3]){
 if(!p||!p->fatalities||!point||!actor||actor>=actor_capacity)return false;
 const auto &a=p->actors[actor];const unsigned frame=a.slot>=death_base&&a.slot<poses?a.slot-death_base:0;
 const auto &v=p->instances[actor];const double w=p->weights[actor],cs=std::cos(v.yaw),sn=std::sin(v.yaw);double local[3];
 for(unsigned k=0;k<3;++k)local[k]=fatality_necks[frame][k]+(fatality_necks[std::min(frame+1,23u)][k]-fatality_necks[frame][k])*w;
 point[0]=p->x+v.x+cs*local[0]+sn*local[2];point[1]=v.y+local[1];point[2]=p->z+v.z-sn*local[0]+cs*local[2];return true;
}
bool world_combat_motion_load_unarmed(WorldCombatMotion *p,const char *dir){
    WorldModelMaterialBatch material_batch_scope;
 if(!p||p->opponents_only||!dir||p->unarmed)return false;
 auto *b=new(std::nothrow)Unarmed;if(!b)return false;
 auto cleanup=[&](){for(auto *v:b->blends)if(v)world_model_pose_blend_free(v);for(auto *a:b->assets)if(a)world_model_free(a);delete b;};
 for(unsigned i=0;i<32;++i){char path[1200];std::snprintf(path,sizeof path,"%s/unarmed_%s_%03u.glb",dir,i<16?"idle":"walk",i%16);b->assets[i]=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION);if(!b->assets[i]){cleanup();return false;}}
 for(unsigned i=0;i<96;++i){const unsigned a=i<32?i:(i-32)/2,to=i<32?a/16:(i-32)%2,next=i<32?to*16+(a+1)%16:to*16+a%16;
  b->blends[i]=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);if(!b->blends[i]||!world_model_pose_blend_set_targets(b->blends[i],b->assets[a],b->assets[next])){cleanup();return false;}}
 p->unarmed=b;return true;
}
bool world_combat_motion_unarmed_ready(const WorldCombatMotion *p){return p&&p->unarmed;}
void world_combat_motion_unarmed(WorldCombatMotion *p,bool active,float dt,float travel){
 if(!p||!p->unarmed)return;auto &b=*p->unarmed;b.active=active&&!p->actors[0].defeated;
 if(!b.active)return;
 if(std::isfinite(dt)&&dt>0){dt=std::min(dt,.05f);const bool moving=p->locomotion?p->locomotion->state.moving:travel/dt>.035f;const unsigned mode=moving?1:0;
  if(b.transition>=1&&!b.finishing&&mode!=b.mode){b.finishing=true;b.finish_from=b.phase;b.finish_to=std::ceil(b.phase*16-1.e-6f)/16;b.finish_time=0;}
  if(b.finishing){
   // Finish the currently displayed A/B segment before using a preloaded
   // single-key mode handoff; dropping its fractional sample caused a pop.
   b.finish_time=std::min(.06f,b.finish_time+dt);b.phase=world_combat_locomotion::wrapped(b.finish_from+(b.finish_to-b.finish_from)*smooth(b.finish_time/.06f));
   const float f=b.phase*16;b.slot=b.mode*16+unsigned(f)%16;b.weight=f-std::floor(f);
   if(b.finish_time>=.06f){b.finishing=false;if(mode!=b.mode){b.from=b.mode*16+unsigned(std::lround(b.phase*16))%16;b.mode=mode;b.transition=0;}}
  }else if(b.transition<1){
   const bool toward=mode==b.mode;b.transition=std::clamp(b.transition+(toward?dt:-dt)/.18f,0.f,1.f);b.slot=32+b.from*2+b.mode;b.weight=smooth(b.transition);
   if(b.transition<=0&&!toward){b.mode=b.from/16;b.phase=float(b.from%16)/16;b.transition=1;}
   else if(b.transition>=1)b.phase=float(b.from%16)/16;
  }else{
   if(moving&&p->locomotion)b.phase=world_combat_locomotion::follow_phase(b.phase,p->locomotion->state.phase,p->locomotion->state.phase_step,dt);
   else b.phase=world_combat_locomotion::wrapped(b.phase+(moving?std::clamp(travel,0.f,1.f)/1.2f:dt/3.f));
   const float f=b.phase*16;b.slot=b.mode*16+unsigned(f)%16;b.weight=f-std::floor(f);
  }
 }
 if(p->compact)return;
 world_model_pose_blend_upload(b.blends[b.slot],&p->instances[0],&b.weight,1);
 if(p->locomotion)commit_locomotion(p,4);
}
bool world_combat_motion_load_aquatics(WorldCombatMotion *p,const char *directory,char *error,std::size_t capacity){
    WorldModelMaterialBatch material_batch_scope;
    if(error&&capacity)error[0]=0;
    if(!p||p->opponents_only||!directory||!*directory)return false;
    if(p->aquatics)return true;
    auto *a=new(std::nothrow)Aquatics;if(!a)return false;
    for(unsigned kind=0;kind<2;++kind){auto &bank=kind?a->arms:a->body;
        for(unsigned i=0;i<aquatic_poses;++i){char path[1200];const int n=std::snprintf(path,sizeof path,"%s/aquatic_%s_%03u.glb",directory,kind?"arms":"body",i);
            if(n<0||std::size_t(n)>=sizeof path||!(bank.assets[i]=world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION)))goto fail;
        }
        for(unsigned i=0;i<aquatic_poses;++i){const unsigned mode=i/world_aquatic_animation::frames,next=mode*world_aquatic_animation::frames+(i+1)%world_aquatic_animation::frames;
            auto *&loop=bank.blends[i];loop=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!loop||!world_model_pose_blend_set_targets(loop,bank.assets[i],bank.assets[next]))goto fail;
            // All handoff VAOs share the already loaded pose buffers/images.
            // No target reconfiguration or asset allocation during gameplay.
            for(unsigned to=0;to<world_aquatic_animation::modes;++to){auto *&blend=bank.blends[aquatic_poses+i*world_aquatic_animation::modes+to];blend=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
                if(!blend||!world_model_pose_blend_set_targets(blend,bank.assets[i],bank.assets[to*world_aquatic_animation::frames+i%world_aquatic_animation::frames]))goto fail;}
        }
    }
    p->aquatics=a;p->stats.aquatic_loaded=true;p->stats.aquatic_pose_assets=aquatic_poses*2;return true;
fail:
    free_aquatics(a);if(error&&capacity)std::snprintf(error,capacity,"Dedicated aquatic hero pose bank missing or incompatible");return false;
}
void world_combat_motion_aquatics(WorldCombatMotion *p,bool swimming,bool underwater,bool moving,float dt,bool paused){
    if(!p||!p->aquatics)return;
    auto &a=*p->aquatics;const auto sample=a.state.update(swimming,underwater,moving,dt,paused);
    p->stats.aquatic_active=sample.active;p->stats.aquatic_mode=unsigned(sample.mode);p->stats.aquatic_phase=sample.phase;
    if(!sample.active)return;
    a.slot=sample.transition?aquatic_poses+sample.from*world_aquatic_animation::modes+unsigned(sample.mode):sample.from;
    if(a.slot>=aquatic_batches)return;
    const WorldModelInstance body=p->instances[0],arms={0,-1.5f,.05f,3.14159265359f,.80f};
    if(world_model_pose_blend_upload(a.body.blends[a.slot],&body,&sample.weight,1)&&world_model_pose_blend_upload(a.arms.blends[a.slot],&arms,&sample.weight,1)){
        ++p->stats.aquatic_updates;p->stats.aquatic_uploads+=2;
    }
}
void world_combat_motion_arena(WorldCombatMotion *p,double x,double z,float feet){if(p&&std::isfinite(x)&&std::isfinite(z)&&std::isfinite(feet)){p->arena_x=x;p->arena_z=z;p->arena_feet=feet;p->custom_arena=true;}}
void world_combat_motion_arena_enemy_walk(WorldCombatMotion *p,bool walking){if(p)p->arena_enemy_walk=walking;}
void world_combat_motion_restore_defeated(WorldCombatMotion *p,unsigned actor){
    if(!p||!actor||actor>=actor_capacity)return;
    auto &a=p->actors[actor];a.defeated=true;a.death_time=world_combat_motion::death_duration;
    a.hit=a.parry=a.guard=0;a.walk=0;a.attacking=false;a.combo=false;
    if(p->deaths){auto &v=p->deaths->actors[actor];v.pose={};v.pose.valid=true;v.pose.slot=171;v.pose.phase=v.pose.alpha=1;v.pose.yaw=a.facing_yaw;v.elapsed=world_combat_motion::death_duration;v.uploaded=v.crowd_override=false;}
}

bool world_combat_motion_prepare_crowd(WorldCombatMotion *p){
    if(!p)return false;
    if(p->crowd_ready)return true;
    if(p->compact){p->crowd_ready=true;return true;}
    const unsigned count=p->stats.polished?death_frames+1:1;
    for(unsigned i=0;i<count;i++){
        const unsigned from=i?death_base+i-1:0,to=i?std::min(from+1,poses-1):(p->stats.polished?0:98);
        auto *batch=world_model_pose_blend_create(npc_capacity,WORLD_MODEL_POSE_BLEND_VERSION);p->arena_crowd[i]=batch;
        if(!batch||!world_model_pose_blend_set_targets(batch,p->body.assets[from],p->body.assets[to])){
            for(auto *&b:p->arena_crowd){if(b)world_model_pose_blend_free(b);b=nullptr;}return false;
        }
    }
    p->crowd_ready=true;return true;
}
bool world_combat_motion_crowd(WorldCombatMotion *p,const WorldCombatCrowdInstance *crowd,unsigned count){
    if(!p||!p->crowd_ready||count>npc_capacity||(count&&!crowd))return false;
    if(p->compact)return compact_crowd(p,crowd,count);
    for(unsigned i=0;i<count;++i)if(!std::isfinite(crowd[i].x)||!std::isfinite(crowd[i].y)||!std::isfinite(crowd[i].z)||!std::isfinite(crowd[i].yaw)||!std::isfinite(crowd[i].defeat_phase)||std::abs(crowd[i].x)>1.e9||std::abs(crowd[i].y)>1.e9||std::abs(crowd[i].z)>1.e9)return false;
    if(p->variant_stats.loaded){unsigned ids=0;for(unsigned i=0;i<count;++i){const unsigned id=crowd[i].actor_id;
        if(!id||id>=actor_capacity||(ids&(1u<<id)))return false;ids|=1u<<id;
    }}
    const double x=count?std::floor(crowd[0].x/128)*128:0,z=count?std::floor(crowd[0].z/128)*128:0;
    std::array<std::array<WorldModelInstance,npc_capacity>,death_frames> cut_instances={};std::array<std::array<float,npc_capacity>,death_frames> cut_weights={};std::array<unsigned,death_frames> cut_counts={};
    std::array<std::array<WorldModelInstance,npc_capacity>,death_frames+1> instances={};std::array<std::array<float,npc_capacity>,death_frames+1> weights={};std::array<unsigned,death_frames+1> counts={};unsigned fallen=0;
    if(p->anatomy){p->anatomy->crowd_count=count;std::copy_n(crowd,count,p->anatomy->crowd.begin());}
    if(p->deaths)for(auto &v:p->deaths->actors)v.crowd_override=false;
    for(unsigned i=0;i<count;++i){
        unsigned group=0;float weight=crowd[i].yielded?1.f:0.f;
        if(crowd[i].yielded){fallen++;if(p->stats.polished){const float f=std::clamp(crowd[i].defeat_phase,0.f,1.f)*(death_frames-1);const unsigned key=std::min(death_frames-1,unsigned(f));group=key+1;weight=f-key;}}
        const unsigned actor=crowd[i].actor_id;
        if(crowd[i].yielded&&actor>0&&actor<actor_capacity&&p->deaths&&p->deaths->actors[actor].pose.valid){death_tick(p,actor,std::clamp(crowd[i].defeat_phase,0.f,1.f)*world_combat_motion::death_duration);}
        if(crowd[i].yielded&&actor>0&&actor<actor_capacity&&p->deaths&&p->deaths->actors[actor].pose.valid){const auto &s=p->deaths->actors[actor].pose;group=s.slot-148+1;weight=s.weight;}
        if(crowd[i].yielded&&death_active(p,actor))p->deaths->actors[actor].crowd_override=true;
        if(p->anatomy&&(p->variant_stats.loaded||crowd[i].severed_parts||death_active(p,actor)))continue;
        const float corpse_yaw=crowd[i].yielded&&p->deaths&&actor>0&&actor<actor_capacity&&p->deaths->actors[actor].pose.valid?p->deaths->actors[actor].pose.yaw:crowd[i].yaw;
        if(crowd[i].yielded&&crowd[i].decapitated&&p->fatalities&&!p->anatomy){const unsigned g=group-1,at=cut_counts[g]++;cut_instances[g][at]={float(crowd[i].x-x),float(crowd[i].y),float(crowd[i].z-z),corpse_yaw,1};cut_weights[g][at]=weight;continue;}
        const unsigned at=counts[group]++;instances[group][at]={float(crowd[i].x-x),float(crowd[i].y),float(crowd[i].z-z),corpse_yaw,1};weights[group][at]=weight;
    }
    for(unsigned i=0;i<counts.size();i++)if(counts[i]&&!world_model_pose_blend_upload(p->arena_crowd[i],instances[i].data(),weights[i].data(),counts[i]))return false;
    if(p->fatalities){for(unsigned i=0;i<death_frames;++i)if(cut_counts[i]&&!world_model_pose_blend_upload(p->fatalities->crowd[i],cut_instances[i].data(),cut_weights[i].data(),cut_counts[i]))return false;p->fatalities->crowd_counts=cut_counts;}
    p->crowd_counts=counts;p->crowd_x=x;p->crowd_z=z;p->stats.crowd_instances=count;p->stats.fallen_instances=fallen;p->stats.peak_fallen_instances=std::max(p->stats.peak_fallen_instances,fallen);return true;
}
void world_combat_motion_draw_crowd(WorldCombatMotion *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!p->crowd_ready||!program)return;
    if(p->compact){compact_draw_crowd(p,program,x,y,z,shadow);return;}
    if(p->variant_stats.loaded&&!p->anatomy)return;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->crowd_x-x),float(-y),float(p->crowd_z-z));glUniform2f(u.anchor,float(p->crowd_x),float(p->crowd_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop,0,0,0);glUniform1f(u.reactive,shadow?0:.85f);
    for(unsigned i=0;i<p->arena_crowd.size();i++)if(p->crowd_counts[i])world_model_pose_blend_draw(p->arena_crowd[i],program);
    if(p->fatalities)for(unsigned i=0;i<24;++i)if(p->fatalities->crowd_counts[i])world_model_pose_blend_draw(p->fatalities->crowd[i],program);
    if(p->anatomy)for(unsigned i=0;i<p->anatomy->crowd_count;++i){const auto &c=p->anatomy->crowd[i];if(!p->variant_stats.loaded&&!c.severed_parts&&!death_active(p,c.actor_id))continue;
        const unsigned id=c.actor_id>0&&c.actor_id<actor_capacity?c.actor_id:i+1;
        if(c.weapon_hand!=TrainingWeaponHand::None)p->anatomy->mirrored[id]=c.weapon_hand==TrainingWeaponHand::Left;
        float key=c.yielded?std::clamp(c.defeat_phase,0.f,1.f)*23:0;unsigned slot=c.yielded?148+std::min(23u,unsigned(key)):0;
        AnatomySnapshot snapshot={{float(c.x-p->crowd_x),float(c.y),float(c.z-p->crowd_z),c.yaw,1},slot,c.yielded?key-unsigned(key):0,p->anatomy->mirrored[id],true};snapshot.variant_mask=world_model_variants::actor(id);
        if(p->deaths&&p->deaths->actors[id].pose.valid){const auto &s=p->deaths->actors[id].pose;snapshot.instance.yaw=s.yaw;snapshot.slot=s.slot;snapshot.weight=s.weight;}
        if(!death_draw_actor(p,program,id,snapshot,c.severed_parts,c.weapon_hand,true)){anatomy_draw_actor(p,program,snapshot,c.severed_parts,c.weapon_hand);if(p->variant_stats.loaded){++p->variant_stats.body_draws[(id-1)%4];++p->variant_stats.crowd_draws[(id-1)%4];}}
    }
    glUniform1f(u.reactive,0);
}
namespace {
bool valid_motion_combat(const WorldTrainingCombat &c){
    return unsigned(c.selected)<5&&unsigned(c.attack_direction)<5&&unsigned(c.enemy_direction)<5&&unsigned(c.enemy_guard)<5&&
        unsigned(c.player_phase)<=3&&unsigned(c.enemy_phase)<=4&&std::isfinite(c.player_time)&&std::isfinite(c.enemy_time)&&
        std::isfinite(c.player_duration)&&std::isfinite(c.enemy_duration)&&std::isfinite(c.clock)&&std::isfinite(c.impact_pause);
}
bool root_ok(double x,double y,double z){return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::abs(x)<=1.e9&&std::abs(y)<=1.e9&&std::abs(z)<=1.e9;}
void update_motion(WorldCombatMotion *p,const WorldTrainingCombat &player_combat,double x,double feet,double z,float yaw,float dt,
                   const WorldCombatArenaActor *enemies,unsigned count,bool paused){
    if(!p||count>npc_capacity||(count&&!enemies)||!root_ok(x,feet,z)||!std::isfinite(yaw)||!std::isfinite(dt)||!valid_motion_combat(player_combat))return;
    for(unsigned i=0;i<count;++i)if(!root_ok(enemies[i].x,enemies[i].y,enemies[i].z)||!valid_motion_combat(enemies[i].combat)||(enemies[i].facing_target_valid&&!root_ok(enemies[i].facing_target_x,0,enemies[i].facing_target_z)))return;
    // New runs reset their real shared clock; stable actor IDs must not inherit
    // the previous run's reaction/collapse/serial ownership.
    if(p->initialized&&player_combat.clock+1.e-6<p->actors[0].last_clock){p->actors={};p->initialized=false;death_reset(p);if(p->fatalities)p->fatalities->state.reset();if(p->anatomy){p->anatomy->masks={};p->anatomy->mirrored={};p->anatomy->pieces={};p->anatomy->contact={};p->anatomy->support_pool.clear();}}
    if(p->initialized&&paused)return;
    bool contact=player_combat.enemy_hits>p->actors[0].enemy_hits||player_combat.blocks>p->actors[0].blocks||player_combat.parries>p->actors[0].parries||
        (player_combat.resolved_attack_serial&&player_combat.resolved_attack_serial!=p->actors[0].resolved_serial)||
        (player_combat.state==TrainingState::Lost&&!p->actors[0].defeated);
    for(unsigned i=0;i<count;++i){if(!(p->actor_mask&(1u<<(i+1))))continue;const auto &c=enemies[i].combat;const auto &a=p->actors[i+1];
        contact|=c.hits>a.hits||c.parries>a.parries||c.guarded>a.guarded||c.blocks>a.blocks||
            (c.resolved_attack_serial&&c.resolved_attack_serial!=a.steel_serial&&c.resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock)||(c.state==TrainingState::Won&&!a.defeated);
    }
    // Commit all newly published contacts once before their common hit-stop.
    if(p->initialized&&player_combat.impact_pause>0&&!contact)return;
    p->loadout=p->loadouts&&unsigned(player_combat.weapon)>=2&&unsigned(player_combat.weapon)<5?unsigned(player_combat.weapon):0;
    p->grip_weapon_allowed=player_combat.weapon!=world_weapon_loadout::Weapon::Bow;
    dt=paused||player_combat.impact_pause>0?0.f:std::clamp(dt,0.f,.05f);p->clock+=dt;
    const double moved_x=p->initialized?x-p->last_x:0,moved_z=p->initialized?z-p->last_z:0;
    double travel=std::hypot(moved_x,moved_z);
    const bool pose_cut=!p->initialized||travel>10;p->last_x=x;p->last_z=z;p->initialized=true;if(travel>1)travel=0;
    if(p->locomotion){auto &g=*p->locomotion;
        const bool body_contact=player_combat.enemy_hits>p->actors[0].enemy_hits;
        const bool enabled=!body_contact&&player_combat.state!=TrainingState::Lost&&p->actors[0].hit<=0&&p->actors[0].parry<=0&&!p->stats.aquatic_active;
        // New damage is published before its hit-stop. Do not retain the old
        // full-weight walking legs over the planted full-body impact pose.
        if(body_contact){g.state.weight=0;g.state.moving=false;++p->stats.reaction_gait_interrupts;}
        g.state.update(moved_x,moved_z,yaw,dt,pose_cut,enabled,g.strides);
        p->stats.locomotion_phase=g.state.phase;p->stats.locomotion_weight=g.state.weight;p->stats.locomotion_speed=g.state.speed;
        p->stats.locomotion_direction=g.state.direction;
    }
    anatomy_update(p,enemies,count,dt);
    p->x=std::floor(x/128)*128;p->z=std::floor(z/128)*128;
    p->stats.actors=!p->opponents_only&&(p->actor_mask&1)?1:0;p->stats.active_enemies=0;
    const float reaction_duration=p->stats.polished?world_combat_motion::reaction_duration:.28f;
    for(unsigned who=0;who<actor_capacity;++who){
        auto &a=p->actors[who];
        const Actor prior_actor=a;const unsigned prior_slot=a.slot;const float prior_weight=p->weights[who],prior_yaw=p->instances[who].yaw;const double prior_clock=a.last_clock;
        if(who>count||!(p->actor_mask&(1u<<who))){a.active=false;if(!who){a.last_clock=player_combat.clock;a.enemy_hits=player_combat.enemy_hits;a.blocks=player_combat.blocks;a.parries=player_combat.parries;a.resolved_serial=player_combat.resolved_attack_serial;a.defeated=player_combat.state==TrainingState::Lost;}continue;}
        const auto &c=who?enemies[who-1].combat:player_combat;
        if(!who&&p->opponents_only){a.active=false;a.last_clock=c.clock;a.enemy_hits=c.enemy_hits;a.blocks=c.blocks;a.defeated=c.state==TrainingState::Lost;continue;}
        const bool mirrored=who&&p->anatomy&&p->anatomy->mirrored[who];
        auto direction=[&](unsigned d){return anatomy_direction(d,mirrored);};
        const bool walking=who&&enemies[who-1].walking;
        a.walking=walking;
        const double ax=who?enemies[who-1].x:x,ay=who?enemies[who-1].y:feet,az=who?enemies[who-1].z:z;
        const bool visible=!who||enemies[who-1].visible;
        const bool local_cut=pose_cut||!a.root_ready||std::hypot(ax-a.last_x,az-a.last_z)>1;
        const double moved=a.root_ready&&!local_cut?std::hypot(ax-a.last_x,az-a.last_z):0;
        a.last_x=ax;a.last_z=az;a.root_ready=true;a.last_clock=c.clock;
        if(who){
            if(local_cut)a.phase=0;
            else if(walking&&!p->stats.polished){a.phase+=float(moved/(p->opponents_only?p->opponent_stride_metres*.30:1.3));a.phase-=std::floor(a.phase);}
            const float facing=float(std::atan2((enemies[who-1].facing_target_valid?enemies[who-1].facing_target_x:x)-ax,(enemies[who-1].facing_target_valid?enemies[who-1].facing_target_z:z)-az));
            if(!a.facing_ready||local_cut){a.facing_yaw=facing;a.facing_ready=true;}
            // A committed in-place cut or full-body reaction cannot swivel
            // planted boots to chase the player. This is the authoritative yaw
            // queried for the actual rendered blade and anatomical contact.
            const bool planted=p->stats.polished&&(a.hit>0||a.parry>0||c.hits>a.hits||c.parries>a.parries||c.guarded>a.guarded||
                (c.engaged()&&(c.enemy_phase==TrainingEnemyPhase::Windup||c.enemy_phase==TrainingEnemyPhase::Strike||c.enemy_phase==TrainingEnemyPhase::Recovery||c.enemy_phase==TrainingEnemyPhase::Stagger)));
            if(!local_cut&&a.facing_ready&&!a.defeated&&!planted)a.facing_yaw+=std::clamp(float(std::remainder(double(facing-a.facing_yaw),6.28318530718)),-2.8f*dt,2.8f*dt);
            a.facing_yaw=float(std::remainder(double(a.facing_yaw),6.28318530718));
        }
        p->instances[who]={float(ax-p->x),float(ay),float(az-p->z),who?a.facing_yaw:yaw,1};
        const bool body_contact=(!who&&c.enemy_hits>a.enemy_hits)||(who&&c.hits>a.hits);
        const bool blocked=(!who&&(c.blocks>a.blocks||c.parries>a.parries))||(who&&c.guarded>a.guarded);
        const bool attack_stopped=who&&p->compact&&c.resolved_attack_serial&&c.resolved_attack_serial!=a.steel_serial&&c.resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock&&std::isfinite(c.resolved_attack_time)&&std::isfinite(c.resolved_attack_phase)&&c.resolved_attack_phase>=0&&c.resolved_attack_phase<=1;
        if(attack_stopped){a.steel_serial=c.resolved_attack_serial;a.steel_started=c.resolved_attack_time;a.steel_phase=float(c.resolved_attack_phase);a.steel_turn=a.turn;a.steel_direction=direction(unsigned(c.enemy_direction));a.steel_rebound=true;a.steel_pose_slot=c.resolved_attack_pose_slot;a.steel_pose_weight=float(c.resolved_attack_pose_weight);a.steel_retreat=0;clash_prepare_actor(p,a);a.guard=0;a.guard_idle_entry=false;a.entry_origin={};a.combo=false;}
        if(a.steel_rebound&&(!c.engaged()||(c.enemy_phase==TrainingEnemyPhase::Windup&&c.attack_start_serial!=a.steel_serial)||(c.clock-a.steel_started>=opponent_steel_seconds&&c.enemy_phase!=TrainingEnemyPhase::Recovery)))a.steel_rebound=false;
        const bool was_reacting=a.hit>0||a.parry>0;
        if(a.reaction_bridge&&c.clock-a.reaction_started>=npc_reaction_seconds)a.reaction_bridge=false;
        if(body_contact){
            const bool compact_npc=who&&p->compact;
            const bool safe_window=compact_npc&&npc_reaction_window(c);
            const bool frozen=safe_window&&npc_reaction_capture(p,who,prior_actor,prior_slot,prior_weight,prior_clock);
            if(safe_window&&!frozen)++p->compact->stats.failures;
            if(frozen){a.reaction_bridge=true;a.reaction_started=c.clock;a.steel_rebound=false;a.reaction_entry=false;a.hit=npc_reaction_seconds;a.reaction_direction=unsigned(c.attack_direction);}
            else if(compact_npc){
                // An unstaggered strike, guard or clash remains exactly on its
                // authored physical path. A late repeat keeps an existing safe
                // reaction's original deadline instead of extending it.
                if(!a.reaction_bridge)a.hit=0;
            }else{
                a.hit=reaction_duration;a.reaction_direction=unsigned(who?c.attack_direction:c.enemy_direction);
                if(p->opponents_only&&who){
                    a.reaction_guard=guard_sample(prior_slot,prior_weight,a.reaction_guard_direction,a.reaction_from_phase);
                    a.reaction_idle=prior_slot<4;
                    if(a.reaction_idle)a.reaction_from_phase=std::clamp((float(prior_slot)+prior_weight)/3.f,0.f,1.f);
                    a.reaction_origin=entry_origin(prior_slot,prior_weight,p->pose_count,false);
                    a.reaction_entry=a.reaction_guard||a.reaction_idle||bool(a.reaction_origin);
                    if(a.reaction_entry)a.hit+=dt;
                }
            }
            if(!compact_npc||frozen){a.guard_recoil=false;a.entry_origin={};if(p->stats.polished)++p->stats.directional_reactions;}
        }
        if(who&&c.parries>a.parries&&!a.steel_rebound){a.parry=a.parry_duration=.30f;a.guard_recoil=false;}
        if(blocked){
            a.parry=a.parry_duration=who?.30f:.20f;
            unsigned direction=0;float phase=0;
            a.guard_recoil=p->stats.polished&&!body_contact&&a.hit<=0&&guard_sample(a.slot,p->weights[who],direction,phase);
            a.guard_recoil_counted=false;
            if(a.guard_recoil){a.guard_direction=float(direction);a.guard=world_combat_transition::inverse_smooth(phase);}
        }
        a.hits=c.hits;a.enemy_hits=c.enemy_hits;a.parries=c.parries;a.blocks=c.blocks;a.guarded=c.guarded;if(!who)a.resolved_serial=c.resolved_attack_serial;
        const bool defeated=p->stats.polished&&c.state==(who?TrainingState::Won:TrainingState::Lost);
        if(defeated&&!a.defeated){a.death_time=0;++p->stats.death_starts;
            if(who&&death_begin(p,who,prior_slot,prior_weight,prior_yaw,mirrored,c.clock)){a.facing_yaw=p->deaths->actors[who].pose.yaw;a.turn=0;p->instances[who].yaw=a.facing_yaw;}
            if(who&&c.enemy_decapitated&&p->fatalities&&!p->anatomy){const double cs=std::cos(a.facing_yaw),sn=std::sin(a.facing_yaw);const auto *q=fatality_head_center;
                // A cut carries the severed head sideways and away from the player.
                const double dx=ax-x,dz=az-z,n=std::max(.001,std::hypot(dx,dz));
                p->fatalities->state.launch(who-1,ax+cs*q[0]+sn*q[2],ay+q[1],az-sn*q[0]+cs*q[2],dx/n+dz/n*.7,dz/n-dx/n*.7,ay,fatality_head_radius,a.facing_yaw);
            }
        }
        else if(defeated){a.death_time=std::min(world_combat_motion::death_duration,a.death_time+dt);if(p->deaths&&p->deaths->actors[who].pose.valid)a.death_time=std::max(a.death_time,p->deaths->actors[who].elapsed);}
        else a.death_time=0;
        if(p->deaths&&!a.steel_rebound&&!a.reaction_bridge&&!defeated)p->deaths->actors[who].clash=false;
        if(!defeated&&a.defeated&&p->deaths){p->deaths->actors[who].pose={};p->deaths->actors[who].uploaded=false;}
        a.defeated=defeated;a.active=visible;
        if(!visible){a.hit=a.parry=a.guard=0;a.walk=0;a.attacking=false;a.combo=false;a.guard_recoil=false;a.steel_rebound=false;a.reaction_bridge=false;a.entry_origin={};if(defeated)death_tick(p,who,a.death_time);continue;}
        if(who){++p->stats.actors;++p->stats.active_enemies;}
        a.hit=std::max(0.f,a.hit-dt);a.parry=std::max(0.f,a.parry-dt);
        unsigned clip=0;float phase=.5f-.5f*std::cos(p->clock*1.6f+who*.3f);bool direct=false;
        const auto pp=who?c.enemy_phase==TrainingEnemyPhase::Windup?TrainingPlayerPhase::Windup:c.enemy_phase==TrainingEnemyPhase::Strike?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery:c.player_phase;
        const bool attack=who?c.engaged()&&(c.enemy_phase==TrainingEnemyPhase::Windup||c.enemy_phase==TrainingEnemyPhase::Strike||c.enemy_phase==TrainingEnemyPhase::Recovery):c.engaged()&&c.player_phase!=TrainingPlayerPhase::Ready;
        const bool new_serial=!who&&c.attack_start_serial!=a.attack_serial;
        if(attack&&(!a.attacking||new_serial)){
            const double duration=who?c.enemy_duration:c.player_duration;
            const bool chain=!who&&a.attacking&&new_serial&&a.attack_serial&&c.combo_count>1&&pp==TrainingPlayerPhase::Windup&&(!p->stats.polished||!was_reacting);
            a.combo=chain;a.combo_from=a.attack_direction;a.combo_to=unsigned(c.attack_direction);
            a.combo_turn=a.turn;
            a.combo_path=world_combat_continuity::Combo::Bridge;a.combo_phase=world_combat_continuity::combo_key;a.late_combo_counted=false;
            if(p->stats.polished&&chain){
                const auto &old=clips[2+a.combo_from];
                if(a.slot>=old.base&&a.slot<old.base+old.count){
                    a.combo_phase=std::clamp((float(a.slot-old.base)+p->weights[who])/float(old.count-1),0.f,1.f);
                    if(a.combo_phase>world_combat_continuity::combo_key)a.combo_path=world_combat_continuity::Combo::Finish;
                }else if(a.slot==p->pose_count+settle_offset+a.combo_from){a.combo_path=world_combat_continuity::Combo::Unwind;a.combo_phase=p->weights[who];}
            }
            unsigned prior_guard_direction=unsigned(a.guard_direction);float prior_guard_phase=0;
            const bool prior_guard=p->stats.polished&&guard_sample(a.slot,p->weights[who],prior_guard_direction,prior_guard_phase);
            a.entry_origin=p->stats.polished&&!chain&&!prior_guard&&pp==TrainingPlayerPhase::Windup&&!a.defeated?entry_origin(a.slot,p->weights[who],p->pose_count,!who&&p->loadout==2):world_combat_transition::Origin{};
            a.entry_counted=false;
            // A real new attack serial is authoritative. A lingering visual
            // block/hit must not conceal that attack through its contact time.
            // Only known, preloaded return routes can yield a reaction.
            if(p->stats.polished&&!who&&new_serial&&!body_contact&&pp==TrainingPlayerPhase::Windup&&(a.hit>0||a.parry>0)&&(prior_guard||a.entry_origin)){
                a.hit=a.parry=0;a.guard_recoil=false;++p->continuity.reaction_counters;
                if(prior_guard)a.guard=world_combat_transition::inverse_smooth(prior_guard_phase);
            }
            if(body_contact&&(!who||!p->compact||a.reaction_bridge))a.entry_origin={};
            a.idle_entry=!chain&&pp==TrainingPlayerPhase::Windup&&a.guard<=.001f&&(a.walk==0||a.walk==6)&&a.slot<4&&a.hit<=0&&a.parry<=0&&!a.defeated;
            a.idle_entry_from=a.slot;
            a.idle_entry_phase=std::clamp((float(a.slot)+p->weights[who])/3.f,0.f,1.f);
            if(chain){a.attack_leadin=std::min(world_combat_timing::flowing_sword(c.weapon,unsigned(c.attack_direction))?.09f:.045f,float(duration)*.34f);++p->stats.combo_transitions;}
            else a.attack_leadin=(prior_guard||a.entry_origin)?world_combat_attack_entry::leadin(duration):a.guard>.001f?(p->stats.polished?world_combat_attack_entry::leadin(duration):std::min(.09f,float(duration)*.3f)):a.idle_entry?world_combat_attack_entry::leadin(duration):0.f;
            a.attack_guard=a.guard;a.attack_guard_direction=prior_guard?prior_guard_direction:unsigned(a.guard_direction);
            a.guard_entry=p->stats.polished&&!chain&&(prior_guard||a.guard>.001f)&&pp==TrainingPlayerPhase::Windup;a.guard_entry_counted=false;
            a.guard_entry_phase=smooth(a.guard);
            const auto &old_guard=clips[7+a.attack_guard_direction];
            if(a.slot>=old_guard.base&&a.slot<old_guard.base+old_guard.count)a.guard_entry_phase=std::clamp((float(a.slot-old_guard.base)+p->weights[who])/float(old_guard.count-1),0.f,1.f);
            a.attack_direction=direction(unsigned(who?c.enemy_direction:c.attack_direction));
        }
        if(!who)a.attack_serial=c.attack_start_serial;
        a.attacking=attack;
        const bool reacting=a.hit>0||a.parry>0;
        const bool guard_recoil=p->stats.polished&&a.guard_recoil&&a.parry>0&&a.hit<=0;
        if(reacting&&!guard_recoil)a.guard=0;
        const unsigned desired=direction(world_combat_guard_pose(unsigned(who?c.enemy_guard:c.selected),who&&p->opponents_only));
        const bool compact_guard=who&&p->compact&&c.engaged()&&c.enemy_phase==TrainingEnemyPhase::Measure;
        const bool guard=(!reacting||guard_recoil)&&!attack&&!a.steel_rebound&&(who?c.engaged()&&c.enemy_phase!=TrainingEnemyPhase::Stagger&&(compact_guard||(!walking&&(!p->stats.polished||a.walk==0))):c.engaged()&&c.guard);
        if(who&&p->compact){
            if(local_cut)a.guarded_gait_phase=0;
            if(compact_guard&&walking)a.guarded_gait_phase=world_combat_locomotion::wrapped(a.guarded_gait_phase+float(moved/(p->opponents_only?p->opponent_stride_metres*.30:1.3)));
            const bool legs=compact_guard&&walking&&!reacting&&!a.steel_rebound;
            a.guarded_gait_mix=std::clamp(a.guarded_gait_mix+(legs?dt/.15f:-dt/.10f),0.f,1.f);
            if(reacting||a.defeated||a.steel_rebound)a.guarded_gait_mix=0;
        }
        // The four baked idle samples breathe around the same neutral as
        // guard frame0. Replacing an arbitrary committed idle with that frame
        // made first-person hands/sword jump before the actual guard motion.
        // Reuse the nearest existing idle endpoint (0 or3) for a <=60ms
        // visual pre-roll. Opponent-only banks need the same bounded path:
        // their authored breathing must not snap to neutral before a guard.
        // The simulation's guard is untouched.
        // A released/re-pressed guard finishes this same bounded path instead
        // of repeatedly capturing a new idle or snapping back on release.
        constexpr float guard_idle_seconds=.06f;
        bool preparing_guard=false;float guard_dt=dt;
        if((!who||p->opponents_only||p->compact)&&p->stats.polished){
            bool began=false;
            if(reacting||attack||a.defeated)a.guard_idle_entry=false;
            else if(!a.guard_idle_entry&&guard&&a.guard<=.001f&&prior_slot<4){
                const float from=std::clamp((float(prior_slot)+prior_weight)/3.f,0.f,1.f);
                if(from>.0001f&&from<.9999f){a.guard_idle_entry=true;a.guard_idle_from=from;a.guard_idle_to=from>.5f?1.f:0.f;a.guard_idle_time=0;began=true;}
            }
            if(a.guard_idle_entry){
                const float elapsed=a.guard_idle_time+(began?0.f:dt);
                a.guard_idle_time=std::min(guard_idle_seconds,elapsed);
                if(elapsed<guard_idle_seconds){preparing_guard=true;guard_dt=0;}
                else{a.guard_idle_entry=false;guard_dt=std::max(0.f,elapsed-guard_idle_seconds);preparing_guard=!guard||guard_dt<=1.e-7f;}
            }
        }
        if(desired!=unsigned(a.guard_direction)&&a.guard>.01f)a.guard=std::max(0.f,a.guard-guard_dt*12);
        else{if(a.guard<.01f)a.guard_direction=float(desired);a.guard=std::clamp(a.guard+(guard?guard_dt*10:-guard_dt*12),0.f,1.f);}
        if(a.defeated){clip=19;phase=a.death_time/world_combat_motion::death_duration;a.walk=0;a.guard=0;a.attacking=false;a.combo=false;a.guard_recoil=false;a.entry_origin={};}
        else if(who&&c.state==TrainingState::Won){clip=12;phase=std::min(.55f,1-a.hit/.28f);a.walk=0;}
        else if(who&&a.reaction_bridge){
            AnatomySnapshot snapshot;if(npc_reaction_snapshot(p,who,c.clock,snapshot)){a.slot=snapshot.slot;p->weights[who]=snapshot.weight;direct=true;}a.walk=0;a.guard=0;
        }
        else if(a.hit>0){
            const bool shield=p->stats.polished&&!who&&p->loadout==2;
            clip=p->stats.polished&&!shield?14+direction(a.reaction_direction):12;phase=1-a.hit/reaction_duration;a.walk=0;
            const bool entry=p->opponents_only&&opponent_reaction_entry(a,std::max(0.f,reaction_duration-a.hit),p->pose_count,a.slot,p->weights[who],clip,phase,direct);
            const double lead=p->opponents_only&&a.reaction_entry?opponent_reaction_entry_seconds:0;
            if(p->stats.polished&&!entry){const auto sample=world_combat_continuity::returning(reaction_duration-a.hit-lead,reaction_duration-lead,false);phase=sample.phase;
                if(sample.settle){const unsigned at=p->pose_count+settle_offset+(shield?5:7+direction(a.reaction_direction));if(a.slot!=at){++p->continuity.reaction_settles;if(shield)++p->continuity.shield_hit_returns;}a.slot=at;p->weights[who]=sample.weight;direct=true;}}
        }
        else if(who&&opponent_steel_sample(p,a,c.clock,a.slot,p->weights[who],clip,phase,direct)){a.walk=0;a.guard=0;a.parry=0;a.guard_recoil=false;}
        else if(a.parry>0){
            if(guard_recoil&&!a.guard_recoil_counted){a.guard_recoil_counted=true;++p->continuity.guard_recoils;}
            clip=guard_recoil?7+unsigned(a.guard_direction):13;const double duration=p->stats.polished?a.parry_duration:who?.30:.20;phase=guard_recoil?(who?opponent_guard_recoil(smooth(a.guard),duration-a.parry,duration):world_combat_transition::guard_recoil(smooth(a.guard),duration-a.parry,duration)):1-a.parry/duration;a.walk=0;
            if(p->stats.polished&&!guard_recoil){const auto sample=world_combat_continuity::returning(duration-a.parry,duration,false);phase=sample.phase;
                if(sample.settle){const unsigned at=p->pose_count+settle_offset+6;if(a.slot!=at)++p->continuity.reaction_settles;a.slot=at;p->weights[who]=sample.weight;direct=true;}}
        }
        else if(attack){
            const double t=who?c.enemy_time:c.player_time,duration=who?c.enemy_duration:c.player_duration;
            if(pp==TrainingPlayerPhase::Windup&&a.combo&&t<a.attack_leadin){
                if(p->stats.polished){
                    const auto sample=world_combat_continuity::combo(a.combo_path,a.combo_phase,t,a.attack_leadin);
                    if(a.combo_path!=world_combat_continuity::Combo::Bridge&&!a.late_combo_counted){a.late_combo_counted=true;++p->continuity.late_combo_finishes;}
                    if(sample.settle){a.slot=p->pose_count+settle_offset+a.combo_from;p->weights[who]=sample.weight;direct=true;}
                    else if(sample.bridge){a.slot=p->pose_count+3+a.combo_from*5+a.combo_to;p->weights[who]=sample.weight;direct=true;}
                    else{clip=2+a.combo_from;phase=sample.phase;}
                }else{a.slot=p->pose_count+3+a.combo_from*5+a.combo_to;p->weights[who]=smooth(float(t/a.attack_leadin));direct=true;}
            }else if(pp==TrainingPlayerPhase::Windup&&a.entry_origin&&t<a.attack_leadin){
                if(!a.entry_counted){a.entry_counted=true;if(a.entry_origin.moving())++p->continuity.moving_entries;}
                const auto sample=world_combat_transition::entry(a.entry_origin,t,a.attack_leadin,a.attack_direction);
                using Kind=world_combat_transition::SampleKind;
                if(sample.kind==Kind::Clip){clip=sample.index;phase=sample.phase;if(clips[clip].loop)phase-=std::floor(phase);}
                else{a.slot=sample.kind==Kind::WalkStart?p->pose_count:sample.kind==Kind::WalkReturn?p->pose_count+1+sample.index:sample.kind==Kind::Return?p->pose_count+settle_offset+sample.index:p->pose_count+3+combo_batches+sample.index;p->weights[who]=sample.phase;direct=true;}
            }else if(pp==TrainingPlayerPhase::Windup&&a.idle_entry&&t<a.attack_leadin){
                const double settle=world_combat_attack_entry::settle_time(a.attack_leadin);
                if(t<settle){clip=0;phase=a.idle_entry_phase*(1-world_combat_attack_entry::weight(t,settle));}
                else{
                    a.slot=p->pose_count+3+combo_batches+a.attack_direction;
                    p->weights[who]=world_combat_attack_entry::weight(t-settle,a.attack_leadin-settle);direct=true;
                }
                const unsigned weapon=!who&&p->loadout?p->loadout-1:0;
                const unsigned witness=1u<<who;
                if(direct&&!(p->idle_entry_witnesses[weapon]&witness)){
                    p->idle_entry_witnesses[weapon]|=witness;
                    std::printf("[Combat motion] Idle entry rendered: actor%u weapon%u prior_key%u batch%u leadin%.6f windup%.6f weight%.6f\n",who,weapon,a.idle_entry_from,a.slot,a.attack_leadin,duration,p->weights[who]);
                }
            }else if(pp==TrainingPlayerPhase::Windup&&a.guard_entry&&t<a.attack_leadin){
                const auto sample=world_combat_continuity::guarding(a.guard_entry_phase,t,a.attack_leadin);
                if(sample.entry){a.slot=p->pose_count+3+combo_batches+a.attack_direction;p->weights[who]=sample.weight;direct=true;if(!a.guard_entry_counted){a.guard_entry_counted=true;++p->continuity.guarded_entries;}}
                else{clip=7+a.attack_guard_direction;phase=sample.phase;}
            }else if(pp==TrainingPlayerPhase::Windup&&t<a.attack_leadin){clip=7+a.attack_guard_direction;phase=smooth(a.attack_guard*float(1-t/a.attack_leadin));}
            else{
                clip=2+direction(unsigned(who?c.enemy_direction:c.attack_direction));
                phase=attack_phase(pp,t,duration,a.attack_leadin,p->stats.polished);
                const bool steel_rebound=!who&&c.resolved_attack_serial==c.attack_start_serial&&c.resolved_attack_serial&&c.resolved_attack_outcome==TrainingAttackOutcome::EnemyBlock;
                if(steel_rebound){
                    // Same certified attack/entry poses, sampled backward only
                    // after an actual steel outcome. Air misses keep their arc.
                    const double elapsed=std::max(0.,c.clock-c.resolved_attack_time);
                    phase=elapsed<.075?.36f-.065f*smooth(float(elapsed/.075)):
                        .295f*(1-smooth(float((elapsed-.075)/.18)));
                }
                // A pending link converges to the exact preloaded recovery
                // endpoint before its serial changes, keeping feet/grip continuous.
                if(!steel_rebound&&!who&&c.pending_attack&&pp==TrainingPlayerPhase::Recovery&&t<=c.committed_timing().link)
                    phase=.60f+(8.f/11.f-.60f)*smooth(float(t/c.committed_timing().link));
                const auto &presentation=!who&&p->loadout?p->loadouts->entries[p->loadout-2].presentation:p->presentation;
                if(presentation.upper_body_aimed)phase=world_combat_presentation::source_phase(phase,presentation.contact_frames[direction(unsigned(who?c.enemy_direction:c.attack_direction))]);
                if(!steel_rebound&&p->stats.polished&&pp==TrainingPlayerPhase::Recovery&&(!c.pending_attack||who||t>c.committed_timing().link)){
                    const auto sample=world_combat_continuity::returning(t,duration,true);
                    phase=.60f+sample.phase*.40f;
                    if(sample.settle){const unsigned at=p->pose_count+settle_offset+direction(unsigned(who?c.enemy_direction:c.attack_direction));if(a.slot!=at)++p->continuity.recovery_settles;a.slot=at;p->weights[who]=sample.weight;direct=true;}
                }
            }
            a.walk=0;
        }else if(preparing_guard){clip=0;phase=a.guard_idle_from+(a.guard_idle_to-a.guard_idle_from)*smooth(a.guard_idle_time/guard_idle_seconds);a.walk=0;}
        else if(a.guard>.001f){clip=7+unsigned(a.guard_direction);phase=smooth(a.guard);a.walk=0;}
        else if(!who||p->stats.polished){
            const bool moving=who?walking:p->locomotion?p->locomotion->state.moving:dt>0&&travel/dt>.035;
            walk_sample(a,p->pose_count,p->stats.polished,p->weights[who],clip,phase,direct,moving,dt,who?moved:travel,!who&&p->locomotion?&p->locomotion->state:nullptr,who&&p->opponents_only?p->opponent_stride_metres:1.3);
        }else if(walking){clip=1;phase=a.phase;a.walk=0;}

        if(clip==0&&!direct&&!attack&&!reacting&&!preparing_guard&&a.guard<=.001f&&!walking&&a.walk==0){a.idle_elapsed=std::min(.14f,a.idle_elapsed+dt);if(p->stats.polished)phase*=world_combat_continuity::idle(a.idle_elapsed);}else a.idle_elapsed=0;
        if(!direct){const auto &q=clips[clip];const float f=std::clamp(phase,0.f,q.loop?.999999f:1.f)*(q.loop?q.count:q.count-1);const unsigned key=std::min(q.count-1,unsigned(f));a.slot=q.base+key;p->weights[who]=f-key;}
        const bool turning=a.attacking&&a.hit<=0&&a.parry<=0&&!a.defeated;
        const auto &presentation=!who&&p->loadout?p->loadouts->entries[p->loadout-2].presentation:p->presentation;
        if(who&&a.steel_rebound)a.turn=a.steel_turn*(1-smooth(float((c.clock-a.steel_started)/opponent_steel_seconds)));
        else if(turning&&p->weapon_sockets&&!presentation.upper_body_aimed){
            const unsigned d=direction(unsigned(who?c.enemy_direction:c.attack_direction));const auto &q=clips[2+d];
            const float f=.36f*(q.count-1);const unsigned k=unsigned(f);const float blend=f-k;
            const auto &sockets=!who&&p->loadout?p->loadouts->entries[p->loadout-2].sockets:p->sockets;
            const auto *sx=sockets[q.base+k][1],*sy=sockets[q.base+k+1][1];
            const float tx=sx[0]+(sy[0]-sx[0])*blend,tz=sx[2]+(sy[2]-sx[2])*blend;
            double t=who?c.enemy_time:c.player_time,duration=who?c.enemy_duration:c.player_duration;
            if(pp==TrainingPlayerPhase::Windup){t=std::max(0.,t-a.attack_leadin);duration=std::max(.001,duration-a.attack_leadin);}
            const float fraction=float(std::clamp(t/std::max(.001,duration),0.,1.));
            const float weight=pp==TrainingPlayerPhase::Windup?smooth(fraction):pp==TrainingPlayerPhase::Strike?1.f:1-smooth(fraction);
            if((a.combo||(p->stats.polished&&(a.entry_origin||a.guard_entry)))&&pp==TrainingPlayerPhase::Windup&&(who?c.enemy_time:c.player_time)<a.attack_leadin)a.turn=a.combo_turn*(1-smooth(float((who?c.enemy_time:c.player_time)/a.attack_leadin)));
            else a.turn=std::clamp(-std::atan2(tx,tz),-1.15f,1.15f)*weight;
        }else if(presentation.upper_body_aimed)a.turn=0;
        else a.turn*=std::exp(-dt*16);
        p->instances[who].yaw+=mirrored?-a.turn:a.turn;
        if(who&&defeated&&p->deaths&&p->deaths->actors[who].pose.valid){p->instances[who].yaw=a.facing_yaw=p->deaths->actors[who].pose.yaw;death_tick(p,who,a.death_time);}
    }
    p->stats.peak_actors=std::max(p->stats.peak_actors,p->stats.actors);
    if(p->compact){p->arms_instance=world_combat_presentation::viewmodel(0);return;} // actor transforms/pose clocks committed; streams upload only when their sample changes
    auto upload=[&](WorldModelPoseBlend *batch,Upload &committed,const WorldModelInstance *instances,const float *weights,unsigned n){
        bool same=committed.count==n;
        for(unsigned i=0;same&&i<n;++i)same=committed.instances[i].x==instances[i].x&&committed.instances[i].y==instances[i].y&&committed.instances[i].z==instances[i].z&&committed.instances[i].yaw==instances[i].yaw&&committed.instances[i].scale==instances[i].scale&&committed.weights[i]==weights[i];
        if(same)return;
        if(world_model_pose_blend_upload(batch,instances,weights,n)){std::copy_n(instances,n,committed.instances.begin());std::copy_n(weights,n,committed.weights.begin());committed.count=n;++p->stats.uploads;}
    };
    for(unsigned slot=0;slot<p->batch_count;++slot){
        std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};unsigned n=0;
        for(unsigned who=0;who<actor_capacity;++who)if(p->actors[who].active&&p->actors[who].slot==slot&&!(who&&death_active(p,who))&&!(who&&p->fatalities&&p->fatalities->state.severed[who-1])&&!(who&&p->anatomy&&(p->anatomy->masks[who]||p->anatomy->mirrored[who]))){instances[n]=p->instances[who];weights[n++]=p->weights[who];}
        if(n)upload(p->body.blend[slot],p->body.uploaded[slot],instances.data(),weights.data(),n);
        n=0;for(unsigned who=1;who<actor_capacity;++who)if(p->actors[who].active&&p->actors[who].slot==slot&&!death_active(p,who)&&!(who&&p->fatalities&&p->fatalities->state.severed[who-1])&&!(who&&p->anatomy&&(p->anatomy->masks[who]||p->anatomy->mirrored[who]))){instances[n]=p->instances[who];weights[n++]=p->weights[who];}
        if(n)upload(p->enemy_only[slot],p->enemy_uploaded[slot],instances.data(),weights.data(),n);
    }
    if(p->fatalities){auto &g=*p->fatalities;if(p->fatality_floor)for(auto &h:g.state.heads)if(h.active&&!h.settled){const double floor=p->fatality_floor(h.x,h.z,p->fatality_floor_context);if(std::isfinite(floor))h.floor=floor;}g.state.update(dt,paused);g.body_counts={};g.head_counts={};
        for(unsigned frame=0;frame<24;++frame){std::array<WorldModelInstance,npc_capacity> instances={};std::array<float,npc_capacity> weights={};unsigned n=0;
            for(unsigned who=1;who<actor_capacity;++who)if(p->actors[who].active&&g.state.severed[who-1]&&p->actors[who].slot==death_base+frame){instances[n]=p->instances[who];weights[n++]=p->weights[who];}
            if(n)world_model_pose_blend_upload(g.body[frame],instances.data(),weights.data(),n);g.body_counts[frame]=n;n=0;
            for(const auto &h:g.state.heads)if(h.active){const double spin=h.spin-std::floor(h.spin),f=spin*24;const unsigned key=std::min(23u,unsigned(f));if(key!=frame)continue;
                instances[n]={float(h.x-p->x),float(h.y),float(h.z-p->z),float(h.yaw),1};weights[n++]=float(f-key);}
            if(n)world_model_pose_blend_upload(g.heads[frame],instances.data(),weights.data(),n);g.head_counts[frame]=n;
        }
    }
    const WorldModelInstance arms=world_combat_presentation::viewmodel(p->loadout);p->arms_instance=arms;
    if(!p->opponents_only)upload(p->arms.blend[p->actors[0].slot],p->arms.uploaded[p->actors[0].slot],&arms,p->weights.data(),1);
    if(p->loadout){auto &entry=p->loadouts->entries[p->loadout-2];const unsigned slot=p->actors[0].slot;
        upload(entry.body.blend[slot],entry.body.uploaded[slot],p->instances.data(),p->weights.data(),1);
        upload(entry.arms.blend[slot],entry.arms.uploaded[slot],&arms,p->weights.data(),1);
    }
    commit_player_deformation(p,p->loadout?p->loadout-1:0);
}
}
void world_combat_motion_update(WorldCombatMotion *p,const WorldTrainingCombat &c,double x,double feet,double z,float yaw,float dt,bool show_enemy,bool paused){
    double ox=0,oz=0;world_atlas_village_origin(&ox,&oz);
    if(p&&p->custom_arena){ox=p->arena_x;oz=p->arena_z;}
    const WorldCombatArenaActor enemy{c,ox+c.enemy_x,p&&p->custom_arena?p->arena_feet:24.f,oz+c.enemy_z,show_enemy,p&&p->arena_enemy_walk};
    update_motion(p,c,x,feet,z,yaw,dt,&enemy,1,paused);
}
void world_combat_motion_update_arena(WorldCombatMotion *p,const WorldTrainingCombat &c,double x,double feet,double z,float yaw,float dt,const WorldCombatArenaActor *actors,unsigned count,bool paused){
    update_motion(p,c,x,feet,z,yaw,dt,actors,count,paused);
}
void world_combat_motion_draw(WorldCombatMotion *p,unsigned program,double x,double y,double z,bool shadow,bool show_player){
    if(!p||!program)return;
    show_player=show_player&&!p->opponents_only;
    if(p->compact){compact_draw(p,program,x,y,z,shadow,show_player);return;}
    if(p->variant_stats.loaded&&!p->anatomy)return;
    auto &u=p->locations(program);glUseProgram(program);glUniform3f(u.offset,float(p->x-x),float(-y),float(p->z-z));glUniform2f(u.anchor,float(p->x),float(p->z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop,0,0,0);glUniform1f(u.reactive,shadow?0:.85f);
    std::array<bool,batches> drawn={};
    const bool aquatic=show_player&&p->aquatics&&p->stats.aquatic_active;
    const bool unarmed=show_player&&!aquatic&&p->unarmed&&p->unarmed->active;
    const bool alternate_player=show_player&&!aquatic&&!unarmed&&p->loadout;
    const bool ordinary_player=show_player&&!aquatic&&!unarmed&&!alternate_player;
    const bool gait=p->locomotion&&p->locomotion->state.weight>0&&!p->actors[0].defeated;
    const bool grip=p->arm_grips&&p->arm_grips->body_active&&!p->actors[0].defeated;
    if(ordinary_player&&p->actors[0].active){if(p->body.rigid[p->actors[0].slot].enabled)++p->stats.rigid_body_draws[0];auto *b=p->body.blend[p->actors[0].slot];if(grip)++p->grip_stats.body_draws;if(gait||grip)world_model_pose_blend_draw_equipped_deformed(b,program,p->equipment,1);else world_model_pose_blend_draw_equipped(b,program,p->equipment,1);}
    for(unsigned who=1;who<actor_capacity;++who)if(p->actors[who].active&&!(p->fatalities&&p->fatalities->state.severed[who-1])){
        const unsigned slot=p->actors[who].slot;actor_blood_wear(p,program,who);
        if(death_active(p,who)){
            bool crowd_owns=false;if(p->anatomy)for(unsigned i=0;i<p->anatomy->crowd_count;++i)crowd_owns|=p->anatomy->crowd[i].yielded&&p->anatomy->crowd[i].actor_id==who;
            if(crowd_owns)continue;
            AnatomySnapshot snapshot={p->instances[who],slot,p->weights[who],p->anatomy->mirrored[who],true};snapshot.variant_mask=world_model_variants::actor(who);
            if(death_draw_actor(p,program,who,snapshot,p->anatomy->masks[who],anatomy_math::surviving_hand(p->anatomy->masks[who])))continue;
        }
        if(p->anatomy&&(p->variant_stats.loaded||p->anatomy->masks[who]||p->anatomy->mirrored[who])){
            AnatomySnapshot snapshot={p->instances[who],slot,p->weights[who],p->anatomy->mirrored[who],true};snapshot.variant_mask=world_model_variants::actor(who);
            anatomy_draw_actor(p,program,snapshot,p->anatomy->masks[who],anatomy_math::surviving_hand(p->anatomy->masks[who]));if(p->variant_stats.loaded)++p->variant_stats.body_draws[(who-1)%4];continue;
        }
        if(drawn[slot])continue;drawn[slot]=true;
        world_model_pose_blend_draw(p->enemy_only[slot],program);
    }
    if(p->fatalities)for(unsigned i=0;i<24;++i){if(p->fatalities->body_counts[i])world_model_pose_blend_draw(p->fatalities->body[i],program);if(p->fatalities->head_counts[i])world_model_pose_blend_draw(p->fatalities->heads[i],program);}
    anatomy_draw_pieces(p,program);
    actor_blood_wear(p,program,0);
    if(alternate_player){if(p->loadouts->entries[p->loadout-2].body.rigid[p->actors[0].slot].enabled)++p->stats.rigid_body_draws[p->loadout-1];auto *b=p->loadouts->entries[p->loadout-2].body.blend[p->actors[0].slot];if(grip)++p->grip_stats.body_draws;if(gait||grip)world_model_pose_blend_draw_equipped_deformed(b,program,p->equipment,1);else world_model_pose_blend_draw_equipped(b,program,p->equipment,1);}
    if(unarmed){auto *b=p->unarmed->blends[p->unarmed->slot];if(gait)world_model_pose_blend_draw_equipped_deformed(b,program,p->equipment,1);else world_model_pose_blend_draw_equipped(b,program,p->equipment,1);}
    if(aquatic)world_model_pose_blend_draw_equipped(p->aquatics->body.blends[p->aquatics->slot],program,p->equipment,1);
    glUniform1f(u.reactive,0);
}
void world_combat_motion_draw_arms(WorldCombatMotion *p,unsigned program){
    if(!p||p->opponents_only)return;
    if(p->compact){compact_draw_arms(p,program);return;}
    // The third-person actor owns the collapse. A standing viewmodel root
    // cannot display that body death clip without pulling the wrists/sleeves
    // across the camera; lower the weapon when the player is defeated.
    if(!p||!program||p->actors[0].defeated)return;
    if(p->aquatics&&p->stats.aquatic_active)world_model_pose_blend_draw_equipped(p->aquatics->arms.blends[p->aquatics->slot],program,p->equipment,1);
    else if(p->loadout){if(p->loadouts->entries[p->loadout-2].arms.rigid[p->actors[0].slot].enabled)++p->stats.rigid_arms_draws[p->loadout-1];auto *b=p->loadouts->entries[p->loadout-2].arms.blend[p->actors[0].slot];if(p->arm_grips&&p->arm_grips->arms_active){++p->grip_stats.arms_draws;world_model_pose_blend_draw_equipped_deformed(b,program,p->equipment,1);}else world_model_pose_blend_draw_equipped(b,program,p->equipment,1);}
    else{if(p->arms.rigid[p->actors[0].slot].enabled)++p->stats.rigid_arms_draws[0];auto *b=p->arms.blend[p->actors[0].slot];if(p->arm_grips&&p->arm_grips->arms_active){++p->grip_stats.arms_draws;world_model_pose_blend_draw_equipped_deformed(b,program,p->equipment,1);}else world_model_pose_blend_draw_equipped(b,program,p->equipment,1);}
}
void world_combat_motion_equipment(WorldCombatMotion *p,unsigned mask){if(p&&!p->opponents_only&&mask<=511)p->equipment=mask;}
unsigned world_combat_motion_equipment(const WorldCombatMotion *p){return p?p->equipment:0;}
bool world_combat_motion_blade(const WorldCombatMotion *p,unsigned actor,WorldCombatBlade &out,bool viewmodel){
    if(!p||(p->opponents_only&&(!actor||viewmodel))||!p->weapon_sockets||actor>=actor_capacity||!p->actors[actor].active||(viewmodel&&actor)||(!actor&&(p->stats.aquatic_active||(p->unarmed&&p->unarmed->active))))return false;
    if(actor&&p->compact&&(p->actors[actor].steel_rebound||p->actors[actor].reaction_bridge)){AnatomySnapshot snapshot;snapshot.instance=p->instances[actor];snapshot.mirrored=p->anatomy&&p->anatomy->mirrored[actor];if(npc_reaction_snapshot(p,actor,p->actors[actor].last_clock,snapshot)||clash_snapshot(p,p->actors[actor],p->actors[actor].last_clock,snapshot)){const auto edge=anatomy_defender_edge(p,snapshot);out.base[0]=edge.base.x;out.base[1]=edge.base.y;out.base[2]=edge.base.z;out.tip[0]=edge.tip.x;out.tip[1]=edge.tip.y;out.tip[2]=edge.tip.z;return true;}}
    const auto slot=p->actors[actor].slot;const auto pair=pose_pair(slot,p->pose_count);const unsigned a=pair[0],b=pair[1];
    const auto &instance=viewmodel?p->arms_instance:p->instances[actor];const double cs=std::cos(instance.yaw),sn=std::sin(instance.yaw),weight=p->weights[actor];
    const auto &sockets=!actor&&p->loadout?p->loadouts->entries[p->loadout-2].sockets:viewmodel&&p->separate_viewmodel_sockets?p->viewmodel_sockets:p->sockets;
    const auto &bank=!actor&&p->loadout?(viewmodel?p->loadouts->entries[p->loadout-2].arms:p->loadouts->entries[p->loadout-2].body):(viewmodel?p->arms:p->body);
    const auto &rigid=bank.rigid[slot];
    for(unsigned point=0;point<2;++point){double local[3];
        if(rigid.enabled){world_rigid_weapon_pose::Point source={sockets[a][point][0],sockets[a][point][1],sockets[a][point][2]};if(viewmodel){source[0]+=.25f;source[2]+=.65f;}
            const auto q=world_rigid_weapon_pose::interpolate(source,rigid.pivot_a,rigid.pivot_b,rigid.rotation,float(weight));std::copy_n(q.begin(),3,local);}
        else{for(unsigned k=0;k<3;++k)local[k]=sockets[a][point][k]+(sockets[b][point][k]-sockets[a][point][k])*weight;if(viewmodel){local[0]+=.25;local[2]+=.65;}}
        if(actor&&p->anatomy&&p->anatomy->mirrored[actor])local[0]=-local[0];
        auto *target=point?out.tip:out.base;target[0]=(cs*local[0]+sn*local[2])*instance.scale+instance.x+(viewmodel?0:p->x);target[1]=local[1]*instance.scale+instance.y;target[2]=(-sn*local[0]+cs*local[2])*instance.scale+instance.z+(viewmodel?0:p->z);
    }return true;
}
WorldCombatMotionStats world_combat_motion_stats(const WorldCombatMotion *p){return p?p->stats:WorldCombatMotionStats{};}
unsigned world_combat_motion_variant_mask(unsigned actor_id){return world_model_variants::actor(actor_id);}
WorldCombatVariantStats world_combat_motion_variant_stats(const WorldCombatMotion *p){return p?p->variant_stats:WorldCombatVariantStats{};}
WorldCombatDeathStats world_combat_motion_death_stats(const WorldCombatMotion *p){return p&&p->deaths?p->deaths->stats:WorldCombatDeathStats{};}
WorldCombatDeathPose world_combat_motion_death_pose(const WorldCombatMotion *p,unsigned actor){return p&&p->deaths&&actor<actor_capacity?p->deaths->actors[actor].pose:WorldCombatDeathPose{};}
WorldCombatContinuityStats world_combat_motion_continuity_stats(const WorldCombatMotion *p){return p?p->continuity:WorldCombatContinuityStats{};}
bool world_combat_motion_facing(const WorldCombatMotion *p,unsigned actor,float &yaw){
    if(!p||actor>=actor_capacity||!p->actors[actor].active||!p->actors[actor].root_ready)return false;
    yaw=p->instances[actor].yaw;return std::isfinite(yaw);
}
WorldCombatPose world_combat_motion_pose(const WorldCombatMotion *p,unsigned actor){
    if(!p||actor>=p->actors.size())return {};
    const auto &a=p->actors[actor];const bool mirror=actor&&p->anatomy&&p->anatomy->mirrored[actor];return {a.slot,anatomy_direction(unsigned(a.guard_direction),mirror),a.guard,p->weights[actor],a.active,a.reaction_direction,a.defeated,a.death_time/world_combat_motion::death_duration,a.slot>=p->pose_count+3&&a.slot<p->pose_count+3+combo_batches,a.combo_from,a.combo_to};
}
void world_combat_motion_free(WorldCombatMotion *p){if(!p)return;compact_free(p->compact);free_deaths(p->deaths);free_anatomy(p->anatomy);free_arm_grips(p->arm_grips);free_locomotion(p->locomotion);free_loadouts(p->loadouts);free_fatalities(p->fatalities);free_aquatics(p->aquatics);if(p->unarmed){for(auto *b:p->unarmed->blends)if(b)world_model_pose_blend_free(b);for(auto *a:p->unarmed->assets)if(a)world_model_free(a);delete p->unarmed;}for(auto *b:p->arena_crowd)if(b)world_model_pose_blend_free(b);for(auto *b:p->enemy_only)if(b)world_model_pose_blend_free(b);for(auto *bank:{&p->body,&p->arms}){for(auto *b:bank->blend)if(b)world_model_pose_blend_free(b);for(auto *a:bank->assets)if(a)world_model_free(a);}delete p;}
