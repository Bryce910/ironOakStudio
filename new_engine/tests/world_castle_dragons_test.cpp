#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_castle_dragons.h"
#include "engine/world_atlas.h"
#include "engine/world_models.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
unsigned load_calls=0,create_calls=0,target_calls=0,fail_load=0,fail_create=0,fail_target=0,live_assets=0,live_batches=0;
unsigned uploads=0,draws=0,uniform_queries=0,hot_allocations=0;bool hot=false,upload_failure=false;
float reactive=0,height=0,wind=0;int grass=-1,vegetation=-1,lod=0;
double ox=0,oz=0;
constexpr unsigned pose_count=50,batch_count=58;
struct WorldDragonClip {const char *name;unsigned base,count;float seconds;bool loop;};
struct WorldDragonPoseDef {float mouth[3],direction[3],feet_y,minimum[3],maximum[3];};
#include "../projects/valwood_world/models/castle_dragons/dragon_metadata.inc"
static_assert(VALWOOD_DRAGON_POSE_COUNT==pose_count,"Update dragon correspondence/failure-count tests with the cooked bank");
}
void *operator new(std::size_t n){if(hot)hot_allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,std::size_t)noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t&)noexcept{if(hot)hot_allocations++;return std::malloc(n?n:1);}void operator delete(void *p,const std::nothrow_t&)noexcept{std::free(p);}
struct WorldModelAsset {unsigned index=0;};
struct WorldModelPoseBlend {WorldModelAsset *a=nullptr,*b=nullptr;WorldModelInstance instance={};float weight=0;unsigned count=0;};
extern "C" WorldModelAsset *world_model_load_pose_origin(const char *path,unsigned version){
    assert(version==WORLD_MODEL_POSE_BLEND_VERSION&&path&&std::strstr(path,"models/castle_dragons/"));
    assert(std::strstr(path,"hellfire_")||std::strstr(path,"frostshade_"));
    if(++load_calls==fail_load)return nullptr;
    live_assets++;return new WorldModelAsset{load_calls-1};
}
extern "C" void world_model_free(WorldModelAsset *a){assert(a&&live_assets);live_assets--;delete a;}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t capacity,unsigned version){assert(capacity==1&&version==WORLD_MODEL_POSE_BLEND_VERSION);if(++create_calls==fail_create)return nullptr;live_batches++;return new WorldModelPoseBlend;}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *b,WorldModelAsset *a,WorldModelAsset *next){
    assert(b&&a&&next&&a->index/pose_count==next->index/pose_count);
    if(++target_calls==fail_target)return false;
    b->a=a;b->b=next;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *b,const WorldModelInstance *instances,const float *weights,std::size_t n){
    assert(b&&b->a&&b->b&&n==1&&instances&&weights);const auto &i=*instances;
    assert(std::isfinite(i.x)&&std::isfinite(i.y)&&std::isfinite(i.z)&&std::isfinite(i.yaw)&&i.scale==1&&std::abs(i.x)<=376&&std::abs(i.z)<=321);
    assert(*weights>=0&&*weights<=1);if(upload_failure)return false;
    b->instance=i;b->weight=*weights;b->count=1;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *b,unsigned program){
    assert(b&&b->count==1&&(program==11||program==22||program==33));
    assert(grass==0&&vegetation==0&&lod==-1&&height==12&&wind==0&&reactive==(program==22?0.f:.85f));draws++;
}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *b){assert(b&&live_batches);live_batches--;delete b;}
extern "C" void APIENTRY glUseProgram(GLuint p){assert(p==11||p==22||p==33);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint,const char *s){uniform_queries++;const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};for(unsigned i=0;i<9;i++)if(!std::strcmp(s,names[i]))return int(i+1);assert(false);return -1;}
extern "C" void APIENTRY glUniform3f(GLint l,GLfloat x,GLfloat y,GLfloat z){assert((l==1||l==9)&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));if(l==9)assert((x==650&&y==50&&z==1)||(x==0&&y==0&&z==0));}
extern "C" void APIENTRY glUniform2f(GLint l,GLfloat x,GLfloat z){assert(l==2&&std::abs(x-ox)<.001&&std::abs(z-oz)<.001);}
extern "C" void APIENTRY glUniform1f(GLint l,GLfloat v){if(l==6)wind=v;else if(l==7)height=v;else{assert(l==8);reactive=v;}}
extern "C" void APIENTRY glUniform1i(GLint l,GLint v){if(l==3)grass=v;else if(l==4)vegetation=v;else{assert(l==5);lod=v;}}
namespace {
double distance(const WorldCastleDragon &a,const WorldCastleDragon &b){return std::sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z));}
void inspect(const WorldCastleDragon &a){
    assert(a.variant<2&&a.pose_a<pose_count&&a.pose_b<pose_count&&a.blend>=0&&a.blend<=1);
    assert(std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::isfinite(a.yaw)&&a.y>=a.ground_y-.001);
    double length=0;for(unsigned i=0;i<3;i++){assert(std::isfinite(a.mouth[i])&&std::isfinite(a.direction[i])&&a.minimum[i]<=a.maximum[i]);length+=double(a.direction[i])*a.direction[i];}assert(std::abs(length-1)<.000001);
    const double x=a.x-ox,z=a.z-oz;
    if(a.grounded){assert(std::abs(x-world_castle_dragon_roosts[a.variant][0])<.000001&&std::abs(z-60)<.000001);assert(std::abs(a.y-a.ground_y)<.005);assert(std::abs(x)>330);}
    // Complete wing envelope remains high above the wall/keep during transit.
    if(std::abs(x)<225&&std::abs(z)<225)assert(a.y+a.minimum[1]>75.);
    if(a.breath>0){
        // Both the mouth and a conservative32m plume stay beyond the outer
        // town wall, moat and every public road. Check the entire segment.
        for(unsigned i=0;i<=32;i++){const double px=a.mouth[0]-ox+a.direction[0]*i,pz=a.mouth[2]-oz+a.direction[2]*i;assert(std::abs(px)>225||std::abs(pz)>225);}
    }
}
double flap_phase(const WorldCastleDragon &a){
    if(a.pose_a<10&&a.pose_b<10)return std::fmod((a.pose_a+a.blend)/10.,1.);
    if(a.pose_a>=42&&a.pose_b>=42)return std::fmod((a.pose_a-42+a.blend)/8.,1.);
    if(a.pose_a==0&&a.pose_b==43)return a.blend*.125;
    if(a.pose_a==49&&a.pose_b==0)return .875+a.blend*.125;
    assert(false);return 0;
}
}
int main(){
    assert(world_atlas_init());world_atlas_village_origin(&ox,&oz);char error[300];
    assert(!world_castle_dragons_create(nullptr)&&!world_castle_dragons_create("")&&!world_castle_dragons_commit(nullptr,true));
    WorldCastleDragon a;assert(!world_castle_dragons_sample(2,0,&a)&&!world_castle_dragons_sample(0,-1,&a)&&!world_castle_dragons_sample(0,NAN,&a)&&!world_castle_dragons_sample(0,0,nullptr));
    assert(world_castle_dragons_ground_exclusion_local(350,60)==1&&world_castle_dragons_ground_exclusion_local(-375,60)==1&&world_castle_dragons_ground_exclusion_local(0,0)==0&&world_castle_dragons_ground_exclusion_local(NAN,0)==0);
    for(double r=0;r<=20;r+=.01){const double w=world_castle_dragons_ground_exclusion_local(350+r,60);assert(w>=0&&w<=1);if(r<12)assert(w==1);if(r>18)assert(w==0);}
    for(unsigned who=0;who<2;who++){
        const double x=world_castle_dragon_roosts[who][0],z=world_castle_dragon_roosts[who][1];
        assert(world_castle_dragons_roost_presentation_local(x,z,0)==1);
        for(unsigned side=0;side<4;side++){
            const double dx=side==0?1.:side==1?-1.:0.,dz=side==2?1.:side==3?-1.:0.;
            assert(world_castle_dragons_roost_presentation_local(x+dx*89.999,z+dz*89.999,0)==1);
            assert(world_castle_dragons_roost_presentation_local(x+dx*90.,z+dz*90.,0)==0);
            assert(world_castle_dragons_roost_presentation_local(x+dx*90.,z+dz*90.,1)==1);
            assert(world_castle_dragons_roost_presentation_local(x+dx*109.999,z+dz*109.999,1)==1);
            assert(world_castle_dragons_roost_presentation_local(x+dx*110.,z+dz*110.,1)==0);
        }
        // Corner points inside the cheap AABB still require the circle test.
        assert(world_castle_dragons_roost_presentation_local(x+63.,z+63.,0)==1);
        assert(world_castle_dragons_roost_presentation_local(x+64.,z+64.,0)==0);
        assert(world_castle_dragons_roost_presentation_local(x+77.,z+77.,1)==1);
        assert(world_castle_dragons_roost_presentation_local(x+78.,z+78.,1)==0);
        int state=world_castle_dragons_roost_presentation_local(x+89.,z,0);assert(state==1);
        state=world_castle_dragons_roost_presentation_local(x+100.,z,state);assert(state==1);
        state=world_castle_dragons_roost_presentation_local(x+109.99,z,state);assert(state==1);
        state=world_castle_dragons_roost_presentation_local(x+110.,z,state);assert(state==0);
        state=world_castle_dragons_roost_presentation_local(x+100.,z,state);assert(state==0);
        state=world_castle_dragons_roost_presentation_local(x+89.,z,state);assert(state==1);
    }
    double jungle_x,jungle_z;world_atlas_uv_to_world(.235,.670,&jungle_x,&jungle_z);
    for(int previous=0;previous<2;previous++){
        assert(world_castle_dragons_roost_presentation_local(0,0,previous)==0);
        assert(world_castle_dragons_roost_presentation_local(jungle_x-ox,jungle_z-oz,previous)==0);
        assert(world_castle_dragons_roost_presentation_local(350,60+1.e9,previous)==0);
        assert(world_castle_dragons_roost_presentation_local(1.e9,60,previous)==0);
        for(double invalid:{NAN,INFINITY,-INFINITY}){
            assert(world_castle_dragons_roost_presentation_local(invalid,60,previous)==0);
            assert(world_castle_dragons_roost_presentation_local(350,invalid,previous)==0);
        }
    }
    unsigned failure_probes=0;
    for(unsigned kind=0;kind<3;kind++)for(unsigned n=1;n<=(kind?batch_count*2:pose_count*2);n++){
        load_calls=create_calls=target_calls=fail_load=fail_create=fail_target=0;(kind==0?fail_load:kind==1?fail_create:fail_target)=n;
        auto *d=world_castle_dragons_create();assert(d&&!world_castle_dragons_commit(d,true,error,sizeof(error))&&error[0]);assert(!world_castle_dragons_stats(d).active&&live_assets==0&&live_batches==0);world_castle_dragons_free(d);failure_probes++;
    }
    load_calls=create_calls=target_calls=fail_load=fail_create=fail_target=0;
    auto *d=world_castle_dragons_create();assert(d);assert(world_castle_dragons_desired(d,ox+899,oz)&&!world_castle_dragons_desired(d,ox+900,oz));
    world_castle_dragons_update(d,0);assert(world_castle_dragons_stats(d).updates==0&&uploads==0);
    assert(world_castle_dragons_commit(d,true,error,sizeof(error))&&live_assets==100&&live_batches==116&&load_calls==100);
    assert(world_castle_dragons_desired(d,ox+979,oz)&&!world_castle_dragons_desired(d,ox+980,oz));
    assert(world_castle_dragons_commit(d,true)&&load_calls==100);
    const double cycle=world_castle_dragons_cycle_seconds();assert(std::abs(cycle-149.9)<.000001);
    // Independently reconstruct the source wing phase from the actual
    // prelinked pose indices/weight, including both cross-clip segments.
    // A held endpoint or retiming the2s clip across3.5s fails every such step.
    double previous_phase=0;
    for(unsigned frame=0;frame<=2520;frame++){
        const double t=19.9+frame/600.;assert(world_castle_dragons_sample(0,t,&a));const double phase=flap_phase(a);
        const double expected=std::fmod(t/2.,1.);assert(std::abs(std::remainder(phase-expected,1.))<.0000002);
        if(frame)assert(std::abs(std::remainder(phase-previous_phase,1.)-1./1200.)<.0000002);
        previous_phase=phase;
    }
    std::array<WorldCastleDragon,2> old={};for(unsigned who=0;who<2;who++)assert(world_castle_dragons_sample(who,0,&old[who]));
    std::array<unsigned,8> stages={};unsigned ground_frames=0,air_breath_frames=0,ground_breath_frames=0;
    double largest_step=0,largest_yaw=0,lowest_support=100.,lowest_tip=100.,lowest_foot=100.,separation=1.e9;hot=true;
    for(unsigned frame=0;frame<=36000;frame++){
        const double time=frame/60.;world_castle_dragons_update(d,time);const auto stats=world_castle_dragons_stats(d);
        assert(stats.actors==2&&stats.airborne+stats.grounded==2&&stats.grounded<=1&&stats.breathing<=1&&stats.failures==0);
        for(unsigned who=0;who<2;who++){
            assert(world_castle_dragons_actor(d,who,&a));inspect(a);stages[unsigned(a.stage)]++;
            WorldCastleDragon same;assert(world_castle_dragons_sample(who,time+cycle,&same));assert(distance(a,same)<.00002);
            const bool same_pair=a.pose_a==same.pose_a&&a.pose_b==same.pose_b&&std::abs(a.blend-same.blend)<.00002;
            const bool same_endpoint=(a.blend<.00002&&same.blend>.99998&&a.pose_a==same.pose_b)||(same.blend<.00002&&a.blend>.99998&&same.pose_a==a.pose_b);
            assert(same_pair||same_endpoint);
            const double step=distance(a,old[who]),yaw=std::abs(std::remainder(double(a.yaw-old[who].yaw),6.283185307179586));largest_step=std::max(largest_step,step);largest_yaw=std::max(largest_yaw,yaw);assert(step<.65&&yaw<.006);
            old[who]=a;if(a.grounded)ground_frames++;if(a.breath>0)(a.grounded?ground_breath_frames:air_breath_frames)++;
            if(a.grounded||a.stage==WorldCastleDragonStage::Takeoff){
                // Bounds used by culling are a conservative union; actual
                // blended mesh minima are no lower than the weighted minima.
                const double root_height=a.y-world_atlas_height(a.x,a.z);lowest_support=std::min(lowest_support,root_height);
                const double minimum=world_dragon_pose_metadata[a.pose_a].minimum[1]*(1-a.blend)+world_dragon_pose_metadata[a.pose_b].minimum[1]*a.blend;
                lowest_tip=std::min(lowest_tip,root_height+minimum);assert(root_height+minimum>.014);
            }
            if(a.grounded&&frame%60==0){
                const double foot=world_dragon_pose_metadata[a.pose_a].feet_y*(1-a.blend)+world_dragon_pose_metadata[a.pose_b].feet_y*a.blend;
                const double c=std::cos(a.yaw),s=std::sin(a.yaw);
                // Independently surveyed sourcefootmesh bounds encompass all
                //1,243 weighted toe/foot vertices of the native ground poses.
                for(int iz=0;iz<=8;iz++)for(int ix=0;ix<=8;ix++){
                    const double fx=-1.37+(1.181+1.37)*ix/8.,fz=-1.863+(1.54+1.863)*iz/8.;
                    const double floor=world_atlas_height(a.x+c*fx+s*fz,a.z-s*fx+c*fz);
                    const double clearance=a.y+foot-floor;lowest_foot=std::min(lowest_foot,clearance);assert(clearance>.01&&clearance<.3);
                }
            }
        }
        separation=std::min(separation,distance(old[0],old[1]));assert(separation>25.);
        if(frame%10==0){
            const auto before=world_castle_dragons_stats(d);const unsigned old_uploads=uploads,old_targets=target_calls;
            WorldCastleDragon actor;assert(world_castle_dragons_actor(d,0,&actor));
            for(unsigned pass=0;pass<3;pass++)world_castle_dragons_draw(d,pass==2?22:pass==1?33:11,actor.x+30,actor.y+10,actor.z+25,-.876058f,-.25018f,16.f/9,pass);
            assert(world_castle_dragons_stats(d).uploads==before.uploads&&uploads==old_uploads&&target_calls==old_targets);
        }
    }
    hot=false;assert(hot_allocations==0&&uniform_queries==27&&ground_frames&&air_breath_frames&&ground_breath_frames);
    for(unsigned count:stages)assert(count);
    const auto before=world_castle_dragons_stats(d);const unsigned old_draws=draws,old_uploads=uploads,old_queries=uniform_queries;
    for(unsigned pass=0;pass<3;pass++)world_castle_dragons_draw(d,pass==2?22:11,ox+2000,100,oz+2000,0,0,16.f/9,pass);
    assert(draws==old_draws&&uploads==old_uploads&&uniform_queries==old_queries&&world_castle_dragons_stats(d).visible==0);
    world_castle_dragons_update(d,NAN);world_castle_dragons_update(d,-1);world_castle_dragons_update(d,1e10);assert(world_castle_dragons_stats(d).updates==before.updates);
    world_castle_dragons_update(d,600);const unsigned unchanged=uploads;world_castle_dragons_update(d,600);assert(uploads==unchanged);
    upload_failure=true;world_castle_dragons_update(d,601);assert(world_castle_dragons_stats(d).failures==2);upload_failure=false;world_castle_dragons_update(d,601);assert(world_castle_dragons_stats(d).failures==2);
    assert(world_castle_dragons_commit(d,false)&&live_assets==0&&live_batches==0);const auto inactive=world_castle_dragons_stats(d);world_castle_dragons_update(d,602);world_castle_dragons_draw(d,11,ox,24,oz,0,0,16.f/9);assert(world_castle_dragons_stats(d).updates==inactive.updates&&!world_castle_dragons_actor(d,0,&a));
    world_castle_dragons_free(d);world_castle_dragons_free(nullptr);world_atlas_shutdown();
    std::printf("PASS castle dragons: %u atomic load failures, 600s/two native pose actors, every stage/air+ground breath, safe outside-wall plumes, terrain/contact reserves, periodic continuous roots (max60Hz step%.5fm/yaw%.6f), zero hot allocations/draw uploads/relinks, three consistent passes, hysteresis/cleanup (minimum planted support%.4fm/wing%.4fm/foot%.4fm/dragon separation%.2fm)\n",failure_probes,largest_step,largest_yaw,lowest_support,lowest_tip,lowest_foot,separation);
}
