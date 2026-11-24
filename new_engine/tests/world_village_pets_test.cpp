/* CPU-only population correctness. All model/GL calls below are fixed-array
 * stubs; this executable never creates a GL context or submits GPU work.
 * cc -std=c11 -O2 -I. -c engine/world_atlas.c -o /tmp/population-atlas.o
 * c++ -std=c++17 -O2 -Wall -Wextra -Werror -I. $(pkg-config --cflags sdl2)
 * tests/world_village_population_test.cpp engine/world_village_population.cpp
 * /tmp/population-atlas.o -o /tmp/valwood-population-test
 * $(pkg-config --libs sdl2) -lGL
 */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_village_pets.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

namespace {
bool track=false;
std::size_t allocations=0,live=0,attempts=0,fail_at=0,uploads=0,draws=0,drawn=0,queries=0,gl_calls=0;
std::size_t blend_live=0,blend_attempts=0,blend_fail_at=0;
std::size_t target_attempts=0,target_fail_at=0;
int vegetation=-1,lod=0,grass=-1;float wind=-1,reactive=-1,prop_range[3]={-1,-1,-1};
bool shadow_pass=false;
constexpr std::size_t capacity=world_village_pet_count;
const char *scene_instances_path="projects/valwood_world/scenery/castle_village_instances.txt";
}
void *operator new(std::size_t size){if(track)allocations++;if(void *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t size,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(size?size:1);}
void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}

struct WorldModelAsset {char path[1024]={};std::array<WorldModelInstance,capacity> instances={};std::size_t count=0;};
namespace {std::array<WorldModelAsset*,160> assets={};}
struct WorldModelPoseBlend {WorldModelAsset *a=nullptr,*b=nullptr;std::array<WorldModelInstance,capacity> instances={};std::array<float,capacity> weights={};std::size_t count=0;};
namespace {std::array<WorldModelPoseBlend*,200> blend_batches={};}
extern "C" WorldModelAsset *world_model_load(const char *path){
    attempts++;if(fail_at&&attempts==fail_at)return nullptr;
    assert(path&&std::strstr(path,"models/village_pet_")==path&&!std::strstr(path,".."));
    auto *a=new WorldModelAsset;assert(std::strlen(path)<sizeof(a->path));std::snprintf(a->path,sizeof(a->path),"%s",path);
    auto slot=std::find(assets.begin(),assets.end(),nullptr);assert(slot!=assets.end());*slot=a;live++;return a;
}
extern "C" void world_model_free(WorldModelAsset *a){
    if(!a)return;
    for(auto *batch:blend_batches)if(batch)assert(batch->a!=a&&batch->b!=a);
    auto slot=std::find(assets.begin(),assets.end(),a);assert(slot!=assets.end());*slot=nullptr;assert(live);live--;delete a;
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){assert(version==WORLD_MODEL_POSE_BLEND_VERSION);return world_model_load(path);}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t size,unsigned version){
    assert(size==capacity&&version==WORLD_MODEL_POSE_BLEND_VERSION);blend_attempts++;if(blend_fail_at&&blend_attempts==blend_fail_at)return nullptr;
    auto *batch=new WorldModelPoseBlend;auto slot=std::find(blend_batches.begin(),blend_batches.end(),nullptr);assert(slot!=blend_batches.end());*slot=batch;blend_live++;return batch;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    target_attempts++;if(target_fail_at&&target_attempts==target_fail_at)return false;
    assert(batch&&a&&b);char aa[32]={},bb[32]={};assert(std::sscanf(a->path,"models/village_pet_%31[^_]_",aa)==1&&std::sscanf(b->path,"models/village_pet_%31[^_]_",bb)==1&&!std::strcmp(aa,bb));batch->a=a;batch->b=b;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,std::size_t count){
    assert(batch&&count<=capacity);for(std::size_t i=0;i<count;i++){assert(std::isfinite(weights[i])&&weights[i]>=0&&weights[i]<=1);assert(std::isfinite(instances[i].yaw)&&instances[i].scale==1);batch->instances[i]=instances[i];batch->weights[i]=weights[i];}batch->count=count;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program){
    assert(batch&&batch->a&&batch->b&&batch->count&&program);assert(vegetation==0&&grass==0&&lod==-1&&wind==0&&prop_range[0]==0&&prop_range[1]==0&&prop_range[2]==0);assert(reactive==(shadow_pass?0.f:.85f));draws++;drawn+=batch->count;
}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *batch){if(!batch)return;auto slot=std::find(blend_batches.begin(),blend_batches.end(),batch);assert(slot!=blend_batches.end());*slot=nullptr;assert(blend_live);blend_live--;delete batch;}
extern "C" bool world_model_upload_instances(WorldModelAsset *a,const WorldModelInstance *p,std::size_t count){
    assert(a&&count<=capacity&&(!count||p));
    for(std::size_t i=0;i<count;i++){assert(std::isfinite(p[i].x)&&std::isfinite(p[i].y)&&std::isfinite(p[i].z)&&std::isfinite(p[i].yaw)&&p[i].scale==1);}
    if(count)std::copy_n(p,count,a->instances.begin());
    a->count=count;uploads++;return true;
}
extern "C" void world_model_draw(WorldModelAsset *a,unsigned program){
    assert(a&&a->count&&a->count<=world_village_pet_count&&program);
    assert(vegetation==0&&grass==0&&lod==-1&&wind==0&&prop_range[0]==0&&prop_range[1]==0&&prop_range[2]==0);
    assert(reactive==(shadow_pass?0.f:.85f));draws++;drawn+=a->count;
}
extern "C" void APIENTRY glUseProgram(GLuint p){gl_calls++;assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint p,const GLchar *name){
    gl_calls++;assert(p==11||p==22);queries++;
    const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint loc,GLfloat x,GLfloat y,GLfloat z){
    gl_calls++;assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));assert(loc==1||loc==9);
    if(loc==9){prop_range[0]=x;prop_range[1]=y;prop_range[2]=z;}
}
extern "C" void APIENTRY glUniform2f(GLint loc,GLfloat x,GLfloat y){gl_calls++;assert(loc==2&&std::isfinite(x)&&std::isfinite(y));}
extern "C" void APIENTRY glUniform1i(GLint loc,GLint v){
    gl_calls++;if(loc==3)grass=v;else if(loc==4)vegetation=v;else if(loc==5)lod=v;else assert(false);
}
extern "C" void APIENTRY glUniform1f(GLint loc,GLfloat v){
    gl_calls++;assert(std::isfinite(v));if(loc==6)wind=v;else if(loc==8)reactive=v;else assert(loc==7&&v==1);
}

