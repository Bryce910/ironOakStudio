#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_trail_camp.h"
#include "engine/world_models.h"
#include "engine/world_scenery.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
unsigned load_calls=0,create_calls=0,target_calls=0,fail_load=0,fail_create=0,fail_target=0,live_assets=0,live_batches=0;
unsigned hot_allocations=0,queries=0,draws=0,submitted_instances=0;bool hot=false,wet=false,invalid=false,blocked=false;
float reactive=0,height=0,wind=0;int grass=-1,vegetation=-1,lod=0;
}
void *operator new(std::size_t n){if(hot)hot_allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,std::size_t)noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t&)noexcept{if(hot)hot_allocations++;return std::malloc(n?n:1);}void operator delete(void *p,const std::nothrow_t&)noexcept{std::free(p);}
struct WorldModelAsset {unsigned index;};
struct WorldModelPoseBlend {WorldModelAsset *a=nullptr,*b=nullptr;std::size_t count=0;};
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
    assert(version==WORLD_MODEL_POSE_BLEND_VERSION&&path&&std::strstr(path,"models/"));
    if(++load_calls==fail_load)return nullptr;
    live_assets++;return new WorldModelAsset{load_calls-1};
}
extern "C" void world_model_free(WorldModelAsset *a){assert(a&&live_assets);live_assets--;delete a;}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t capacity,unsigned version){assert(capacity==4&&version==WORLD_MODEL_POSE_BLEND_VERSION);if(++create_calls==fail_create)return nullptr;live_batches++;return new WorldModelPoseBlend;}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *b,WorldModelAsset *a,WorldModelAsset *c){assert(b&&a&&c);if(++target_calls==fail_target)return false;b->a=a;b->b=c;return true;}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *b,const WorldModelInstance *instances,const float *weights,std::size_t n){
    assert(b&&b->a&&b->b&&n<=4);for(std::size_t i=0;i<n;i++){assert(std::isfinite(instances[i].x)&&std::isfinite(instances[i].y)&&std::isfinite(instances[i].z));assert(std::abs(instances[i].x)<15&&std::abs(instances[i].z)<15);assert(weights[i]>=0&&weights[i]<=1);}b->count=n;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *b,unsigned program){assert(b&&b->count&&(program==11||program==22));assert(grass==0&&vegetation==0&&lod==-1&&height==2&&wind==0&&reactive==(program==22?0.f:.85f));draws++;submitted_instances+=unsigned(b->count);}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *b){assert(b&&live_batches);live_batches--;delete b;}
