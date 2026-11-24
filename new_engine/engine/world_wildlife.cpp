#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_wildlife.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_scenery.h"
#include "engine/world_castle_visibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
constexpr unsigned species_count=std::size(kWildlifeSpecies),poses=12,passes=3;
constexpr double pi=3.14159265358979323846;
static_assert(std::size(kWildlifeSpecies)==species_count);
static_assert(std::size(kWildlifeActors)<=world_wildlife_capacity);
struct Batch {
    WorldModelPoseBlend *gpu=nullptr;
    std::array<WorldModelInstance,world_wildlife_capacity> next={},uploaded={};
    std::array<float,world_wildlife_capacity> weights={},old_weights={};
    std::size_t count=0,old_count=0;
};
struct Uniforms {unsigned long long epoch=0;
    unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop_lod=-1;
};
bool supported(const WorldScenery *scene,const WildlifeActorDef &d,double x,double z,float yaw,float &y){
    const auto &s=kWildlifeSpecies[d.species];
    if(s.airborne){y=world_atlas_height(x,z)+30.f;return std::isfinite(y);}
    float lo=1e9f,hi=-1e9f;
    const double c=std::cos(yaw),r=std::sin(yaw);
    for(int dz=-1;dz<=1;dz++)for(int dx=-1;dx<=1;dx++){
        const double lx=dx*s.foot_half_x,lz=dz*s.foot_half_z;
        const double px=x+c*lx+r*lz,pz=z-r*lx+c*lz;
        const float h=world_atlas_height(px,pz);
        if(!std::isfinite(h)||h<.3f||world_atlas_local_water(px,pz,nullptr,nullptr))return false;
        lo=std::min(lo,h);hi=std::max(hi,h);
    }
    if(hi-lo>.05f)return false;
    y=hi+.005f;
    // The full pose sphere radius, rather than only the narrow contact soles,
    // keeps bodies clear of walls/trunks. Authoring also proves entire routes.
    return world_scenery_can_move(scene,d.x,d.z,x,z,y,s.height,s.bound_radius);
}
}
struct WorldWildlife {
    char directory[768]={};
    bool expanded_jungle=false;
    std::array<WildlifeHabitatDef,3> habitats={kWildlifeHabitats[0],kWildlifeHabitats[1],kWildlifeHabitats[2]};
    std::array<WildlifeActorDef,world_wildlife_capacity> catalog={};
    std::size_t catalog_count=std::size(kWildlifeActors);
    std::array<WorldModelAsset *,species_count*poses> assets={};
    std::array<Batch,species_count*poses*passes> batches={};
    std::array<WorldWildlifeActor,world_wildlife_capacity> actors={};
    std::array<Uniforms,2> uniforms={};unsigned next_uniform=0;
    double anchor_x=0,anchor_z=0;
    WorldWildlifeStats stats;
    WorldWildlifePreparationStats preparation;
    Uniforms &locations(unsigned program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");
        u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");
        u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");
        u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");
        u.prop_lod=glGetUniformLocation(program,"uPropLodRange");return u;
    }
    void unload(){
        for(auto &b:batches){if(b.gpu)world_model_pose_blend_free(b.gpu);b.gpu=nullptr;b.count=b.old_count=0;}
        for(auto &a:assets){if(a)world_model_free(a);a=nullptr;}
        if(stats.habitat>=0)stats.unloads++;
        stats.habitat=-1;stats.actors=stats.walking=stats.idling=stats.flying=stats.species=stats.pose_assets=0;
        stats.visible=stats.shadow_visible=stats.reflection_visible=stats.draw_groups=0;
    }
};
WorldWildlife *world_wildlife_create(const char *directory){
    if(!directory||!*directory||std::strlen(directory)>=sizeof(WorldWildlife::directory))return nullptr;
    auto *w=new(std::nothrow)WorldWildlife;
    if(w){std::strcpy(w->directory,directory);std::copy(std::begin(kWildlifeActors),std::end(kWildlifeActors),w->catalog.begin());}
    return w;
}
bool world_wildlife_expand_jungle(WorldWildlife *w){
    if(!w||w->stats.habitat>=0)return false;
    w->expanded_jungle=true;w->catalog_count=0;
    for(const auto &a:kWildlifeActors)if(a.habitat!=2)w->catalog[w->catalog_count++]=a;
    constexpr WildlifeActorDef jungle[]={
        {7,2,-26780.,9612.496705929329,0,0,0,14.94f},
        {7,2,-26786.,9608.496705929329,0,0,0,15.77f},
        {6,2,-26798.,9409.496705929329,.3f,2.2f,.48f,.4f},
        {6,2,-26793.,9409.496705929329,1.2f,2.f,.44f,2.2f},
        {7,2,-26629.,9355.496705929329,.5f,2.5f,.6f,1.2f},
        {7,2,-26624.,9355.496705929329,2.f,2.1f,.55f,3.5f},
        {8,2,-26714.,9708.496705929329,0,21,5.8f,.2f},
        {8,2,-26714.,9708.496705929329,0,24,5.8f,.42f},
        {8,2,-26714.,9708.496705929329,0,27,5.8f,.64f},
        {8,2,-26703.,9340.496705929329,0,26,5.2f,1.1f},
        {8,2,-26703.,9340.496705929329,0,30,5.2f,1.32f},
        {8,2,-26703.,9340.496705929329,0,34,5.2f,1.54f},
    };
    static_assert(18+std::size(jungle)<=world_wildlife_capacity);
    for(const auto &a:jungle)w->catalog[w->catalog_count++]=a;
    w->habitats[2]={"rainforest",-26712.,9522.496705929329,310,340,105,0x1c0u};
    return true;
}
int world_wildlife_desired(const WorldWildlife *w,double x,double z){
    if(!w||!std::isfinite(x)||!std::isfinite(z))return -2;
    if(w->stats.habitat>=0){const auto &h=w->habitats[w->stats.habitat];if(std::hypot(x-h.center_x,z-h.center_z)<=h.exit_radius)return w->stats.habitat;}
    for(unsigned i=0;i<std::size(kWildlifeHabitats);i++){const auto &h=w->habitats[i];if(std::hypot(x-h.center_x,z-h.center_z)<=h.enter_radius)return int(i);}
    return -1;
}
int world_wildlife_loading_desired(const WorldWildlife *w,double x,double z){
    const int wanted=world_wildlife_desired(w,x,z);
    if(wanted!=-1)return wanted; // preserves current-bank priority/invalid input
    for(unsigned i=0;i<std::size(kWildlifeHabitats);i++){
        const auto &h=w->habitats[i];
        if(std::hypot(x-h.center_x,z-h.center_z)<=h.exit_radius)return int(i);
    }
    return -1;
}
bool world_wildlife_commit(WorldWildlife *w,int habitat,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!w||habitat< -1||habitat>=int(std::size(kWildlifeHabitats)))return false;
    if(habitat==w->stats.habitat)return true;
    w->unload();if(habitat<0)return true;
    const auto &h=w->habitats[habitat];
    auto fail=[&](const char *detail){
        if(error&&capacity)std::snprintf(error,capacity,"Wildlife %s: %s",h.name,detail);
        w->unload();w->stats.load_failures++;return false;
    };
    for(unsigned s=0;s<species_count;s++)if(h.species_mask&(1u<<s)){
        for(unsigned p=0;p<poses;p++){
            char path[1024]={};const int n=std::snprintf(path,sizeof(path),"%s/%s_%s_%02u.glb",w->directory,kWildlifeSpecies[s].stem,p<8?"walk":"idle",p<8?p:p-8);
            if(n<0||std::size_t(n)>=sizeof(path))return fail("path too long");
            w->assets[s*poses+p]=kWildlifeSpecies[s].airborne?world_model_load_pose_origin(path,WORLD_MODEL_POSE_BLEND_VERSION):world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!w->assets[s*poses+p])return fail(path);
        }
        for(unsigned pass=0;pass<passes;pass++)for(unsigned p=0;p<poses;p++){
            auto &b=w->batches[(pass*species_count+s)*poses+p];
            const unsigned next=p<8?(p+1)%8:8+(p-8+1)%4;
            b.gpu=world_model_pose_blend_create(world_wildlife_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.gpu||!world_model_pose_blend_set_targets(b.gpu,w->assets[s*poses+p],w->assets[s*poses+next]))return fail("incompatible adjacent pose targets");
        }
        w->stats.species++;w->stats.pose_assets+=poses;
    }
    w->anchor_x=std::floor(h.center_x/128)*128;w->anchor_z=std::floor(h.center_z/128)*128;
    w->stats.habitat=habitat;w->stats.loads++;
    w->stats.peak_pose_assets=std::max(w->stats.peak_pose_assets,w->stats.pose_assets);
    return true;
}
void world_wildlife_update(WorldWildlife *w,const WorldScenery *scene,float time){
    if(!w||!std::isfinite(time)||time<0||time>1e7f)return;
    w->stats.actors=w->stats.walking=w->stats.idling=w->stats.flying=0;
    if(w->stats.habitat<0)return;
    for(std::size_t index=0;index<w->catalog_count;index++){
        const auto &d=w->catalog[index];if(d.habitat!=w->stats.habitat||w->stats.actors>=world_wildlife_capacity)continue;
        const auto &s=kWildlifeSpecies[d.species];WorldWildlifeActor a;
        a.species=d.species;a.habitat=d.habitat;a.walking=d.route_radius>0&&d.speed>0;
        a.x=d.x;a.z=d.z;a.yaw=d.yaw;double gait_clock=time;
        if(a.walking){
            // Wander, then rest, without discontinuities in the ground route.
            double travel=time;
            if(w->expanded_jungle&&d.habitat==2&&!s.airborne){
                const double clock=time+d.phase*4,cycle=std::floor(clock/30),part=std::fmod(clock,30.);
                travel=cycle*20+std::min(part,20.);a.walking=part<20;
            }
            gait_clock=travel;const double theta=d.phase+travel*d.speed/d.route_radius;
            a.x+=d.route_radius*std::cos(theta);a.z+=d.route_radius*std::sin(theta);
            a.yaw=float(std::remainder(-theta,2*pi));a.speed=a.walking?d.speed:0;
        }
        if(!supported(scene,d,a.x,a.z,a.yaw,a.y)){
            // Deterministic safe fallback. Invalid authoring never sends an
            // animal through a wall, across water, or floating down a slope.
            w->stats.unsafe_roots++;a.x=d.x;a.z=d.z;a.yaw=d.yaw;a.speed=0;a.walking=false;
            if(!supported(scene,d,a.x,a.z,a.yaw,a.y))continue;
        }
        if(s.airborne){a.y+=float(1.8*std::sin(time*.45+d.phase)+d.phase*1.4);a.walking=std::fmod(time+d.phase*3,11.f)<7;}
        double phase=a.walking?gait_clock*d.speed/s.stride+d.phase/(2*pi):(double(time)/s.idle_seconds+d.phase/(2*pi));
        phase-=std::floor(phase);const unsigned count=a.walking?8:4,base=a.walking?0:8;
        const double f=phase*count;const unsigned p=unsigned(f);
        a.cycle_phase=float(phase);a.pose_a=base+p;a.pose_b=base+(p+1)%count;a.pose_blend=float(f-p);
        w->actors[w->stats.actors++]=a;
        if(a.walking)w->stats.walking++;else w->stats.idling++;
        if(s.airborne)w->stats.flying++;
    }
    w->stats.updates++;
}
bool world_wildlife_prepare(WorldWildlife *w,const WorldWildlifePrepareFrame &f){
    if(!w)return false;
    auto &stats=w->preparation;
    auto fail=[&](){stats.failures++;return false;};
    if(!f.loading||!f.program||f.pass>=passes||
       !std::isfinite(f.x)||!std::isfinite(f.y)||!std::isfinite(f.z)||
       !std::isfinite(f.yaw)||!std::isfinite(f.pitch)||
       std::abs(f.x)>1e8||std::abs(f.y)>1e8||std::abs(f.z)>1e8||
       (f.constrain_minimum_y&&(!std::isfinite(f.minimum_y)||std::abs(f.minimum_y)>1e8)))return fail();
    stats.calls++;
    if(w->stats.habitat<0)return true;
    // Isolated loading-only ownership: regular per-pass batches and their CPU
    // commit records are never borrowed as temporary instance storage.
    auto *scratch=world_model_pose_blend_create(1,WORLD_MODEL_POSE_BLEND_VERSION);
    if(!scratch)return fail();
    const auto &h=w->habitats[w->stats.habitat];auto &u=w->locations(f.program);
    glUseProgram(f.program);
    glUniform3f(u.offset,float(w->anchor_x-f.x),float(-f.y),float(w->anchor_z-f.z));
    glUniform2f(u.anchor,float(w->anchor_x),float(w->anchor_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);
    glUniform1f(u.wind,0);glUniform1f(u.height,1);glUniform1f(u.reactive,f.pass==2?0.f:.85f);
    glUniform3f(u.prop_lod,h.draw_radius-8,8,1);
    bool ready=glGetError()==GL_NO_ERROR;
    const double cp=std::cos(double(f.pitch)),forward_x=std::sin(double(f.yaw))*cp,
        forward_y=std::sin(double(f.pitch)),forward_z=-std::cos(double(f.yaw))*cp;
    for(unsigned species=0;ready&&species<species_count;species++)if(h.species_mask&(1u<<species)){
        const auto &s=kWildlifeSpecies[species];
        const double distance=std::max({12.,double(s.bound_radius)*3.+double(s.height)*2.,
            f.constrain_minimum_y?2.*std::abs(f.minimum_y-f.y):0.});
        const WorldModelInstance instance={float(f.x-w->anchor_x+forward_x*distance),
            float(std::max(f.y+forward_y*distance-double(s.height)*.5,
                f.constrain_minimum_y?f.minimum_y:-1e100)),
            float(f.z-w->anchor_z+forward_z*distance),f.yaw,1};
        const float weight=.5f;
        for(unsigned pose=0;ready&&pose<poses;pose++){
            const unsigned next=pose<8?(pose+1)%8:8+(pose-8+1)%4;
            ready=world_model_pose_blend_set_targets(scratch,w->assets[species*poses+pose],w->assets[species*poses+next])&&
                world_model_pose_blend_upload(scratch,&instance,&weight,1);
            if(!ready)break;
            // Each pose must execute its fragment/material work, rather than
            // fail depth behind an earlier temporary pose. Caller owns depth
            // test/write state and redraws/clears this loading-only target.
            glClear(GL_DEPTH_BUFFER_BIT);
            world_model_pose_blend_draw(scratch,f.program);
            ready=glGetError()==GL_NO_ERROR;
            if(ready)stats.pose_draws++;
        }
    }
    world_model_pose_blend_free(scratch);
    glUniform1f(u.reactive,0);glUniform3f(u.prop_lod,0,0,0);
    ready=glGetError()==GL_NO_ERROR&&ready;
    if(!ready)return fail();
    if(f.pass==2)stats.shadow_passes++;else if(f.pass==1)stats.reflection_passes++;else stats.main_passes++;
    return true;
}
WorldWildlifePreparationStats world_wildlife_preparation_stats(const WorldWildlife *w){return w?w->preparation:WorldWildlifePreparationStats{};}
void world_wildlife_draw(WorldWildlife *w,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass){
    if(!w||!program||pass>=passes||w->stats.habitat<0||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(aspect)||aspect<=0)return;
    const auto &h=w->habitats[w->stats.habitat];
    const auto view=world_vegetation_visibility_view(x,y,z,yaw,pitch,aspect);
    const unsigned begin=pass*species_count*poses,end=begin+species_count*poses;
    for(unsigned i=begin;i<end;i++)w->batches[i].count=0;
    std::size_t visible=0,groups=0;
    for(std::size_t i=0;i<w->stats.actors;i++){
        const auto &a=w->actors[i];const auto &s=kWildlifeSpecies[a.species];
        if(std::hypot(a.x-x,a.z-z)>h.draw_radius)continue;
        if(pass!=2&&world_vegetation_sphere_outside(view,a.x,a.y+s.height*.5,a.z,s.bound_radius,h.draw_radius))continue;
        auto &b=w->batches[begin+a.species*poses+a.pose_a];
        b.next[b.count]={float(a.x-w->anchor_x),a.y,float(a.z-w->anchor_z),a.yaw,1};b.weights[b.count++]=a.pose_blend;visible++;
    }
    auto &u=w->locations(program);glUseProgram(program);
    glUniform3f(u.offset,float(w->anchor_x-x),float(-y),float(w->anchor_z-z));glUniform2f(u.anchor,float(w->anchor_x),float(w->anchor_z));
    glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,1);
    glUniform3f(u.prop_lod,h.draw_radius-8,8,1);glUniform1f(u.reactive,pass==2?0.f:.85f);
    for(unsigned i=begin;i<end;i++){
        auto &b=w->batches[i];if(!b.gpu)continue;
        if(b.count!=b.old_count||std::memcmp(b.next.data(),b.uploaded.data(),b.count*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.old_weights.data(),b.count*sizeof(float))){
            if(world_model_pose_blend_upload(b.gpu,b.next.data(),b.weights.data(),b.count)){
                std::copy_n(b.next.begin(),b.count,b.uploaded.begin());std::copy_n(b.weights.begin(),b.count,b.old_weights.begin());b.old_count=b.count;w->stats.uploads++;
            }
        }
        if(b.old_count){world_model_pose_blend_draw(b.gpu,program);groups++;}
    }
    glUniform1f(u.reactive,0);glUniform3f(u.prop_lod,0,0,0);
    if(pass==0){w->stats.visible=visible;w->stats.draw_groups=groups;}else if(pass==1)w->stats.reflection_visible=visible;else w->stats.shadow_visible=visible;
}
WorldWildlifeStats world_wildlife_stats(const WorldWildlife *w){return w?w->stats:WorldWildlifeStats{};}
bool world_wildlife_actor(const WorldWildlife *w,std::size_t i,WorldWildlifeActor *out){if(!w||!out||i>=w->stats.actors)return false;*out=w->actors[i];return true;}
void world_wildlife_free(WorldWildlife *w){if(!w)return;w->unload();delete w;}
