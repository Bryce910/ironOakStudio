#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_combat_motion.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
namespace {
struct Clip{unsigned base,count;bool loop;};
constexpr Clip clips[]={{0,4,false},{4,8,true},{12,12,false},{24,12,false},{36,12,false},{48,12,false},{60,12,false},{72,4,false},{76,4,false},{80,4,false},{84,4,false},{88,4,false},{92,8,false},{100,8,false},{108,8,false},{116,8,false},{124,8,false},{132,8,false},{140,8,false},{148,24,false}};
constexpr unsigned legacy_poses=108,poses=172,combo_batches=25,batches=poses+3+combo_batches,death_base=148,death_frames=24,actor_capacity=9;
#include "engine/world_combat_weapon_sockets.inc"
struct Upload {std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};unsigned count=0;};
struct Bank{std::array<WorldModelAsset*,poses> assets={};std::array<WorldModelPoseBlend*,batches> blend={};std::array<Upload,batches> uploaded={};};
struct Actor{
    unsigned hits=0,enemy_hits=0,parries=0,blocks=0,guarded=0;
    std::uint64_t attack_serial=0;unsigned attack_direction=2,combo_from=2,combo_to=2;bool combo=false,root_ready=false;
    double last_x=0,last_z=0,last_clock=0;
    unsigned slot=0;bool active=false,attacking=false,facing_ready=false,defeated=false;float facing_yaw=0,combo_turn=0;float attack_leadin=0,attack_guard=0;unsigned attack_guard_direction=2,reaction_direction=2;float guard=0,guard_direction=2,phase=0,transition=0,settle_from=0,settle_to=0;int walk=0;unsigned half=0;float hit=0,parry=0,turn=0,death_time=0;};
