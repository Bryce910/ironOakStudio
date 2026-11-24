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
#include "tests/world_village_population_runtime_cases.h"
#include "engine/world_models.h"
#include "engine/world_npc_appearance.h"
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
std::size_t bounds_queries=0,bounds_fail_at=0;
int vegetation=-1,lod=0,grass=-1;float wind=-1,reactive=-1,prop_range[3]={-1,-1,-1};
bool shadow_pass=false;
constexpr std::size_t capacity=world_village_resident_count;
constexpr std::size_t storage=capacity+WORLD_EDITED_ACTOR_CAPACITY;
const char *scene_instances_path="projects/valwood_world/scenery/castle_village_instances.txt";
}
void *operator new(std::size_t size){if(track)allocations++;if(void *p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}
void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t size,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(size?size:1);}
void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}

struct WorldModelAsset {char path[1024]={};std::array<WorldModelInstance,storage> instances={};std::size_t count=0;float minimum[3]={},maximum[3]={};};
namespace {std::array<WorldModelAsset*,160> assets={};}
struct WorldModelPoseBlend {WorldModelAsset *a=nullptr,*b=nullptr;std::array<WorldModelInstance,storage> instances={};std::array<float,storage> weights={};std::size_t count=0;};
namespace {std::array<WorldModelPoseBlend*,256> blend_batches={};}
namespace {std::array<bool,200> batch_drawn={};WorldModelPoseBlend *upload_failure=nullptr;}
extern "C" WorldModelAsset *world_model_load(const char *path){
    attempts++;if(fail_at&&attempts==fail_at)return nullptr;
    assert(path&&(std::strstr(path,"models/village_character_")==path||std::strstr(path,"models/purchased_character_")==path)&&!std::strstr(path,".."));
    auto *a=new WorldModelAsset;assert(std::strlen(path)<sizeof(a->path));std::snprintf(a->path,sizeof(a->path),"%s",path);
    unsigned frame=0;const char *tail=std::strrchr(path,'_');assert(tail&&std::sscanf(tail+1,"%u.glb",&frame)==1);
    a->minimum[0]=-.27f-frame*.03f;a->minimum[1]=-.02f;a->minimum[2]=-.35f-frame*.02f;
    a->maximum[0]=.31f+frame*.04f;a->maximum[1]=1.7f+frame*.02f;a->maximum[2]=.45f+frame*.02f;
    auto slot=std::find(assets.begin(),assets.end(),nullptr);assert(slot!=assets.end());*slot=a;live++;return a;
}
extern "C" void world_model_free(WorldModelAsset *a){
    if(!a)return;
    for(auto *batch:blend_batches)if(batch)assert(batch->a!=a&&batch->b!=a);
    auto slot=std::find(assets.begin(),assets.end(),a);assert(slot!=assets.end());*slot=nullptr;assert(live);live--;delete a;
}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){assert(version==WORLD_MODEL_POSE_BLEND_VERSION);return world_model_load(path);}
extern "C" bool world_model_bounds(const WorldModelAsset *asset,float minimum[3],float maximum[3]){
    assert(asset&&minimum&&maximum);bounds_queries++;if(bounds_fail_at==bounds_queries)return false;
    std::copy_n(asset->minimum,3,minimum);std::copy_n(asset->maximum,3,maximum);return true;
}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t size,unsigned version){
    assert((size==storage||size==1)&&version==WORLD_MODEL_POSE_BLEND_VERSION);blend_attempts++;if(blend_fail_at&&blend_attempts==blend_fail_at)return nullptr;
    auto *batch=new WorldModelPoseBlend;auto slot=std::find(blend_batches.begin(),blend_batches.end(),nullptr);assert(slot!=blend_batches.end());*slot=batch;blend_live++;return batch;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *batch,WorldModelAsset *a,WorldModelAsset *b){
    target_attempts++;if(target_fail_at&&target_attempts==target_fail_at)return false;
    assert(batch&&a&&b);char aa[32]={},bb[32]={};assert(std::sscanf(a->path,"models/%*[^_]_character_%31[^_]_",aa)==1&&std::sscanf(b->path,"models/%*[^_]_character_%31[^_]_",bb)==1&&!std::strcmp(aa,bb));batch->a=a;batch->b=b;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *batch,const WorldModelInstance *instances,const float *weights,std::size_t count){
    if(batch==upload_failure){upload_failure=nullptr;return false;}
    assert(batch&&count<=storage);for(std::size_t i=0;i<count;i++){assert(std::isfinite(weights[i])&&weights[i]>=0&&weights[i]<=1);assert(std::isfinite(instances[i].yaw)&&instances[i].scale>0);batch->instances[i]=instances[i];batch->weights[i]=weights[i];}batch->count=count;uploads++;return true;
}
extern "C" bool world_model_pose_blend_enable_character(WorldModelPoseBlend *b){return b!=nullptr;}
extern "C" unsigned world_model_character_capabilities(const WorldModelAsset *){return WORLD_MODEL_CHARACTER_HAIR|WORLD_MODEL_CHARACTER_MASKED_HAIR;}
extern "C" bool world_model_pose_blend_upload_character(WorldModelPoseBlend *b,const WorldModelInstance *p,const float *w,const WorldModelCharacterLook *,std::size_t n){return world_model_pose_blend_upload(b,p,w,n);}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *batch,unsigned program){
    const auto slot=std::find(blend_batches.begin(),blend_batches.end(),batch);assert(slot!=blend_batches.end());batch_drawn[std::size_t(slot-blend_batches.begin())]=true;
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
extern "C" void APIENTRY glUseProgram(GLuint p){gl_calls++;assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint p,const GLchar *name){
    gl_calls++;assert(p==11||p==22);queries++;
    const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange","uStudioGraphIndex"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint loc,GLfloat x,GLfloat y,GLfloat z){
    gl_calls++;assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));assert(loc==1||loc==9);
    if(loc==9){prop_range[0]=x;prop_range[1]=y;prop_range[2]=z;}
}
extern "C" void APIENTRY glUniform2f(GLint loc,GLfloat x,GLfloat y){gl_calls++;assert(loc==2&&std::isfinite(x)&&std::isfinite(y));}
extern "C" void APIENTRY glUniform1i(GLint loc,GLint v){
    gl_calls++;if(loc==3)grass=v;else if(loc==4)vegetation=v;else if(loc==5)lod=v;else if(loc==10)assert(v>=-1&&v<8);else assert(false);
}
extern "C" void APIENTRY glUniform1f(GLint loc,GLfloat v){
    gl_calls++;assert(std::isfinite(v));if(loc==6)wind=v;else if(loc==8)reactive=v;else assert(loc==7&&v==2);
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
bool body_clear(const WorldVillageResident &a,const std::vector<Box> &boxes){
    for(const auto &b:boxes){
        if(a.y+1.75f<=b.ymin||a.y>=b.ymax)continue;
        const double dx=a.x-b.x,dz=a.z-b.z,x=b.c*dx-b.s*dz,z=b.s*dx+b.c*dz;
        const double rx=x-std::clamp(x,double(b.xmin),double(b.xmax)),rz=z-std::clamp(z,double(b.zmin),double(b.zmax));
        if(rx*rx+rz*rz<.30*.30)return false;
    }
    return true;
}
std::array<WorldVillageResident,world_village_resident_count> residents(const WorldVillagePopulation *p){
    std::array<WorldVillageResident,world_village_resident_count> result;for(std::size_t i=0;i<result.size();i++)assert(world_village_population_resident(p,i,&result[i]));return result;
}
void same(const WorldVillageResident &a,const WorldVillageResident &b){assert(!std::strcmp(a.role,b.role)&&!std::strcmp(a.archetype,b.archetype)&&a.x==b.x&&a.z==b.z&&a.y==b.y&&a.yaw==b.yaw&&a.walking==b.walking&&a.speed==b.speed&&a.cycle_phase==b.cycle_phase&&a.pose_a==b.pose_a&&a.pose_b==b.pose_b&&a.pose_blend==b.pose_blend&&a.transitioning==b.transitioning);}
void original_residents_unchanged(WorldVillagePopulation *p,double ox,double oz){
    std::uint64_t hash=1469598103934665603ull;
    auto append=[&](std::int64_t value){for(unsigned byte=0;byte<8;byte++){hash^=(std::uint64_t(value)>>(byte*8))&255;hash*=1099511628211ull;}};
    for(float time:{0.f,.017f,.1f,.99f,1.5f,7.f,13.37f,22.f,55.f,100.f,199.5f,300.f,419.75f,599.99f,600.f}){
        world_village_population_update(p,ox,25.75,oz,time,.017f);
        for(std::size_t i=0;i<world_village_original_resident_count;i++){WorldVillageResident a;assert(world_village_population_resident(p,i,&a));
            append(std::llround((a.x-ox)*100000));append(std::llround((a.z-oz)*100000));append(std::llround(a.yaw*100000));
            append(a.pose_a);append(a.pose_b);append(std::llround(a.pose_blend*100000));append(a.walking);
        }
    }
    assert(hash==10346289246193520808ull);
    std::puts("PASS original128 regression: all original positions, headings, poses and blends at15 absolute timestamps unchanged");
}
void check_groups(){
    constexpr auto appearance_count=std::size(world_village_pose_archetypes);
    unsigned role_modes[appearance_count][2]={};bool frames[appearance_count][2][8]={};
    for(auto *a:assets)if(a){
        char role[32]={},mode[16]={};unsigned frame=99;
        assert(std::sscanf(a->path,"models/%*[^_]_character_%31[^_]_%15[^_]_%u.glb",role,mode,&frame)==3);
        std::size_t r=0;while(r<appearance_count&&std::strcmp(role,world_village_pose_archetypes[r].name))r++;assert(r<appearance_count);
        unsigned m=!std::strcmp(mode,"idle")?1:0;assert(m||!std::strcmp(mode,"walk"));assert(frame<(m?4u:8u));
        assert(!frames[r][m][frame]);frames[r][m][frame]=true;role_modes[r][m]++;
    }
    for(std::size_t i=0;i<appearance_count;i++){
        assert(role_modes[i][0]==world_village_pose_archetypes[i].walk_count);
        assert(role_modes[i][1]==world_village_pose_archetypes[i].idle_count);
    }
    for(std::size_t i=0;i<world_village_pose_group_count;i++){
        const auto *a=assets[i];assert(a);
        bool covered=false;
        for(const auto &appearance:world_village_pose_archetypes){
            for(unsigned mode=0;mode<2;mode++){
                const unsigned base=mode?appearance.idle_group:appearance.walk_group;
                const unsigned count=mode?appearance.idle_count:appearance.walk_count;
                if(i<base||i>=base+count)continue;
                char expected[160];std::snprintf(expected,sizeof(expected),"models/%s_%s_%s_%02zu.glb",appearance.file_prefix,appearance.name,mode?"idle":"walk",i-base);
                assert(!covered&&!std::strcmp(expected,a->path));covered=true;
            }
        }
        assert(covered);
    }
}
void check_appearance_instances(const WorldVillagePopulation *p){
    double ox,oz;world_atlas_village_origin(&ox,&oz);
    const double anchor_x=std::floor(ox/128)*128,anchor_z=std::floor(oz/128)*128;
    const auto people=residents(p);std::array<bool,world_village_resident_count> matched={};
    for(std::size_t ai=0;ai<world_village_pose_blend_group_count;ai++){
        const auto *batch=blend_batches[ai];assert(batch);const auto *asset=batch->a;assert(asset);char appearance[32]={};
        assert(std::sscanf(asset->path,"models/%*[^_]_character_%31[^_]_",appearance)==1);
        for(std::size_t pi=0;pi<batch->count;pi++){
            const auto &placement=batch->instances[pi];std::size_t ri=0;
            for(;ri<people.size();ri++){
                const auto &person=people[ri];
                if(std::hypot(person.x-anchor_x-placement.x,person.z-anchor_z-placement.z)<.001&&person.y+person.visual_y_offset==placement.y&&person.yaw==placement.yaw)break;
            }
            assert(ri<people.size()&&!matched[ri]&&!std::strcmp(people[ri].archetype,appearance)&&people[ri].pose_blend==batch->weights[pi]);matched[ri]=true;
        }
    }
    for(bool found:matched)assert(found);
}
void draw_check(WorldVillagePopulation *p,bool shadow){
    const auto s=world_village_population_stats(p);drawn=draws=0;shadow_pass=shadow;
    const auto previous_gl=gl_calls;
    world_village_population_draw(p,shadow?22:11,0,25.75,0,shadow);
    assert(drawn==s.visible&&draws==s.draw_groups&&reactive==0);
    if(s.draw_groups)assert(gl_calls>previous_gl);
}
void empty_draw_check(WorldVillagePopulation *p){
    const auto state=world_village_population_stats(p);assert(!state.visible&&!state.draw_groups);
    const auto actors=residents(p);const auto previous_gl=gl_calls,previous_queries=queries,previous_draws=draws,previous_uploads=uploads;
    const float previous_reactive=reactive;reactive=.37f;
    // Color, mirrored color, frustum color, and shadow draws must all leave
    // even unrelated incoming GL state untouched when no committed batch exists.
    world_village_population_draw(p,11,0,25.75,0);
    world_village_population_draw(p,11,0,-25.75,0);
    world_village_population_draw_frustum(p,11,0,25.75,0,nullptr);
    world_village_population_draw(p,22,0,25.75,0,true);
    assert(gl_calls==previous_gl&&queries==previous_queries&&draws==previous_draws&&uploads==previous_uploads&&reactive==.37f);
    const auto after=world_village_population_stats(p);
    assert(after.updates==state.updates&&after.frustum_draw_calls==state.frustum_draw_calls);
    const auto after_actors=residents(p);for(std::size_t i=0;i<actors.size();i++)same(actors[i],after_actors[i]);
    reactive=previous_reactive;
}
std::array<float,16> clip_box(const std::array<double,3> &center,double half,bool reflected=false){
    std::array<float,16> result={};result[15]=1;
    for(unsigned axis=0;axis<3;axis++){
        const double sign=reflected&&axis==1?-1.:1.;
        result[axis*4+axis]=float(sign/half);result[12+axis]=float(-sign*center[axis]/half);
    }
    return result;
}
void batch_frustum_checks(WorldVillagePopulation *p,double ox,double oz){
    world_village_population_update(p,ox,25.75,oz,17,0);
    draw_check(p,false);draw_check(p,true);
    const auto warm_queries=queries,warm_bounds=bounds_queries,warm_uploads=uploads;
    const auto old_stats=world_village_population_stats(p);
    const double anchor_x=std::floor(ox/128)*128,anchor_z=std::floor(oz/128)*128;
    std::size_t clip_probes=0;
    const bool previously_tracking=track;const auto old_allocations=allocations;track=true;
    // Each source A/B corner is a witness inside a tiny valid clip box. Probe
    // real yaw-transformed corners, all six clip sides and the mirrored camera.
    // This fails if bounds use only one pose, omit rotation or use main-camera
    // coordinates in reflection. Other source geometry may conservatively stay.
    for(std::size_t slot=0;slot<world_village_pose_blend_group_count;slot++){
        const auto *batch=blend_batches[slot];if(!batch->count)continue;
        const auto instance=batch->instances[0];
        for(const auto *asset:{batch->a,batch->b})for(unsigned corner=0;corner<8;corner++)for(bool reflection:{false,true}){
            const double x=corner&1?asset->maximum[0]:asset->minimum[0],y=corner&2?asset->maximum[1]:asset->minimum[1],z=corner&4?asset->maximum[2]:asset->minimum[2];
            const double c=std::cos(double(instance.yaw)),s=std::sin(double(instance.yaw)),camera_y=reflection?-25.75:25.75;
            const std::array<double,3> point={float(anchor_x-ox)+instance.x+c*x+s*z,float(-camera_y)+instance.y+y,float(anchor_z-oz)+instance.z-s*x+c*z};
            auto matrix=clip_box(point,.02,reflection);batch_drawn.fill(false);drawn=draws=0;shadow_pass=false;
            world_village_population_draw_frustum(p,11,ox,camera_y,oz,matrix.data());
            assert(batch_drawn[slot]&&draws>0&&reactive==0);clip_probes++;
        }
    }
    // Homogeneous all-space view and an empty view prove useful rejection;
    // touching near/side planes above must never become a false rejection.
    const auto full=clip_box({0,0,0},10000),empty=clip_box({10000,10000,10000},1);
    drawn=draws=0;world_village_population_draw_frustum(p,11,ox,25.75,oz,full.data());assert(drawn==old_stats.visible&&draws==old_stats.draw_groups);
    drawn=draws=0;world_village_population_draw_frustum(p,11,ox,25.75,oz,empty.data());assert(!drawn&&!draws);
    auto bad=full;bad[0]=NAN;auto infinite=full;infinite[5]=INFINITY;
    std::array<float,16> zero={},singular=full;singular[5]=0;
    for(const float *matrix:std::array<const float*,5>{nullptr,bad.data(),infinite.data(),zero.data(),singular.data()}){
        drawn=draws=0;world_village_population_draw_frustum(p,11,ox,25.75,oz,matrix);
        assert(drawn==old_stats.visible&&draws==old_stats.draw_groups);
    }
    const auto measured=world_village_population_stats(p);
    assert(measured.frustum_draw_calls-old_stats.frustum_draw_calls==clip_probes+7);
    assert(measured.frustum_culled_batches>old_stats.frustum_culled_batches&&measured.frustum_submitted_batches>old_stats.frustum_submitted_batches);
    assert((measured.frustum_culled_batches-old_stats.frustum_culled_batches)+(measured.frustum_submitted_batches-old_stats.frustum_submitted_batches)==(clip_probes+7)*old_stats.draw_groups);
    draw_check(p,true);draw_check(p,false);
    const auto legacy=world_village_population_stats(p);
    assert(legacy.frustum_draw_calls==measured.frustum_draw_calls&&legacy.frustum_culled_batches==measured.frustum_culled_batches&&legacy.frustum_submitted_batches==measured.frustum_submitted_batches);
    world_village_population_update(p,ox,25.75,oz,17,.5f);
    assert(uploads==warm_uploads&&queries==warm_queries&&bounds_queries==warm_bounds);
    // Retain one failed zero-count upload while all other batches disappear.
    // Its old committed geometry must still cull correctly, both ways.
    std::size_t retained=0;while(retained<world_village_pose_blend_group_count&&!blend_batches[retained]->count)retained++;
    assert(retained<world_village_pose_blend_group_count);const auto retained_count=blend_batches[retained]->count;
    upload_failure=blend_batches[retained];world_village_population_update(p,ox+2000,25.75,oz+2000,17,0);
    assert(!upload_failure&&blend_batches[retained]->count==retained_count&&world_village_population_stats(p).draw_groups==1);
    drawn=draws=0;world_village_population_draw_frustum(p,11,ox,25.75,oz,full.data());assert(draws==1&&drawn==retained_count);
    drawn=draws=0;world_village_population_draw_frustum(p,11,ox,25.75,oz,empty.data());assert(!draws&&!drawn);
    world_village_population_update(p,ox+2000,25.75,oz+2000,17,0);assert(!world_village_population_stats(p).draw_groups);
    world_village_population_update(p,ox,25.75,oz,0,0);
    assert(allocations==old_allocations);track=previously_tracking;
    std::printf("PASS population batch frustum: %zu rotated A/B source-corner/clip-edge/reflection witnesses; useful rejection, invalid-matrix fallback, committed failed-upload retention, unchanged all-shadow pass, stable uploads/bounds queries and zero hot allocations\n",clip_probes);
}
double pose_delta(const WorldVillageResident &a,const WorldVillageResident &b){
    const unsigned indices[]={a.pose_a,a.pose_b,b.pose_a,b.pose_b};double delta=0;
    auto weight=[](const WorldVillageResident &p,unsigned index){return (p.pose_a==index?1.-p.pose_blend:0.)+(p.pose_b==index?p.pose_blend:0.);};
    for(unsigned i=0;i<4;i++){bool seen=false;for(unsigned j=0;j<i;j++)seen=seen||indices[i]==indices[j];if(!seen)delta+=std::abs(weight(a,indices[i])-weight(b,indices[i]));}
    return delta;
}
void continuous_locomotion(WorldVillagePopulation *p,double ox,double oz,const std::vector<Box> &boxes){
    world_village_population_update(p,ox,25.75,oz,0,0);auto previous=residents(p);
    std::array<std::array<double,2>,world_village_resident_count> velocity={};
    std::size_t handovers=0,opposite_stops=0,cruise=0,pair_probes=0,field_probes=0;
    double maximum_pose_delta=0,maximum_yaw_delta=0,maximum_acceleration=0,maximum_contact_step=0,minimum_pair=1e9,minimum_district_pair=1e9;
    for(int frame=1;frame<=36000;frame++){
        const float time=float(frame/60.);world_village_population_update(p,ox,25.75,oz,time,1.f/60);const auto current=residents(p);
        for(std::size_t i=0;i<current.size();i++){
            const auto &a=current[i],&b=previous[i];
            const float floor=world_atlas_walk_height(a.x,a.z);
            assert((a.seated||body_clear(a,boxes))&&std::abs(a.y-floor-(i<128||i>=138?0.f:.015f))<.00001f&&floor>=24&&floor<=24.021f&&(i>=128||world_atlas_landscape_mask(a.x,a.z).trail>.85f));field_probes++;
            assert(a.pose_a<world_village_pose_group_count&&a.pose_b<world_village_pose_group_count&&a.pose_blend>=0&&a.pose_blend<=1&&a.speed>=0&&a.speed<=1.350001f);
            const double delta=pose_delta(a,b),yaw=std::abs(std::remainder(double(a.yaw)-b.yaw,6.283185307179586));
            maximum_pose_delta=std::max(maximum_pose_delta,delta);maximum_yaw_delta=std::max(maximum_yaw_delta,yaw);
            assert(delta<.7&&yaw<.08);
            maximum_contact_step=std::max(maximum_contact_step,double(std::abs(a.visual_y_offset-b.visual_y_offset)));
            assert(a.visual_y_offset<=0&&a.visual_y_offset>=-.05f);
            // A complete opposite-foot handover lasts180ms; its38.6mm
            // support tent may move faster than the1.35m/s horizontal root.
            // Bound per-frame correction independently of route acceleration.
            assert(std::abs(a.visual_y_offset-b.visual_y_offset)<.025f);
            const double vx=(a.x-b.x)*60,vz=(a.z-b.z)*60;
            assert(std::hypot(vx,vz)<1.36);
            if(frame>1){const double acceleration=std::hypot(vx-velocity[i][0],vz-velocity[i][1])*60;maximum_acceleration=std::max(maximum_acceleration,acceleration);assert(acceleration<5.5);}
            velocity[i]={vx,vz};cruise+=a.speed>1.34f;
            if(a.transitioning!=b.transitioning){handovers++;if(a.transitioning&&a.cycle_phase==.5f)opposite_stops++;}
            for(std::size_t j=i+1;j<current.size();j++){
                const double separation=std::hypot(a.x-current[j].x,a.z-current[j].z);
                minimum_pair=std::min(minimum_pair,separation);pair_probes++;
                // One micrometre allows floating subtraction of the village
                // world origin; the actual body-room requirement stays 1.4 m.
                assert(separation>=1.4-1e-6);
                const auto southgate=[](std::size_t n){return n==60||n==61||(n>=102&&n<108);};
                const auto coopers=[](std::size_t n){return n==62||n==63||(n>=108&&n<114);};
                if((southgate(i)&&southgate(j))||(coopers(i)&&coopers(j))){minimum_district_pair=std::min(minimum_district_pair,separation);assert(separation>=1.6-1e-6);}
            }
        }
        previous=current;
    }
    std::printf("PASS renderer sole support: maximum60Hz root adjustment %.3fmm; navigation floor unchanged\n",maximum_contact_step*1000);
    assert(handovers>1000&&opposite_stops>10&&cruise>100000);
    std::printf("PASS continuous locomotion:600s/60Hz/%zu exact-pose handovers including%zu opposite-foot stops; max sparse-pose delta%.4f, yaw%.4frad/frame, acceleration%.3fm/s2; source strides/1.35m/s cruise; no phase/pop/turn snaps\n",handovers,opposite_stops,maximum_pose_delta,maximum_yaw_delta,maximum_acceleration);
    std::printf("PASS full crowd 600s/60Hz: %zu pairs/%zu collision+ground probes; minimum %.6fm overall/%.6fm district; zero allocations and original 1.4m body room preserved\n",pair_probes,field_probes,minimum_pair,minimum_district_pair);
}
}
int main(int argc,char **argv){
    assert(argc<=3);if(argc>=2)assert(chdir(argv[1])==0);
    if(argc==3)scene_instances_path=argv[2];
    assert(world_atlas_init());double ox,oz;world_atlas_village_origin(&ox,&oz);
    const auto boxes=collision_boxes();char error[256]={};
    assert(!world_village_population_load(nullptr,error,sizeof(error))&&!world_village_population_load("",error,sizeof(error))&&!live);
    for(std::size_t i=1;i<=world_village_pose_group_count;i++){
        attempts=0;fail_at=i;auto *failed=world_village_population_load("models",error,sizeof(error));
        assert(!failed&&error[0]&&!live);for(auto *a:assets)assert(!a);
    }
    fail_at=0;
    for(std::size_t failure:{std::size_t(1),world_village_pose_group_count/2,world_village_pose_group_count}){
        bounds_queries=0;bounds_fail_at=failure;auto *failed=world_village_population_load("models",error,sizeof(error));assert(!failed&&error[0]&&!live&&!blend_live);
    }
    bounds_fail_at=0;
    for(std::size_t failure:{std::size_t(1),world_village_pose_blend_group_count/2,world_village_pose_blend_group_count}){
        blend_attempts=0;blend_fail_at=failure;auto *failed=world_village_population_load("models",error,sizeof(error));assert(!failed&&error[0]&&!live&&!blend_live);
    }
    blend_fail_at=0;blend_attempts=0;
    for(std::size_t failure:{std::size_t(1),world_village_pose_blend_group_count}){
        target_attempts=0;target_fail_at=failure;auto *failed=world_village_population_load("models",error,sizeof(error));assert(!failed&&error[0]&&!live&&!blend_live);
    }
    target_fail_at=0;
    for(std::size_t failure=96;failure<=107;failure++){
        blend_attempts=0;blend_fail_at=failure;
        assert(!world_village_population_load("models",error,sizeof(error))&&!live&&!blend_live);
        blend_fail_at=0;target_attempts=0;target_fail_at=failure;
        assert(!world_village_population_load("models",error,sizeof(error))&&!live&&!blend_live);
        target_fail_at=0;
    }
    fail_at=0;attempts=0;auto *p=world_village_population_load("models",error,sizeof(error));assert(p&&!error[0]&&live==world_village_pose_group_count&&attempts==world_village_pose_group_count);
    {
        WorldVillageAppearanceInfo info;assert(world_village_population_appearance_info(p,16,&info)&&info.neutral_height>1);
        world_npc_memory::Appearance attributes;attributes.height_cm=204;attributes.build="Sturdy";attributes.hair_color="#cc5522";attributes.hair_thickness="fine";attributes.hair_density="sparse";
        world_npc_appearance::Resolved look;std::string reason;assert(world_npc_appearance::resolve(attributes,info.neutral_height,info.capabilities,look,reason));assert(std::abs(look.scale*info.neutral_height-2.04f)<1e-5&&look.look.width>1&&look.look.color_strength==1&&look.look.cutoff_offset>0&&look.look.alpha_exponent>1);
        const auto calls=gl_calls;assert(world_village_population_appearance(p,16,look.scale,&look.look)&&gl_calls==calls);
        world_village_population_update(p,ox,25.75,oz,0,0);WorldVillageResident changed,other;assert(world_village_population_resident(p,16,&changed)&&world_village_population_resident(p,18,&other));assert(changed.appearance_scale==look.scale&&other.appearance_scale==1);
        auto bad=look.look;bad.width=0;assert(!world_village_population_appearance(p,16,1,&bad));assert(world_village_population_resident(p,16,&changed)&&changed.appearance_scale==look.scale);
        const WorldVillageAppearanceChange transaction[]={{16,1,{1,0,1,0,{1,1,1},0},true},{9999,1,{1,0,1,0,{1,1,1},0},false}};
        assert(!world_village_population_apply_appearances(p,transaction,2));assert(world_village_population_resident(p,16,&changed)&&changed.appearance_scale==look.scale);
        assert(world_village_population_appearance(p,16,1,nullptr));world_village_population_update(p,ox,25.75,oz,0,0);assert(world_village_population_resident(p,16,&changed)&&changed.appearance_scale==1&&changed.body_width==1);
        world_npc_appearance::Resolved solid;assert(world_npc_appearance::resolve(attributes,1.8f,WORLD_MODEL_CHARACTER_HAIR,solid,reason)&&solid.look.alpha_exponent==1&&solid.look.cutoff_offset==0);
        assert(world_npc_appearance::resolve(attributes,1.8f,0,solid,reason)&&solid.look.color_strength==0);
        float color[3];assert(!world_npc_appearance::hair_color("#badhex",color)&&world_npc_appearance::hair_color("Dark brown",color));
        attributes.hair_density="bald";assert(world_npc_appearance::resolve(attributes,1.8f,WORLD_MODEL_CHARACTER_HAIR,solid,reason)&&solid.look.hide_hair==1);
        char temp[]="/tmp/valwood-looks-XXXXXX";assert(mkdtemp(temp));const auto path=std::string(temp)+"/bindings.vwlooks";
        std::vector<world_npc_appearance::Binding> bindings={{16,"ai/memories/a \\\"profile\\\".vwmemory"},{18,"ai/Étoile.vwmemory"}},restored;
        assert(world_npc_appearance::save_bindings(path,bindings,reason)&&world_npc_appearance::load_bindings(path,restored,reason));assert(restored.size()==2&&restored[0].file==bindings[0].file&&restored[1].file==bindings[1].file);
        {std::ofstream bad_file(path);bad_file<<"VALWOOD_NPC_LOOKS 1 33\n";}assert(!world_npc_appearance::load_bindings(path,restored,reason)&&restored.size()==2);
        bindings[1].resident=16;assert(!world_npc_appearance::save_bindings(path,bindings,reason));std::filesystem::remove_all(temp);
        std::puts("PASS NPC appearance: rooted height/build, per-resident isolation and restore, validated inputs, mesh capabilities, hair color/coverage, bounded atomic binding persistence");
    }
    check_groups();original_residents_unchanged(p,ox,oz);auto stats=world_village_population_stats(p);assert(stats.residents==world_village_resident_count&&stats.knights==16&&stats.archers==16&&stats.traders==20&&stats.travelers==76&&stats.loaded_pose_assets==world_village_pose_group_count);
    assert(stats.sheltered_residents==4&&stats.tavern_residents==6&&stats.street_residents==2&&stats.seated==4);
    for(float time:{0.f,.5f,2.f,8.083333f,31.5f,600.f}){
        world_village_population_update(p,ox-145,25.75,oz-70,time,.017f);
        std::size_t seated=0;
        for(std::size_t i=128;i<140;i++){
            WorldVillageResident a;assert(world_village_population_resident(p,i,&a)&&a.district&&a.name);
            const bool sits=i==128||i==129||i==132||i==133;
            assert(a.seated==sits);if(i<137)assert(!a.walking);
            if(sits){assert(!a.walking&&a.speed==0&&!a.transitioning&&a.pose_a>=76&&a.pose_a<80&&a.pose_b>=76&&a.pose_b<80&&std::abs(a.y-24.015f)<.00001f);seated++;}
            else if(i<137)assert(!a.walking&&a.speed==0&&a.pose_a<76&&a.pose_b<76);
            if(i==137)assert(a.x>=ox-147.501&&a.x<=ox-144.199&&std::abs(a.z-(oz-87.5))<.00001&&a.speed<=.80001f);
        }
        assert(seated==4);
        const auto before=world_village_population_stats(p).uploads;
        world_village_population_update(p,ox-145,25.75,oz-70,time,.5f);
        assert(world_village_population_stats(p).uploads==before);
    }
    std::puts("PASS appended village life:4 sheltered/6 tavern/2 western neighbors;4 genuine seated source cycles, fixed chair roots andfloor; stable repeated-time uploads");
    auto *q=world_village_population_load("models",error,sizeof(error));assert(q&&live==2*world_village_pose_group_count);
    empty_draw_check(q);
    world_village_population_update(p,ox,25.75,oz,0,0);world_village_population_update(q,ox,25.75,oz,0,0);
    auto previous=residents(p);WorldVillageResident out;
    check_appearance_instances(p);
    unsigned appearance_counts[10]={};
    for(std::size_t i=0;i<world_village_original_resident_count;i++){
        const auto &person=previous[i];constexpr unsigned male[]={3,8,7};unsigned expected=i<8?0:i<16?1:i<22?((i-16)%2?4:2):i<54?((i-22)%2?5:male[((i-22)/2)%3]):i<58?6:i<64?((i-58)%2?7:8):i<72?0:i<80?1:i<90?((i-80)%3==0?6:((i-80)%2?4:2)):((i-90)%2?5:male[((i-90)/2)%3]);
        assert(!std::strcmp(person.archetype,world_village_pose_archetypes[expected].name));appearance_counts[expected]++;
    }
    assert(appearance_counts[0]==16&&appearance_counts[1]==16&&appearance_counts[2]==6&&appearance_counts[3]==13&&appearance_counts[4]==6&&appearance_counts[5]==35&&appearance_counts[6]==8&&appearance_counts[7]==14&&appearance_counts[8]==14);
    assert(!world_village_population_resident(p,world_village_resident_count,&out)&&!world_village_population_resident(p,0,nullptr)&&!world_village_population_resident(nullptr,0,&out));
    assert(world_village_population_closest(p,previous[8].x,previous[8].z,0,&out)&&!std::strcmp(out.role,"Archer"));
    assert(!world_village_population_closest(p,ox+2000,oz+2000,1,&out));
    assert(!world_village_population_closest(p,NAN,oz,1,&out)&&!world_village_population_closest(p,ox,oz,-1,&out)&&!world_village_population_closest(p,ox,oz,INFINITY,&out));
    auto unchanged=world_village_population_stats(p);
    for(const auto &args:std::array<std::array<double,6>,8>{{{NAN,25,oz,1,.1,0},{ox,INFINITY,oz,1,.1,0},{ox,25,NAN,1,.1,0},{ox,25,oz,NAN,.1,0},{ox,25,oz,-1,.1,0},{ox,25,oz,1,NAN,0},{ox,25,oz,1,-.1,0},{ox,25,oz,1,1.1,0}}})
        world_village_population_update(p,args[0],args[1],args[2],float(args[3]),float(args[4]));
    assert(world_village_population_stats(p).updates==unchanged.updates);for(std::size_t i=0;i<world_village_resident_count;i++)same(previous[i],residents(p)[i]);
    draw_check(p,false);draw_check(p,true);const auto warm_queries=queries,warm_targets=target_attempts,warm_batches=blend_attempts;
    batch_frustum_checks(p,ox,oz);
    std::array<bool,world_village_resident_count> moved={};unsigned knight=0,archer=0,trader=0,traveler=0;std::size_t probes=0;
    double minimum_pair_clearance=1e9,minimum_alley_pair_clearance=1e9,minimum_market_pair_clearance=1e9,minimum_southgate_pair_clearance=1e9,minimum_return_pair_clearance=1e9;
    std::size_t alley_probes=0,market_probes=0,southgate_probes=0,return_probes=0;
    allocations=0;track=true;
    for(int step=1;step<=2400;step++){
        const float time=step*.25f;const double cx=ox+std::sin(time*.07)*70,cz=oz+std::cos(time*.09)*70;
        world_village_population_update(p,cx,25.75,cz,time,.25f);stats=world_village_population_stats(p);
        assert(stats.residents==world_village_resident_count&&stats.walking+stats.idling==world_village_resident_count&&stats.visible<=world_village_resident_count&&stats.draw_groups<=world_village_pose_group_count);
        auto current=residents(p);
        // Check every pair, including separate loops sharing the main avenue.
        // The old varied-speed, duplicate-lane layout reached exactly0m and
        // intersected twice in the fixed22s review view. Keep body room through
        // corners/dwells and arbitrary absolute-time updates, not only at spawn.
        for(std::size_t a=0;a<current.size();a++)for(std::size_t b=a+1;b<current.size();b++){
            const double clearance=std::hypot(current[a].x-current[b].x,current[a].z-current[b].z);
            minimum_pair_clearance=std::min(minimum_pair_clearance,clearance);
            assert(clearance>=1.4-1e-6);
            if(a>=22&&a<26&&b>=22&&b<26){
                minimum_alley_pair_clearance=std::min(minimum_alley_pair_clearance,clearance);
                assert(clearance>=1.59);
            }
            if(a>=26&&a<34&&b>=26&&b<34){minimum_market_pair_clearance=std::min(minimum_market_pair_clearance,clearance);assert(clearance>=1.79);}
            if(a==60&&b==61){minimum_southgate_pair_clearance=std::min(minimum_southgate_pair_clearance,clearance);assert(clearance>=1.59);}
            if(a==62&&b==63){minimum_return_pair_clearance=std::min(minimum_return_pair_clearance,clearance);assert(clearance>=1.59);}
        }
        unsigned alley_residents=0;
        for(std::size_t i=0;i<current.size();i++){
            const auto &a=current[i];assert(a.name&&a.role&&a.archetype&&std::isfinite(a.x)&&std::isfinite(a.z)&&std::isfinite(a.y)&&std::isfinite(a.yaw));
            const double distance=std::hypot(a.x-previous[i].x,a.z-previous[i].z);assert(distance<=.25*2+1e-5);moved[i]=moved[i]||distance>.001;
            const float floor=world_atlas_walk_height(a.x,a.z);
            assert(std::abs(a.y-floor-(i<128||i>=138?0.f:.015f))<.00001f&&floor>=24&&floor<=24.021f&&(i>=128||world_atlas_landscape_mask(a.x,a.z).trail>.85f));
            assert(std::abs(a.x-ox)<180&&a.z-oz>=-90&&a.z-oz<180&&(a.seated||body_clear(a,boxes)));
            const bool in_alley=a.x-ox>60.8&&a.x-ox<62.7&&a.z-oz>=-25.000001&&a.z-oz<=47.000001;
            assert(in_alley==((i>=22&&i<26)||(i>=90&&i<94)));
            if(in_alley){assert(!std::strcmp(a.role,"Traveler"));++alley_residents;++alley_probes;}
            if((i>=26&&i<34)||(i>=94&&i<102)){assert(!std::strcmp(a.role,"Traveler")&&a.x-ox>=16.999999&&a.x-ox<=18.800001&&a.z-oz>=-12.000001&&a.z-oz<=32.000001);++market_probes;}
            const bool in_southgate=a.x-ox>=-36.800001&&a.x-ox<=-35.199999&&a.z-oz>=67.999999&&a.z-oz<=115.000001;
            const bool in_return=a.x-ox>=24.999999&&a.x-ox<=65.000001&&a.z-oz>=150.199999&&a.z-oz<=151.800001;
            assert(in_southgate==(i==60||i==61||(i>=102&&i<108))&&in_return==(i==62||i==63||(i>=108&&i<114)));
            if(in_southgate){assert(!std::strcmp(a.role,"Traveler"));southgate_probes++;}
            if(in_return){assert(!std::strcmp(a.role,"Traveler"));return_probes++;}
            if(step==1){knight+=!std::strcmp(a.role,"Knight");archer+=!std::strcmp(a.role,"Archer");trader+=!std::strcmp(a.role,"Trader");traveler+=!std::strcmp(a.role,"Traveler");}
            if(i<64&&!std::strcmp(a.role,"Trader")){
                assert(std::abs(std::abs(a.x-ox)-(i<54?25.35:13.35))<1e-8);
                const double dz=a.z-oz;assert(std::abs(dz+6)<1e-8||std::abs(dz-9)<1e-8||std::abs(dz-24)<1e-8);
            }
            probes++;
        }
        assert(alley_residents==8);
        if(step%40==0){
            world_village_population_update(q,cx,25.75,cz,time,.5f);auto copy=residents(q);
            for(std::size_t i=0;i<world_village_resident_count;i++)same(current[i],copy[i]);
        }
        if(step%8==0){draw_check(p,false);draw_check(p,true);}
        previous=current;
    }
    continuous_locomotion(p,ox,oz,boxes);
    track=false;assert(!allocations&&queries==warm_queries&&target_attempts==warm_targets&&blend_attempts==warm_batches&&knight==16&&archer==16&&trader==20&&traveler==76);
    for(std::size_t i=0;i<world_village_resident_count;i++)assert(moved[i]==(i<8||(i>=22&&i<54)||(i>=58&&i<72)||(i>=90&&i<128)||i>=137));
    world_village_population_update(p,ox+35000,25.75,oz+35000,601,.1f);assert(world_village_population_stats(p).visible==0);empty_draw_check(p);
    world_village_population_update(q,ox,25.75,oz,601,.1f);
    const auto far_first=residents(p),near_first=residents(q);for(std::size_t i=0;i<far_first.size();i++)same(far_first[i],near_first[i]);
    world_village_population_update(p,ox+35000,25.75,oz+35000,602,.1f);empty_draw_check(p);
    world_village_population_update(q,ox,25.75,oz,602,.1f);
    const auto far_second=residents(p),near_second=residents(q);bool far_motion=false;
    for(std::size_t i=0;i<far_second.size();i++){same(far_second[i],near_second[i]);far_motion=far_motion||far_first[i].x!=far_second[i].x||far_first[i].z!=far_second[i].z;}
    assert(far_motion&&world_village_population_stats(p).walking==world_village_population_stats(q).walking);
    world_village_population_update(p,ox,25.75,oz,602,.1f);assert(world_village_population_stats(p).visible==world_village_resident_count);
    const auto returned=residents(p);for(std::size_t i=0;i<far_second.size();i++)same(far_second[i],returned[i]);
    draw_check(p,false);draw_check(p,true);
    std::puts("PASS empty population draws: zero GL/query/upload/state work in color/reflection/frustum/shadow; all140 absolute-time states progress35km away and match reentry");
    check_appearance_instances(p);
    std::array<WorldVillageActorConfig,2> edits={};auto &extra=edits[0];extra.count=3;extra.points[0]={ox-130,24,oz-18};extra.points[1]={ox-130,24,oz+8};extra.points[2]=extra.points[0];std::snprintf(extra.name.data(),extra.name.size(),"Authored patrol");auto &hidden=edits[1];hidden.source=8;hidden.appearance=1;hidden.enabled=false;hidden.count=1;hidden.points[0]={ox-35,24,oz-72};assert(world_village_population_configure(p,edits.data(),edits.size()));world_village_population_update(p,ox-130,25.75,oz,10,.016f);WorldVillageResident authored;assert(world_village_population_resident(p,140,&authored)&&!std::strcmp(authored.name,"Authored patrol")&&!world_village_population_resident(p,8,&authored));auto invalid=edits;invalid[0].appearance=1;assert(!world_village_population_configure(p,invalid.data(),invalid.size()));const auto batches_before=blend_attempts;allocations=0;track=true;for(unsigned i=0;i<120;i++)world_village_population_update(p,ox-130,25.75,oz,10+i/60.f,1/60.f);track=false;assert(!allocations&&blend_attempts==batches_before);assert(world_village_population_configure(p,nullptr,0));world_village_population_update(p,ox,25.75,oz,602,.1f);assert(world_village_population_resident(p,8,&authored)&&!world_village_population_resident(p,140,&authored));std::puts("PASS editable residents: added distance-driven patrol, hidden source, invalid appearance rollback, no hot allocations/model owners and clear restores original population");
    allocations=0;track=true;check_authored_population_runtime(p,ox,oz);track=false;assert(!allocations);
    world_village_population_free(p);world_village_population_free(q);world_village_population_free(nullptr);assert(!live);for(auto *a:assets)assert(!a);
    assert(!blend_live);world_atlas_shutdown();
    assert(alley_probes==19200);
    assert(southgate_probes==19200&&return_probes==19200);
    std::printf("PASS district browsing: eight Southgate/%zu probes/%.3fm and eight Coopers Return/%zu probes/%.3fm separation; appearances and actors26/27 original market route preserved\n",southgate_probes,minimum_southgate_pair_clearance,return_probes,minimum_return_pair_clearance);
    assert(market_probes==38400);std::printf("PASS market browsing: sixteen travelers/%zu probes/minimum%.3fm pair separation; total population140 (original128 preserved)\n",market_probes,minimum_market_pair_clearance);
    std::printf("PASS population CPU:80 atomic partial-load failures,140 roles/80 pose groups including all5 purchased families,6 female traders and35 female travelers;600s/%zu resident probes on clear24m village ground; eight Merchants Lane travelers/%zu probes/minimum%.3fm separation; all-pair crowd clearance minimum%.3fm; deterministic bounded motion/closest/culling, correct appearance instances; zero hot allocations/uniform queries; all model ownership released\n",probes,alley_probes,minimum_alley_pair_clearance,minimum_pair_clearance);
}
