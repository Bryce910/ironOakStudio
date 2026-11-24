#ifndef VALWOOD_WORLD_BATTLEFIELD_SCENE_H
#define VALWOOD_WORLD_BATTLEFIELD_SCENE_H
#include "engine/world_battlefield_actors.h"
#include "engine/world_arena_render.h"
#include "engine/world_atlas.h"
#include "engine/world_torch_flames.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Deterministic, fixed-budget battlefield direction. The player encounter owns
// gameplay/time/checkpoints; these soldiers remain occupied with one another.
// No random per-frame decisions, dynamic allocations, file/asset/GL work here.
namespace world_battlefield_scene {
constexpr double origin_x=-12085.,origin_z=5150.;
constexpr unsigned actor_capacity=28,arrow_capacity=24;
using Actor=WorldBattlefieldActors::Background;
using Motion=WorldBattlefieldActors::Motion;
struct Point {double x=0,y=0,z=0;};
struct Impact {Point position,direction;std::uint64_t serial=0;bool blood=false;float strength=1;};
struct Frame {
 std::array<Actor,actor_capacity> actors={};unsigned actor_count=0;
 std::array<WorldArenaRenderArrow,arrow_capacity> arrows={};unsigned arrow_count=0;
 std::array<Impact,8> impacts={};unsigned impact_count=0;
 bool visible=false;double distance=0;
};
inline double fraction(double x){return x-std::floor(x);}
inline double ease(double x){x=std::clamp(x,0.,1.);return x*x*(3.-2.*x);}
inline double yaw_toward(double x,double z,double tx,double tz){return std::atan2(tx-x,tz-z);}
inline Point operator+(Point a,Point b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Point operator-(Point a,Point b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Point operator*(Point a,double t){return {a.x*t,a.y*t,a.z*t};}
inline Point unit(Point a){const double n=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);return n>1e-9?a*(1./n):Point{0,0,1};}
class Director {
 struct Site{double x,z,yaw;};
 static constexpr std::array<Site,12> casualties={{
  {-12118,5142,-.6},{-12107,5158,1.7},{-12103,5134,-2.3},{-12094,5167,.4},
  {-12087,5138,2.1},{-12077,5163,-.9},{-12069,5131,.7},{-12058,5167,-2.7},
  {-12049,5138,.3},{-12036,5158,-1.2},{-12128,5155,2.8},{-12083,5154,1.0}}};
 std::array<double,12> casualty_y_={};
 std::array<double,8> fixed_y_={};
 bool prepared_=false;
 static Actor actor(double x,double y,double z,double yaw,Motion motion,double progress){Actor a;a.x=x;a.y=y;a.z=z;a.yaw=float(yaw);a.motion=motion;a.progress=progress;return a;}
public:
 // CPU loading boundary, after the exact project terrain/studio height is bound.
 void prepare(){
  for(unsigned i=0;i<casualties.size();++i)casualty_y_[i]=world_atlas_walk_height(casualties[i].x,casualties[i].z);
  static constexpr double p[8][2]={{-12118,5138},{-12091,5162},{-12061,5139},{-12129,5133},{-12068,5132},{-12109,5147},{-12098,5167},{-12045,5140}};
  for(unsigned i=0;i<8;++i)fixed_y_[i]=world_atlas_walk_height(p[i][0],p[i][1]);prepared_=true;
 }
 Frame sample(double seconds,unsigned quality,double camera_x,double camera_z,bool active=true) const {
  Frame f;f.distance=std::hypot(camera_x-origin_x,camera_z-origin_z);
  if(!prepared_||!active||!std::isfinite(seconds)||seconds<0||!std::isfinite(camera_x)||!std::isfinite(camera_z)||f.distance>220)return f;
  f.visible=true;quality=std::min(quality,3u);
  auto add=[&](const Actor&a){if(f.actor_count<f.actors.size())f.actors[f.actor_count++]=a;};
  // Three paired exchanges: telegraph, strike, receiving guard/recoil, reset.
  // Distance stays constant: blades meet, rather than both soldiers attacking
  // the same air. Small foot shifts keep the exchange alive without skating.
  static constexpr double pair[3][3]={{-12103,5159,.65},{-12091,5162,-.25},{-12061,5139,1.1}};
  for(unsigned i=0;i<3;++i){
   const double cycle=fraction(seconds/4.6+i*.27),angle=pair[i][2],dx=std::sin(angle),dz=std::cos(angle);
   const double closing=.10*std::sin(cycle*6.28318530718),half=.82-closing;
   for(unsigned side=0;side<2;++side){
    const double s=side?1.:-1.;double x=pair[i][0]+dx*half*s,z=pair[i][1]+dz*half*s;
    Motion m=Motion::Guard;double p=fraction(cycle*1.6);
    const bool attack=side==0?cycle<.36:(cycle>.51&&cycle<.87);
    if(attack){m=(i+side)%2?Motion::AttackLeft:Motion::AttackRight;p=side==0?cycle/.36:(cycle-.51)/.36;}
    const double mortal=i==0?8.6:i==1?18.4:1e9;const unsigned victim=i==0?1:0;
    if(seconds>=mortal){if(side==victim){m=seconds<mortal+2.?Motion::Death:Motion::Dead;p=std::clamp((seconds-mortal)/2.,0.,1.);}else{m=Motion::Guard;p=fraction(seconds*.28);}}

    add(actor(x,world_atlas_walk_height(x,z),z,angle+(side?3.14159265359:0.),m,p));
   }
  }
  // Archers keep their attention across the field. Continuous arm IK draws and
  // releases the skinned bow/string once per cycle, matching the arrow release.
  static constexpr double archer[2][4]={{-12129,5133,-12075,5163},{-12068,5132,-12104,5161}};
  for(unsigned i=0;i<2;++i){const double *p=archer[i];add(actor(p[0],fixed_y_[3+i],p[1],yaw_toward(p[0],p[1],p[2],p[3]),Motion::Archer,fraction(seconds/4.2+i*.5)));}
  // Motionless casualties are grounded static props sharing two real mocap
  // bakes. Keep CPU skin/floor/palette work for live action and witnessed falls.
  const unsigned dead_count=0;
  for(unsigned i=0;i<dead_count;++i){const auto&s=casualties[i];add(actor(s.x,casualty_y_[i],s.z,s.yaw,Motion::Dead,1));}
  // The first witnessed arrow hit always occurs during the arrival camera.
  // This victim dies once and remains dead; a loop never resurrects him.
  const double hit=2.40,fall=2.0;
  add(actor(-12109,fixed_y_[5],5147,-1.15,seconds<hit?Motion::Guard:seconds<hit+fall?Motion::Death:Motion::Dead,std::clamp((seconds-hit)/fall,0.,1.)));
  // Two soldiers run across the route in opposite directions, outside the
  // personal QTE lane. No collider or hostile AI steals the encounter focus.
  for(unsigned i=0;i<2;++i){
   const double angle=seconds/(i?10.7:12.9)*6.28318530718+i*3.1,cs=std::cos(angle),sn=std::sin(angle),x=(i?-12051.:-12109.)+cs*8.5,z=(i?5139.:5164.)+sn*5.;
   add(actor(x,world_atlas_walk_height(x,z),z,yaw_toward(x,z,x-sn*8.5,z+cs*5.),Motion::Run,fraction(seconds*1.4+i*.3)));
  }
  auto arrow=[&](Point a,Point b,double start,double duration,double arc,std::uint64_t serial,bool blood,bool repeat,double period){
   double local=seconds-start;std::uint64_t loop=0;if(local<0)return;
   if(repeat){loop=std::uint64_t(std::floor(local/period));local=std::fmod(local,period);}
   if(local<duration&&f.arrow_count<f.arrows.size()){
    const double t=std::clamp(local/duration,0.,1.);Point tip=a+(b-a)*t;tip.y+=4*arc*t*(1-t);
    Point direction=(b-a)+Point{0,4*arc*(1-2*t),0};direction=unit(direction);Point base=tip-direction*.82;
    auto&r=f.arrows[f.arrow_count++];r.visible=true;r.base[0]=base.x;r.base[1]=base.y;r.base[2]=base.z;r.tip[0]=tip.x;r.tip[1]=tip.y;r.tip[2]=tip.z;
   }
   // Stable serials let the host consume audio/blood once, even after pause.
   // The short post-impact window also supports low FPS without one-frame loss.
   if(local>=duration&&local<duration+.30&&f.impact_count<f.impacts.size())f.impacts[f.impact_count++]={b,unit(b-a),serial+loop,blood,blood?1.2f:.5f};
  };
  // Two later arrows interrupt real exchanges and leave their victims dead.
  // Their shooters belong to the obscured field-edge rank, not the two visible
  // foreground archers; no impossible rapid-fire bow performance is implied.
  for(unsigned i=0;i<2;++i){const double mortal=i==0?8.6:18.4,cycle=fraction(mortal/4.6+i*.27),angle=pair[i][2],side=i==0?1.:-1.,half=.82-.10*std::sin(cycle*6.28318530718);
   const double x=pair[i][0]+std::sin(angle)*half*side,z=pair[i][1]+std::cos(angle)*half*side;
   arrow({-12149.+i*12.,14.0,5128.},{x,world_atlas_walk_height(x,z)+1.20,z},mortal-1.42,1.42,3.8,2+i,true,false,0);
  }
  arrow({-12149,14.0,5128},{-12109,fixed_y_[5]+1.35,5147},1.18,1.22,2.8,1,true,false,0);
  const unsigned volleys=quality==0?4:quality==1?6:quality==2?9:12;
  for(unsigned i=0;i<volleys;++i){
   const unsigned side=i&1;const auto *a=archer[side];const double offset=(double(i/2)-2.)*2.4;
   const double tx=a[2]+offset,tz=a[3]+std::sin(i*2.1)*3.2,ty=world_atlas_walk_height(tx,tz);
   const Point source=i<2?Point{a[0]+.16,fixed_y_[3+side]+1.52,a[1]}:Point{-12166.+double(i-2)*4.8,14.8,5127.+std::sin(i*.9)*3.};
   arrow(source,{tx,ty+.10,tz},.42+side*2.1+(i<2?0.:(i/2)*.11),1.42+double(i%3)*.12,5.+double(i%3),100+std::uint64_t(i)*1000000u,false,true,4.2);
  }
  return f;
 }
 // Host can append these three fire effects to its existing camp/Studio set.
 std::array<WorldTorchFlameEmitter,3> flames()const{
  std::array<WorldTorchFlameEmitter,3> a={};const double p[3][2]={{-12124,5156},{-12101,5132},{-12079,5170}};
  for(unsigned i=0;i<3;++i){a[i].x=p[i][0];a[i].z=p[i][1];a[i].y=world_atlas_walk_height(a[i].x,a[i].z)+.35;a[i].height=.9f;a[i].width=.46f;a[i].intensity=1.2f;a[i].seed=11222026+i;}return a;
 }
};
// Existing production arrow geometry/lighting, independently owned. Only arrows
// draw: the helper's arena pad/rails are never submitted. No new shader path.
class ArrowRenderer {
 WorldArenaRender *renderer_=nullptr;
public:
 bool init(char *error=nullptr,std::size_t capacity=0){if(renderer_)return true;WorldArenaRenderConfig c;c.x=origin_x;c.z=origin_z;c.ground_y=11.3;c.half_x=c.half_z=5;renderer_=world_arena_render_create(c,error,capacity);return renderer_!=nullptr;}
 bool update(const Frame&f){return renderer_&&world_arena_render_update(renderer_,{},f.arrows.data(),f.arrow_count);}
 void draw(unsigned p,double x,double y,double z,bool shadow=false){world_arena_render_draw_arrows(renderer_,p,x,y,z,shadow);}
 void release(){if(renderer_)world_arena_render_free(renderer_);renderer_=nullptr;}
 // Explicit release before GL context destruction; no destructor GL calls.
 WorldArenaRenderStats stats()const{return world_arena_render_stats(renderer_);}
};
}
#endif
