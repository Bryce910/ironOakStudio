#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_castle_training.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace {
constexpr std::size_t count=world_castle_training_actor_count,pose_count=world_castle_training_pose_count;
static_assert(std::size(world_castle_training_stations)==count);
struct Pose {WorldModelAsset *asset=nullptr;};
struct Batch {
    WorldModelPoseBlend *blend=nullptr;
    std::array<WorldModelInstance,count> next={},uploaded={};
    std::array<float,count> weights={},uploaded_weights={};
    std::size_t size=0,uploaded_size=0;
};
struct Uniforms {unsigned long long epoch=0;GLuint program=0;GLint offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop_lod=-1;};
const char *names[]={"West court sparring partner","West court sparring partner","Senior poleaxe pupil","Senior poleaxe pupil","Practice dummy pupil","Practice dummy pupil","Master of arms"};
}
struct WorldCastleTraining {
    std::array<Pose,pose_count> poses={};
    std::array<Batch,world_castle_training_blend_count> batches={};
    std::array<WorldCastleTrainingActor,count> actors={};
    std::array<WorldModelPoseBlend *,24> player_batches={};unsigned player_slot=0;bool player_ready=false;
    double player_x=0,player_z=0;
    std::array<Uniforms,2> uniforms={};unsigned next_uniform=0;
    double x=0,z=0,anchor_x=0,anchor_z=0;WorldCastleTrainingStats stats;
    bool replace_opponent=false,replace_instructor=false;
    bool sparring=false;float sparring_pose=0,sparring_yaw=world_castle_training_stations[0].yaw;
    Uniforms &locations(GLuint program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");
        u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");
        u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");
        u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");
        u.prop_lod=glGetUniformLocation(program,"uPropLodRange");return u;
    }
};
WorldCastleTraining *world_castle_training_load(const char *directory,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!directory||!directory[0])return nullptr;
    auto *p=new(std::nothrow)WorldCastleTraining;if(!p)return nullptr;
    world_atlas_village_origin(&p->x,&p->z);p->anchor_x=std::floor(p->x/128)*128;p->anchor_z=std::floor(p->z/128)*128;
    for(std::size_t i=0;i<pose_count;i++){
        char path[1024];const int n=i<world_castle_training_drill_pose_count?
            std::snprintf(path,sizeof(path),"%s/castle_training_knight_drill_%02zu.glb",directory,i):
            std::snprintf(path,sizeof(path),"%s/village_character_knight_idle_%02zu.glb",directory,i-world_castle_training_drill_pose_count);
        if(n<0||std::size_t(n)>=sizeof(path)||(p->poses[i].asset=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION))==nullptr){
            if(error&&capacity)std::snprintf(error,capacity,"Missing castle practice pose %zu",i);
            world_castle_training_free(p);return nullptr;
        }
        auto &b=p->batches[i];b.blend=world_model_pose_blend_create(count,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!b.blend){if(error&&capacity)std::snprintf(error,capacity,"Castle practice batch allocation failed");world_castle_training_free(p);return nullptr;}
    }
    for(std::size_t i=0;i<pose_count;i++){
        const std::size_t begin=i<world_castle_training_drill_pose_count?0:world_castle_training_drill_pose_count;
        const std::size_t n=i<world_castle_training_drill_pose_count?world_castle_training_drill_pose_count:pose_count-world_castle_training_drill_pose_count;
        const std::size_t next=begin+(i-begin+1)%n;
        if(!world_model_pose_blend_set_targets(p->batches[i].blend,p->poses[i].asset,p->poses[next].asset)){
            if(error&&capacity)std::snprintf(error,capacity,"Incompatible castle practice pose %zu",i);
            world_castle_training_free(p);return nullptr;
        }
    }
    for(unsigned i=0;i<24;i++){
        const unsigned next=(i+1)%24;
        auto *&b=p->player_batches[i];b=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!b||!world_model_pose_blend_set_targets(b,p->poses[i].asset,p->poses[next].asset)){
            if(error&&capacity)std::snprintf(error,capacity,"Player pose batch allocation failed");
            world_castle_training_free(p);return nullptr;
        }
    }
    p->stats.actors=count;p->stats.trainees=count-1;p->stats.instructors=1;p->stats.loaded_pose_assets=pose_count;
    return p;
}
void world_castle_training_replace_instructor(WorldCastleTraining *p,bool active){if(p)p->replace_instructor=active;}
void world_castle_training_update(WorldCastleTraining *p,double x,double y,double z,float time,float dt){
    if(!p||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(time)||time<0||time>1e7f||!std::isfinite(dt)||dt<0||dt>1)return;
    p->stats.visible=p->stats.draw_groups=0;for(auto &b:p->batches)b.size=0;
    for(std::size_t i=0;i<count;i++){
        const auto &s=world_castle_training_stations[i];auto &a=p->actors[i];
        if(p->replace_opponent&&i==0)continue;
        if(p->replace_instructor&&s.instructor)continue;
        if(p->sparring&&i==1)continue; // player's reserved station
        double phase=p->sparring&&i==0?p->sparring_pose:double(time)/s.cycle_seconds+s.phase_offset;phase-=std::floor(phase);
        const unsigned n=s.instructor?4:world_castle_training_drill_pose_count,base=s.instructor?world_castle_training_drill_pose_count:0;
        const double f=phase*n;const unsigned frame=unsigned(f);
        a.name=names[i];a.role=s.instructor?"Instructor":"Poleaxe training";a.x=p->x+s.x;a.z=p->z+s.z;
        if(p->sparring&&i==0){
            const double target=std::atan2(x-a.x,z-a.z),turn=std::remainder(target-p->sparring_yaw,6.283185307179586);
            p->sparring_yaw+=float(std::clamp(turn,-2.8*double(dt),2.8*double(dt)));
        }
        a.y=24;a.yaw=p->sparring&&i==0?p->sparring_yaw:s.yaw;a.cycle_phase=float(phase);a.pose_a=base+frame;a.pose_b=base+(frame+1)%n;a.pose_blend=float(f-frame);a.instructor=s.instructor;
        if(std::hypot(a.x-x,a.z-z)>180)continue;
        auto &b=p->batches[a.pose_a];b.next[b.size]={float(a.x-p->anchor_x),a.y,float(a.z-p->anchor_z),a.yaw,1};b.weights[b.size++]=a.pose_blend;p->stats.visible++;
    }
    for(auto &b:p->batches){
        const bool changed=b.size!=b.uploaded_size||std::memcmp(b.next.data(),b.uploaded.data(),b.size*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.uploaded_weights.data(),b.size*sizeof(float));
        if(changed&&world_model_pose_blend_upload(b.blend,b.next.data(),b.weights.data(),b.size)){
            std::copy_n(b.next.begin(),b.size,b.uploaded.begin());std::copy_n(b.weights.begin(),b.size,b.uploaded_weights.begin());b.uploaded_size=b.size;p->stats.uploads++;
        }
        if(b.uploaded_size)p->stats.draw_groups++;
    }
    p->stats.updates++;
}
void world_castle_training_draw(WorldCastleTraining *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!program||!p->stats.draw_groups)return;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->anchor_x-x),float(-y),float(p->anchor_z-z));glUniform2f(u.anchor,float(p->anchor_x),float(p->anchor_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform3f(u.prop_lod,0,0,0);
    glUniform1f(u.reactive,shadow?0.f:.85f);for(auto &b:p->batches)if(b.uploaded_size)world_model_pose_blend_draw(b.blend,program);glUniform1f(u.reactive,0);
}
void world_castle_training_sparring(WorldCastleTraining *p,bool active,float pose){
    if(!p||!std::isfinite(pose))return;
    if(active&&!p->sparring)p->sparring_yaw=world_castle_training_stations[0].yaw;
    p->sparring=active;p->sparring_pose=std::clamp(pose,0.f,.99999f);
}
void world_castle_training_replace_opponent(WorldCastleTraining *p,bool active){if(p)p->replace_opponent=active;}
WorldCastleTrainingStats world_castle_training_stats(const WorldCastleTraining *p){return p?p->stats:WorldCastleTrainingStats{};}
bool world_castle_training_actor(const WorldCastleTraining *p,std::size_t index,WorldCastleTrainingActor *out){
    if(!p||!out||index>=p->actors.size())return false;
    *out=p->actors[index];return true;
}

bool world_castle_training_player_update(WorldCastleTraining *p,double x,double feet,double z,float yaw,float phase,bool walking){
    if(!p||!std::isfinite(x)||!std::isfinite(feet)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(phase))return false;
    phase-=std::floor(phase);
    const float f=phase*24.f;
    const unsigned slot=unsigned(f);const float weight=f-std::floor(f);
    (void)walking;
    // The player owns a local origin, independent of the village's position.
    p->player_x=std::floor(x/128)*128;p->player_z=std::floor(z/128)*128;
    WorldModelInstance instance={float(x-p->player_x),float(feet),float(z-p->player_z),yaw,1};
    p->player_slot=slot;p->player_ready=world_model_pose_blend_upload(p->player_batches[slot],&instance,&weight,1);return p->player_ready;
}
void world_castle_training_player_draw(WorldCastleTraining *p,unsigned program,double x,double y,double z,bool shadow){
    if(!p||!program||!p->player_ready)return;
    auto &u=p->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(p->player_x-x),float(-y),float(p->player_z-z));glUniform2f(u.anchor,float(p->player_x),float(p->player_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);
    glUniform1f(u.height,2);glUniform3f(u.prop_lod,0,0,0);glUniform1f(u.reactive,shadow?0.f:.85f);
    world_model_pose_blend_draw(p->player_batches[p->player_slot],program);glUniform1f(u.reactive,0);
}

void world_castle_training_free(WorldCastleTraining *p){
    if(!p)return;
    for(auto *b:p->player_batches)if(b)world_model_pose_blend_free(b);
    for(auto &b:p->batches)if(b.blend)world_model_pose_blend_free(b.blend);
    for(auto &pose:p->poses)if(pose.asset)world_model_free(pose.asset);
    delete p;
}
