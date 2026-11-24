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
#include "engine/world_village_population.h"
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
std::size_t allocations=0,live=0,attempts=0,fail_at=0,uploads=0,draws=0,drawn=0,queries=0;
std::size_t blend_live=0,blend_attempts=0,blend_fail_at=0;
std::size_t target_attempts=0,target_fail_at=0;
int vegetation=-1,lod=0,grass=-1;float wind=-1,reactive=-1,prop_range[3]={-1,-1,-1};
bool shadow_pass=false;
constexpr std::size_t capacity=world_village_resident_count;

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
    assert(path&&(std::strstr(path,"models/village_character_")==path||std::strstr(path,"models/purchased_character_")==path||std::strstr(path,"models/village_pet_")==path)&&!std::strstr(path,".."));
    auto *a=new WorldModelAsset;assert(std::strlen(path)<sizeof(a->path));std::snprintf(a->path,sizeof(a->path),"%s",path);
    auto slot=std::find(assets.begin(),assets.end(),nullptr);assert(slot!=assets.end());*slot=a;live++;return a;
}
extern "C" void world_model_free(WorldModelAsset *a){
    if(!a)return;
    for(auto *batch:blend_batches)if(batch)assert(batch->a!=a&&batch->b!=a);
    auto slot=std::find(assets.begin(),assets.end(),a);assert(slot!=assets.end());*slot=nullptr;assert(live);live--;delete a;
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){assert(version==WORLD_MODEL_POSE_BLEND_VERSION);return world_model_load(path);}
extern "C" bool world_model_bounds(const WorldModelAsset *asset,float minimum[3],float maximum[3]){
    assert(asset&&minimum&&maximum);const float lo[]={-.5f,0,-.8f},hi[]={.5f,2,.8f};std::copy_n(lo,3,minimum);std::copy_n(hi,3,maximum);return true;
}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t size,unsigned version){
    assert((size==capacity||size==world_village_pet_count)&&version==WORLD_MODEL_POSE_BLEND_VERSION);blend_attempts++;if(blend_fail_at&&blend_attempts==blend_fail_at)return nullptr;
    auto *batch=new WorldModelPoseBlend;auto slot=std::find(blend_batches.begin(),blend_batches.end(),nullptr);assert(slot!=blend_batches.end());*slot=batch;blend_live++;return batch;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    target_attempts++;if(target_fail_at&&target_attempts==target_fail_at)return false;
    assert(batch&&a&&b);char aa[32]={},bb[32]={};const char *format=std::strstr(a->path,"village_pet_")?"models/village_pet_%31[^_]_":"models/%*[^_]_character_%31[^_]_";assert(std::sscanf(a->path,format,aa)==1&&std::sscanf(b->path,format,bb)==1&&!std::strcmp(aa,bb));batch->a=a;batch->b=b;return true;
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
    assert(a&&a->count&&a->count<=world_village_resident_count&&program);
    assert(vegetation==0&&grass==0&&lod==-1&&wind==0&&prop_range[0]==0&&prop_range[1]==0&&prop_range[2]==0);
    assert(reactive==(shadow_pass?0.f:.85f));draws++;drawn+=a->count;
}
extern "C" void APIENTRY glUseProgram(GLuint p){assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint p,const GLchar *name){
    assert(p==11||p==22);queries++;
    const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint loc,GLfloat x,GLfloat y,GLfloat z){
    assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));assert(loc==1||loc==9);
    if(loc==9){prop_range[0]=x;prop_range[1]=y;prop_range[2]=z;}
}
extern "C" void APIENTRY glUniform2f(GLint loc,GLfloat x,GLfloat y){assert(loc==2&&std::isfinite(x)&&std::isfinite(y));}
extern "C" void APIENTRY glUniform1i(GLint loc,GLint v){
    if(loc==3)grass=v;else if(loc==4)vegetation=v;else if(loc==5)lod=v;else assert(false);
}
extern "C" void APIENTRY glUniform1f(GLint loc,GLfloat v){
    assert(std::isfinite(v));if(loc==6)wind=v;else if(loc==8)reactive=v;else assert(loc==7&&(v==1||v==2));
}

int main(){
    assert(world_atlas_init());double x,z;world_atlas_village_origin(&x,&z);char error[256]={};
    auto *humans=world_village_population_load("models",error,sizeof(error));assert(humans);
    auto *pets=world_village_pets_load("models",error,sizeof(error));assert(pets);
    assert(world_village_population_stats(humans).residents==world_village_resident_count&&world_village_pets_stats(pets).pets==12);
    std::array<WorldVillageResident,world_village_resident_count> residents;
    double minimum=1e9;std::size_t probes=0;allocations=0;track=true;
    for(int frame=0;frame<=36000;frame++){
        const float time=float(frame/60.);world_village_population_update(humans,x,25.75,z,time,1.f/60);world_village_pets_update(pets,x,25.75,z,time,1.f/60);
        for(std::size_t i=0;i<residents.size();i++)assert(world_village_population_resident(humans,i,&residents[i]));
        for(std::size_t i=0;i<12;i++){
            WorldVillagePet pet;assert(world_village_pets_pet(pets,i,&pet));
            const double radius=i<6?.4:.75;
            for(const auto &person:residents){const double d=std::hypot(pet.x-person.x,pet.z-person.z);minimum=std::min(minimum,d);assert(d>radius+.45+.12);probes++;}
        }
    }
    track=false;assert(!allocations);world_village_pets_free(pets);world_village_population_free(humans);assert(!live&&!blend_live);world_atlas_shutdown();
    std::printf("PASS combined living district: actual140resident+12pet runtime 600s/60Hz/%zu human-animal pairs; minimum %.6fm; full .45m human plus .4/.75m animal capsules and .12m safety gap; zero hot allocations or retained ownership\n",probes,minimum);
}
