// Independent geometric regression of the production camera safety helper.
// This fixture uses conservative swept-sphere/AABB bounds, not copied helper logic.
#include "engine/world_follow_camera_safety.h"
#include "engine/world_follow_camera.h"
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using P=WorldCameraPoint;
struct Box{P lo,hi;};
static unsigned checks=0,scenarios=0,max_queries=0,recovery_samples=0,blocked_release_frames=0,component_recovery_frames=0;
static double maximum_recovery_eye_step_m=0,maximum_recovery_bound_excess_m=0;
static void require(bool x,const char *message){++checks;if(!x){std::cerr<<message<<"\n";std::exit(1);}}
static bool finite(P p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
static double component(P p,unsigned i){return i==0?p.x:i==1?p.y:p.z;}
struct Geometry{
 std::vector<Box> boxes;unsigned queries=0;
 bool operator()(P a,P b,float radius){
  ++queries;if(!finite(a)||!finite(b)||!std::isfinite(radius)||radius<=0)return false;
  for(const auto&box:boxes){double lo=0,hi=1;bool intersects=true;
   for(unsigned k=0;k<3;++k){const double start=component(a,k),delta=component(b,k)-start,mn=component(box.lo,k)-radius,mx=component(box.hi,k)+radius;
    if(std::abs(delta)<1e-12){if(start<mn||start>mx){intersects=false;break;}}
    else{double t0=(mn-start)/delta,t1=(mx-start)/delta;if(t0>t1)std::swap(t0,t1);lo=std::max(lo,t0);hi=std::min(hi,t1);if(lo>hi){intersects=false;break;}}
   }
   if(intersects)return false;
  }return true;
 }
};
static WorldCameraSafetyFrame step(WorldFollowCameraSafety&s,Geometry&g,P pivot,P goal,float radius=.22f,float dt=1.f/60,bool cut=false,bool component_blocked=false){
 const auto before=g.queries;auto out=s.update(pivot,goal,radius,dt,cut,[&](P a,P b,float r){return g(a,b,r);},component_blocked);const auto count=g.queries-before;max_queries=std::max(max_queries,count);require(count<=90,"camera queries exceed fixed bounded budget");require(finite(out.eye),"finite input produced nonfinite camera eye");return out;
}
static bool same(P a,P b){return world_camera_distance(a,b)<1e-10;}
int main(){
 const P pivot{0,1.3,0};
 {++scenarios;Geometry g;WorldFollowCameraSafety s;P goal{.45,1.55,-2.25};auto a=step(s,g,pivot,goal);require(same(a.eye,goal),"open camera changed");require(a.view_clear&&!a.line_blocked&&!a.motion_blocked&&!a.recovering,"open camera reports obstruction");auto freely=step(s,g,pivot,{-2,1.8,-1});require(same(freely.eye,{-2,1.8,-1})&&!freely.recovering,"clear-space orbit was unnecessarily smoothed");a=step(s,g,pivot,goal);auto b=step(s,g,pivot,goal,.22f,0);require(same(a.eye,b.eye),"stationary paused camera moved");}
 {++scenarios;Geometry g{{{{.5,.3,-1.8},{.8,2.5,-1.2}}}};WorldFollowCameraSafety s;P boom{0,1.3,-3},shoulder{1.4,1.3,-3};require(g(pivot,boom,.22f),"fixture boom must be clear");require(g(boom,shoulder,.22f),"fixture shoulder must be clear");require(!g(pivot,shoulder,.22f),"fixture must reproduce composed line obstruction");auto a=step(s,g,pivot,shoulder);require(a.line_blocked&&a.view_clear,"composed jamb line not repaired");require(g(pivot,a.eye,.22f),"repaired final eye still sees through jamb");require(!same(a.eye,shoulder),"blocked goal retained");auto b=step(s,g,pivot,shoulder,.22f,0);require(same(a.eye,b.eye),"paused blocked line moved");}
 {++scenarios;Geometry g{{{{-.3,.3,-2.3},{.3,2.5,-1.7}}}};WorldFollowCameraSafety s;P left{-2,1.3,-2},right{2,1.3,-2};require(g(pivot,left,.22f)&&g(pivot,right,.22f),"both orbit rays must be clear");require(!g(left,right,.22f),"fixture must reproduce orbit crossing");auto a=step(s,g,pivot,left);auto b=step(s,g,pivot,right);require(b.motion_blocked,"orbit crossing not detected");require(b.view_clear&&g(pivot,b.eye,.22f),"orbit repair lost player line");require(g(a.eye,b.eye,.22f),"repaired orbit still crosses jamb");require(!same(b.eye,right),"blocked temporal goal retained");
  auto cut=step(s,g,pivot,right,.22f,1.f/60,true);require(same(cut.eye,right)&&!cut.motion_blocked&&cut.view_clear&&!cut.recovering,"valid camera cut reused stale temporal obstruction");s.reset();auto reset=step(s,g,pivot,left);require(same(reset.eye,left)&&!reset.motion_blocked&&!reset.recovering,"reset retained old orbit state");
  P far_pivot{30,1.3,0},far_eye{32,1.3,-2};auto tele=step(s,g,far_pivot,far_eye);require(same(tele.eye,far_eye)&&tele.view_clear&&!tele.motion_blocked&&!tele.recovering,"distant teleport reused old eye corridor");}
 {++scenarios;Geometry g{{{{-.2,.2,-3},{.2,2.6,1}}}};WorldFollowCameraSafety s;P oldp{-1.5,1.3,0},olde{-2,1.3,-1.5},newp{1.5,1.3,0},newe{2,1.3,-1.5};auto a=step(s,g,oldp,olde);auto paused=step(s,g,newp,newe,.22f,0);require(paused.motion_blocked&&!paused.view_clear,"occluded convex fallback fixture not exercised");require(same(paused.eye,a.eye),"zero-time fallback moved eye");auto b=step(s,g,newp,newe);require(b.motion_blocked&&!b.view_clear,"fallback must report temporary player occlusion honestly");require(g(a.eye,b.eye,.22f),"fallback retreat crossed wall");require(b.eye.x<-.42,"fallback teleported through corner");}
 {++scenarios;Geometry g{{{{-.3,.3,-2.3},{.3,2.5,-1.7}}}};WorldFollowCameraSafety s;P previous{};bool have=false;
  for(unsigned i=0;i<240;++i){const double yaw=-1.3+2.6*i/239.;P goal{2.8*std::sin(yaw),1.3,-2.8*std::cos(yaw)};auto a=step(s,g,pivot,goal);require(a.view_clear&&g(pivot,a.eye,.22f),"continuous orbit lost safe player sightline");if(have)require(g(previous,a.eye,.22f),"continuous orbit crossed obstacle between frames");previous=a.eye;have=true;}
 }
 {++scenarios;Geometry g{{{{.5,.3,-1.8},{.8,2.5,-1.2}}}};WorldFollowCameraSafety s;P goal{1.4,1.3,-3},old_eye{0,1.3,-3};auto previous=step(s,g,pivot,old_eye);require(g(pivot,previous.eye,.22f),"old eye must retain player sightline");
  // Independently bracket the unobstructed prefix endpoint only to measure the
  // old abrupt correction distance; safety and continuity use actual geometry.
  double lo=0,hi=1;for(unsigned i=0;i<32;++i){double t=(lo+hi)*.5;if(g(pivot,world_camera_mix(pivot,goal,t),.22f))lo=t;else hi=t;}
  require(world_camera_distance(previous.eye,world_camera_mix(pivot,goal,lo))>2.,"fixture does not reproduce a two-meter correction");
  P last=previous.eye;bool first=true;for(unsigned i=0;i<360;++i){const float dt=i%3==0?1.f/120:i%3==1?1.f/60:1.f/30;const auto a=step(s,g,pivot,goal,.22f,dt);require(a.view_clear&&g(pivot,a.eye,.22f),"smooth composed correction loses safe player line");require(g(last,a.eye,.22f),"smooth correction crosses obstacle");const double distance=world_camera_distance(last,a.eye),bound=8*double(dt);maximum_recovery_eye_step_m=std::max(maximum_recovery_eye_step_m,distance);maximum_recovery_bound_excess_m=std::max(maximum_recovery_bound_excess_m,distance-bound);require(distance<=bound+1e-9,"collision correction exceeds eight meters per second");if(first){require(a.line_blocked&&a.recovering,"two-meter correction was not marked recovering");require(distance<.14,"two-meter correction still jumps");first=false;}if(a.recovering)++recovery_samples;
   const auto paused=step(s,g,pivot,goal,.22f,0);if(!same(a.eye,paused.eye))std::cerr<<std::setprecision(17)<<"pause-delta="<<world_camera_distance(a.eye,paused.eye)<<" frame="<<i<<" before="<<a.eye.x<<","<<a.eye.y<<","<<a.eye.z<<" after="<<paused.eye.x<<","<<paused.eye.y<<","<<paused.eye.z<<"\n";require(same(a.eye,paused.eye),"paused collision recovery or near-goal snap moves eye");last=paused.eye;
  }
  const auto settled=step(s,g,pivot,goal);require(world_camera_distance(last,settled.eye)<.002,"smooth collision correction never settles");
  auto cut=step(s,g,pivot,old_eye,.22f,1.f/60,true);require(same(cut.eye,old_eye)&&!cut.recovering,"cut failed to clear recovery latch");
  step(s,g,pivot,goal);s.reset();auto reset=step(s,g,pivot,old_eye);require(same(reset.eye,old_eye)&&!reset.recovering,"reset failed to clear recovery latch");
 }
 {++scenarios;Geometry g{{{{-.3,.3,-2.3},{.3,2.5,-1.7}}}};WorldFollowCameraSafety s;P left{-2,1.3,-2},right{2,1.3,-2};auto previous=step(s,g,pivot,left);bool settled=false;P last_pivot=pivot;
  for(unsigned i=0;i<600;++i){P current_pivot{.0002*std::min(i,100u),1.3,0};const float dt=1.f/60;const auto a=step(s,g,current_pivot,right,.22f,dt);require(a.view_clear&&g(current_pivot,a.eye,.22f),"recovering temporal orbit loses player line");require(g(previous.eye,a.eye,.22f),"recovering temporal orbit crosses jamb");if(a.recovering){++recovery_samples;const double distance=world_camera_distance(previous.eye,a.eye),bound=8*double(dt)+world_camera_distance(current_pivot,last_pivot);maximum_recovery_eye_step_m=std::max(maximum_recovery_eye_step_m,distance);maximum_recovery_bound_excess_m=std::max(maximum_recovery_bound_excess_m,distance-bound);require(distance<=bound+1e-9,"orbit recovery ignores root-pivot motion bound");}
   previous=a;last_pivot=current_pivot;if(world_camera_distance(a.eye,right)<1e-9&&!a.recovering){settled=true;break;}
  }require(settled,"temporal collision recovery did not settle and release latch");
 }
 for(unsigned fps:{30u,60u,120u}){++scenarios;const float dt=1.f/fps;Geometry g{{{{.5,.3,-1.8},{.8,2.5,-1.2}}}};WorldFollowCameraSafety s;P goal{1.4,1.3,-3},old_eye{0,1.3,-3};auto previous=step(s,g,pivot,old_eye);
  auto bounded=[&](P desired,bool component){auto a=step(s,g,pivot,desired,.22f,dt,false,component);const double distance=world_camera_distance(previous.eye,a.eye),bound=8*double(dt);require(distance<=bound+1e-9,"blocked hold or first-clear release jumps beyond recovery bound");require(a.view_clear&&g(pivot,a.eye,.22f)&&g(previous.eye,a.eye,.22f),"blocked hold/release loses safe camera path");maximum_recovery_eye_step_m=std::max(maximum_recovery_eye_step_m,distance);maximum_recovery_bound_excess_m=std::max(maximum_recovery_bound_excess_m,distance-bound);auto paused=step(s,g,pivot,desired,.22f,0,false,component);require(same(a.eye,paused.eye),"paused wall release advances or snaps near goal");previous=paused;++blocked_release_frames;return a;};
  for(unsigned i=0;i<3*fps;++i){const auto a=bounded(goal,false);require(a.line_blocked&&a.recovering,"holding a clipped goal releases recovery latch");}
  g.boxes.clear();bool settled=false;for(unsigned i=0;i<10*fps;++i){const auto a=bounded(goal,false);if(i==0)require(a.recovering,"first clear frame after blocked hold pops out of recovery");if(same(a.eye,goal)&&!a.recovering){settled=true;break;}}require(settled,"wall release never settles and clears latch");
  P free_goal{-3,1.3,-.3};const auto free=step(s,g,pivot,free_goal,.22f,dt);require(same(free.eye,free_goal)&&!free.recovering,"completed collision recovery changes later ordinary clear orbit");
 }
 for(unsigned fps:{30u,60u,120u}){++scenarios;const float dt=1.f/fps;Geometry g{{{{-.4,0,1.5},{.4,3,1.7}}}};const P base{0,1.3,0},old_eye{-2,1.3,0};WorldFollowCameraSafety s;auto previous=step(s,g,base,old_eye);
  const auto follow=world_follow_camera(0,1.2,0,0,0,3.f,dt,false,[](double,double){return -10.;},[&](double ax,double az,double bx,double bz,float y,float h,float radius){return g({ax,y+h*.5,az},{bx,y+h*.5,bz},radius);},3.f);
  const P contracted{follow.x,follow.y,follow.z};require(follow.clear_distance<2.f&&follow.clear_distance==follow.distance,"real component boom contraction metadata missing");require(world_camera_distance(old_eye,contracted)>2.,"component fixture correction is too small");require(g(base,contracted,.22f)&&g(old_eye,contracted,.22f),"component fixture must avoid final line/motion obstruction flags");
  for(unsigned i=0;i<3*fps;++i){const auto a=step(s,g,base,contracted,.22f,dt,false,true);require(!a.line_blocked&&!a.motion_blocked&&a.recovering,"component-only contraction not retained in recovery");const double distance=world_camera_distance(previous.eye,a.eye),bound=8*double(dt);require(distance<=bound+1e-9,"component-only contraction jumps");require(g(previous.eye,a.eye,.22f)&&a.view_clear,"component recovery crosses geometry");maximum_recovery_eye_step_m=std::max(maximum_recovery_eye_step_m,distance);maximum_recovery_bound_excess_m=std::max(maximum_recovery_bound_excess_m,distance-bound);previous=a;++component_recovery_frames;}
  g.boxes.clear();const auto follow_open=world_follow_camera(0,1.2,0,0,0,3.f,dt,false,[](double,double){return -10.;},[](double,double,double,double,float,float,float){return true;},3.f);const P goal{follow_open.x,follow_open.y,follow_open.z};require(follow_open.clear_distance==3.f,"clear boom metadata not restored");bool settled=false;
  for(unsigned i=0;i<10*fps;++i){const auto a=step(s,g,base,goal,.22f,dt,false,false);const double distance=world_camera_distance(previous.eye,a.eye),bound=8*double(dt);require(distance<=bound+1e-9,"component first-clear release jumps");require(a.view_clear&&g(previous.eye,a.eye,.22f),"component release loses safe path");auto paused=step(s,g,base,goal,.22f,0);require(same(a.eye,paused.eye),"paused component release snaps near goal");if(i==0)require(a.recovering,"component latch lost before open goal settled");previous=paused;++component_recovery_frames;if(same(a.eye,goal)&&!a.recovering){settled=true;break;}}require(settled,"component-only release never settles");
 }
 {++scenarios;const float near=.12f,fov=1.0471975512f,ordinary=world_follow_camera_lens_radius(near,fov,16.f/9),ultra=world_follow_camera_lens_radius(near,1.24f,32.f/9);
  require(std::abs(ordinary-.22f)<1e-7,"ordinary 60degree16:9 lens changed22cm clearance");
  for(float aspect:{16.f/9,32.f/9,8.f})for(float lens:{fov,1.24f,2.f}){const auto r=world_follow_camera_lens_radius(near,lens,aspect);const double h=near*std::tan(lens*.5),corner=std::sqrt(double(near)*near+h*h*(1+double(aspect)*aspect));require(r>=corner+.0149,"lens sphere misses a near-plane corner");require(std::isfinite(r)&&r>=.22f,"lens radius nonfinite or too small");}
  require(ultra>ordinary,"ultrawide corner did not enlarge radius");float nan=std::numeric_limits<float>::quiet_NaN();require(std::isfinite(world_follow_camera_lens_radius(nan,nan,nan)),"invalid lens config not sanitized");
  Geometry g{{{{.25,.5,-.3},{.45,2.5,.3}}}};WorldFollowCameraSafety s;P goal{0,1.3,-1};require(g(pivot,goal,ordinary),"ordinary lens fixture unexpectedly obstructed");require(!g(pivot,goal,ultra),"ultrawide corner fixture not obstructed");auto a=step(s,g,pivot,goal,ultra);require(a.line_blocked,"ultrawide near corner not checked");
 }
 {++scenarios;Geometry g;WorldFollowCameraSafety s;for(P p:{P{-3500,601.3,-11000},P{100000,603,-20000},P{0,1.3,0}}){P goal{p.x+.35,p.y+.2,p.z-2.25};auto a=step(s,g,p,goal);require(finite(a.eye)&&same(a.eye,goal),"finite large coordinate camera invalid");}}
 std::cout<<std::setprecision(12)<<"{\"passed\":true,\"gpu_used\":false,\"scenarios\":"<<scenarios<<",\"assertions\":"<<checks<<",\"maximum_clear_queries_per_update\":"<<max_queries<<",\"query_bound\":90,\"recovery_samples\":"<<recovery_samples<<",\"maximum_recovery_eye_step_m\":"<<maximum_recovery_eye_step_m<<",\"maximum_recovery_bound_excess_m\":"<<maximum_recovery_bound_excess_m<<",\"ordinary_60degree_radius_m\":"<<world_follow_camera_lens_radius(.12f,1.0471975512f,16.f/9)<<",\"viewer_minimum_71degree_radius_m\":"<<world_follow_camera_lens_radius(.12f,1.24f,16.f/9)<<",\"ultrawide_71degree_radius_m\":"<<world_follow_camera_lens_radius(.12f,1.24f,32.f/9)<<"}\n";
}
