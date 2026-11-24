#include "engine/world_combat_closeup_camera.h"
#include "engine/world_combat_shoulder_camera.h"
#include "engine/world_follow_camera.h"
#include <cstdio>
#include <array>
#include <cstdlib>
#include <limits>

static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#v);std::exit(1);}}while(0)
static bool close(double a,double b,double tolerance=1e-5){return std::abs(a-b)<=tolerance;}
struct Point {double x,y,z;};
struct View {double x,y,z,yaw,pitch;};
struct Screen {double x,y,depth;};
static Screen project(const View &c,Point p,double fov,double aspect){
 const double x=p.x-c.x,y=p.y-c.y,z=p.z-c.z,sy=std::sin(c.yaw),cy=std::cos(c.yaw),sp=std::sin(c.pitch),cp=std::cos(c.pitch);
 const double depth=x*sy*cp+y*sp-z*cy*cp;
 return {(x*cy+z*sy)/(depth*std::tan(fov*.5)*aspect),(-x*sy*sp+y*cp+z*cy*sp)/(depth*std::tan(fov*.5)),depth};
}
static View frame(float yaw,float pitch,double distance,WorldCombatCloseupFrame profile){
 auto floor=[](double,double){return 0.;};auto clear=[](double,double,double,double,float,float,float){return true;};
 const auto boom=world_follow_camera(0,1.75,0,yaw,pitch,4.2,1.f/60.f,true,floor,clear,profile.boom,profile.lift);
 const auto shoulder=world_combat_shoulder_camera(boom.x,boom.y,boom.z,yaw,0,boom.distance,1.f/60.f,true,true,floor,clear,profile.shoulder);
 // The production free-melee look target stays on the player's own aim line.
 const Point target={std::sin(yaw)*std::cos(pitch)*2,1.75+std::sin(pitch)*2-profile.target_drop,-std::cos(yaw)*std::cos(pitch)*2};
 const double dx=target.x-shoulder.x,dz=target.z-shoulder.z;
 (void)distance;
 return {shoulder.x,boom.y,shoulder.z,std::atan2(dx,-dz),std::atan2(target.y-boom.y,std::hypot(dx,dz))};
}
int main(){
 // Bounded profile, exact far/default behavior, and no invalid frame state.
 for(float dt:{0.f,1.f/144.f,1.f/60.f,1.f/30.f,.1f,1.f})for(double distance:{0.,1.,1.3,1.6,2.,2.8,3.3,100.}){
  WorldCombatCloseupCamera camera;auto p=camera.update(distance,dt,true,true);
  CHECK(p.weight>=0&&p.weight<=1&&p.boom>=3.35f-1e-6&&p.boom<=4.2f&&p.lift>=0&&p.lift<=.2f&&p.shoulder>=1.04f&&p.shoulder<=2.30f);
  if(distance<=1.3)CHECK(close(p.boom,3.85)&&close(p.lift,.2)&&close(p.shoulder,2.30)&&close(p.target_drop,.48));
  if(distance>=3.3)CHECK(p.weight==0&&close(p.boom,3.35)&&p.lift==0&&close(p.shoulder,1.04));
  p=camera.update(1,dt,false,false);CHECK(p.weight==0&&close(p.boom,4.2)&&p.lift==0&&close(p.shoulder,1.04));
 }
 for(double distance:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
  WorldCombatCloseupCamera camera;auto p=camera.update(distance,.016,true,false);CHECK(p.weight==0&&p.lift==0&&std::isfinite(p.boom));
 }
 // The exponential is independent of render rate at shared timestamps, and
 // approaching/receding enemies cannot cause an instantaneous profile jump.
 for(float hz:{30.f,60.f,144.f}){
  WorldCombatCloseupCamera camera;camera.update(5,0,true,true);float last=0;
  for(unsigned i=0;i<unsigned(hz*.6f);++i){auto p=camera.update(1.1,1/hz,true,false);CHECK(p.weight>last&&p.weight-last<.18f);last=p.weight;}
  CHECK(last>.95f&&last<1);
  for(unsigned i=0;i<unsigned(hz*.6f);++i){auto p=camera.update(5,1/hz,true,false);CHECK(p.weight<last&&p.weight>=0);last=p.weight;}
  CHECK(last<.04f);
 }
 WorldCombatCloseupCamera one,split;one.update(5,0,true,true);split.update(5,0,true,true);
 const auto once=one.update(1,.1f,true,false);WorldCombatCloseupFrame many;
 for(unsigned i=0;i<10;++i)many=split.update(1,.01f,true,false);
 CHECK(close(once.weight,many.weight,1e-6));
 // Exploration enlarges the hero without changing the lens. Combat widens
 // continuously for one opponent, and more for a surrounding group.
 WorldCombatCloseupCamera exploration,duel,crowd;
 const auto explore=exploration.update(5,.016f,true,true);
 const auto combat=duel.update(1.2,.016f,true,true,1);
 const auto group=crowd.update(1.2,.016f,true,true,3);
 CHECK(explore.boom<combat.boom&&combat.boom<group.boom&&group.boom<=4.2f);
 CHECK(4.2f/explore.boom>1.20f&&4.2f/explore.boom<1.30f);
 auto floor=[](double,double){return 0.;};unsigned sweeps=0;
 auto open=[&](double,double,double,double,float,float,float){++sweeps;return true;};
 // Combat enter/leave and free-look/stow use the live profile's zero target,
 // including its height. No vertical snap is hidden by boom damping.
 for(float hz:{30.f,60.f,144.f}){
  WorldCombatCloseupCamera camera;float previous=4.2f;double last_y=0;
  for(unsigned frame=0;frame<unsigned(hz*3);++frame){
   const double distance=frame<unsigned(hz*.5f)||frame>=unsigned(hz*1.5f)?3.3:1.1;
   const auto p=camera.update(distance,1/hz,true,frame==0);
   const auto eye=world_follow_camera(0,1.75,0,0,-.06f,previous,1/hz,frame==0,floor,open,p.boom,p.lift);
   if(frame)CHECK(std::abs(eye.y-last_y)<=3./hz);
   CHECK(eye.distance>=0&&eye.distance<=4.2f&&std::isfinite(eye.y));previous=eye.distance;last_y=eye.y;
  }
  CHECK(close(camera.update(3.3,0,true,false).lift,0,.0002));
 }
 // Default callers retain the exact accepted placement and sweep count.
 sweeps=0;
 const auto baseline=world_follow_camera(0,1.75,0,0,0,4.2,.016,true,floor,open);
 CHECK(sweeps==28&&close(baseline.distance,4.2)&&close(baseline.y,2.2));
 const auto explicit_base=world_follow_camera(0,1.75,0,0,0,4.2,.016,true,floor,open,4.2,0);
 CHECK(baseline.x==explicit_base.x&&baseline.y==explicit_base.y&&baseline.z==explicit_base.z);
 // Whole pivot lift corridor and the raised boom both respect a low ceiling.
 unsigned lift_checks=0;
 auto ceiling=[&](double,double,double,double,float y,float h,float){++lift_checks;return y+h<=2.5f;};
 const auto under=world_follow_camera(0,1.75,0,0,0,4.2,.016,true,floor,ceiling,2.6,.5);
 CHECK(lift_checks>28&&under.y+.18<=2.5+1e-5&&under.distance<=2.6f&&under.distance>=0);
 // A thin wall contracts immediately; clearing it restores the boom gradually.
 auto wall=[](double,double az,double,double bz,float,float,float radius){return !(az<1-radius&&bz>=1-radius);};
 const auto hit=world_follow_camera(0,1.75,0,0,0,4.2,.016,false,floor,wall,2.6,.5);
 CHECK(hit.distance<.8f&&hit.distance>.5f);
 const auto extend=world_follow_camera(0,1.75,0,0,0,hit.distance,.016,false,floor,open,2.6,.5);
 CHECK(extend.distance>hit.distance&&extend.distance<2.6);
 auto side_wall=[](double ax,double,double bx,double,float,float,float radius){return !(ax<.7-radius&&bx>=.7-radius);};
 const auto side=world_combat_shoulder_camera(0,2.4,3,0,0,2.6,.016,true,true,floor,side_wall,1.35);
 CHECK(side.blocked&&side.offset<.5&&side.offset>=0);
 const auto clear_side=world_combat_shoulder_camera(0,2.4,3,0,0,2.6,.016,true,true,floor,open,1.35);
 CHECK(close(clear_side.offset,1.35));
 // Independently project hero/defender faces through actual swept camera
 // placement. Near melee must improve separation while retaining visible faces.
 double minimum_improvement=100,minimum_new_separation=100;
 for(float yaw:{-3.13f,-1.7f,0.f,1.7f,3.13f})for(double aspect:{4./3,16./9,21./9})for(double distance:{1.,1.3,1.8,2.4})for(double enemy_height:{1.8,2.1}){
  WorldCombatCloseupCamera camera;const auto profile=camera.update(distance,.016,true,true);const float pitch=-.06f;
  const auto before=frame(yaw,pitch,distance,{0,4.2f,0,1.04f,0}),after=frame(yaw,pitch,distance,profile);const double fov=world_combat_close_melee_fov(distance);
  const Point hero={0,1.85,0},enemy={std::sin(yaw)*distance,enemy_height,-std::cos(yaw)*distance};
  const auto hb=project(before,hero,fov,aspect),eb=project(before,enemy,fov,aspect),ha=project(after,hero,fov,aspect),ea=project(after,enemy,fov,aspect);
  const double old_separation=std::hypot((hb.x-eb.x)*aspect,hb.y-eb.y),new_separation=std::hypot((ha.x-ea.x)*aspect,ha.y-ea.y);
  CHECK(ea.depth>0&&std::abs(ea.x)<.9&&std::abs(ea.y)<.9&&ha.depth>0&&std::abs(ha.x)<.9&&std::abs(ha.y)<.9);
  CHECK(new_separation>old_separation*1.35);
  minimum_improvement=std::min(minimum_improvement,new_separation/old_separation);minimum_new_separation=std::min(minimum_new_separation,new_separation);
 }
 // Independent torso-box projection, against the prior shipped close
 // profile. Readability must improve without pushing either pair of boots
 // or faces offscreen. This is framing evidence, not a mesh-overlap claim.
 double worst_overlap_before=0,worst_overlap_after=0;
 for(float yaw:{-3.13f,-1.7f,0.f,1.7f,3.13f})for(double aspect:{4./3,16./9,21./9})for(double distance:{1.,1.3,1.8,2.4}){
  WorldCombatCloseupCamera camera;const auto profile=camera.update(distance,.016,true,true);
  const float w=profile.weight;
  const auto before=frame(yaw,-.06f,distance,{w,4.2f-1.f*w,.2f*w,1.04f+1.26f*w,.48f*w}),after=frame(yaw,-.06f,distance,profile);
  const auto bounds=[&](const View&view,double z){std::array<double,4>b={100,-100,100,-100};
   for(double x:{-.32,.32})for(double y:{0.,1.9}){const Point point={std::cos(yaw)*x+std::sin(yaw)*z,y,std::sin(yaw)*x-std::cos(yaw)*z};const auto p=project(view,point,world_combat_close_melee_fov(distance),aspect);CHECK(p.depth>0);b[0]=std::min(b[0],p.x);b[1]=std::max(b[1],p.x);b[2]=std::min(b[2],p.y);b[3]=std::max(b[3],p.y);}return b;};
  const auto hb=bounds(before,0),eb=bounds(before,distance),ha=bounds(after,0),ea=bounds(after,distance);
  const auto overlap=[](const auto&a,const auto&b){return std::max(0.,std::min(a[1],b[1])-std::max(a[0],b[0]))/std::min(a[1]-a[0],b[1]-b[0]);};
  const double old_overlap=overlap(hb,eb),new_overlap=overlap(ha,ea);
  CHECK(new_overlap<.4);
  CHECK(ha[0]>-.9&&ha[1]<.9&&ha[2]>-.9&&ha[3]<.9&&ea[0]>-.9&&ea[1]<.9&&ea[2]>-.9&&ea[3]<.9);
  if(distance<=1.3){CHECK(new_overlap<.4);worst_overlap_before=std::max(worst_overlap_before,old_overlap);worst_overlap_after=std::max(worst_overlap_after,new_overlap);}
 }
 std::printf("Close torso overlap before %.3f after %.3f; full-body screen margins passed\n",worst_overlap_before,worst_overlap_after);
 // Captions avoid the arena's fixed top-left/top-right HUD blocks and stay
 // above the central action at 1600/900, HD, 720p and narrow 800/600.
 for(const auto &size:{std::pair<float,float>{800,600},{1280,720},{1600,900},{1920,1080},{2560,1440}}){
  const float w=size.first,h=size.second,u=std::clamp(std::min(w/1600,h/900),.65f,1.4f),plate=260*u;
  for(unsigned row=0;row<2;++row){const auto notice=world_combat_notice_layout(w,h,u,plate,row);const float left=notice.center_x-plate/2,right=notice.center_x+plate/2,top=notice.caption_y-5*u,bottom=notice.caption_y+42*u;
   CHECK(left>=0&&right<=w&&top>=0&&bottom<h*.42f);
   CHECK(top>=163*u||left>=590*u||right<=22*u);
   CHECK(top>=303*u||right<=w-300*u||left>=w-22*u);
   if(row){const auto previous=world_combat_notice_layout(w,h,u,plate,0);CHECK(top>previous.caption_y+42*u);}
  }
 }
 std::printf("PASS close-melee camera: %u checks; minimum projected face-separation improvement %.3fx; minimum separation %.5f; sweep/ceiling/side collision and HUD layout passed\n",checks,minimum_improvement,minimum_new_separation);
}