extern "C" float world_atlas_height(double x,double z){assert(std::isfinite(x)&&std::isfinite(z));return invalid?NAN:float(WORLD_TRAIL_CAMP_Y);}
extern "C" bool world_atlas_local_water(double,double,float *,float *){return wet;}
bool world_scenery_can_move(const WorldScenery *,double,double,double x,double z,float y,float h,float r){assert(std::isfinite(x)&&std::isfinite(z)&&h==2&&r==.28f&&std::abs(y-float(WORLD_TRAIL_CAMP_Y+.015))<.001);return !blocked;}
extern "C" void APIENTRY glUseProgram(GLuint p){assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint,const char *s){queries++;const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};for(unsigned i=0;i<9;i++)if(!std::strcmp(s,names[i]))return int(i+1);assert(false);return -1;}
extern "C" void APIENTRY glUniform3f(GLint l,GLfloat x,GLfloat y,GLfloat z){assert((l==1||l==9)&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));}
extern "C" void APIENTRY glUniform2f(GLint l,GLfloat x,GLfloat z){assert(l==2&&x==float(WORLD_TRAIL_CAMP_X)&&z==float(WORLD_TRAIL_CAMP_Z));}
extern "C" void APIENTRY glUniform1f(GLint l,GLfloat v){if(l==6)wind=v;else if(l==7)height=v;else{assert(l==8);reactive=v;}}
extern "C" void APIENTRY glUniform1i(GLint l,GLint v){if(l==3)grass=v;else if(l==4)vegetation=v;else{assert(l==5);lod=v;}}
int main(){
    char error[256];assert(!world_trail_camp_create(nullptr));assert(!world_trail_camp_commit(nullptr,true));
    unsigned failure_probes=0;
    for(unsigned kind=0;kind<3;kind++)for(unsigned n=1;n<=(kind?60u:20u);n++){
        load_calls=create_calls=target_calls=fail_load=fail_create=fail_target=0;
        (kind==0?fail_load:kind==1?fail_create:fail_target)=n;
        auto *c=world_trail_camp_create();assert(c&&!world_trail_camp_commit(c,true,error,sizeof(error))&&error[0]);
        assert(!world_trail_camp_stats(c).active&&live_assets==0&&live_batches==0);world_trail_camp_free(c);failure_probes++;
    }
    load_calls=create_calls=target_calls=fail_load=fail_create=fail_target=0;
    auto *c=world_trail_camp_create();assert(c&&world_trail_camp_desired(c,WORLD_TRAIL_CAMP_X+259,WORLD_TRAIL_CAMP_Z));
    assert(!world_trail_camp_desired(c,WORLD_TRAIL_CAMP_X+260,WORLD_TRAIL_CAMP_Z));
    assert(world_trail_camp_commit(c,true,error,sizeof(error)));assert(live_assets==20&&live_batches==60);
    assert(world_trail_camp_desired(c,WORLD_TRAIL_CAMP_X+299,WORLD_TRAIL_CAMP_Z));
    assert(!world_trail_camp_desired(c,WORLD_TRAIL_CAMP_X+300,WORLD_TRAIL_CAMP_Z));
    const auto *scene=reinterpret_cast<const WorldScenery *>(1);
    WorldTrailCamper previous;world_trail_camp_update(c,scene,0);assert(world_trail_camp_actor(c,3,&previous));
    hot=true;
    for(unsigned frame=1;frame<=36000;frame++){
        const float t=float(frame/60.);world_trail_camp_update(c,scene,t);
        const auto stats=world_trail_camp_stats(c);assert(stats.actors==4&&stats.walking==1&&stats.seated==2&&stats.unsafe_roots==0);
        WorldTrailCamper a;assert(world_trail_camp_actor(c,3,&a));assert(a.walking&&!a.seated&&a.pose_a>=8&&a.pose_a<16&&a.pose_b>=8&&a.pose_b<16);
        assert(std::hypot(a.x-previous.x,a.z-previous.z)<.018&&a.blend>=0&&a.blend<=1);previous=a;
        for(unsigned i=0;i<4;i++){
            assert(world_trail_camp_actor(c,i,&a));assert(std::abs(a.y-float(WORLD_TRAIL_CAMP_Y+.015))<.0001);
            // Upper-body idle motion must not drag a stationary camper's feet
            // by swiveling the complete root transform around the fire.
            if(i==2){assert(a.x==WORLD_TRAIL_CAMP_X-2.5&&a.z==WORLD_TRAIL_CAMP_Z-4.);
                assert(std::abs(a.yaw-std::atan2(2.5,4.))<.000001);}
        }
        if(frame%10==0){
            world_trail_camp_draw(c,11,WORLD_TRAIL_CAMP_X-12,WORLD_TRAIL_CAMP_Y+1.75,WORLD_TRAIL_CAMP_Z+2,1.7f,-.08f,16.f/9,0);
            world_trail_camp_draw(c,11,WORLD_TRAIL_CAMP_X-12,WORLD_TRAIL_CAMP_Y-2,WORLD_TRAIL_CAMP_Z+2,1.7f,.08f,16.f/9,1);
            world_trail_camp_draw(c,22,WORLD_TRAIL_CAMP_X-12,WORLD_TRAIL_CAMP_Y+1.75,WORLD_TRAIL_CAMP_Z+2,1.7f,-.08f,16.f/9,2);
        }
    }
    hot=false;assert(hot_allocations==0&&queries==18&&draws>100);
    const auto before_hidden=world_trail_camp_stats(c);const unsigned old_draws=draws,old_queries=queries;
    for(unsigned pass=0;pass<3;pass++)world_trail_camp_draw(c,pass==2?22:11,WORLD_TRAIL_CAMP_X-300,WORLD_TRAIL_CAMP_Y+1.75,WORLD_TRAIL_CAMP_Z,1.57f,0,16.f/9,pass);
    assert(draws==old_draws&&queries==old_queries&&world_trail_camp_stats(c).uploads==before_hidden.uploads&&world_trail_camp_stats(c).visible==0);
    world_trail_camp_update(c,scene,601);
    for(unsigned pass=0;pass<3;pass++){
        const unsigned instances=submitted_instances;
        world_trail_camp_draw(c,pass==2?22:11,WORLD_TRAIL_CAMP_X-12,WORLD_TRAIL_CAMP_Y+1.75,WORLD_TRAIL_CAMP_Z+2,1.7f,-.08f,16.f/9,pass);
        assert(submitted_instances-instances==4&&queries==old_queries);
    }

    blocked=true;world_trail_camp_update(c,scene,30);assert(world_trail_camp_stats(c).walking==0&&world_trail_camp_stats(c).unsafe_roots==1);blocked=false;
    wet=true;world_trail_camp_update(c,scene,30);assert(world_trail_camp_stats(c).actors==0);wet=false;
    invalid=true;world_trail_camp_update(c,scene,30);assert(world_trail_camp_stats(c).actors==0);invalid=false;
    assert(world_trail_camp_commit(c,false));assert(live_assets==0&&live_batches==0);world_trail_camp_free(c);world_trail_camp_free(nullptr);
    std::printf("PASS trail camp: %u atomic failure probes, 600 seconds/four campers, continuous gait, seated support, three independent passes, bounded uniforms, zero hot allocations, hysteresis/water/terrain/collision rejection and complete cleanup\n",failure_probes);
}
