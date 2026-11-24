/* CPU-only habitat streaming and genuine-pose ownership proof. Model/GL/atlas
 * stubs never create a window, read a GLB, or issue native GPU commands. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_wildlife.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_scenery.h"
#include "engine/world_castle_visibility.h"
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
constexpr unsigned species_count=std::size(kWildlifeSpecies),pose_count=12,pass_count=3;
constexpr std::size_t capacity=world_wildlife_capacity;
constexpr double pi=3.14159265358979323846;
std::size_t load_attempts=0,create_attempts=0,target_attempts=0;
std::size_t fail_load=0,fail_create=0,fail_target=0,pose_live=0,batch_live=0;
std::size_t uploads=0,draws=0,drawn=0,queries=0,allocations=0,safety_queries=0;
std::array<std::array<double,2>,9> ground_points={};std::size_t ground_point_index=0;
bool track=false,allow_movement=true,center_only=false,water=false;
bool preparing=false,fail_preparation_draw=false;
GLenum preparation_error=GL_NO_ERROR;
std::size_t preparation_clears=0,preparation_upload_attempts=0,fail_preparation_upload=0;
WorldWildlifePrepareFrame preparation_frame;
std::array<std::array<unsigned,species_count*pose_count>,pass_count> preparation_seen={};
std::size_t last_preparation_clear=0;
enum class Ground { Flat,Invalid,Low,Steep };Ground ground=Ground::Flat;
unsigned draw_pass=0,draw_program=0;int habitat=-1;bool expanded_profile=false;
float reactive=0,wind=0,height=0,prop_start=0,prop_width=0,prop_on=0;
int grass=-1,vegetation=-1,lod=0;
double camera_x=0,camera_y=0,camera_z=0;
}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(n?n:1);}
void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}

struct WorldModelAsset {unsigned species=0,pose=0;};
struct WorldModelPoseBlend {
    unsigned slot=0,pass=0;WorldModelAsset *a=nullptr,*b=nullptr;
    std::array<WorldModelInstance,capacity> instances={};
    std::array<float,capacity> weights={};std::size_t size=0;
};
namespace {
std::array<WorldModelAsset *,species_count*pose_count> assets={};
std::array<WorldModelPoseBlend *,species_count*pose_count*pass_count> batches={};
unsigned next_pose(unsigned pose){return pose<8?(pose+1)%8:8+(pose-8+1)%4;}
bool selected(unsigned species){return habitat>=0&&(((expanded_profile&&habitat==2)?0x1c0u:kWildlifeHabitats[habitat].species_mask)&(1u<<species));}
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
    assert(version==WORLD_MODEL_POSE_BLEND_VERSION&&path);load_attempts++;
    if(load_attempts==fail_load)return nullptr;
    unsigned species=species_count,pose=pose_count;
    for(unsigned s=0;s<species_count;s++)for(unsigned p=0;p<pose_count;p++){
        char wanted[1024]={};std::snprintf(wanted,sizeof(wanted),"models/%s_%s_%02u.glb",kWildlifeSpecies[s].stem,p<8?"walk":"idle",p<8?p:p-8);
        if(!std::strcmp(path,wanted)){species=s;pose=p;}
    }
    assert(species<species_count&&pose<pose_count&&selected(species));
    auto &slot=assets[species*pose_count+pose];assert(!slot);
    slot=new WorldModelAsset{species,pose};pose_live++;return slot;
}
extern "C" WorldModelAsset *world_model_load_pose_origin(const char *path,unsigned version){return world_model_load_pose(path,version);}
extern "C" void world_model_free(WorldModelAsset *asset){
    if(!asset)return;
    auto &slot=assets[asset->species*pose_count+asset->pose];assert(pose_live&&slot==asset);
    for(const auto *b:batches)if(b)assert(b->a!=asset&&b->b!=asset);
    slot=nullptr;pose_live--;delete asset;
}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t count,unsigned version){
    assert(count==(preparing?1:capacity)&&version==WORLD_MODEL_POSE_BLEND_VERSION);create_attempts++;
    if(create_attempts==fail_create)return nullptr;
    unsigned slot=0;while(slot<batches.size()&&batches[slot])slot++;assert(slot<batches.size());
    auto *batch=new WorldModelPoseBlend;batch->slot=slot;batch->pass=preparing?draw_pass:(unsigned(create_attempts)-1)/pose_count%pass_count;
    batches[slot]=batch;batch_live++;return batch;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    assert(batch&&a&&b);target_attempts++;if(target_attempts==fail_target)return false;
    assert(a->species==b->species&&b->pose==next_pose(a->pose));
    batch->a=a;batch->b=b;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,std::size_t count){
    assert(batch&&batch->a&&batch->b&&count<=capacity&&(!count||(instances&&weights))&&batch->pass==draw_pass);
    if(preparing&&++preparation_upload_attempts==fail_preparation_upload)return false;
    for(std::size_t i=0;i<count;i++){
        const auto &p=instances[i];assert(std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::isfinite(p.yaw)&&p.scale==1);
        assert(std::isfinite(weights[i])&&weights[i]>=0&&weights[i]<=1);
        if(preparing){
            assert(count==1&&weights[i]==.5f);
            const auto &f=preparation_frame;const auto &animal=kWildlifeSpecies[batch->a->species];
            const auto &h=kWildlifeHabitats[habitat];const double ax=std::floor(h.center_x/128)*128,az=std::floor(h.center_z/128)*128;
            const double distance=std::max({12.,double(animal.bound_radius)*3.+double(animal.height)*2.,f.constrain_minimum_y?2.*std::abs(f.minimum_y-f.y):0.});
            assert(distance<15000);
            assert(p.x==float(f.x-ax+std::sin(double(f.yaw))*std::cos(double(f.pitch))*distance));
            assert(p.z==float(f.z-az-std::cos(double(f.yaw))*std::cos(double(f.pitch))*distance));
            const double foot=std::max(f.y+std::sin(double(f.pitch))*distance-animal.height*.5,f.constrain_minimum_y?f.minimum_y:-1e100);
            assert(p.y==float(foot)&&p.yaw==f.yaw);
            if(f.constrain_minimum_y)assert(p.y>=float(f.minimum_y));
        }else assert(std::abs(p.y-24.005f)<.00001f);
    }
    if(count){std::copy_n(instances,count,batch->instances.begin());std::copy_n(weights,count,batch->weights.begin());}
    batch->size=count;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program){
    assert(batch&&batch->a&&batch->b&&batch->size&&batch->pass==draw_pass&&program==draw_program);
    assert(grass==0&&vegetation==0&&lod==-1&&wind==0&&height==1&&reactive==(draw_pass==2?0.f:.85f));
    assert(prop_start==kWildlifeHabitats[habitat].draw_radius-8&&prop_width==8&&prop_on==1);
    if(preparing){
        assert(preparation_clears==last_preparation_clear+1);last_preparation_clear=preparation_clears;
        preparation_seen[draw_pass][batch->a->species*pose_count+batch->a->pose]++;
        if(fail_preparation_draw){preparation_error=GL_INVALID_OPERATION;fail_preparation_draw=false;}
    }
    draws++;drawn+=batch->size;
}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *batch){
    if(!batch)return;
    assert(batch_live&&batches[batch->slot]==batch);batches[batch->slot]=nullptr;batch_live--;delete batch;
}
extern "C" float world_atlas_height(double x,double z){
    assert(std::isfinite(x)&&std::isfinite(z));safety_queries++;
    ground_points[ground_point_index++%ground_points.size()]={x,z};
    if(ground==Ground::Invalid)return NAN;
    if(ground==Ground::Low)return .1f;
    if(ground==Ground::Steep)return float(24+(x-kWildlifeHabitats[habitat].center_x)*100);
    return 24;
}
extern "C" bool world_atlas_local_water(double x,double z,float *surface,float *depth){
    assert(std::isfinite(x)&&std::isfinite(z));if(surface)*surface=water?25:0;if(depth)*depth=water?1:0;safety_queries++;return water;
}
bool world_scenery_can_move(const WorldScenery *,double from_x,double from_z,double x,double z,float feet,float animal_height,float radius){
    assert(std::isfinite(x)&&std::isfinite(z)&&std::abs(feet-24.005f)<.00001f&&animal_height>0&&radius>0);safety_queries++;
    std::array<double,9> distances={};
    for(std::size_t i=0;i<ground_points.size();i++)distances[i]=std::hypot(ground_points[i][0]-x,ground_points[i][1]-z);
    std::sort(distances.begin(),distances.end());bool source_rectangle=false;
    for(const auto &s:kWildlifeSpecies)if(s.height==animal_height&&s.bound_radius==radius){
        const double a=s.foot_half_x,b=s.foot_half_z,c=std::hypot(a,b);
        std::array<double,9> expected={0,a,a,b,b,c,c,c,c};std::sort(expected.begin(),expected.end());
        bool match=true;for(std::size_t i=0;i<distances.size();i++)match&=std::abs(distances[i]-expected[i])<.00001;
        source_rectangle|=match;
    }
    assert(source_rectangle);
    if(!allow_movement)return false;
    return !center_only||(x==from_x&&z==from_z);
}
extern "C" void APIENTRY glUseProgram(GLuint program){assert(program==draw_program);}
extern "C" GLenum APIENTRY glGetError(void){const auto result=preparation_error;preparation_error=GL_NO_ERROR;return result;}
extern "C" void APIENTRY glClear(GLbitfield bits){assert(preparing&&bits==GL_DEPTH_BUFFER_BIT);preparation_clears++;}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint program,const GLchar *name){
    assert(program==11||program==22);queries++;
    const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint location,GLfloat x,GLfloat y,GLfloat z){
    assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));
    if(location==1){
        const auto &h=kWildlifeHabitats[habitat];assert(x==float(std::floor(h.center_x/128)*128-camera_x)&&y==float(-camera_y)&&z==float(std::floor(h.center_z/128)*128-camera_z));
    }else{assert(location==9);prop_start=x;prop_width=y;prop_on=z;}
}
extern "C" void APIENTRY glUniform2f(GLint location,GLfloat x,GLfloat z){
    const auto &h=kWildlifeHabitats[habitat];assert(location==2&&x==float(std::floor(h.center_x/128)*128)&&z==float(std::floor(h.center_z/128)*128));
}
extern "C" void APIENTRY glUniform1i(GLint location,GLint value){
    if(location==3)grass=value;else if(location==4)vegetation=value;else if(location==5)lod=value;else assert(false);
}
extern "C" void APIENTRY glUniform1f(GLint location,GLfloat value){
    assert(std::isfinite(value));if(location==6)wind=value;else if(location==7)height=value;else{assert(location==8);reactive=value;}
}

namespace {
using Actors=std::array<WorldWildlifeActor,capacity>;
struct Snapshot {Actors rows={};std::size_t count=0;};
Snapshot snapshot(const WorldWildlife *world){
    Snapshot s;s.count=world_wildlife_stats(world).actors;assert(s.count<=capacity);
    for(std::size_t i=0;i<s.count;i++)assert(world_wildlife_actor(world,i,&s.rows[i]));
    WorldWildlifeActor a;assert(!world_wildlife_actor(world,s.count,&a));return s;
}
void same(const Snapshot &a,const Snapshot &b){
    assert(a.count==b.count);
    for(std::size_t i=0;i<a.count;i++){
        const auto &x=a.rows[i],&y=b.rows[i];
        assert(x.species==y.species&&x.habitat==y.habitat&&x.x==y.x&&x.z==y.z&&x.y==y.y&&x.yaw==y.yaw&&x.speed==y.speed&&x.walking==y.walking&&x.cycle_phase==y.cycle_phase&&x.pose_a==y.pose_a&&x.pose_b==y.pose_b&&x.pose_blend==y.pose_blend);
    }
}
std::size_t expected_species(unsigned bank){
    unsigned mask=kWildlifeHabitats[bank].species_mask;std::size_t n=0;while(mask){n+=mask&1;mask>>=1;}return n;
}
std::size_t expected_actors(unsigned bank){std::size_t n=0;for(const auto &a:kWildlifeActors)n+=a.habitat==int(bank);return std::min(n,capacity);}
void empty(){assert(!pose_live&&!batch_live);for(auto *a:assets)assert(!a);for(auto *b:batches)assert(!b);}
void reset_attempts(){load_attempts=create_attempts=target_attempts=fail_load=fail_create=fail_target=0;}
bool commit(WorldWildlife *w,int bank,char *error=nullptr,std::size_t size=0){habitat=bank;reset_attempts();return world_wildlife_commit(w,bank,error,size);}
void valid(const WorldWildlife *world){
    const auto s=world_wildlife_stats(world);assert(s.actors<=capacity&&s.walking+s.idling==s.actors);
    const auto a=snapshot(world);std::size_t walking=0;
    for(std::size_t i=0;i<a.count;i++){
        const auto &p=a.rows[i];assert(p.habitat==habitat&&p.species>=0&&p.species<int(species_count)&&selected(unsigned(p.species)));
        assert(std::isfinite(p.x)&&std::isfinite(p.z)&&std::isfinite(p.yaw)&&p.y==24.005f&&p.speed>=0&&p.pose_blend>=0&&p.pose_blend<=1&&p.cycle_phase>=0&&p.cycle_phase<=1);
        const unsigned base=p.walking?0:8,count=p.walking?8:4;
        assert(p.pose_a>=base&&p.pose_a<base+count&&p.pose_b==base+(p.pose_a-base+1)%count);
        const double f=(p.pose_a-base+p.pose_blend)/count;assert(std::abs(f-p.cycle_phase)<.000001);walking+=p.walking;
    }
    assert(s.walking==walking);
}
void valid_paths(const WorldWildlife *world){
    const auto people=snapshot(world);std::size_t index=0;
    for(const auto &d:kWildlifeActors)if(d.habitat==habitat&&index<capacity){
        assert(index<people.count);const auto &a=people.rows[index++];const auto &s=kWildlifeSpecies[d.species];
        const bool walking=d.route_radius>0&&d.speed>0;assert(a.walking==walking&&a.species==d.species);
        assert(s.stride>0&&s.idle_seconds>0);
        if(walking){
            const double rx=a.x-d.x,rz=a.z-d.z,fx=std::sin(double(a.yaw)),fz=std::cos(double(a.yaw));
            assert(std::abs(std::hypot(rx,rz)-d.route_radius)<.000001&&a.speed==d.speed);
            assert(std::abs(rx*fx+rz*fz)<.00001&&rx*fz-rz*fx>0); // Face forward along the safe circle.
        }else assert(a.x==d.x&&a.z==d.z&&a.speed==0&&a.yaw==d.yaw);
    }
    assert(index==people.count);
}
std::array<std::uint64_t,species_count*pose_count*pass_count> batch_hashes(){
    std::array<std::uint64_t,species_count*pose_count*pass_count> result={};
    for(std::size_t i=0;i<batches.size();i++)if(const auto *b=batches[i]){
        std::uint64_t hash=1469598103934665603ULL;
        auto bytes=[&](const void *p,std::size_t count){const auto *data=static_cast<const unsigned char *>(p);for(std::size_t j=0;j<count;j++){hash^=data[j];hash*=1099511628211ULL;}};
        bytes(&b->size,sizeof(b->size));bytes(b->instances.data(),b->size*sizeof(WorldModelInstance));bytes(b->weights.data(),b->size*sizeof(float));result[i]=hash;
    }
    return result;
}
std::size_t draw(WorldWildlife *world,unsigned pass,double x,double y,double z,float yaw=0,float pitch=0,float aspect=1.6f){
    const auto before=batch_hashes();
    draw_pass=pass;draw_program=pass==2?22:11;camera_x=x;camera_y=y;camera_z=z;draws=drawn=0;
    world_wildlife_draw(world,draw_program,x,y,z,yaw,pitch,aspect,pass);
    const auto after=batch_hashes();
    for(std::size_t i=0;i<batches.size();i++)if(batches[i]&&batches[i]->pass!=pass)assert(before[i]==after[i]);
    const auto people=snapshot(world);std::array<bool,capacity> seen={};std::size_t count=0;
    const auto &h=kWildlifeHabitats[habitat];const double ax=std::floor(h.center_x/128)*128,az=std::floor(h.center_z/128)*128;
    const auto view=world_vegetation_visibility_view(x,y,z,yaw,pitch,aspect);
    for(const auto *b:batches)if(b&&b->pass==pass)for(std::size_t j=0;j<b->size;j++){
        const auto &instance=b->instances[j];std::size_t actor=0;
        for(;actor<people.count;actor++)if(people.rows[actor].species==int(b->a->species)&&std::hypot(people.rows[actor].x-ax-instance.x,people.rows[actor].z-az-instance.z)<.0001)break;
        assert(actor<people.count&&!seen[actor]);seen[actor]=true;count++;
        const auto &a=people.rows[actor];assert(b->a->pose==a.pose_a&&b->b->pose==a.pose_b&&b->weights[j]==a.pose_blend&&instance.y==a.y&&instance.yaw==a.yaw);
    }
    for(std::size_t i=0;i<people.count;i++){
        const auto &a=people.rows[i];const auto &s=kWildlifeSpecies[a.species];
        const bool expected=std::hypot(a.x-x,a.z-z)<=h.draw_radius&&(pass==2||!world_vegetation_sphere_outside(view,a.x,a.y+s.height*.5,a.z,s.bound_radius,h.draw_radius));
        assert(seen[i]==expected);
    }
    const auto stats=world_wildlife_stats(world);assert(count==drawn&&reactive==0&&prop_start==0&&prop_width==0&&prop_on==0);
    assert(count==(pass==0?stats.visible:pass==1?stats.reflection_visible:stats.shadow_visible));if(!pass)assert(draws==stats.draw_groups);
    return count;
}
void failure_probes(WorldWildlife *world,unsigned bank,std::size_t &probes){
    const auto poses=expected_species(bank)*pose_count,groups=poses*pass_count;char error[256]={};
    for(unsigned kind=0;kind<3;kind++)for(std::size_t attempt=1;attempt<=(kind?groups:poses);attempt++){
        habitat=int(bank);reset_attempts();if(kind==0)fail_load=attempt;else if(kind==1)fail_create=attempt;else fail_target=attempt;
        const auto before=world_wildlife_stats(world).load_failures;
        assert(!world_wildlife_commit(world,int(bank),error,sizeof(error))&&error[0]);empty();
        const auto s=world_wildlife_stats(world);assert(s.habitat==-1&&!s.actors&&!s.pose_assets&&!s.species&&s.load_failures==before+1);probes++;
    }
}
}

namespace {
void unchanged_stats(const WorldWildlifeStats &a,const WorldWildlifeStats &b){
#define EQ(name) assert(a.name==b.name)
    EQ(habitat);EQ(actors);EQ(walking);EQ(idling);EQ(visible);EQ(shadow_visible);EQ(reflection_visible);
    EQ(species);EQ(pose_assets);EQ(draw_groups);EQ(peak_pose_assets);EQ(updates);EQ(uploads);EQ(loads);EQ(unloads);EQ(load_failures);EQ(unsafe_roots);
#undef EQ
}
bool prepare(WorldWildlife *world,unsigned pass,bool loading=true){
    const auto &h=kWildlifeHabitats[habitat];draw_pass=pass;draw_program=pass==2?22:11;
    preparation_frame={draw_program,pass,h.center_x+5000,pass==1?20.:25.75,h.center_z+5000,.3f,pass==1?.08f:-.08f,loading,pass==1,24.01};
    camera_x=preparation_frame.x;camera_y=preparation_frame.y;camera_z=preparation_frame.z;
    last_preparation_clear=preparation_clears;preparing=true;
    const bool okay=world_wildlife_prepare(world,preparation_frame);preparing=false;return okay;
}
void preparation_probes(WorldWildlife *world){
    const auto actors=snapshot(world);const auto ordinary=world_wildlife_stats(world);const auto committed=batch_hashes();
    const auto source_loads=load_attempts,source_live=pose_live,batch_count=batch_live;
    const auto before=world_wildlife_preparation_stats(world);preparation_seen={};
    for(unsigned pass=0;pass<3;pass++)assert(prepare(world,pass));
    const auto after=world_wildlife_preparation_stats(world);const auto source_poses=expected_species(unsigned(habitat))*pose_count;
    assert(after.calls==before.calls+3&&after.main_passes==before.main_passes+1&&after.reflection_passes==before.reflection_passes+1&&after.shadow_passes==before.shadow_passes+1);
    assert(after.pose_draws==before.pose_draws+source_poses*3&&after.failures==before.failures);
    for(unsigned pass=0;pass<3;pass++)for(unsigned species=0;species<species_count;species++)for(unsigned pose=0;pose<pose_count;pose++)
        assert(preparation_seen[pass][species*pose_count+pose]==unsigned(selected(species)));
    // All admitted passes draw every resident walk/idle A/B pair, even with a
    // preparation camera5km outside ordinary actor visibility. Every draw has
    // a fresh depth clear; normal batches, actors and counters stay bit-exact.
    same(actors,snapshot(world));unchanged_stats(ordinary,world_wildlife_stats(world));assert(batch_hashes()==committed);
    assert(load_attempts==source_loads&&pose_live==source_live&&batch_live==batch_count);
    const auto guard_creates=create_attempts,guard_targets=target_attempts,guard_uploads=uploads,guard_queries=queries;
    allocations=0;track=true;assert(!prepare(world,0,false));track=false;
    assert(!allocations&&create_attempts==guard_creates&&target_attempts==guard_targets&&uploads==guard_uploads&&queries==guard_queries);
    for(unsigned stage=0;stage<4;stage++){
        fail_create=stage==0?create_attempts+1:0;fail_target=stage==1?target_attempts+2:0;
        fail_preparation_upload=stage==2?preparation_upload_attempts+2:0;fail_preparation_draw=stage==3;
        assert(!prepare(world,0));fail_create=fail_target=fail_preparation_upload=0;fail_preparation_draw=false;
        same(actors,snapshot(world));unchanged_stats(ordinary,world_wildlife_stats(world));assert(batch_hashes()==committed);
        assert(load_attempts==source_loads&&pose_live==source_live&&batch_live==batch_count&&preparation_error==GL_NO_ERROR);
        // A fresh attempt succeeds after each failure; no stale temporary
        // geometry, borrowed-asset owner or GL error survives the failed pass.
        assert(prepare(world,0));assert(batch_hashes()==committed&&batch_live==batch_count);
    }
    std::printf("PASS wildlife loading preparation bank%d: all%zu resident pose pairs in main/reflection/shadow, near-eye placement and reflectionclip floor; per-pose depth clears, actors/normal stats/committed batches unchanged; explicit loading guard, create/target/upload/draw failure cleanup+retry, no source IO/new resident assets\n",habitat,source_poses);
}
}

int main(){
    static_assert(capacity==32);static_assert(std::size(kWildlifeSpecies)==species_count);static_assert(std::size(kWildlifeHabitats)==3);
    assert(!world_wildlife_create(nullptr)&&!world_wildlife_create(""));char oversized[800];std::memset(oversized,'a',sizeof(oversized));oversized[799]=0;assert(!world_wildlife_create(oversized));
    auto *world=world_wildlife_create("models");assert(world);empty();assert(!load_attempts&&!create_attempts);
    assert(world_wildlife_desired(nullptr,0,0)==-2&&world_wildlife_desired(world,NAN,0)==-2&&world_wildlife_desired(world,0,INFINITY)==-2);
    assert(world_wildlife_desired(world,0,0)==-1&&world_wildlife_commit(world,-1));empty();
    assert(!world_wildlife_commit(nullptr,0)&&!world_wildlife_commit(world,-2)&&!world_wildlife_commit(world,3));
    // Initial/destination loading can prepare the existing hysteresis annulus
    // without changing ordinary enter radii or acquiring any resources here.
    assert(world_wildlife_loading_desired(nullptr,0,0)==-2);
    assert(world_wildlife_loading_desired(world,NAN,0)==-2&&world_wildlife_loading_desired(world,0,INFINITY)==-2);
    assert(world_wildlife_loading_desired(world,0,0)==-1);
    const auto loading_queries=queries,loading_allocations=allocations;
    track=true;
    for(unsigned repeat=0;repeat<512;repeat++)for(unsigned bank=0;bank<std::size(kWildlifeHabitats);bank++){
        const auto &h=kWildlifeHabitats[bank];const double annulus=(h.enter_radius+h.exit_radius)*.5;
        assert(world_wildlife_desired(world,h.center_x+annulus,h.center_z)==-1);
        assert(world_wildlife_loading_desired(world,h.center_x+annulus,h.center_z)==int(bank));
        assert(world_wildlife_loading_desired(world,h.center_x+h.exit_radius,h.center_z)==int(bank));
        assert(world_wildlife_loading_desired(world,h.center_x+h.exit_radius+.001,h.center_z)==-1);
        assert(world_wildlife_loading_desired(world,h.center_x,h.center_z)==world_wildlife_desired(world,h.center_x,h.center_z));
    }
    track=false;
    assert(allocations==loading_allocations&&queries==loading_queries&&!load_attempts&&!create_attempts&&!target_attempts&&!pose_live&&!batch_live);
    assert(world_wildlife_stats(world).habitat==-1);
    std::puts("PASS loading-only residency hints: all3 existing exit envelopes,1536 annulus/edge/outside/normal-priority probes; empty catalog queries allocate/load/create/query0, ordinary enter hysteresis unchanged");
    std::size_t failures=0;
    for(unsigned bank=0;bank<std::size(kWildlifeHabitats);bank++)failure_probes(world,bank,failures);
    for(unsigned bank=0;bank<std::size(kWildlifeHabitats);bank++){
        const auto &h=kWildlifeHabitats[bank];assert(h.enter_radius>0&&h.exit_radius>h.enter_radius&&h.draw_radius>8);
        assert(world_wildlife_desired(world,h.center_x,h.center_z)==int(bank));assert(commit(world,int(bank)));
        auto stats=world_wildlife_stats(world);assert(stats.habitat==int(bank)&&stats.species==expected_species(bank)&&stats.pose_assets==stats.species*pose_count);
        assert(pose_live==stats.pose_assets&&batch_live==pose_live*pass_count&&load_attempts==pose_live&&create_attempts==batch_live&&target_attempts==batch_live);
        assert(world_wildlife_desired(world,h.center_x+h.enter_radius+.1,h.center_z)==int(bank));
        assert(world_wildlife_loading_desired(world,h.center_x+h.enter_radius+.1,h.center_z)==int(bank));
        assert(world_wildlife_loading_desired(world,h.center_x+h.exit_radius+.1,h.center_z)!=int(bank));
        assert(world_wildlife_desired(world,h.center_x+h.exit_radius+.1,h.center_z)!=int(bank));
        const auto loads=load_attempts,creates=create_attempts;assert(world_wildlife_commit(world,int(bank)));assert(load_attempts==loads&&create_attempts==creates);
        world_wildlife_update(world,nullptr,17.125f);valid(world);valid_paths(world);assert(world_wildlife_stats(world).actors==expected_actors(bank));
        const auto fixed=snapshot(world);
        assert(!world_wildlife_commit(world,-2)&&!world_wildlife_commit(world,3));
        assert(world_wildlife_stats(world).habitat==int(bank)&&load_attempts==loads&&create_attempts==creates);same(fixed,snapshot(world));
        for(int rate:{45,60}){
            for(int frame=0;frame<=rate*4;frame++)world_wildlife_update(world,nullptr,float(frame)/rate);
            world_wildlife_update(world,nullptr,17.125f);same(fixed,snapshot(world));
        }
        for(float time:{0.f,.013f,4.9f,2.2f,19.f,.016f,17.125f})world_wildlife_update(world,nullptr,time);
        same(fixed,snapshot(world));
        const auto updated=world_wildlife_stats(world).updates;
        for(float time:{-1.f,NAN,INFINITY,10000001.f})world_wildlife_update(world,nullptr,time);
        assert(world_wildlife_stats(world).updates==updated);same(fixed,snapshot(world));
        std::size_t differing_reflections=0,offscreen_shadow=0;
        for(float yaw:{0.f,float(pi*.5),float(pi),float(pi*1.5)}){
            const auto main=draw(world,0,h.center_x,25.75,h.center_z+20,yaw,-.05f);
            const auto reflection=draw(world,1,h.center_x,-25.75,h.center_z+20,yaw,.05f);
            const auto shadow=draw(world,2,h.center_x,25.75,h.center_z+20,yaw,-.05f);
            differing_reflections+=main!=reflection;offscreen_shadow+=shadow>main;
        }
        assert(differing_reflections&&offscreen_shadow);
        draw(world,0,h.center_x,25.75,h.center_z);draw(world,1,h.center_x,-25.75,h.center_z);draw(world,2,h.center_x,25.75,h.center_z);
        const auto steady_uploads=uploads,warm_queries=queries,steady_loads=load_attempts,steady_creates=create_attempts,steady_targets=target_attempts;
        const auto valid_batches=batch_hashes();
        for(const auto &bad:std::array<std::array<double,6>,7>{{
            {NAN,25.75,h.center_z,0,0,1.6},{h.center_x,INFINITY,h.center_z,0,0,1.6},
            {h.center_x,25.75,NAN,0,0,1.6},{h.center_x,25.75,h.center_z,NAN,0,1.6},
            {h.center_x,25.75,h.center_z,0,INFINITY,1.6},{h.center_x,25.75,h.center_z,0,0,0},
            {h.center_x,25.75,h.center_z,0,0,NAN}}})
            world_wildlife_draw(world,11,bad[0],bad[1],bad[2],float(bad[3]),float(bad[4]),float(bad[5]));
        world_wildlife_draw(world,0,h.center_x,25.75,h.center_z,0,0,1.6);
        world_wildlife_draw(world,11,h.center_x,25.75,h.center_z,0,0,1.6,3);
        assert(batch_hashes()==valid_batches&&uploads==steady_uploads&&queries==warm_queries);
        allocations=0;track=true;
        for(unsigned i=0;i<30;i++){
            world_wildlife_update(world,nullptr,17.125f);
            draw(world,0,h.center_x,25.75,h.center_z);draw(world,1,h.center_x,-25.75,h.center_z);draw(world,2,h.center_x,25.75,h.center_z);
        }
        track=false;assert(!allocations&&uploads==steady_uploads&&queries==warm_queries&&load_attempts==steady_loads&&create_attempts==steady_creates&&target_attempts==steady_targets);
        allocations=0;track=true;
        for(unsigned i=1;i<=1200;i++){
            world_wildlife_update(world,nullptr,float(i)/60);valid(world);valid_paths(world);
            if(i%12==0){draw(world,0,h.center_x,25.75,h.center_z);draw(world,1,h.center_x,-25.75,h.center_z);draw(world,2,h.center_x,25.75,h.center_z);}
        }
        track=false;assert(!allocations&&queries==warm_queries&&load_attempts==steady_loads&&create_attempts==steady_creates&&target_attempts==steady_targets);
        // Each pass has independent uploads: a reflected camera change may
        // not mutate main/shadow batches, and stale groups clear on exit.
        for(unsigned pass=0;pass<3;pass++)assert(!draw(world,pass,h.center_x+h.draw_radius+500,25.75,h.center_z));
        world_wildlife_update(world,nullptr,17.125f);same(fixed,snapshot(world));
        const auto before_fallback=world_wildlife_stats(world);
        center_only=true;world_wildlife_update(world,nullptr,17.125f);valid(world);assert(!world_wildlife_stats(world).walking);center_only=false;
        if(before_fallback.walking)assert(world_wildlife_stats(world).unsafe_roots>before_fallback.unsafe_roots);
        for(Ground mode:{Ground::Invalid,Ground::Low,Ground::Steep}){ground=mode;world_wildlife_update(world,nullptr,10);assert(!world_wildlife_stats(world).actors);}ground=Ground::Flat;
        water=true;world_wildlife_update(world,nullptr,10);assert(!world_wildlife_stats(world).actors);water=false;
        allow_movement=false;world_wildlife_update(world,nullptr,10);assert(!world_wildlife_stats(world).actors);allow_movement=true;
        for(unsigned pass=0;pass<3;pass++)assert(!draw(world,pass,h.center_x,25.75,h.center_z));
        world_wildlife_update(world,nullptr,17.125f);same(fixed,snapshot(world));
        preparation_probes(world);
        assert(commit(world,-1));empty();assert(!world_wildlife_stats(world).actors&&!world_wildlife_stats(world).pose_assets);
        world_wildlife_update(world,nullptr,0);assert(!world_wildlife_actor(world,0,nullptr));
    }
    // Failed destination loading discards the old bank and every partial new
    // resource; no out-of-habitat actors or borrowed source buffers survive.
    assert(commit(world,0));world_wildlife_update(world,nullptr,2);
    const auto before_transition=world_wildlife_stats(world);habitat=1;reset_attempts();fail_load=1;
    char error[256]={};assert(!world_wildlife_commit(world,1,error,sizeof(error))&&error[0]);empty();
    assert(world_wildlife_stats(world).habitat==-1&&!world_wildlife_stats(world).actors&&world_wildlife_stats(world).unloads==before_transition.unloads+1);
    assert(commit(world,0));world_wildlife_update(world,nullptr,2);
    const auto before_replace=world_wildlife_stats(world);assert(commit(world,1));
    assert(world_wildlife_stats(world).unloads==before_replace.unloads+1&&world_wildlife_stats(world).loads==before_replace.loads+1);
    assert(pose_live==expected_species(1)*pose_count&&batch_live==pose_live*pass_count);assert(commit(world,-1));empty();
    // Destination loading screens alone change residency. Walking into the
    // central market requests an empty bank and releases every pose/batch.
    assert(commit(world,0));world_wildlife_update(world,nullptr,2);assert(world_wildlife_desired(world,0,0)==-1);assert(commit(world,-1));empty();
    WorldWildlifeActor actor;assert(!world_wildlife_actor(nullptr,0,&actor)&&!world_wildlife_actor(world,0,&actor));
    assert(!world_wildlife_stats(nullptr).actors);world_wildlife_update(nullptr,nullptr,0);world_wildlife_draw(nullptr,0,0,0,0,0,0,1);
    world_wildlife_free(world);empty();
    world=world_wildlife_create("models");assert(world);assert(!world_wildlife_expand_jungle(nullptr));
    expanded_profile=true;ground=Ground::Flat;water=false;allow_movement=true;center_only=false;assert(world_wildlife_expand_jungle(world));
    assert(world_wildlife_desired(world,-26712,9295)==2&&world_wildlife_desired(world,-26712,9750)==2);
    assert(commit(world,2));assert(!world_wildlife_expand_jungle(world));
    assert(world_wildlife_stats(world).species==3&&world_wildlife_stats(world).pose_assets==36);
    for(float time:{0.f,17.f,26.f,31.f,110.f}){
        const auto old_loads=load_attempts;allocations=0;track=true;world_wildlife_update(world,nullptr,time);track=false;
        assert(!allocations&&load_attempts==old_loads&&world_wildlife_stats(world).actors==12);
        unsigned flying=0,grounded=0;
        for(std::size_t i=0;i<12;i++){
            WorldWildlifeActor a;assert(world_wildlife_actor(world,i,&a));
            if(kWildlifeSpecies[a.species].airborne){assert(a.y>50&&a.speed>0);flying++;}
            else{assert(a.y==24.005f);grounded++;}
        }
        assert(flying==6&&grounded==6);
    }
    assert(world_wildlife_stats(world).unsafe_roots==0);
    world_wildlife_free(world);world_wildlife_free(nullptr);empty();
    std::puts("PASS expanded rainforest: both ends resident; 6 grounded boars/leopards, 6 airborne macaws,36 compatible poses; deterministic bounded updates without allocations/source IO; loaded-profile mutation rejected");
    std::printf("PASS wildlife CPU: 3 banks, 8 types, 12 poses each; %zu pose/create/target failure probes; 32-actor bound; 45/60/mixed timing; source foot rectangles, terrain/water/collision rejection and safe idle fallback; real main/reflection frusta and offscreen shadows; zero hot allocations/IO, unchanged uploads suppressed; all borrowed batches released before poses\n",failures);
}