struct Uniforms {unsigned long long epoch=0;unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1;};
float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
float attack_phase(TrainingPlayerPhase phase,double t,double duration,double leadin,bool polished){
    if(phase==TrainingPlayerPhase::Windup){t=std::max(0.,t-leadin);duration=std::max(.001,duration-leadin);}
    float f=float(std::clamp(t/std::max(.001,duration),0.,1.));
    // Only visual sampling changes: preparation accelerates into contact,
    // follow-through sheds speed, and the return settles into the stance.
    // Keep the gameplay's exact .36 contact/.60 recovery/1 neutral anchors.
    if(polished)f=phase==TrainingPlayerPhase::Windup?std::pow(f,1.6f):phase==TrainingPlayerPhase::Strike?1-std::pow(1-f,1.4f):smooth(f);
    return phase==TrainingPlayerPhase::Windup?f*.36f:phase==TrainingPlayerPhase::Strike?.36f+f*.24f:.60f+f*.40f;
}
}
struct WorldCombatMotion{
    Bank body,arms;std::array<WorldModelPoseBlend*,batches> enemy_only={};std::array<Upload,batches> enemy_uploaded={};std::array<Actor,actor_capacity> actors={};std::array<Uniforms,4> uniforms={};unsigned next_uniform=0;
    std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};
    double x=0,z=0,last_x=0,last_z=0;bool initialized=false;bool weapon_sockets=false;WorldModelInstance arms_instance={};float clock=0;WorldCombatMotionStats stats;
    double arena_x=0,arena_z=0;float arena_feet=24;bool custom_arena=false;
    unsigned pose_count=legacy_poses,batch_count=legacy_poses+3+combo_batches;
    float sockets[poses][2][3]={};
    bool arena_enemy_walk=false,crowd_ready=false;std::array<WorldModelPoseBlend*,death_frames+1> arena_crowd={};std::array<unsigned,death_frames+1> crowd_counts={};double crowd_x=0,crowd_z=0;
    Uniforms &locations(unsigned program){for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");u.prop=glGetUniformLocation(program,"uPropLodRange");return u;}
};
namespace {
bool load_sockets(WorldCombatMotion *p,const char *directory){
    char path[1200];const int n=std::snprintf(path,sizeof path,"%s/combat_weapon_sockets.bin",directory);
    if(n<0||std::size_t(n)>=sizeof path)return false;
    auto *file=std::fopen(path,"rb");if(!file)return false;
    auto word=[&](std::uint32_t &out){unsigned char b[4];if(std::fread(b,1,4,file)!=4)return false;out=std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);return true;};
    char magic[8];std::uint32_t count=0;bool okay=std::fread(magic,1,8,file)==8&&!std::memcmp(magic,"VWMOT172",8)&&word(count)&&count==poses;
    for(unsigned i=0;okay&&i<poses;i++)for(unsigned j=0;okay&&j<2;j++)for(unsigned k=0;okay&&k<3;k++){
        std::uint32_t bits=0;okay=word(bits);float f=0;std::memcpy(&f,&bits,4);
        okay=okay&&std::isfinite(f)&&std::abs(f)<=16&&(i>=legacy_poses||std::abs(f-combat_weapon_sockets[i][j][k])<=1.e-5f);p->sockets[i][j][k]=f;
    }
    okay=okay&&std::fgetc(file)==EOF;std::fclose(file);return okay;
}
}
WorldCombatMotion *world_combat_motion_load(const char *directory,char *error,std::size_t capacity,bool polished){
    if(error&&capacity)error[0]=0;
    if(!directory||!*directory)return nullptr;
    auto *p=new(std::nothrow)WorldCombatMotion;if(!p)return nullptr;
    std::memcpy(p->sockets,combat_weapon_sockets,sizeof combat_weapon_sockets);
    if(polished){p->pose_count=poses;p->batch_count=batches;p->stats.polished=true;if(!load_sockets(p,directory))goto fail;}
    for(unsigned kind=0;kind<2;kind++){auto &b=kind?p->arms:p->body;
        for(unsigned i=0;i<p->pose_count;i++){char path[1200];const int n=std::snprintf(path,sizeof path,"%s/combat_%s_%03u.glb",directory,kind?"arms":"body",i);
            if(n<0||std::size_t(n)>=sizeof path||!(b.assets[i]=(kind||i>=legacy_poses)?world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION):world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION)))goto fail;}
        for(const auto &c:clips)if(c.base<p->pose_count)for(unsigned i=0;i<c.count;i++){const unsigned slot=c.base+i,next=c.base+(c.loop?(i+1)%c.count:std::min(i+1,c.count-1));
            b.blend[slot]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);if(!b.blend[slot]||!world_model_pose_blend_set_targets(b.blend[slot],b.assets[slot],b.assets[next]))goto fail;}
        for(unsigned i=0;i<3;i++){b.blend[p->pose_count+i]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);const unsigned from=i==0?0:(i==1?4:8),to=i==0?4:0;
            if(!b.blend[p->pose_count+i]||!world_model_pose_blend_set_targets(b.blend[p->pose_count+i],b.assets[from],b.assets[to]))goto fail;}
        for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to){
            const unsigned at=p->pose_count+3+from*5+to;
            b.blend[at]=world_model_pose_blend_create(actor_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.blend[at]||!world_model_pose_blend_set_targets(b.blend[at],b.assets[12+12*from+8],b.assets[12+12*to]))goto fail;
        }
    }
    for(const auto &c:clips)if(c.base<p->pose_count)for(unsigned i=0;i<c.count;i++){
        const unsigned slot=c.base+i,next=c.base+(c.loop?(i+1)%c.count:std::min(i+1,c.count-1));
        p->enemy_only[slot]=world_model_pose_blend_create(8,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[slot]||!world_model_pose_blend_set_targets(p->enemy_only[slot],p->body.assets[slot],p->body.assets[next]))goto fail;
    }
    for(unsigned i=0;i<3;i++){
        const unsigned from=i==0?0:(i==1?4:8),to=i==0?4:0;
        p->enemy_only[p->pose_count+i]=world_model_pose_blend_create(8,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[p->pose_count+i]||!world_model_pose_blend_set_targets(p->enemy_only[p->pose_count+i],p->body.assets[from],p->body.assets[to]))goto fail;
    }
    for(unsigned from=0;from<5;++from)for(unsigned to=0;to<5;++to){
        const unsigned at=p->pose_count+3+from*5+to;
        p->enemy_only[at]=world_model_pose_blend_create(8,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->enemy_only[at]||!world_model_pose_blend_set_targets(p->enemy_only[at],p->body.assets[12+12*from+8],p->body.assets[12+12*to]))goto fail;
    }
    p->stats.combo_batches=combo_batches;
    p->weapon_sockets=polished||std::string(directory).find("longsword_mocap")!=std::string::npos;
    p->stats.pose_assets=p->pose_count*2;p->stats.reaction_assets=polished?40:0;p->stats.death_assets=polished?24:0;return p;
