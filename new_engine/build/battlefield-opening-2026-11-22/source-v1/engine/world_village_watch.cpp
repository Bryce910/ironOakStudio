#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_watch.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
constexpr std::size_t actor_count=world_village_watch_actor_count, pose_count=world_village_watch_pose_count;
static_assert(std::size(world_village_watch_stations)==actor_count);
static_assert(world_village_watch_cycle_seconds>0);
struct Batch {
    WorldModelPoseBlend *blend=nullptr;
    std::array<WorldModelInstance,actor_count> next={}, uploaded={};
    std::array<float,actor_count> weights={}, uploaded_weights={};
    std::size_t size=0, uploaded_size=0;
};
struct Uniforms {unsigned long long epoch=0;
    GLuint program=0;
    GLint offset=-1, anchor=-1, grass=-1, vegetation=-1, lod=-1, wind=-1, height=-1, reactive=-1, prop_lod=-1;
};
}
struct WorldVillageWatch {
    std::array<WorldModelAsset *,pose_count> poses={};
    std::array<Batch,pose_count> batches={};
    std::array<WorldVillageWatchActor,actor_count> actors={};
    std::array<Uniforms,2> uniforms={};
    unsigned next_uniform=0;
    double x=0, z=0, anchor_x=0, anchor_z=0;
    WorldVillageWatchStats stats;
    Uniforms &locations(GLuint program) {
        for(auto &u:uniforms) if(u.program==program&&u.epoch==world_program_epoch) return u;
        auto &u=uniforms[next_uniform++%uniforms.size()]; u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset"); u.anchor=glGetUniformLocation(program,"uWorldAnchor");
        u.grass=glGetUniformLocation(program,"uGrass"); u.vegetation=glGetUniformLocation(program,"uVegetation");
        u.lod=glGetUniformLocation(program,"uLodLevel"); u.wind=glGetUniformLocation(program,"uWind");
        u.height=glGetUniformLocation(program,"uTreeHeight"); u.reactive=glGetUniformLocation(program,"uActorReactive");
        u.prop_lod=glGetUniformLocation(program,"uPropLodRange"); return u;
    }
};
WorldVillageWatch *world_village_watch_load(const char *directory,char *error,std::size_t capacity) {
    if(error&&capacity) error[0]=0;
    if(!directory||!directory[0]) return nullptr;
    auto *p=new(std::nothrow) WorldVillageWatch; if(!p) return nullptr;
    world_atlas_village_origin(&p->x,&p->z);
    p->anchor_x=std::floor(p->x/128)*128; p->anchor_z=std::floor(p->z/128)*128;
    for(std::size_t i=0;i<pose_count;i++) {
        char path[1024]; const int n=std::snprintf(path,sizeof(path),"%s/village_shield_knight_idle_%02zu.glb",directory,i);
        if(n<0||std::size_t(n)>=sizeof(path)||(p->poses[i]=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION))==nullptr) {
            if(error&&capacity) std::snprintf(error,capacity,"Missing shield watch pose %zu",i);
            world_village_watch_free(p); return nullptr;
        }
        p->batches[i].blend=world_model_pose_blend_create(actor_count,WORLD_MODEL_POSE_BLEND_VERSION);
        if(!p->batches[i].blend) {
            if(error&&capacity) std::snprintf(error,capacity,"Shield watch batch allocation failed");
            world_village_watch_free(p); return nullptr;
        }
    }
    for(std::size_t i=0;i<pose_count;i++) if(!world_model_pose_blend_set_targets(p->batches[i].blend,p->poses[i],p->poses[(i+1)%pose_count])) {
        if(error&&capacity) std::snprintf(error,capacity,"Incompatible shield watch pose %zu",i);
        world_village_watch_free(p); return nullptr;
    }
    p->stats.actors=actor_count; p->stats.loaded_pose_assets=pose_count; return p;
}
void world_village_watch_update(WorldVillageWatch *p,double x,double y,double z,float time,float dt) {
    if(!p||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(time)||time<0||time>1e7f||!std::isfinite(dt)||dt<0||dt>1) return;
    p->stats.visible=p->stats.draw_groups=0; for(auto &b:p->batches) b.size=0;
    for(std::size_t i=0;i<actor_count;i++) {
        const auto &s=world_village_watch_stations[i]; auto &a=p->actors[i];
        double phase=double(time)/world_village_watch_cycle_seconds+s.phase; phase-=std::floor(phase);
        const double frame=phase*pose_count; const auto index=unsigned(frame);
        a.x=p->x+s.x; a.z=p->z+s.z; a.y=24; a.yaw=s.yaw;
        a.cycle_phase=float(phase); a.pose_a=index; a.pose_b=(index+1)%pose_count; a.pose_blend=float(frame-index);
        if(std::hypot(a.x-x,a.z-z)>180) continue;
        auto &b=p->batches[index];
        b.next[b.size]={float(a.x-p->anchor_x),a.y,float(a.z-p->anchor_z),a.yaw,1};
        b.weights[b.size++]=a.pose_blend; p->stats.visible++;
    }
    for(auto &b:p->batches) {
        const bool changed=b.size!=b.uploaded_size||std::memcmp(b.next.data(),b.uploaded.data(),b.size*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.uploaded_weights.data(),b.size*sizeof(float));
        if(changed&&world_model_pose_blend_upload(b.blend,b.next.data(),b.weights.data(),b.size)) {
            std::copy_n(b.next.begin(),b.size,b.uploaded.begin()); std::copy_n(b.weights.begin(),b.size,b.uploaded_weights.begin());
            b.uploaded_size=b.size; p->stats.uploads++;
        }
        if(b.uploaded_size) p->stats.draw_groups++;
    }
    p->stats.updates++;
}
void world_village_watch_draw(WorldVillageWatch *p,unsigned program,double x,double y,double z,bool shadow) {
    if(!p||!program||!p->stats.draw_groups) return;
    auto &u=p->locations(program); glUseProgram(program);
    glUniform3f(u.offset,float(p->anchor_x-x),float(-y),float(p->anchor_z-z)); glUniform2f(u.anchor,float(p->anchor_x),float(p->anchor_z));
    glUniform1i(u.grass,0); glUniform1i(u.vegetation,0); glUniform1i(u.lod,-1); glUniform1f(u.wind,0); glUniform1f(u.height,2);
    glUniform3f(u.prop_lod,0,0,0); glUniform1f(u.reactive,shadow?0.f:.85f);
    for(auto &b:p->batches) if(b.uploaded_size) world_model_pose_blend_draw(b.blend,program);
    glUniform1f(u.reactive,0);
}
WorldVillageWatchStats world_village_watch_stats(const WorldVillageWatch *p) { return p?p->stats:WorldVillageWatchStats{}; }
bool world_village_watch_actor(const WorldVillageWatch *p,std::size_t index,WorldVillageWatchActor *out) {
    if(!p||!out||index>=actor_count) return false;
    *out=p->actors[index]; return true;
}
void world_village_watch_free(WorldVillageWatch *p) {
    if(!p) return;
    for(auto &b:p->batches) if(b.blend) world_model_pose_blend_free(b.blend);
    for(auto *pose:p->poses) if(pose) world_model_free(pose);
    delete p;
}
