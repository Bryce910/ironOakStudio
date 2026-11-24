/* CPU-only shield-watch proof. Fixed renderer stubs inspect uploaded roots,
 * adjacent pose targets/weights, pass parity and complete resource ownership.
 * No window, GL context, image import, or GPU commands are created here. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_watch.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

namespace {
constexpr std::size_t actor_count=world_village_watch_actor_count,pose_count=world_village_watch_pose_count;
constexpr double village_x=1000000.25,village_z=-2000000.75,pi=3.14159265358979323846;
constexpr double center_z=village_z+72.5;
std::size_t attempts=0,batch_attempts=0,target_attempts=0,fail_pose=0,fail_batch=0,fail_target=0;
std::size_t pose_live=0,batch_live=0,uploads=0,draws=0,drawn=0,queries=0,allocations=0;
bool track=false,shadow_pass=false;
float reactive=0,wind=0;int grass=0,vegetation=0,lod=-1;
double draw_camera_x=0,draw_camera_y=0,draw_camera_z=0;
}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(n?n:1);}
void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}

struct WorldModelAsset {unsigned index=0;};
struct WorldModelPoseBlend {
    unsigned index=0;WorldModelAsset *a=nullptr,*b=nullptr;
    std::array<WorldModelInstance,actor_count> instances={};
    std::array<float,actor_count> weights={};std::size_t size=0;
};
namespace {
std::array<WorldModelAsset *,pose_count> poses={};
std::array<WorldModelPoseBlend *,pose_count> batches={};
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
    assert(version==WORLD_MODEL_POSE_BLEND_VERSION);attempts++;
    if(attempts==fail_pose)return nullptr;
    assert(path);unsigned index=99;int consumed=0;
    assert(std::sscanf(path,"models/village_shield_knight_idle_%u.glb%n",&index,&consumed)==1);
    assert(index<pose_count&&consumed==int(std::strlen(path))&&index==attempts-1&&!poses[index]);
    auto *asset=new WorldModelAsset;asset->index=index;poses[index]=asset;pose_live++;return asset;
}
extern "C" void world_model_free(WorldModelAsset *asset){
    if(!asset)return;
    assert(pose_live&&asset->index<pose_count&&poses[asset->index]==asset);
    // Blend batches borrow pose buffers; they must be released first.
    for(const auto *batch:batches)if(batch)assert(batch->a!=asset&&batch->b!=asset);
    poses[asset->index]=nullptr;pose_live--;delete asset;
}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t capacity,unsigned version){
    assert(capacity==actor_count&&version==WORLD_MODEL_POSE_BLEND_VERSION);batch_attempts++;
    if(batch_attempts==fail_batch)return nullptr;
    const unsigned index=unsigned(batch_attempts-1);assert(index<pose_count&&!batches[index]);
    auto *batch=new WorldModelPoseBlend;batch->index=index;batches[index]=batch;batch_live++;return batch;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    assert(batch&&a&&b);target_attempts++;
    if(target_attempts==fail_target)return false;
    assert(batch->index==a->index&&b->index==(a->index+1)%pose_count);
    batch->a=a;batch->b=b;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,std::size_t count){
    assert(batch&&batch->a&&batch->b&&count<=actor_count&&(!count||(instances&&weights)));
    for(std::size_t i=0;i<count;i++){
        const auto &p=instances[i];assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.yaw)&&p.y==24&&p.scale==1);
        assert(std::isfinite(weights[i])&&weights[i]>=0&&weights[i]<=1);
    }
    if(count){std::copy_n(instances,count,batch->instances.begin());std::copy_n(weights,count,batch->weights.begin());}
    batch->size=count;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program){
    assert(batch&&batch->a&&batch->b&&batch->size&&program==(shadow_pass?22u:11u));
    assert(grass==0&&vegetation==0&&lod==-1&&wind==0&&reactive==(shadow_pass?0.f:.85f));
    draws++;drawn+=batch->size;
}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *batch){
    if(!batch)return;
    assert(batch_live&&batch->index<pose_count&&batches[batch->index]==batch);
    batches[batch->index]=nullptr;batch_live--;delete batch;
}
extern "C" void world_atlas_village_origin(double *x,double *z){*x=village_x;*z=village_z;}
extern "C" void APIENTRY glUseProgram(GLuint program){assert(program==11||program==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint program,const GLchar *name){
    assert(program==11||program==22);queries++;
    const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint location,GLfloat x,GLfloat y,GLfloat z){
    assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));
    if(location==1){
        assert(x==float(std::floor(village_x/128)*128-draw_camera_x));
        assert(y==float(-draw_camera_y));assert(z==float(std::floor(village_z/128)*128-draw_camera_z));
    }else assert(location==9&&x==0&&y==0&&z==0);
}
extern "C" void APIENTRY glUniform2f(GLint location,GLfloat x,GLfloat z){
    assert(location==2&&x==float(std::floor(village_x/128)*128)&&z==float(std::floor(village_z/128)*128));
}
extern "C" void APIENTRY glUniform1i(GLint location,GLint value){
    if(location==3)grass=value;else if(location==4)vegetation=value;else if(location==5)lod=value;else assert(false);
}
extern "C" void APIENTRY glUniform1f(GLint location,GLfloat value){
    assert(std::isfinite(value));if(location==6)wind=value;else if(location==8)reactive=value;else assert(location==7&&value==2);
}

namespace {
using Actors=std::array<WorldVillageWatchActor,actor_count>;
void reset(){attempts=batch_attempts=target_attempts=0;fail_pose=fail_batch=fail_target=0;}
void empty(){assert(!pose_live&&!batch_live);for(auto *p:poses)assert(!p);for(auto *b:batches)assert(!b);}
Actors actors(const WorldVillageWatch *watch){Actors result={};for(std::size_t i=0;i<result.size();i++)assert(world_village_watch_actor(watch,i,&result[i]));return result;}
void same(const Actors &a,const Actors &b){
    for(std::size_t i=0;i<a.size();i++)assert(a[i].x==b[i].x&&a[i].z==b[i].z&&a[i].y==b[i].y&&a[i].yaw==b[i].yaw&&a[i].cycle_phase==b[i].cycle_phase&&a[i].pose_a==b[i].pose_a&&a[i].pose_b==b[i].pose_b&&a[i].pose_blend==b[i].pose_blend);
}
void inspect_batches(const WorldVillageWatch *watch,double camera_x,double camera_z){
    const auto people=actors(watch);std::array<bool,actor_count> seen={};std::size_t visible=0,groups=0;
    const double anchor_x=std::floor(village_x/128)*128,anchor_z=std::floor(village_z/128)*128;
    for(const auto *batch:batches){
        assert(batch&&batch->a&&batch->b&&batch->size<=actor_count);groups+=batch->size>0;
        for(std::size_t j=0;j<batch->size;j++){
            const auto &instance=batch->instances[j];std::size_t i=0;
            for(;i<people.size();i++)if(std::hypot(people[i].x-anchor_x-instance.x,people[i].z-anchor_z-instance.z)<.00005)break;
            assert(i<people.size()&&!seen[i]);seen[i]=true;visible++;
            assert(instance.y==people[i].y&&instance.yaw==people[i].yaw&&instance.scale==1);
            assert(batch->a->index==people[i].pose_a&&batch->b->index==people[i].pose_b&&batch->weights[j]==people[i].pose_blend);
        }
    }
    for(std::size_t i=0;i<people.size();i++)assert(seen[i]==(std::hypot(people[i].x-camera_x,people[i].z-camera_z)<=180));
    auto s=world_village_watch_stats(watch);assert(s.visible==visible&&s.draw_groups==groups);
}
struct UploadSnapshot {unsigned a=0,b=0;std::size_t size=0;std::array<WorldModelInstance,actor_count> instances={};std::array<float,actor_count> weights={};};
using Snapshot=std::array<UploadSnapshot,pose_count>;
Snapshot snapshot(){
    Snapshot result={};for(std::size_t i=0;i<result.size();i++){const auto *b=batches[i];assert(b&&b->a&&b->b);result[i]={b->a->index,b->b->index,b->size,b->instances,b->weights};}return result;
}
void same(const Snapshot &a,const Snapshot &b){
    for(std::size_t i=0;i<a.size();i++){
        assert(a[i].a==b[i].a&&a[i].b==b[i].b&&a[i].size==b[i].size);
        for(std::size_t j=0;j<a[i].size;j++){
            const auto &x=a[i].instances[j],&y=b[i].instances[j];assert(x.x==y.x&&x.y==y.y&&x.z==y.z&&x.yaw==y.yaw&&x.scale==y.scale&&a[i].weights[j]==b[i].weights[j]);
        }
    }
}
void draw(WorldVillageWatch *watch,bool shadow,bool mirror=false){
    const auto before=snapshot();const auto before_uploads=uploads;
    draws=drawn=0;shadow_pass=shadow;draw_camera_x=village_x;draw_camera_y=mirror?-25.75:25.75;draw_camera_z=center_z;
    world_village_watch_draw(watch,shadow?22:11,draw_camera_x,draw_camera_y,draw_camera_z,shadow);
    const auto s=world_village_watch_stats(watch);assert(drawn==s.visible&&draws==s.draw_groups&&reactive==0&&uploads==before_uploads);same(before,snapshot());
}
std::array<double,2> virtual_vertex(const WorldVillageWatchActor &actor){
    const double a=actor.pose_a*2*pi/pose_count,b=actor.pose_b*2*pi/pose_count,w=actor.pose_blend;
    return {(1-w)*std::cos(a)+w*std::cos(b),(1-w)*std::sin(a)+w*std::sin(b)};
}
void valid_roots(const Actors &people){
    for(std::size_t i=0;i<people.size();i++){
        const auto &a=people[i];const auto &station=world_village_watch_stations[i];
        assert(a.x==village_x+station.x&&a.z==village_z+station.z&&a.y==24&&a.yaw==station.yaw);
        assert(std::isfinite(a.cycle_phase)&&a.cycle_phase>=0&&a.cycle_phase<=1&&a.pose_blend>=0&&a.pose_blend<=1);
        assert(a.pose_a<pose_count&&a.pose_b==(a.pose_a+1)%pose_count);
        double phase=a.cycle_phase;if(phase==1)phase=0;
        double represented=(a.pose_a+a.pose_blend)/pose_count;if(represented==1)represented=0;
        const double difference=std::abs(phase-represented);assert(std::min(difference,1-difference)<.000001);
    }
}
}

int main(){
    static_assert(actor_count==6&&pose_count==8);
    char error[256]={};assert(!world_village_watch_load(nullptr,error,sizeof(error))&&!world_village_watch_load("",error,sizeof(error)));empty();
    std::size_t failure_probes=0;
    for(std::size_t i=1;i<=pose_count;i++){
        reset();fail_pose=i;assert(!world_village_watch_load("models",error,sizeof(error))&&error[0]);empty();failure_probes++;
        reset();fail_batch=i;assert(!world_village_watch_load("models",error,sizeof(error))&&error[0]);empty();failure_probes++;
        reset();fail_target=i;assert(!world_village_watch_load("models",error,sizeof(error))&&error[0]);empty();failure_probes++;
    }
    reset();auto *watch=world_village_watch_load("models",error,sizeof(error));assert(watch&&!error[0]&&pose_live==8&&batch_live==8&&attempts==8&&batch_attempts==8&&target_attempts==8);
    auto stats=world_village_watch_stats(watch);assert(stats.actors==actor_count&&stats.loaded_pose_assets==8);
    world_village_watch_update(watch,village_x,25.75,center_z,0,0);inspect_batches(watch,village_x,center_z);valid_roots(actors(watch));
    assert(world_village_watch_stats(watch).visible==5);draw(watch,false);draw(watch,true);draw(watch,false,true);assert(queries==18);
    const auto stationary=snapshot();const auto stationary_uploads=uploads;
    for(int i=0;i<120;i++)world_village_watch_update(watch,village_x,25.75,center_z,0,1.f/60);
    assert(uploads==stationary_uploads);same(stationary,snapshot());
    const auto warm_queries=queries;Actors previous=actors(watch);std::size_t wrap_crossings=0,resident_probes=0;
    allocations=0;track=true;
    for(unsigned frame=1;frame<=36000;frame++){
        const float time=float(frame)/60;world_village_watch_update(watch,village_x,25.75,center_z,time,1.f/60);
        const auto current=actors(watch);valid_roots(current);inspect_batches(watch,village_x,center_z);
        for(std::size_t i=0;i<current.size();i++){
            const auto a=virtual_vertex(previous[i]),b=virtual_vertex(current[i]);
            assert(std::hypot(a[0]-b[0],a[1]-b[1])<.05);
            wrap_crossings+=current[i].pose_a<previous[i].pose_a;resident_probes++;
        }
        if(frame%600==0){draw(watch,false);draw(watch,true);draw(watch,false,true);}
        previous=current;
    }
    track=false;assert(!allocations&&queries==warm_queries&&wrap_crossings>600&&resident_probes==36000*actor_count);
    // Absolute time, independent of update order, dt, and previous uploads.
    world_village_watch_update(watch,village_x,25.75,center_z,77.125f,0);const auto absolute=actors(watch);const auto absolute_batches=snapshot();
    world_village_watch_update(watch,village_x,25.75,center_z,439.75f,.75f);
    world_village_watch_update(watch,village_x,25.75,center_z,77.125f,1);same(absolute,actors(watch));same(absolute_batches,snapshot());
    // Every adjacent pair, including 7->0, remains continuous across the
    // nearest representable times on both sides of each station's boundary.
    for(std::size_t station=0;station<actor_count;station++)for(unsigned step=1;step<=24;step++){
        const double seconds=(double(step)/pose_count-world_village_watch_stations[station].phase)*world_village_watch_cycle_seconds;
        if(seconds<=0)continue;
        const float at=float(seconds),left=std::nextafter(at,0.f),right=std::nextafter(at,std::numeric_limits<float>::infinity());
        world_village_watch_update(watch,village_x,25.75,center_z,left,0);const auto a=virtual_vertex(actors(watch)[station]);
        world_village_watch_update(watch,village_x,25.75,center_z,right,0);const auto b=virtual_vertex(actors(watch)[station]);
        assert(std::hypot(a[0]-b[0],a[1]-b[1])<.00001);
    }
    // Invalid finite/time domains leave both public and uploaded state intact.
    const auto before=world_village_watch_stats(watch);const auto saved=actors(watch);const auto saved_batches=snapshot();
    for(const auto &arguments:std::array<std::array<double,5>,12>{{
        {NAN,25,center_z,1,.1},{village_x,INFINITY,center_z,1,.1},{village_x,25,NAN,1,.1},
        {village_x,25,center_z,NAN,.1},{village_x,25,center_z,INFINITY,.1},
        {village_x,25,center_z,-1,.1},{village_x,25,center_z,10000001,.1},
        {village_x,25,center_z,1,NAN},{village_x,25,center_z,1,INFINITY},
        {village_x,25,center_z,1,-.1},{village_x,25,center_z,1,1.01},
        {-INFINITY,25,center_z,1,0}}})
        world_village_watch_update(watch,arguments[0],arguments[1],arguments[2],float(arguments[3]),float(arguments[4]));
    assert(world_village_watch_stats(watch).updates==before.updates&&world_village_watch_stats(watch).uploads==before.uploads);same(saved,actors(watch));same(saved_batches,snapshot());
    world_village_watch_update(nullptr,0,0,0,0,0);world_village_watch_draw(nullptr,0,0,0,0);world_village_watch_draw(watch,0,0,0,0);assert(queries==warm_queries);
    // Isolate north/south stations, clear all stale groups outside the cull
    // distance, then re-enter at the same time without a pose discontinuity.
    world_village_watch_update(watch,village_x,25.75,village_z-48,100,0);inspect_batches(watch,village_x,village_z-48);assert(world_village_watch_stats(watch).visible==4);
    world_village_watch_update(watch,village_x,25.75,village_z+193,100,0);inspect_batches(watch,village_x,village_z+193);assert(world_village_watch_stats(watch).visible==2);
    world_village_watch_update(watch,0,25.75,0,100,0);inspect_batches(watch,0,0);assert(world_village_watch_stats(watch).visible==0&&world_village_watch_stats(watch).draw_groups==0);draw(watch,false);draw(watch,true);
    const auto invisible_uploads=uploads;world_village_watch_update(watch,0,25.75,0,100,0);assert(uploads==invisible_uploads);
    world_village_watch_update(watch,village_x,25.75,center_z,100,0);inspect_batches(watch,village_x,center_z);assert(world_village_watch_stats(watch).visible==5);draw(watch,false);draw(watch,true);draw(watch,false,true);
    WorldVillageWatchActor actor={};assert(!world_village_watch_actor(watch,actor_count,&actor)&&!world_village_watch_actor(nullptr,0,&actor)&&!world_village_watch_actor(watch,0,nullptr));
    assert(!world_village_watch_stats(nullptr).actors&&!world_village_watch_stats(nullptr).updates);
    world_village_watch_free(watch);world_village_watch_free(nullptr);empty();
    std::printf("PASS village watch CPU:6 actors/8 adjacent source poses;%zu atomic load/batch/target failures;600s/%zu fixed-root probes, continuous cyclic weights;zero hot allocations/18 cached uniform queries;stationary uploads suppressed;absolute time,finite guards,cull/reentry/stale-group clearing;color/shadow/reflection upload parity/reactive reset;all ownership released\n",failure_probes,resident_probes);
}
