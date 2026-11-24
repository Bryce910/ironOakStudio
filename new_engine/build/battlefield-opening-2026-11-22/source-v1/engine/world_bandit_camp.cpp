#include "engine/world_program_epoch.h"
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include "engine/world_bandit_camp.h"
#include "engine/world_models.h"
#include "engine/world_atlas.h"
#include "engine/world_scenery.h"
#include "engine/world_castle_visibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>
namespace {
constexpr unsigned poses=bandit_camp_pose_count,passes=3;
static_assert(std::size(bandit_camp_actors)==world_bandit_camp_capacity);
struct Batch {
 WorldModelPoseBlend *gpu=nullptr;
 std::array<WorldModelInstance,world_bandit_camp_capacity> next={},old={};
 std::array<float,world_bandit_camp_capacity> weights={},old_weights={};std::size_t count=0,old_count=0;
};
struct Uniforms {unsigned long long epoch=0;unsigned program=0;int offset=-1,anchor=-1,grass=-1,vegetation=-1,lod=-1,wind=-1,height=-1,reactive=-1,prop=-1;};
void path_at(unsigned path,double distance,double &x,double &z,float &yaw){
 const auto &p=bandit_camp_paths[path];distance=std::fmod(distance,p.length);if(distance<0)distance+=p.length;
 auto point=[&](double d,double &px,double &pz){
  d=std::fmod(d,p.length);if(d<0)d+=p.length;
  unsigned lo=0,hi=p.count-1;
  while(hi-lo>1){const unsigned m=(lo+hi)/2;if(bandit_camp_path_points[p.first+m].distance<=d)lo=m;else hi=m;}
  const auto &a=bandit_camp_path_points[p.first+lo],&b=bandit_camp_path_points[p.first+hi];const double w=(d-a.distance)/(b.distance-a.distance);
  px=a.x+(b.x-a.x)*w;pz=a.z+(b.z-a.z)*w;
 };
 point(distance,x,z);double ax,az,bx,bz;point(distance-.20,ax,az);point(distance+.20,bx,bz);yaw=float(std::atan2(bx-ax,bz-az));
}
float correction(unsigned mode,float phase){const double f=double(phase)*512.;const unsigned i=unsigned(f)%512;const float w=float(f-std::floor(f));return bandit_camp_ground_corrections[mode][i]*(1-w)+bandit_camp_ground_corrections[mode][(i+1)%512]*w;}
bool supported(const WorldScenery *scene,WorldBanditCampActor &a,double from_x,double from_z){
 const auto &c=bandit_camp_clips[a.mode];const double co=std::cos(a.yaw),si=std::sin(a.yaw);float low=1e9f,high=-1e9f;
 for(int iz=-1;iz<=1;iz++)for(int ix=-1;ix<=1;ix++){
  const double lx=ix*c.foot_x,lz=iz*c.foot_z,x=a.x+co*lx+si*lz,z=a.z-si*lx+co*lz;const float h=world_atlas_height(x,z);
  if(!std::isfinite(h)||h<.3f||world_atlas_local_water(x,z,nullptr,nullptr))return false;
  low=std::min(low,h);high=std::max(high,h);
 }
 if(high-low>.05f)return false;
 a.ground_correction=correction(a.mode,a.cycle_phase);a.y=high+.005f-a.ground_correction;
 return world_scenery_can_move(scene,from_x,from_z,a.x,a.z,high+.005f,2.f,.4f);
}
}
struct WorldBanditCamp {
 char directory[768]={};std::array<WorldModelAsset *,poses> assets={};std::array<Batch,poses*passes> batches={};
 std::array<WorldBanditCampActor,world_bandit_camp_capacity> actors={};std::array<Uniforms,2> uniforms={};unsigned next_uniform=0;
 double ax=std::floor(bandit_camp_x/128.)*128.,az=std::floor(bandit_camp_z/128.)*128.;WorldBanditCampStats stats;
 Uniforms &locations(unsigned program){
  for(auto &u:uniforms)if(u.program==program&&u.epoch==world_program_epoch)return u;
  auto &u=uniforms[next_uniform++%uniforms.size()];u.program=program;u.epoch=world_program_epoch;
  u.offset=glGetUniformLocation(program,"uOffset");u.anchor=glGetUniformLocation(program,"uWorldAnchor");u.grass=glGetUniformLocation(program,"uGrass");u.vegetation=glGetUniformLocation(program,"uVegetation");u.lod=glGetUniformLocation(program,"uLodLevel");u.wind=glGetUniformLocation(program,"uWind");u.height=glGetUniformLocation(program,"uTreeHeight");u.reactive=glGetUniformLocation(program,"uActorReactive");u.prop=glGetUniformLocation(program,"uPropLodRange");return u;
 }
 void unload(){
  for(auto &b:batches){if(b.gpu)world_model_pose_blend_free(b.gpu);b.gpu=nullptr;b.count=b.old_count=0;}
  for(auto &a:assets){if(a)world_model_free(a);a=nullptr;}
  if(stats.active)stats.unloads++;
  stats.active=false;stats.actors=stats.walking=stats.running=stats.sentries=stats.visible=stats.reflection_visible=stats.shadow_visible=stats.pose_assets=stats.draw_groups=0;
 }
};
WorldBanditCamp *world_bandit_camp_create(const char *directory){if(!directory||!*directory||std::strlen(directory)>=sizeof(WorldBanditCamp::directory))return nullptr;auto *w=new(std::nothrow)WorldBanditCamp;if(w)std::strcpy(w->directory,directory);return w;}
int world_bandit_camp_desired(const WorldBanditCamp *w,double x,double z){if(!w||!std::isfinite(x)||!std::isfinite(z))return-2;return std::hypot(x-bandit_camp_x,z-bandit_camp_z)<=(w->stats.active?bandit_camp_exit:bandit_camp_enter)?0:-1;}
bool world_bandit_camp_commit(WorldBanditCamp *w,bool active,char *error,std::size_t cap){
 if(error&&cap)error[0]=0;
 if(!w)return false;
 if(w->stats.active==active)return true;
 w->unload();if(!active)return true;
 auto fail=[&](const char *msg){if(error&&cap)std::snprintf(error,cap,"Bandit camp: %s",msg);w->unload();w->stats.load_failures++;return false;};
 const char *names[]={"walk","run","stance"};
 for(unsigned mode=0;mode<3;mode++){
  const auto &clip=bandit_camp_clips[mode];
  for(unsigned i=0;i<clip.count;i++){
   char path[1024];const int n=std::snprintf(path,sizeof(path),"%s/tanthoon_bandit_%s_%02u.glb",w->directory,names[mode],i);
   if(n<0||std::size_t(n)>=sizeof(path))return fail("path too long");
   w->assets[clip.base+i]=world_model_load_pose(path,WORLD_MODEL_POSE_BLEND_VERSION);if(!w->assets[clip.base+i])return fail(path);
  }
  for(unsigned pass=0;pass<passes;pass++)for(unsigned i=0;i<clip.count;i++){
   auto &b=w->batches[pass*poses+clip.base+i];b.gpu=world_model_pose_blend_create(world_bandit_camp_capacity,WORLD_MODEL_POSE_BLEND_VERSION);
   if(!b.gpu||!world_model_pose_blend_set_targets(b.gpu,w->assets[clip.base+i],w->assets[clip.base+(i+1)%clip.count]))return fail("incompatible source pose pair");
  }
 }
 w->stats.active=true;w->stats.loads++;w->stats.pose_assets=poses;w->stats.peak_pose_assets=std::max(w->stats.peak_pose_assets,std::size_t(poses));return true;
}
void world_bandit_camp_update(WorldBanditCamp *w,const WorldScenery *scene,float time){
 if(!w||!std::isfinite(time)||time<0||time>1e7f)return;
 w->stats.updates++;w->stats.actors=w->stats.walking=w->stats.running=w->stats.sentries=0;if(!w->stats.active)return;
 for(const auto &d:bandit_camp_actors){
  WorldBanditCampActor a;a.mode=unsigned(d.mode);a.x=d.x;a.z=d.z;a.yaw=d.yaw;a.patrolling=d.path>=0;
  const auto &clip=bandit_camp_clips[a.mode];double phase=double(time)/clip.seconds+d.phase,from_x=a.x,from_z=a.z;
  if(a.patrolling){
   a.speed=clip.stride/clip.seconds;const double distance=double(time)*a.speed+d.phase*bandit_camp_paths[d.path].length;
   path_at(unsigned(d.path),distance,a.x,a.z,a.yaw);float ignored;path_at(unsigned(d.path),distance-.20,from_x,from_z,ignored);
   phase=distance/clip.stride;
  }
  phase-=std::floor(phase);a.cycle_phase=float(phase);double f=phase*clip.count;const unsigned i=unsigned(f);a.pose_a=clip.base+i;a.pose_b=clip.base+(i+1)%clip.count;a.pose_blend=float(f-i);
  if(!supported(scene,a,from_x,from_z)){
   w->stats.unsafe_roots++;
   // Remain on the authored route's safe start, never at the campfire origin.
   if(d.path>=0)path_at(unsigned(d.path),d.phase*bandit_camp_paths[d.path].length,a.x,a.z,a.yaw);
   a.mode=2;a.patrolling=false;a.speed=0;const auto &idle=bandit_camp_clips[2];phase=double(time)/idle.seconds+d.phase;phase-=std::floor(phase);a.cycle_phase=float(phase);f=phase*idle.count;const unsigned j=unsigned(f);a.pose_a=idle.base+j;a.pose_b=idle.base+(j+1)%idle.count;a.pose_blend=float(f-j);
   if(!supported(scene,a,a.x,a.z))continue;
  }
  w->actors[w->stats.actors++]=a;if(a.patrolling){if(a.mode==1)w->stats.running++;else w->stats.walking++;}else w->stats.sentries++;
 }
}
void world_bandit_camp_draw(WorldBanditCamp *w,unsigned program,double x,double y,double z,float yaw,float pitch,float aspect,unsigned pass){
 if(!w||!program||pass>=passes||!w->stats.active||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||!std::isfinite(yaw)||!std::isfinite(pitch)||!std::isfinite(aspect)||aspect<=0)return;
 const auto view=world_vegetation_visibility_view(x,y,z,yaw,pitch,aspect);const unsigned start=pass*poses;for(unsigned i=start;i<start+poses;i++)w->batches[i].count=0;std::size_t visible=0,groups=0;
 for(std::size_t i=0;i<w->stats.actors;i++){
  const auto &a=w->actors[i];const auto &c=bandit_camp_clips[a.mode];if(std::hypot(a.x-x,a.z-z)>bandit_camp_draw_radius)continue;
  if(pass!=2&&world_vegetation_sphere_outside(view,a.x,a.y+c.height*.5,a.z,c.bound,bandit_camp_draw_radius))continue;
  auto &b=w->batches[start+a.pose_a];b.next[b.count]={float(a.x-w->ax),a.y,float(a.z-w->az),a.yaw,1};b.weights[b.count++]=a.pose_blend;visible++;
 }
 auto &u=w->locations(program);glUseProgram(program);glUniform3f(u.offset,float(w->ax-x),float(-y),float(w->az-z));glUniform2f(u.anchor,float(w->ax),float(w->az));glUniform1i(u.grass,0);glUniform1i(u.vegetation,0);glUniform1i(u.lod,-1);glUniform1f(u.wind,0);glUniform1f(u.height,1);glUniform1f(u.reactive,pass==2?0.f:.85f);glUniform3f(u.prop,bandit_camp_draw_radius-8,8,1);
 for(unsigned i=start;i<start+poses;i++){
  auto &b=w->batches[i];if(!b.gpu)continue;
  if(b.count!=b.old_count||std::memcmp(b.next.data(),b.old.data(),b.count*sizeof(WorldModelInstance))||std::memcmp(b.weights.data(),b.old_weights.data(),b.count*sizeof(float))){
   if(world_model_pose_blend_upload(b.gpu,b.next.data(),b.weights.data(),b.count)){std::copy_n(b.next.begin(),b.count,b.old.begin());std::copy_n(b.weights.begin(),b.count,b.old_weights.begin());b.old_count=b.count;w->stats.uploads++;}
  }
  if(b.old_count){world_model_pose_blend_draw(b.gpu,program);groups++;}
 }
 glUniform1f(u.reactive,0);glUniform3f(u.prop,0,0,0);
 if(pass==0){w->stats.visible=visible;w->stats.draw_groups=groups;w->stats.peak_visible=std::max(w->stats.peak_visible,visible);}else if(pass==1)w->stats.reflection_visible=visible;else w->stats.shadow_visible=visible;
}
WorldBanditCampStats world_bandit_camp_stats(const WorldBanditCamp *w){return w?w->stats:WorldBanditCampStats{};}
bool world_bandit_camp_actor(const WorldBanditCamp *w,std::size_t i,WorldBanditCampActor *a){if(!w||!a||i>=w->stats.actors)return false;*a=w->actors[i];return true;}
void world_bandit_camp_free(WorldBanditCamp *w){if(!w)return;w->unload();delete w;}
