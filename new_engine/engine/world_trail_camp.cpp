#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_trail_camp.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_scenery.h"
#include "engine/world_castle_visibility.h"
#include "engine/world_village_population.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace {
constexpr unsigned poses=20,people=4,passes=3;
constexpr double pi=3.14159265358979323846;
constexpr float fade_center=114.f,fade_half_width=8.f;
constexpr double cull_distance=double(fade_center+fade_half_width)+1.;
struct Clip {const char *prefix;unsigned base,count;float seconds;};
constexpr Clip clips[]={
    {"village_character_seated_idle",0,4,194.f/24.f},
    {"purchased_character_wayfarer_idle",4,4,4.5f},
    {"purchased_character_craftsman_walk",8,8,1.3333333f},
    {"purchased_character_craftsman_idle",16,4,4.f}
};
struct Batch {WorldModelPoseBlend *gpu=nullptr;std::array<WorldModelInstance,people> next={},old={};std::array<float,people> weights={},old_weights={};unsigned count=0,committed=0;};
struct Uniforms {unsigned long long epoch=0;unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1;};
}
struct WorldTrailCamp {
    char directory[768]={};
    std::array<WorldModelAsset *,poses> assets={};
    std::array<Batch,poses*passes> batches={};
    std::array<WorldTrailCamper,people> actors={};
    std::array<Uniforms,2> uniforms={};unsigned next_uniform=0;
    double center_x=WORLD_TRAIL_CAMP_X,center_y=WORLD_TRAIL_CAMP_Y,center_z=WORLD_TRAIL_CAMP_Z;
    bool arrival=false,arrival_merchant=false;
    std::array<WorldTrailCampPlacement,3> placements={};
    std::array<float,3> facing_yaw={};
    WorldTrailCampStats stats;
    Uniforms &locations(unsigned program){
        for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
        auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
        u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");u.prop=glGetUniformLocation(program,"uPropLodRange");return u;
    }
    void unload(){
        for(auto &b:batches){if(b.gpu)world_model_pose_blend_free(b.gpu);b={};}
        for(auto &a:assets){if(a)world_model_free(a);a=nullptr;}
        if(stats.active)stats.unloads++;
        stats.active=false;stats.actors=stats.walking=stats.seated=stats.visible=stats.pose_assets=0;
    }
};
WorldTrailCamp *world_trail_camp_create(const char *directory){
    if(!directory||!*directory||std::strlen(directory)>=sizeof(WorldTrailCamp::directory))return nullptr;
    auto *c=new(std::nothrow)WorldTrailCamp;if(c)std::strcpy(c->directory,directory);return c;
}
bool world_trail_camp_configure_arrival(WorldTrailCamp *c,double x,double y,double z,const WorldTrailCampPlacement *placements,unsigned count){
    if(!c||c->stats.active||!placements||count!=3||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
        std::abs(x)>1e6||std::abs(y)>1e4||std::abs(z)>1e6)return false;
    for(unsigned i=0;i<count;i++){
        const auto &p=placements[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.yaw)||
            std::hypot(p.x-x,p.z-z)>24.||std::abs(p.y-y)>12.||std::abs(p.yaw)>1e6)return false;
    }
    c->center_x=x;c->center_y=y;c->center_z=z;c->arrival=true;
    // The all-standing opening reuses otherwise-unused seated pose slots for
    // a third existing appearance; seated authoring keeps its real sitting clip.
    c->arrival_merchant=std::none_of(placements,placements+count,[](const WorldTrailCampPlacement &p){return p.seated;});
    std::copy_n(placements,3,c->placements.begin());
    for(unsigned i=0;i<3;i++)c->facing_yaw[i]=placements[i].yaw;
    return true;
}
bool world_trail_camp_focus(WorldTrailCamp *c,int actor,double x,double z,float dt){
    if(!c||!c->arrival||actor< -1||actor>=3||!std::isfinite(x)||!std::isfinite(z)||
        !std::isfinite(dt)||dt<0||dt>.25f)return false;
    for(unsigned i=0;i<3;i++){
        const auto &p=c->placements[i];double goal=p.yaw;
        if(actor==int(i)&&std::hypot(x-p.x,z-p.z)>.1)goal=std::atan2(x-p.x,z-p.z);
        const double difference=std::remainder(goal-double(c->facing_yaw[i]),2*pi);
        const double step=std::clamp(difference*(1.-std::exp(-7.5*double(dt))),-2.7*double(dt),2.7*double(dt));
        c->facing_yaw[i]=float(std::remainder(double(c->facing_yaw[i])+step,2*pi));
    }
    return true;
}
bool world_trail_camp_desired(const WorldTrailCamp *c,double x,double z){return c&&std::isfinite(x)&&std::isfinite(z)&&std::hypot(x-c->center_x,z-c->center_z)<(c->arrival?(c->stats.active?95.:75.):(c->stats.active?300.:260.));}
bool world_trail_camp_commit(WorldTrailCamp *c,bool active,char *error,std::size_t capacity){
    if(error&&capacity)error[0]=0;
    if(!c)return false;
    if(c->stats.active==active)return true;
    c->unload();
    if(!active)return true;
    auto fail=[&](const char *message){if(error&&capacity)std::snprintf(error,capacity,"Trail camp: %s",message);c->unload();c->stats.failures++;return false;};
    for(const auto &clip:clips){
        // The arrival group is planted around its hearth; its unused walking
        // clip and associated GPU batches never enter the memory budget.
        if(c->arrival&&clip.base==8)continue;
        for(unsigned i=0;i<clip.count;i++){
            char path[1024];const int n=std::snprintf(path,sizeof(path),"%s/%s_%02u.glb",c->directory,
                c->arrival_merchant&&clip.base==0?"purchased_character_merchant_idle":clip.prefix,i);
            if(n<0||std::size_t(n)>=sizeof(path))return fail("path too long");
            c->assets[clip.base+i]=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!c->assets[clip.base+i])return fail(path);
        }
        for(unsigned pass=0;pass<passes;pass++)for(unsigned i=0;i<clip.count;i++){
            auto &b=c->batches[pass*poses+clip.base+i];b.gpu=world_model_pose_blend_create(people,WORLD_MODEL_POSE_BLEND_VERSION);
            if(!b.gpu||!world_model_pose_blend_set_targets(b.gpu,c->assets[clip.base+i],c->assets[clip.base+(i+1)%clip.count]))return fail("incompatible retained pose pair");
        }
    }
    c->stats.active=true;c->stats.pose_assets=c->arrival?12:poses;c->stats.loads++;return true;
}
void world_trail_camp_update(WorldTrailCamp *c,const WorldScenery *scene,float time){
    if(!c||!std::isfinite(time)||time<0||time>1e7f)return;
    c->stats.updates++;c->stats.actors=c->stats.walking=c->stats.seated=0;if(!c->stats.active)return;
    for(unsigned i=0;i<people;i++){
        if(c->arrival&&i==3)break;
        WorldTrailCamper a;unsigned clip=0;double phase=time/clips[0].seconds+i*.37;
        a.x=WORLD_TRAIL_CAMP_X-1.;a.z=WORLD_TRAIL_CAMP_Z+(i?3.3:-3.3);a.yaw=i?float(pi):0;a.seated=i<2;
        if(i==2){
            a.x=WORLD_TRAIL_CAMP_X-2.5;a.z=WORLD_TRAIL_CAMP_Z-4.;
            // Face the hearth without rotating the entire planted body. The
            // retained idle already contains an upper-body/head glance.
            a.yaw=float(std::atan2(2.5,4.));clip=1;phase=time/(clips[clip].seconds*1.08)+.23;
        }
        if(i==3){
            const double angle=.3+double(time)*1.05/2.;
            a.x=WORLD_TRAIL_CAMP_X+5.+2.*std::cos(angle);a.z=WORLD_TRAIL_CAMP_Z-1.+2.*std::sin(angle);a.yaw=float(std::remainder(-angle,2*pi));a.walking=true;clip=2;
            // Full two-step source stride, shared with village craftspeople.
            // Translation is1.05m/s; absolute distance drives the gait rather
            // than the old unrelated1.333s preview cycle.
            phase=double(time)*1.05/world_village_walk_stride_m[7]+.13;
            const double previous=angle-.07;
            if(scene&&!world_scenery_can_move(scene,WORLD_TRAIL_CAMP_X+5.+2.*std::cos(previous),WORLD_TRAIL_CAMP_Z-1.+2.*std::sin(previous),a.x,a.z,float(WORLD_TRAIL_CAMP_Y+.015),2.f,.28f)){
                c->stats.unsafe_roots++;a.walking=false;a.x=WORLD_TRAIL_CAMP_X+5.;a.z=WORLD_TRAIL_CAMP_Z-1.;clip=3;phase=time/clips[clip].seconds;
            }
        }
        if(c->arrival){
            const auto &p=c->placements[i];a.x=p.x;a.z=p.z;a.yaw=c->facing_yaw[i];a.walking=false;a.seated=p.seated;
            clip=p.seated?0:(i==1?1:(i==2&&c->arrival_merchant?0:3));
            const double seconds=c->arrival_merchant&&clip==0?4.7:double(clips[clip].seconds);
            phase=double(time)/seconds+double(i)*.37;
        }
        const float ground=world_atlas_height(a.x,a.z);
        if(!std::isfinite(ground)||world_atlas_local_water(a.x,a.z,nullptr,nullptr)){c->stats.unsafe_roots++;continue;}
        // Retained Sitting1 geometry already includes its.480m seat offset;
        // its root, like the tavern patrons, belongs at the15mm floor margin.
        if(c->arrival&&std::abs(double(ground)-c->placements[i].y)>.45){c->stats.unsafe_roots++;continue;}
        a.y=c->arrival?float(c->placements[i].y+.015):ground+.015f;
        phase-=std::floor(phase);const auto &d=clips[clip];const double f=phase*d.count;const unsigned p=unsigned(f);
        a.pose_a=d.base+p;a.pose_b=d.base+(p+1)%d.count;a.blend=float(f-p);c->actors[c->stats.actors++]=a;
        c->stats.walking+=unsigned(a.walking);c->stats.seated+=unsigned(a.seated);
    }
}
void world_trail_camp_draw(WorldTrailCamp *c,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass){
    if(!c||!c->stats.active||!program||pass>=passes||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(aspect)||aspect<=0)return;
    const auto view=world_vegetation_visibility_view(x,y,z,yaw,pitch,aspect);unsigned visible=0;const unsigned first=pass*poses;
    for(unsigned i=first;i<first+poses;i++)c->batches[i].count=0;
    for(unsigned i=0;i<c->stats.actors;i++){
        // The shared color/depth fade is already exactly zero beyond122m.
        // A metre of guard exceeds float rebasing error; these draws cannot
        // contribute color, reactive coverage or shadows.
        const auto &a=c->actors[i];if(std::hypot(a.x-x,a.z-z)>cull_distance)continue;
        if(pass!=2&&world_vegetation_sphere_outside(view,a.x,a.y+1.,a.z,3.,130.))continue;
        auto &b=c->batches[first+a.pose_a];b.next[b.count]={float(a.x-c->center_x),a.y,float(a.z-c->center_z),a.yaw,1.f};b.weights[b.count++]=a.blend;visible++;
    }
    // Preserve cached GPU batches while the entire pass is invisible. The
    // next visible pass reconciles all counts before submitting any draw.
    if(!visible){if(pass==0)c->stats.visible=0;return;}
    auto &u=c->locations(program);glUseProgram(program);glUniform3f(u.offset,float(c->center_x-x),float(-y),float(c->center_z-z));glUniform2f(u.anchor,float(c->center_x),float(c->center_z));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,2);glUniform1f(u.reactive,pass==2?0.f:.85f);glUniform3f(u.prop,fade_center,fade_half_width,1);
    for(unsigned i=first;i<first+poses;i++){
        auto &b=c->batches[i];
        if(b.count!=b.committed||std::memcmp(b.next.data(),b.old.data(),b.count*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.old_weights.data(),b.count*sizeof(float))){
            if(world_model_pose_blend_upload(b.gpu,b.next.data(),b.weights.data(),b.count)){std::copy_n(b.next.begin(),b.count,b.old.begin());std::copy_n(b.weights.begin(),b.count,b.old_weights.begin());b.committed=b.count;c->stats.uploads++;}
        }
        if(b.committed)world_model_pose_blend_draw(b.gpu,program);
    }
    glUniform1f(u.reactive,0);glUniform3f(u.prop,0,0,0);if(pass==0)c->stats.visible=visible;
}
WorldTrailCampStats world_trail_camp_stats(const WorldTrailCamp *c){return c?c->stats:WorldTrailCampStats{};}
bool world_trail_camp_actor(const WorldTrailCamp *c,unsigned i,WorldTrailCamper *out){if(!c||!out||i>=c->stats.actors)return false;*out=c->actors[i];return true;}
void world_trail_camp_free(WorldTrailCamp *c){if(c){c->unload();delete c;}}
