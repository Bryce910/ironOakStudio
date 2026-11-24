/* CPU-only bandit bank ownership, absolute gait and bounded render-pass proof. */
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_bandit_camp.h"
#include "engine/world_models.h"
#include "engine/world_scenery.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
constexpr unsigned N=bandit_camp_pose_count,C=world_bandit_camp_capacity;
std::size_t la=0,ca=0,ta=0,fl=0,fc=0,ft=0,live=0,blive=0,uploads=0,queries=0,drawn=0,allocations=0;
bool track=false,wet=false,blocked=false,invalid=false;unsigned current_pass=0;float reactive=0,wind=0,height=0;int grass=-1,vegetation=-1,lod=0;
}
void *operator new(std::size_t n){if(track)allocations++;if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p) noexcept{std::free(p);}void operator delete(void *p,std::size_t) noexcept{std::free(p);}
void *operator new(std::size_t n,const std::nothrow_t &) noexcept{if(track)allocations++;return std::malloc(n?n:1);}void operator delete(void *p,const std::nothrow_t &) noexcept{std::free(p);}
struct WorldModelAsset {unsigned pose=0;};
struct WorldModelPoseBlend {unsigned slot=0,pass=0;WorldModelAsset *a=nullptr,*b=nullptr;std::size_t count=0;};
namespace {std::array<WorldModelAsset *,N> assets={};std::array<WorldModelPoseBlend *,N*3> batches={};}
extern "C" WorldModelAsset *world_model_load_pose(const char *path,unsigned version){
 assert(version==WORLD_MODEL_POSE_BLEND_VERSION);if(++la==fl)return nullptr;
 const char *modes[]={"walk","run","stance"};unsigned pose=N;
 for(unsigned m=0;m<3;m++)for(unsigned i=0;i<bandit_camp_clips[m].count;i++){char p[128];std::snprintf(p,sizeof(p),"models/tanthoon_bandit_%s_%02u.glb",modes[m],i);if(!std::strcmp(path,p))pose=bandit_camp_clips[m].base+i;}
 assert(pose<N&&!assets[pose]);assets[pose]=new WorldModelAsset{pose};live++;return assets[pose];
}
extern "C" void world_model_free(WorldModelAsset *a){if(!a)return;for(auto *b:batches)if(b)assert(b->a!=a&&b->b!=a);assert(assets[a->pose]==a);assets[a->pose]=nullptr;live--;delete a;}
extern "C" WorldModelPoseBlend *world_model_pose_blend_create(std::size_t n,unsigned version){
 assert(n==C&&version==WORLD_MODEL_POSE_BLEND_VERSION);if(++ca==fc)return nullptr;unsigned i=0;while(batches[i])i++;assert(i<batches.size());auto *b=new WorldModelPoseBlend;b->slot=i;batches[i]=b;blive++;return b;
}
extern "C" bool world_model_pose_blend_set_targets(WorldModelPoseBlend *b,WorldModelAsset *a,WorldModelAsset *other){
 assert(b&&a&&other);if(++ta==ft)return false;bool good=false;
 for(const auto &c:bandit_camp_clips)if(a->pose>=c.base&&a->pose<c.base+c.count){assert(other->pose==c.base+(a->pose-c.base+1)%c.count);const unsigned local=unsigned(ca-1)%(c.count*3);b->pass=local/c.count;good=true;}
 assert(good);b->a=a;b->b=other;return true;
}
extern "C" bool world_model_pose_blend_upload(WorldModelPoseBlend *b,const WorldModelInstance *p,const float *w,std::size_t n){
 assert(b&&b->a&&b->b&&n<=C&&b->pass==current_pass);for(std::size_t i=0;i<n;i++)assert(std::isfinite(p[i].x)&&p[i].y>187.9&&p[i].y<=188.006&&std::isfinite(p[i].z)&&std::isfinite(p[i].yaw)&&p[i].scale==1&&w[i]>=0&&w[i]<=1);b->count=n;uploads++;return true;
}
extern "C" void world_model_pose_blend_draw(WorldModelPoseBlend *b,unsigned program){assert(b&&b->count&&b->pass==current_pass&&program==(current_pass==2?22u:11u));assert(grass==0&&vegetation==0&&lod==-1&&wind==0&&height==1&&reactive==(current_pass==2?0.f:.85f));drawn+=b->count;}
extern "C" void world_model_pose_blend_free(WorldModelPoseBlend *b){if(!b)return;assert(batches[b->slot]==b);batches[b->slot]=nullptr;blive--;delete b;}
extern "C" float world_atlas_height(double x,double z){assert(std::isfinite(x)&&std::isfinite(z));return invalid?NAN:188.f;}
extern "C" bool world_atlas_local_water(double,double,float *,float *){return wet;}
bool world_scenery_can_move(const WorldScenery *,double x,double z,double tx,double tz,float y,float h,float r){assert(std::isfinite(x)&&std::isfinite(z)&&std::isfinite(tx)&&std::isfinite(tz)&&y==188.005f&&h==2&&r==.4f);return!blocked;}
extern "C" void APIENTRY glUseProgram(GLuint p){assert(p==11||p==22);}
extern "C" GLint APIENTRY glGetUniformLocation(GLuint,const GLchar *s){queries++;const char *n[]={"uOffset","uWorldAnchor","uGrass","uVegetation","uLodLevel","uWind","uTreeHeight","uActorReactive","uPropLodRange"};for(unsigned i=0;i<9;i++)if(!std::strcmp(n[i],s))return int(i+1);assert(false);return-1;}
extern "C" void APIENTRY glUniform3f(GLint l,GLfloat x,GLfloat y,GLfloat z){assert((l==1||l==9)&&std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z));}
extern "C" void APIENTRY glUniform2f(GLint l,GLfloat x,GLfloat z){assert(l==2&&x==float(std::floor(bandit_camp_x/128.)*128.)&&z==float(std::floor(bandit_camp_z/128.)*128.));}
extern "C" void APIENTRY glUniform1i(GLint l,GLint v){if(l==3)grass=v;else if(l==4)vegetation=v;else{assert(l==5);lod=v;}}
extern "C" void APIENTRY glUniform1f(GLint l,GLfloat v){if(l==6)wind=v;else if(l==7)height=v;else{assert(l==8);reactive=v;}}
namespace {
using Actors=std::array<WorldBanditCampActor,C>;
void empty(){assert(!live&&!blive);for(auto *a:assets)assert(!a);for(auto *b:batches)assert(!b);}
void reset(){la=ca=ta=fl=fc=ft=0;}
Actors snapshot(const WorldBanditCamp *w){Actors r={};assert(world_bandit_camp_stats(w).actors==C);for(unsigned i=0;i<C;i++)assert(world_bandit_camp_actor(w,i,&r[i]));return r;}
void same(const Actors &a,const Actors &b){for(unsigned i=0;i<C;i++)assert(a[i].x==b[i].x&&a[i].z==b[i].z&&a[i].y==b[i].y&&a[i].yaw==b[i].yaw&&a[i].pose_a==b[i].pose_a&&a[i].pose_b==b[i].pose_b&&a[i].pose_blend==b[i].pose_blend);}
std::size_t draw(WorldBanditCamp *w,unsigned pass,double z,float yaw=0){current_pass=pass;drawn=0;world_bandit_camp_draw(w,pass==2?22:11,bandit_camp_x,189.75,z,yaw,-.05,1.6,pass);const auto s=world_bandit_camp_stats(w);assert(drawn==(pass==0?s.visible:pass==1?s.reflection_visible:s.shadow_visible)&&reactive==0);return drawn;}
}
int main(){
 assert(!world_bandit_camp_create(nullptr)&&!world_bandit_camp_create(""));auto *w=world_bandit_camp_create("models");assert(w);empty();assert(!la&&!ca);
 assert(world_bandit_camp_desired(w,bandit_camp_x,bandit_camp_z)==0&&world_bandit_camp_desired(w,NAN,0)==-2);
 char error[256];std::size_t failures=0;
 for(unsigned kind=0;kind<3;kind++)for(unsigned i=1;i<=(kind?N*3:N);i++){reset();if(kind==0)fl=i;else if(kind==1)fc=i;else ft=i;assert(!world_bandit_camp_commit(w,true,error,sizeof(error))&&error[0]);empty();failures++;}
 reset();assert(world_bandit_camp_commit(w,true,error,sizeof(error))&&!error[0]);assert(live==40&&blive==120&&la==40&&ca==120&&ta==120);
 assert(world_bandit_camp_desired(w,bandit_camp_x+190,bandit_camp_z)==0&&world_bandit_camp_desired(w,bandit_camp_x+211,bandit_camp_z)==-1);
 world_bandit_camp_update(w,nullptr,17.125f);auto stable=snapshot(w);auto s=world_bandit_camp_stats(w);assert(s.walking==5&&s.running==1&&s.sentries==4&&!s.unsafe_roots);
 for(unsigned rate:{45u,60u}){for(unsigned i=0;i<rate*4;i++)world_bandit_camp_update(w,nullptr,float(i)/rate);world_bandit_camp_update(w,nullptr,17.125f);same(stable,snapshot(w));}
 for(float t:{100.f,.1f,55.4f,17.125f})world_bandit_camp_update(w,nullptr,t);
 same(stable,snapshot(w));
 for(unsigned pass=0;pass<3;pass++)draw(w,pass,bandit_camp_z+50);
 const auto warmed_queries=queries,old_uploads=uploads;
 for(unsigned pass=0;pass<3;pass++)draw(w,pass,bandit_camp_z+50);
 assert(uploads==old_uploads&&queries==warmed_queries);
 track=true;allocations=0;auto previous=stable;
 for(unsigned frame=1;frame<=36000;frame++){
  world_bandit_camp_update(w,nullptr,float(frame)/60);const auto current=snapshot(w);
  for(unsigned i=0;i<C;i++){
   const auto &a=current[i];const auto &c=bandit_camp_clips[a.mode];assert(a.pose_a>=c.base&&a.pose_a<c.base+c.count&&a.pose_b==c.base+(a.pose_a-c.base+1)%c.count&&a.pose_blend>=0&&a.pose_blend<=1);
   assert(std::abs(a.y+a.ground_correction-188.005f)<.00002&&a.ground_correction>=0&&a.ground_correction<.06f);
   if(frame>1&&a.patrolling){assert(std::hypot(a.x-previous[i].x,a.z-previous[i].z)<.10);assert(std::abs(std::remainder(double(a.yaw-previous[i].yaw),6.283185307179586))<.15);}
   for(unsigned j=0;j<i;j++){
    if(std::hypot(a.x-current[j].x,a.z-current[j].z)<=.8)std::fprintf(stderr,"pair %u/%u time%.4f local(%.3f,%.3f)/(%.3f,%.3f)\n",i,j,float(frame)/60,a.x-bandit_camp_x,a.z-bandit_camp_z,current[j].x-bandit_camp_x,current[j].z-bandit_camp_z);
    assert(std::hypot(a.x-current[j].x,a.z-current[j].z)>.8);
   }
  }
  if(frame%600==0)for(unsigned pass=0;pass<3;pass++)draw(w,pass,bandit_camp_z+50);
  previous=current;
 }
 track=false;assert(!allocations&&queries==warmed_queries&&la==40&&ca==120&&ta==120&&!world_bandit_camp_stats(w).unsafe_roots);
 world_bandit_camp_update(w,nullptr,17.125f);same(stable,snapshot(w));
 const auto updates=world_bandit_camp_stats(w).updates;for(float bad:{-1.f,NAN,INFINITY,10000001.f})world_bandit_camp_update(w,nullptr,bad);assert(world_bandit_camp_stats(w).updates==updates);same(stable,snapshot(w));
 const auto forward=draw(w,0,bandit_camp_z+50),behind=draw(w,0,bandit_camp_z+50,3.14159265f),shadow=draw(w,2,bandit_camp_z+50,3.14159265f);assert(forward>behind&&shadow>behind);
 for(unsigned pass=0;pass<3;pass++)assert(!draw(w,pass,bandit_camp_z+500));
 wet=true;world_bandit_camp_update(w,nullptr,0);assert(!world_bandit_camp_stats(w).actors);wet=false;
 invalid=true;world_bandit_camp_update(w,nullptr,0);assert(!world_bandit_camp_stats(w).actors);invalid=false;
 blocked=true;world_bandit_camp_update(w,nullptr,0);assert(!world_bandit_camp_stats(w).actors);blocked=false;
 world_bandit_camp_update(w,nullptr,17.125f);same(stable,snapshot(w));
 assert(world_bandit_camp_commit(w,false));empty();assert(!world_bandit_camp_stats(w).actors&&!world_bandit_camp_stats(w).pose_assets&&world_bandit_camp_stats(w).loads==1&&world_bandit_camp_stats(w).unloads==1);
 const auto unloaded_updates=world_bandit_camp_stats(w).updates;world_bandit_camp_update(w,nullptr,1);assert(world_bandit_camp_stats(w).updates==unloaded_updates+1);
 world_bandit_camp_free(w);world_bandit_camp_free(nullptr);empty();
 std::printf("PASS bandit camp CPU: 10 actors/40 poses/3 independent passes, %zu atomic failure probes;600 seconds smooth distance-driven paths and bounded turning; pair separation; blend-foot correction;45/60/mixed timing;zero hot IO/allocations;terrain/water/scenery rejection;hysteresis and complete unload\n",failures);
}