namespace {
struct VillageCollisionBox{const char *alias;float xmin,xmax,zmin,zmax,ymin,ymax;};
#include "engine/world_village_colliders.inc"
struct Box {double x,z,c,s;float xmin,xmax,zmin,zmax,ymin,ymax;};
std::vector<Box> collision_boxes(){
    std::ifstream file(scene_instances_path);assert(file);
    std::vector<Box> boxes;std::string line;
    while(std::getline(file,line)){
        line.erase(line.find('#')==std::string::npos?line.size():line.find('#'));
        std::istringstream fields(line);std::string name;double u,v,yaw,scale,burial,elevation=0;
        if(!(fields>>name))continue;
        assert(fields>>u>>v>>yaw>>scale>>burial);fields>>elevation;
        double x,z;world_atlas_uv_to_world(u,v,&x,&z);const double angle=yaw*3.14159265358979323846/180;
        const double c=std::cos(angle),s=std::sin(angle);float root=world_atlas_height(x,z)-float(burial)+float(elevation);
        auto add=[&](float xmin,float xmax,float zmin,float zmax,float ymin,float ymax){boxes.push_back({x,z,c,s,float(xmin*scale),float(xmax*scale),float(zmin*scale),float(zmax*scale),root+float(ymin*scale),root+float(ymax*scale)});};
        for(const auto &b:village_collision_boxes)if(name==b.alias)add(b.xmin,b.xmax,b.zmin,b.zmax,b.ymin,b.ymax);
        if(name=="starting_wall")add(-3.70512f,3.70512f,-1.27489f,1.27489f,0,7.79366f);
        else if(name=="starting_gate"){
            add(-3.70512f,-1.750665f,-1.335985f,1.335985f,0,8.616226f);
            add(1.750665f,3.70512f,-1.335985f,1.335985f,0,8.616226f);
            add(-1.750665f,1.750665f,-1.335985f,1.335985f,4.2f,8.616226f);
        }else if(name=="starting_tower")add(-7.919224f,7.919224f,-7.919224f,7.919224f,0,13.498152f);
        else if(name=="starting_crate")add(-.413f,.413f,-.205f,.205f,0,.35f);
        else if(name=="starting_barrel")add(-.372f,.372f,-.372f,.372f,0,.917f);
        else if(name=="starting_table")add(-1.121f,1.121f,-1.512f,1.512f,0,.746f);
        // Conservative real model box covers the stool's native circle too.
        else if(name=="starting_stump")add(-.713f,.713f,-.795f,.795f,0,.575f);
    }
    assert(boxes.size()>300);return boxes;
}
bool body_clear(const WorldVillagePet &a,const std::vector<Box> &boxes){
    for(const auto &b:boxes){
        if(a.y+(!std::strcmp(a.species,"Cat")?.42f:.95f)<=b.ymin||a.y>=b.ymax)continue;
        const double dx=a.x-b.x,dz=a.z-b.z,x=b.c*dx-b.s*dz,z=b.s*dx+b.c*dz;
        const double rx=x-std::clamp(x,double(b.xmin),double(b.xmax)),rz=z-std::clamp(z,double(b.zmin),double(b.zmax));
        const double radius=!std::strcmp(a.species,"Cat")?.4:.75;
        if(rx*rx+rz*rz<radius*radius){std::fprintf(stderr,"Pet %s collision at %.3f/%.3f radius%.2f versus box %.3f/%.3f local x%.3f..%.3f z%.3f..%.3f\n",a.name,a.x,a.z,radius,b.x,b.z,b.xmin,b.xmax,b.zmin,b.zmax);return false;}
    }
    return true;
}
std::array<WorldVillagePet,world_village_pet_count> pets(const WorldVillagePets *p){
    std::array<WorldVillagePet,world_village_pet_count> result;
    for(std::size_t i=0;i<result.size();i++)assert(world_village_pets_pet(p,i,&result[i]));
    return result;
}
void draw_check(WorldVillagePets *p,bool shadow){
    auto s=world_village_pets_stats(p);drawn=draws=0;shadow_pass=shadow;
    const auto previous_gl=gl_calls;
    world_village_pets_draw(p,shadow?22:11,0,25.75,0,shadow);
    assert(drawn==s.visible&&draws==s.draw_groups&&reactive==0);
    if(s.draw_groups)assert(gl_calls>previous_gl);
}
void equal(const WorldVillagePet &a,const WorldVillagePet &b){
    assert(!std::strcmp(a.name,b.name)&&!std::strcmp(a.species,b.species)&&a.x==b.x&&a.z==b.z&&a.y==b.y&&a.yaw==b.yaw&&a.speed==b.speed&&a.cycle_phase==b.cycle_phase&&a.pose_a==b.pose_a&&a.pose_b==b.pose_b&&a.pose_blend==b.pose_blend&&a.walking==b.walking&&a.seated==b.seated&&a.transitioning==b.transitioning);
}
void empty_draw_check(WorldVillagePets *p){
    const auto state=world_village_pets_stats(p);assert(!state.visible&&!state.draw_groups);
    const auto actors=pets(p);const auto previous_gl=gl_calls,previous_queries=queries,previous_draws=draws,previous_uploads=uploads;
    const float previous_reactive=reactive;reactive=.37f;
    world_village_pets_draw(p,11,0,25.75,0);
    world_village_pets_draw(p,11,0,-25.75,0);
    world_village_pets_draw(p,22,0,25.75,0,true);
    assert(gl_calls==previous_gl&&queries==previous_queries&&draws==previous_draws&&uploads==previous_uploads&&reactive==.37f);
    assert(world_village_pets_stats(p).updates==state.updates);
    const auto after=pets(p);for(std::size_t i=0;i<actors.size();i++)equal(actors[i],after[i]);
    reactive=previous_reactive;
}
}
int main(int argc,char **argv){
    assert(argc<=3);if(argc>=2)assert(chdir(argv[1])==0);if(argc==3)scene_instances_path=argv[2];
    assert(world_atlas_init());double ox,oz;world_atlas_village_origin(&ox,&oz);
    auto boxes=collision_boxes();char error[256]={};
    assert(!world_village_pets_load(nullptr,error,sizeof(error))&&!world_village_pets_load("",error,sizeof(error)));
    for(std::size_t i=1;i<=world_village_pet_pose_count;i++){
        attempts=0;fail_at=i;auto *p=world_village_pets_load("models",error,sizeof(error));assert(!p&&error[0]&&!live&&!blend_live);
    }
    fail_at=0;
    for(std::size_t i:{std::size_t(1),world_village_pet_blend_count/2,world_village_pet_blend_count}){
        blend_attempts=0;blend_fail_at=i;auto *p=world_village_pets_load("models",error,sizeof(error));assert(!p&&error[0]&&!live&&!blend_live);
    }
    blend_fail_at=0;
    for(std::size_t i:{std::size_t(1),world_village_pet_blend_count}){
        target_attempts=0;target_fail_at=i;auto *p=world_village_pets_load("models",error,sizeof(error));assert(!p&&error[0]&&!live&&!blend_live);
    }
    target_fail_at=0;attempts=0;auto *p=world_village_pets_load("models",error,sizeof(error));assert(p&&!error[0]);
    auto s=world_village_pets_stats(p);assert(s.pets==12&&s.cats==6&&s.dogs==6&&s.loaded_pose_assets==32&&live==32&&blend_live==38);
    auto *q=world_village_pets_load("models",error,sizeof(error));assert(q&&live==64&&blend_live==76);
    empty_draw_check(q);
    world_village_pets_update(p,ox,25.75,oz,0,0);auto previous=pets(p);draw_check(p,false);draw_check(p,true);
    WorldVillagePet out;assert(!world_village_pets_pet(p,12,&out)&&!world_village_pets_pet(p,0,nullptr)&&!world_village_pets_pet(nullptr,0,&out));
    const auto updates=world_village_pets_stats(p).updates;
    world_village_pets_update(p,NAN,25.75,oz,1,.1);world_village_pets_update(p,ox,25.75,oz,NAN,.1);world_village_pets_update(p,ox,25.75,oz,-1,.1);world_village_pets_update(p,ox,25.75,oz,1,1.1);
    assert(world_village_pets_stats(p).updates==updates);
    std::size_t moving=0,seated=0,probes=0;double minimum_pair=1e9,maximum_speed=0,maximum_turn=0;
    const auto warm_queries=queries,warm_batches=blend_attempts,warm_targets=target_attempts;
    allocations=0;track=true;
    for(int frame=1;frame<=36000;frame++){
        const float time=float(frame/60.);world_village_pets_update(p,ox,25.75,oz,time,1.f/60);
        const auto current=pets(p);s=world_village_pets_stats(p);
        assert(s.pets==12&&s.walking+s.idling==12&&s.visible<=12&&s.draw_groups<=12);
        for(std::size_t i=0;i<current.size();i++){
            const auto &a=current[i],&b=previous[i];assert(a.name&&a.species&&a.y==24&&world_atlas_height(a.x,a.z)==24&&body_clear(a,boxes));
            assert(a.pose_a<32&&a.pose_b<32&&a.pose_blend>=0&&a.pose_blend<=1&&a.speed>=0&&a.speed<=.550001f);
            assert(!std::strcmp(a.species,i<6?"Cat":"Dog")&&a.seated==(i>=6&&i<9));
            const double speed=std::hypot(a.x-b.x,a.z-b.z)*60,turn=std::abs(std::remainder(double(a.yaw)-b.yaw,6.283185307179586));
            maximum_speed=std::max(maximum_speed,speed);maximum_turn=std::max(maximum_turn,turn);assert(speed<.56&&turn<.08);
            moving+=speed>.001;seated+=a.seated;probes++;
            for(std::size_t j=i+1;j<current.size();j++){
                const double d=std::hypot(a.x-current[j].x,a.z-current[j].z);minimum_pair=std::min(minimum_pair,d);
                const double radius=(i<6?.4:.75)+(j<6?.4:.75);assert(d>radius+.1);
            }
        }
        if(frame%60==0){
            world_village_pets_update(q,ox,25.75,oz,time,.5);auto other=pets(q);
            for(std::size_t i=0;i<12;i++)equal(current[i],other[i]);
            draw_check(p,false);draw_check(p,true);
        }
        previous=current;
    }
    track=false;assert(!allocations&&queries==warm_queries&&blend_attempts==warm_batches&&target_attempts==warm_targets&&moving>100000&&seated==108000);
    world_village_pets_update(p,ox+35000,25.75,oz+35000,601,.1);assert(world_village_pets_stats(p).visible==0);empty_draw_check(p);
    world_village_pets_update(q,ox,25.75,oz,601,.1);const auto far_first=pets(p),near_first=pets(q);
    for(std::size_t i=0;i<far_first.size();i++)equal(far_first[i],near_first[i]);
    world_village_pets_update(p,ox+35000,25.75,oz+35000,602,.1);empty_draw_check(p);
    world_village_pets_update(q,ox,25.75,oz,602,.1);const auto far_second=pets(p),near_second=pets(q);bool far_motion=false;
    for(std::size_t i=0;i<far_second.size();i++){equal(far_second[i],near_second[i]);far_motion=far_motion||far_first[i].x!=far_second[i].x||far_first[i].z!=far_second[i].z;}
    assert(far_motion&&world_village_pets_stats(p).walking==world_village_pets_stats(q).walking);
    world_village_pets_update(p,ox,25.75,oz,602,.1);const auto returned=pets(p);
    for(std::size_t i=0;i<returned.size();i++)equal(far_second[i],returned[i]);
    draw_check(p,false);draw_check(p,true);
    std::puts("PASS empty pet draws: zero GL/query/upload/state work in color/reflection/shadow; all12 absolute-time states progress35km away and match reentry");
    world_village_pets_free(p);world_village_pets_free(q);world_village_pets_free(nullptr);assert(!live&&!blend_live);
    world_atlas_shutdown();
    std::printf("PASS pets CPU:12 pets/6 cats/6 dogs/3 seated;32 atomic asset failures/38 bounded interpolation groups;600s/60Hz/%zu actual-scene collider+ground probes; min pair%.3fm/max speed%.3fm/s/turn%.4frad; deterministic poses, no phase snaps, culling, shared-pass reactive contract, zero hot allocations/uniform queries/relinks and no leaks\n",probes,minimum_pair,maximum_speed,maximum_turn);
}
