/* CPU-only scripted practice proof. Renderer stubs validate bounded batches,
 * atomic load failures, complete pass parity and allocation-free updates. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_castle_training.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
std::size_t attempts=0,fail_pose=0,pose_live=0,batch_attempts=0,fail_batch=0,batch_live=0,target_attempts=0,fail_target=0,uploads=0,draws=0,drawn=0,queries=0,allocations=0,gl_calls=0;
bool track=false,shadow_pass=false;float reactive=0,wind=0;int grass=0,vegetation=0,lod=-1;
constexpr std::size_t capacity=world_castle_training_actor_count;
}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(n?n:1);}
void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}
struct WorldModelAsset {unsigned index=0;};
struct WorldModelPoseBlend {WorldModelAsset *a=nullptr,*b=nullptr;std::size_t size=0;};
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
    assert(version==WORLD_MODEL_POSE_BLEND_VERSION);attempts++;if(attempts==fail_pose)return nullptr;
    unsigned i=0;assert(std::sscanf(path,"models/castle_training_knight_drill_%u.glb",&i)==1||std::sscanf(path,"models/village_character_knight_idle_%u.glb",&i)==1);
    auto *a=new WorldModelAsset;a->index=unsigned(attempts-1);pose_live++;return a;
}
extern "C" void world_model_free(WorldModelAsset *a){if(a){assert(pose_live);pose_live--;delete a;}}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t n,unsigned version){
    assert((n==capacity||n==1)&&version==WORLD_MODEL_POSE_BLEND_VERSION);batch_attempts++;if(batch_attempts==fail_batch)return nullptr;batch_live++;return new WorldModelPoseBlend;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *b,WorldModelAsset *a,WorldModelAsset *c){
    assert(b&&a&&c);target_attempts++;if(target_attempts==fail_target)return false;
    const unsigned base=a->index<24?0:24,n=a->index<24?24:4;
    assert(c->index==base+(a->index-base+1)%n);b->a=a;b->b=c;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *b,const WorldModelInstance *p,const float *weights,std::size_t n){
    assert(b&&b->a&&b->b&&n<=capacity);for(std::size_t i=0;i<n;i++){
        assert(std::isfinite(p[i].x)&&std::isfinite(p[i].z)&&p[i].y==24&&p[i].scale==1);
        assert(std::isfinite(weights[i])&&weights[i]>=0&&weights[i]<=1);
    }b->size=n;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *b,unsigned program){assert(b&&b->size&&program);assert(!grass&&!vegetation&&lod==-1&&wind==0&&reactive==(shadow_pass?0:.85f));draws++;drawn+=b->size;}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *b){if(b){assert(batch_live);batch_live--;delete b;}}
extern "C" void world_atlas_village_origin(double *x,double *z){*x=1000000.25;*z=-2000000.75;}
extern "C" void APIENTRY glUseProgram(GLuint p){gl_calls++;assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint p,const GLchar *name){
    gl_calls++;assert(p==11||p==22);queries++;const char *names[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};
    for(std::size_t i=0;i<std::size(names);i++)if(!std::strcmp(name,names[i]))return GLint(i+1);
    assert(false);return -1;
}
extern "C" void APIENTRY glUniform3f(GLint n,GLfloat x,GLfloat y,GLfloat z){gl_calls++;assert(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));assert(n==1||(n==9&&x==0&&y==0&&z==0));}
extern "C" void APIENTRY glUniform2f(GLint n,GLfloat x,GLfloat z){gl_calls++;assert(n==2&&std::isfinite(x)&&std::isfinite(z));}
extern "C" void APIENTRY glUniform1i(GLint n,GLint x){gl_calls++;if(n==3)grass=x;else if(n==4)vegetation=x;else if(n==5)lod=x;else assert(false);}
extern "C" void APIENTRY glUniform1f(GLint n,GLfloat x){gl_calls++;assert(std::isfinite(x));if(n==6)wind=x;else if(n==8)reactive=x;else assert(n==7&&x==2);}
namespace {
void reset(){attempts=batch_attempts=target_attempts=0;fail_pose=fail_batch=fail_target=0;}
void draw(WorldCastleTraining *p,bool shadow){drawn=draws=0;shadow_pass=shadow;const auto previous_gl=gl_calls;world_castle_training_draw(p,shadow?22:11,1000000,25.75,-2000000,shadow);auto s=world_castle_training_stats(p);assert(drawn==s.visible&&draws==s.draw_groups&&reactive==0);if(s.draw_groups)assert(gl_calls>previous_gl);}
std::array<WorldCastleTrainingActor,capacity> actors(const WorldCastleTraining *p){
    std::array<WorldCastleTrainingActor,capacity> result={};for(std::size_t i=0;i<result.size();i++)assert(world_castle_training_actor(p,i,&result[i]));return result;
}
void equal(const WorldCastleTrainingActor &a,const WorldCastleTrainingActor &b){
    assert(a.name==b.name&&a.role==b.role&&a.x==b.x&&a.z==b.z&&a.y==b.y&&a.yaw==b.yaw&&a.cycle_phase==b.cycle_phase&&a.pose_a==b.pose_a&&a.pose_b==b.pose_b&&a.pose_blend==b.pose_blend&&a.instructor==b.instructor);
}
void empty_draw_check(WorldCastleTraining *p){
    const auto state=world_castle_training_stats(p);assert(!state.visible&&!state.draw_groups);
    const auto before=actors(p);const auto previous_gl=gl_calls,previous_queries=queries,previous_draws=draws,previous_uploads=uploads;
    const float previous_reactive=reactive;reactive=.37f;
    world_castle_training_draw(p,11,1000000,25.75,-2000000);
    world_castle_training_draw(p,11,1000000,-25.75,-2000000);
    world_castle_training_draw(p,22,1000000,25.75,-2000000,true);
    assert(gl_calls==previous_gl&&queries==previous_queries&&draws==previous_draws&&uploads==previous_uploads&&reactive==.37f);
    assert(world_castle_training_stats(p).updates==state.updates);
    const auto after=actors(p);for(std::size_t i=0;i<before.size();i++)equal(before[i],after[i]);
    reactive=previous_reactive;
}
}
int main(){
    char error[256]={};assert(!world_castle_training_load(nullptr,error,sizeof(error)));assert(!world_castle_training_load("",error,sizeof(error)));
    for(std::size_t i=1;i<=world_castle_training_pose_count;i++){
        reset();fail_pose=i;assert(!world_castle_training_load("models",error,sizeof(error))&&error[0]&&!pose_live&&!batch_live);
        reset();fail_batch=i;assert(!world_castle_training_load("models",error,sizeof(error))&&error[0]&&!pose_live&&!batch_live);
        reset();fail_target=i;assert(!world_castle_training_load("models",error,sizeof(error))&&error[0]&&!pose_live&&!batch_live);
    }
    for(std::size_t i=29;i<=52;i++){
        reset();fail_batch=i;assert(!world_castle_training_load("models",error,sizeof(error))&&!pose_live&&!batch_live);
        reset();fail_target=i;assert(!world_castle_training_load("models",error,sizeof(error))&&!pose_live&&!batch_live);
    }
    reset();auto *p=world_castle_training_load("models",error,sizeof(error));assert(p&&!error[0]);
    assert(pose_live==28&&batch_live==52);auto s=world_castle_training_stats(p);assert(s.actors==7&&s.trainees==6&&s.instructors==1&&s.loaded_pose_assets==28);
    empty_draw_check(p);assert(!queries);
    world_castle_training_update(p,1000000,25.75,-2000000,0,0);s=world_castle_training_stats(p);assert(s.visible==7&&s.draw_groups<=7);
    draw(p,false);draw(p,true);assert(queries==18);
    const auto u=s.uploads;world_castle_training_update(p,1000000,25.75,-2000000,0,0);assert(world_castle_training_stats(p).uploads==u);
    allocations=0;track=true;double min_separation=1e9;
    for(unsigned frame=0;frame<=36000;frame++){
        const float time=float(frame)/60;world_castle_training_update(p,1000000,25.75,-2000000,time,1.f/60);
        std::array<WorldCastleTrainingActor,7> a={};
        for(std::size_t i=0;i<a.size();i++){
            assert(world_castle_training_actor(p,i,&a[i]));const auto &station=world_castle_training_stations[i];
            assert(a[i].x==1000000.25+station.x&&a[i].z==-2000000.75+station.z&&a[i].y==24&&a[i].yaw==station.yaw);
            assert(a[i].pose_blend>=0&&a[i].pose_blend<1&&a[i].cycle_phase>=0&&a[i].cycle_phase<1);
            assert(a[i].instructor==(i==6)&&a[i].pose_a<(i==6?28:24)&&a[i].pose_b<(i==6?28:24));
            if(i==6)assert(a[i].pose_a>=24&&a[i].pose_b>=24);
            for(std::size_t j=0;j<i;j++)min_separation=std::min(min_separation,std::hypot(a[i].x-a[j].x,a[i].z-a[j].z));
        }
        if(frame%600==0){draw(p,false);draw(p,true);draw(p,false);}
    }
    track=false;assert(!allocations&&queries==18&&min_separation>3.99);
    auto before=world_castle_training_stats(p);world_castle_training_update(p,NAN,0,0,0,0);world_castle_training_update(p,0,0,0,NAN,0);world_castle_training_update(p,0,0,0,-1,0);world_castle_training_update(p,0,0,0,0,-1);assert(world_castle_training_stats(p).updates==before.updates);
    WorldCastleTrainingActor saved={},again={};assert(world_castle_training_actor(p,0,&saved));
    world_castle_training_update(p,1000000,25.75,-2000000,77,0);assert(world_castle_training_actor(p,0,&again));
    world_castle_training_update(p,1000000,25.75,-2000000,600,0);assert(world_castle_training_actor(p,0,&again));assert(saved.pose_a==again.pose_a&&saved.pose_b==again.pose_b&&saved.pose_blend==again.pose_blend);
    world_castle_training_update(p,1035000,25.75,-1965000,600,0);assert(world_castle_training_stats(p).visible==0&&world_castle_training_stats(p).draw_groups==0);empty_draw_check(p);
    const auto far_first=actors(p);
    world_castle_training_update(p,1035000,25.75,-1965000,601,0);empty_draw_check(p);const auto far_second=actors(p);
    assert(far_first[0].cycle_phase!=far_second[0].cycle_phase);
    world_castle_training_update(p,1000000,25.75,-2000000,601,0);const auto returned=actors(p);
    for(std::size_t i=0;i<returned.size();i++)equal(far_second[i],returned[i]);
    draw(p,false);draw(p,true);
    world_castle_training_update(p,1000000,25.75,-2000000,600,0);assert(world_castle_training_stats(p).visible==7);draw(p,false);draw(p,true);
    std::puts("PASS empty training draws: zero GL/query/upload/state work in color/reflection/shadow; all7 absolute-time states progress35km away and match reentry");
    assert(!world_castle_training_actor(p,7,&saved)&&!world_castle_training_actor(nullptr,0,&saved)&&!world_castle_training_actor(p,0,nullptr));
    // The interactive lane reuses the same assets/batches and removes the
    // partner from all passes while the player occupies its station.
    world_castle_training_sparring(p,true,.25f);
    track=true;
    world_castle_training_update(p,1000000-50,25.75,-2000000-73,600,.016f);
    assert(world_castle_training_stats(p).visible==6);assert(world_castle_training_actor(p,0,&again));
    assert(again.pose_a==6&&again.pose_b==7&&again.pose_blend==0&&again.y==24);
    const float first_yaw=again.yaw;
    world_castle_training_update(p,1000000-47,25.75,-2000000-76,600,.016f);assert(world_castle_training_actor(p,0,&again));
    assert(std::abs(std::remainder(double(again.yaw-first_yaw),6.283185307179586))<=2.8*.016+.000001);
    draw(p,false);draw(p,true);
    world_castle_training_replace_opponent(p,true);
    world_castle_training_update(p,1000000-47,25.75,-2000000-76,600,.016f);
    assert(world_castle_training_stats(p).visible==5);draw(p,false);draw(p,true);
    world_castle_training_replace_opponent(p,false);
    world_castle_training_update(p,1000000-47,25.75,-2000000-76,600,.016f);
    assert(world_castle_training_stats(p).visible==6);draw(p,false);draw(p,true);
    track=false;assert(allocations==0);
    world_castle_training_sparring(p,false,0);world_castle_training_update(p,1000000,25.75,-2000000,600,0);assert(world_castle_training_stats(p).visible==7);
    std::puts("PASS interactive station:6 drawn/7 retained, exact reused pose pair, bounded facing turns, zero allocations, shadow parity and full restoration");
    track=true;
    for(unsigned i=0;i<240;i++){
        assert(world_castle_training_player_update(p,1000000,24,-2000000,0,i/240.f,false));
        shadow_pass=false;world_castle_training_player_draw(p,11,1000000,25.75,-2000000);
        shadow_pass=true;world_castle_training_player_draw(p,22,1000000,25.75,-2000000,true);
    }
    track=false;assert(allocations==0);
    world_castle_training_free(p);assert(!pose_live&&!batch_live);world_castle_training_free(nullptr);
    std::puts("PASS castle training: 7 actors / 28 poses; 600 seconds absolute-time animation; zero update allocations; all 132 atomic load/batch/target failures; player pose/shadow parity; color/reflection/shadow parity; cull/reentry; finite guards");
}