fail:
    if(error&&capacity)std::snprintf(error,capacity,"Owned combat pose bank missing or incompatible");
    world_combat_motion_free(p);return nullptr;
}
void world_combat_motion_arena(WorldCombatMotion *p,double x,double z,float feet){if(p&&std::isfinite(x)&&std::isfinite(z)&&std::isfinite(feet)){p->arena_x=x;p->arena_z=z;p->arena_feet=feet;p->custom_arena=true;}}
void world_combat_motion_arena_enemy_walk(WorldCombatMotion *p,bool walking){if(p)p->arena_enemy_walk=walking;}
bool world_combat_motion_prepare_crowd(WorldCombatMotion *p){
    if(!p)return false;
    if(p->crowd_ready)return true;
    const unsigned count=p->stats.polished?death_frames+1:1;
    for(unsigned i=0;i<count;i++){
        const unsigned from=i?death_base+i-1:0,to=i?std::min(from+1,poses-1):(p->stats.polished?0:98);
        auto *batch=world_model_pose_blend_create(8,WORLD_MODEL_POSE_BLEND_VERSION);p->arena_crowd[i]=batch;
        if(!batch||!world_model_pose_blend_set_targets(batch,p->body.assets[from],p->body.assets[to])){
            for(auto *&b:p->arena_crowd){if(b)world_model_pose_blend_free(b);b=nullptr;}return false;
        }
    }
    p->crowd_ready=true;return true;
}
bool world_combat_motion_crowd(WorldCombatMotion *p,const WorldCombatCrowdInstance *crowd,unsigned count){
    if(!p||!p->crowd_ready||count>8||(count&&!crowd))return false;
    for(unsigned i=0;i<count;++i)if(!std::isfinite(crowd[i].x)||!std::isfinite(crowd[i].y)||!std::isfinite(crowd[i].z)||!std::isfinite(crowd[i].yaw)||!std::isfinite(crowd[i].defeat_phase)||std::abs(crowd[i].x)>1.e9||std::abs(crowd[i].y)>1.e9||std::abs(crowd[i].z)>1.e9)return false;
    const double x=count?std::floor(crowd[0].x/128)*128:0,z=count?std::floor(crowd[0].z/128)*128:0;
    std::array<std::array<WorldModelInstance,8>,death_frames+1> instances={};std::array<std::array<float,8>,death_frames+1> weights={};std::array<unsigned,death_frames+1> counts={};unsigned fallen=0;
    for(unsigned i=0;i<count;++i){
        unsigned group=0;float weight=crowd[i].yielded?1.f:0.f;
        if(crowd[i].yielded){fallen++;if(p->stats.polished){const float f=std::clamp(crowd[i].defeat_phase,0.f,1.f)*(death_frames-1);const unsigned key=std::min(death_frames-1,unsigned(f));group=key+1;weight=f-key;}}
        const unsigned at=counts[group]++;instances[group][at]={float(crowd[i].x-x),float(crowd[i].y),float(crowd[i].z-z),crowd[i].yaw,1};weights[group][at]=weight;
    }
    for(unsigned i=0;i<counts.size();i++)if(counts[i]&&!world_model_pose_blend_upload(p->arena_crowd[i],instances[i].data(),weights[i].data(),counts[i]))return false;
    p->crowd_counts=counts;p->crowd_x=x;p->crowd_z=z;p->stats.crowd_instances=count;p->stats.fallen_instances=fallen;p->stats.peak_fallen_instances=std::max(p->stats.peak_fallen_instances,fallen);return true;
}
void world_combat_motion_draw_crowd(WorldCombatMotion *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!p->crowd_ready||!program)return;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->crowd_x-x),float(-y),float(p->crowd_z-z));glUniform2f(u.anchor,float(p->crowd_x),float(p->crowd_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop,0,0,0);glUniform1f(u.reactive,shadow?0:.85f);
    for(unsigned i=0;i<p->arena_crowd.size();i++)if(p->crowd_counts[i])world_model_pose_blend_draw(p->arena_crowd[i],program);
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
    if(!p||count>8||(count&&!enemies)||!root_ok(x,feet,z)||!std::isfinite(yaw)||!std::isfinite(dt)||!valid_motion_combat(player_combat))return;
    for(unsigned i=0;i<count;++i)if(!root_ok(enemies[i].x,enemies[i].y,enemies[i].z)||!valid_motion_combat(enemies[i].combat))return;
    // New runs reset their real shared clock; stable actor IDs must not inherit
    // the previous run's reaction/collapse/serial ownership.
    if(p->initialized&&player_combat.clock+1.e-6<p->actors[0].last_clock){p->actors={};p->initialized=false;}
    if(p->initialized&&paused)return;
    bool contact=player_combat.enemy_hits>p->actors[0].enemy_hits||player_combat.blocks>p->actors[0].blocks||
        (player_combat.state==TrainingState::Lost&&!p->actors[0].defeated);
    for(unsigned i=0;i<count;++i){const auto &c=enemies[i].combat;const auto &a=p->actors[i+1];
        contact|=c.hits>a.hits||c.parries>a.parries||c.guarded>a.guarded||(c.state==TrainingState::Won&&!a.defeated);
    }
    // Commit all newly published contacts once before their common hit-stop.
    if(p->initialized&&player_combat.impact_pause>0&&!contact)return;
    dt=paused||player_combat.impact_pause>0?0.f:std::clamp(dt,0.f,.05f);p->clock+=dt;
    double travel=p->initialized?std::hypot(x-p->last_x,z-p->last_z):0;
    const bool pose_cut=!p->initialized||travel>10;p->last_x=x;p->last_z=z;p->initialized=true;if(travel>1)travel=0;
    p->x=std::floor(x/128)*128;p->z=std::floor(z/128)*128;
    p->stats.actors=1;p->stats.active_enemies=0;
    const float reaction_duration=p->stats.polished?world_combat_motion::reaction_duration:.28f;
    for(unsigned who=0;who<actor_capacity;++who){
        auto &a=p->actors[who];
        if(who>count){a.active=false;continue;}
        const auto &c=who?enemies[who-1].combat:player_combat;
        const bool walking=who&&enemies[who-1].walking;
        const double ax=who?enemies[who-1].x:x,ay=who?enemies[who-1].y:feet,az=who?enemies[who-1].z:z;
        const bool visible=!who||enemies[who-1].visible;
        const bool local_cut=pose_cut||!a.root_ready||std::hypot(ax-a.last_x,az-a.last_z)>1;
        const double moved=a.root_ready&&!local_cut?std::hypot(ax-a.last_x,az-a.last_z):0;
        a.last_x=ax;a.last_z=az;a.root_ready=true;a.last_clock=c.clock;
        if(who){
            if(local_cut)a.phase=0;
            else if(walking){a.phase+=float(moved/1.3);a.phase-=std::floor(a.phase);}
            const float facing=float(std::atan2(x-ax,z-az));
            if(!a.facing_ready||local_cut){a.facing_yaw=facing;a.facing_ready=true;}
            else a.facing_yaw+=std::clamp(float(std::remainder(double(facing-a.facing_yaw),6.28318530718)),-2.8f*dt,2.8f*dt);
            a.facing_yaw=float(std::remainder(double(a.facing_yaw),6.28318530718));
        }
        p->instances[who]={float(ax-p->x),float(ay),float(az-p->z),who?a.facing_yaw:yaw,1};
        if((!who&&c.enemy_hits>a.enemy_hits)||(who&&c.hits>a.hits)){
            a.hit=reaction_duration;a.reaction_direction=unsigned(who?c.attack_direction:c.enemy_direction);
            if(p->stats.polished)++p->stats.directional_reactions;
        }
        if(who&&c.parries>a.parries)a.parry=.30f;
        if(!who&&c.blocks>a.blocks)a.parry=.20f;
        if(who&&c.guarded>a.guarded)a.parry=.30f;
        a.hits=c.hits;a.enemy_hits=c.enemy_hits;a.parries=c.parries;a.blocks=c.blocks;a.guarded=c.guarded;
        const bool defeated=p->stats.polished&&c.state==(who?TrainingState::Won:TrainingState::Lost);
        if(defeated&&!a.defeated){a.death_time=0;++p->stats.death_starts;}
        else if(defeated)a.death_time=std::min(world_combat_motion::death_duration,a.death_time+dt);
        else a.death_time=0;
        a.defeated=defeated;a.active=visible;
        if(!visible){a.hit=a.parry=a.guard=0;a.walk=0;a.attacking=false;a.combo=false;continue;}
        if(who){++p->stats.actors;++p->stats.active_enemies;}
        a.hit=std::max(0.f,a.hit-dt);a.parry=std::max(0.f,a.parry-dt);
        unsigned clip=0;float phase=.5f-.5f*std::cos(p->clock*1.6f+who*.3f);bool direct=false;
        const auto pp=who?c.enemy_phase==TrainingEnemyPhase::Windup?TrainingPlayerPhase::Windup:c.enemy_phase==TrainingEnemyPhase::Strike?TrainingPlayerPhase::Strike:TrainingPlayerPhase::Recovery:c.player_phase;
        const bool attack=who?c.engaged()&&(c.enemy_phase==TrainingEnemyPhase::Windup||c.enemy_phase==TrainingEnemyPhase::Strike||c.enemy_phase==TrainingEnemyPhase::Recovery):c.engaged()&&c.player_phase!=TrainingPlayerPhase::Ready;
        const bool new_serial=!who&&c.attack_start_serial!=a.attack_serial;
        if(attack&&(!a.attacking||new_serial)){
            const double duration=who?c.enemy_duration:c.player_duration;
            const bool chain=!who&&a.attacking&&new_serial&&a.attack_serial&&c.combo_count>1&&pp==TrainingPlayerPhase::Windup;
            a.combo=chain;a.combo_from=a.attack_direction;a.combo_to=unsigned(c.attack_direction);
            a.combo_turn=a.turn;
            if(chain){a.attack_leadin=std::min(.045f,float(duration)*.34f);++p->stats.combo_transitions;}
            else a.attack_leadin=a.guard>.001f?std::min(.09f,float(duration)*.3f):0.f;
            a.attack_guard=a.guard;a.attack_guard_direction=unsigned(a.guard_direction);
            a.attack_direction=unsigned(who?c.enemy_direction:c.attack_direction);
        }
        if(!who)a.attack_serial=c.attack_start_serial;
        a.attacking=attack;
        const bool reacting=a.hit>0||a.parry>0;if(reacting)a.guard=0;
        const unsigned desired=unsigned(who?c.enemy_guard:c.selected);
        const bool guard=!reacting&&!attack&&(who?c.engaged()&&c.enemy_phase!=TrainingEnemyPhase::Stagger&&!walking:c.engaged()&&c.guard);
        if(desired!=unsigned(a.guard_direction)&&a.guard>.01f)a.guard=std::max(0.f,a.guard-dt*12);
        else{if(a.guard<.01f)a.guard_direction=float(desired);a.guard=std::clamp(a.guard+(guard?dt*10:-dt*12),0.f,1.f);}
        if(a.defeated){clip=19;phase=a.death_time/world_combat_motion::death_duration;a.walk=0;a.guard=0;a.attacking=false;a.combo=false;}
        else if(who&&c.state==TrainingState::Won){clip=12;phase=std::min(.55f,1-a.hit/.28f);a.walk=0;}
        else if(a.hit>0){clip=p->stats.polished?14+a.reaction_direction:12;phase=1-a.hit/reaction_duration;a.walk=0;}
        else if(a.parry>0){clip=13;phase=1-a.parry/(who?.30f:.20f);a.walk=0;}
        else if(attack){
            const double t=who?c.enemy_time:c.player_time,duration=who?c.enemy_duration:c.player_duration;
            if(pp==TrainingPlayerPhase::Windup&&a.combo&&t<a.attack_leadin){
                a.slot=p->pose_count+3+a.combo_from*5+a.combo_to;p->weights[who]=smooth(float(t/a.attack_leadin));direct=true;
            }else if(pp==TrainingPlayerPhase::Windup&&t<a.attack_leadin){clip=7+a.attack_guard_direction;phase=smooth(a.attack_guard*float(1-t/a.attack_leadin));}
            else{
                clip=2+unsigned(who?c.enemy_direction:c.attack_direction);
                phase=attack_phase(pp,t,duration,a.attack_leadin,p->stats.polished);
                // A pending link converges to the exact preloaded recovery
                // endpoint before its serial changes, keeping feet/grip continuous.
                if(!who&&c.pending_attack&&pp==TrainingPlayerPhase::Recovery&&t<=WorldTrainingCombat::combo_link_seconds)
                    phase=.60f+(8.f/11.f-.60f)*smooth(float(t/WorldTrainingCombat::combo_link_seconds));
            }
            a.walk=0;
        }else if(a.guard>.001f){clip=7+unsigned(a.guard_direction);phase=smooth(a.guard);a.walk=0;}
        else if(who&&walking){clip=1;phase=a.phase;a.walk=0;}
        else if(!who){
            const bool moving=travel>.0005;
            if(moving){
                if(a.walk==0){a.walk=1;a.transition=0;a.phase=0;}
                if(a.walk==3||a.walk==4)a.walk=2;
                if(a.walk==1){a.transition=std::min(1.f,a.transition+dt/.18f);a.slot=p->pose_count;p->weights[who]=smooth(a.transition);direct=true;if(a.transition>=1)a.walk=2;}
                else{a.phase+=float(travel/1.3);a.phase-=std::floor(a.phase);clip=1;phase=a.phase;}
            }else if(a.walk!=0){
                if(a.walk==1){a.slot=p->pose_count;a.transition=std::max(0.f,a.transition-dt/.14f);p->weights[who]=smooth(a.transition);direct=true;if(a.transition<=0)a.walk=0;}
                else{
                    if(a.walk==2){a.walk=3;a.transition=0;a.settle_from=a.phase;a.settle_to=std::ceil(a.phase*2)*.5f;}
                    if(a.walk==3){a.transition=std::min(1.f,a.transition+dt/.10f);phase=a.settle_from+(a.settle_to-a.settle_from)*smooth(a.transition);phase-=std::floor(phase);clip=1;if(a.transition>=1){a.walk=4;a.transition=0;a.half=(int(std::lround(a.settle_to*2))&1);}}
                    else{a.transition=std::min(1.f,a.transition+dt/.18f);a.slot=p->pose_count+1+a.half;p->weights[who]=smooth(a.transition);direct=true;if(a.transition>=1)a.walk=0;}
                }
            }
        }
        if(!direct){const auto &q=clips[clip];const float f=std::clamp(phase,0.f,q.loop?.999999f:1.f)*(q.loop?q.count:q.count-1);const unsigned key=std::min(q.count-1,unsigned(f));a.slot=q.base+key;p->weights[who]=f-key;}
        const bool turning=a.attacking&&a.hit<=0&&a.parry<=0&&!a.defeated;
        if(turning&&p->weapon_sockets){
            const unsigned d=unsigned(who?c.enemy_direction:c.attack_direction);const auto &q=clips[2+d];
            const float f=.36f*(q.count-1);const unsigned k=unsigned(f);const float blend=f-k;
            const auto *sx=p->sockets[q.base+k][1],*sy=p->sockets[q.base+k+1][1];
            const float tx=sx[0]+(sy[0]-sx[0])*blend,tz=sx[2]+(sy[2]-sx[2])*blend;
            double t=who?c.enemy_time:c.player_time,duration=who?c.enemy_duration:c.player_duration;
            if(pp==TrainingPlayerPhase::Windup){t=std::max(0.,t-a.attack_leadin);duration=std::max(.001,duration-a.attack_leadin);}
            const float fraction=float(std::clamp(t/std::max(.001,duration),0.,1.));
            const float weight=pp==TrainingPlayerPhase::Windup?smooth(fraction):pp==TrainingPlayerPhase::Strike?1.f:1-smooth(fraction);
            if(a.combo&&pp==TrainingPlayerPhase::Windup&&c.player_time<a.attack_leadin)a.turn=a.combo_turn*(1-smooth(float(c.player_time/a.attack_leadin)));
            else a.turn=std::clamp(-std::atan2(tx,tz),-1.15f,1.15f)*weight;
        }else a.turn*=std::exp(-dt*16);
        p->instances[who].yaw+=a.turn;
    }
    p->stats.peak_actors=std::max(p->stats.peak_actors,p->stats.actors);
    auto upload=[&](WorldModelPoseBlend *batch,Upload &committed,const WorldModelInstance *instances,const float *weights,unsigned n){
        bool same=committed.count==n;
        for(unsigned i=0;same&&i<n;++i)same=committed.instances[i].x==instances[i].x&&committed.instances[i].y==instances[i].y&&committed.instances[i].z==instances[i].z&&committed.instances[i].yaw==instances[i].yaw&&committed.instances[i].scale==instances[i].scale&&committed.weights[i]==weights[i];
        if(same)return;
        if(world_model_pose_blend_upload(batch,instances,weights,n)){std::copy_n(instances,n,committed.instances.begin());std::copy_n(weights,n,committed.weights.begin());committed.count=n;++p->stats.uploads;}
    };
    for(unsigned slot=0;slot<p->batch_count;++slot){
        std::array<WorldModelInstance,actor_capacity> instances={};std::array<float,actor_capacity> weights={};unsigned n=0;
        for(unsigned who=0;who<actor_capacity;++who)if(p->actors[who].active&&p->actors[who].slot==slot){instances[n]=p->instances[who];weights[n++]=p->weights[who];}
        if(n)upload(p->body.blend[slot],p->body.uploaded[slot],instances.data(),weights.data(),n);
        n=0;for(unsigned who=1;who<actor_capacity;++who)if(p->actors[who].active&&p->actors[who].slot==slot){instances[n]=p->instances[who];weights[n++]=p->weights[who];}
        if(n)upload(p->enemy_only[slot],p->enemy_uploaded[slot],instances.data(),weights.data(),n);
    }
    const WorldModelInstance arms={0,-1.15f,.05f,3.14159265359f,.80f};p->arms_instance=arms;
    upload(p->arms.blend[p->actors[0].slot],p->arms.uploaded[p->actors[0].slot],&arms,p->weights.data(),1);
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
    auto &u=p->locations(program);glUseProgram(program);glUniform3f(u.offset,float(p->x-x),float(-y),float(p->z-z));glUniform2f(u.anchor,float(p->x),float(p->z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop,0,0,0);glUniform1f(u.reactive,shadow?0:.85f);
    std::array<bool,batches> drawn={};
    for(unsigned who=show_player?0:1;who<actor_capacity;++who)if(p->actors[who].active){
        const unsigned slot=p->actors[who].slot;if(drawn[slot])continue;drawn[slot]=true;
        world_model_pose_blend_draw(show_player?p->body.blend[slot]:p->enemy_only[slot],program);
    }
    glUniform1f(u.reactive,0);
}
void world_combat_motion_draw_arms(WorldCombatMotion *p,unsigned program){if(p&&program)world_model_pose_blend_draw(p->arms.blend[p->actors[0].slot],program);}
bool world_combat_motion_blade(const WorldCombatMotion *p,unsigned actor,WorldCombatBlade &out,bool viewmodel){
    if(!p||!p->weapon_sockets||actor>=actor_capacity||!p->actors[actor].active||(viewmodel&&actor))return false;
    const auto slot=p->actors[actor].slot;unsigned a=slot,b=slot;
    if(slot<p->pose_count){for(const auto &c:clips)if(slot>=c.base&&slot<c.base+c.count){b=c.base+(c.loop?(slot-c.base+1)%c.count:std::min(slot-c.base+1,c.count-1));break;}}
    else if(slot<p->pose_count+3){a=slot==p->pose_count?0:slot==p->pose_count+1?4:8;b=slot==p->pose_count?4:0;}
    else{const unsigned pair=slot-p->pose_count-3;a=12+12*(pair/5)+8;b=12+12*(pair%5);}
    const auto &instance=viewmodel?p->arms_instance:p->instances[actor];const double cs=std::cos(instance.yaw),sn=std::sin(instance.yaw),weight=p->weights[actor];
    for(unsigned point=0;point<2;++point){double local[3];for(unsigned k=0;k<3;++k)local[k]=p->sockets[a][point][k]+(p->sockets[b][point][k]-p->sockets[a][point][k])*weight;
        if(viewmodel){local[0]+=.25;local[2]+=.65;}auto *target=point?out.tip:out.base;target[0]=(cs*local[0]+sn*local[2])*instance.scale+instance.x+(viewmodel?0:p->x);target[1]=local[1]*instance.scale+instance.y;target[2]=(-sn*local[0]+cs*local[2])*instance.scale+instance.z+(viewmodel?0:p->z);
    }return true;
}
WorldCombatMotionStats world_combat_motion_stats(const WorldCombatMotion *p){return p?p->stats:WorldCombatMotionStats{};}
WorldCombatPose world_combat_motion_pose(const WorldCombatMotion *p,unsigned actor){
    if(!p||actor>=p->actors.size())return {};
    const auto &a=p->actors[actor];return {a.slot,unsigned(a.guard_direction),a.guard,p->weights[actor],a.active,a.reaction_direction,a.defeated,a.death_time/world_combat_motion::death_duration,a.slot>=p->pose_count+3,a.combo_from,a.combo_to};
}
void world_combat_motion_free(WorldCombatMotion *p){if(!p)return;for(auto *b:p->arena_crowd)if(b)world_model_pose_blend_free(b);for(auto *b:p->enemy_only)if(b)world_model_pose_blend_free(b);for(auto *bank:{&p->body,&p->arms}){for(auto *b:bank->blend)if(b)world_model_pose_blend_free(b);for(auto *a:bank->assets)if(a)world_model_free(a);}delete p;}
