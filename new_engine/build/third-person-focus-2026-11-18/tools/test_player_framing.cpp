#include "engine/world_combat_focus_camera.h"
#include <array>
#include <cstdio>
#include <cstdlib>
struct Bearing{double yaw,pitch;};
static bool visible(double yaw,double pitch,double fov,double aspect,Bearing b){
    const double x=std::sin(b.yaw)*std::cos(b.pitch),y=std::sin(b.pitch),z=-std::cos(b.yaw)*std::cos(b.pitch);
    const double sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    const double depth=x*sy*cp+y*sp-z*cy*cp;
    return depth>0&&std::abs(x*cy+z*sy)<=depth*std::tan(fov*.5)*aspect&&
        std::abs(-x*sy*sp+y*cp+z*cy*sp)<=depth*std::tan(fov*.5);
}
static Bearing bearing(double yaw,double pitch,double fov,double aspect,double u,double v){
    const double h=std::tan(fov*.5),sy=std::sin(yaw),cy=std::cos(yaw),sp=std::sin(pitch),cp=std::cos(pitch);
    const double x=sy*cp+cy*u*h*aspect-sy*sp*v*h,y=sp+cp*v*h,z=-cy*cp+sy*u*h*aspect+cy*sp*v*h;
    return {std::atan2(x,-z),std::atan2(y,std::hypot(x,z))};
}
int main(){unsigned cases=0,frames=0,clamps=0;
 const double fov=WorldCombatFocusCamera::pi/3;
 for(double aspect:{.6,4./3,16./9,32./9})for(double hz:{30.,60.,144.})
 for(double goal:{-3.13,-1.,1.,3.13})for(double side:{-1.,1.})for(double pitch:{-.35,0.,.35}){
    WorldCombatFocusCamera camera;camera.update(goal+side*.7,pitch,0,false,0,1,false,fov,aspect);
    std::array<Bearing,2> points={bearing(goal,pitch,fov,aspect,-side*.9,-.55),bearing(goal,pitch,fov,aspect,-side*.9,.55)};
#ifdef FRAMING_CONSTRAINT
    WorldCombatFocusFraming framing;
    for(unsigned i=0;i<2;++i)framing.player[i]={points[i].yaw,points[i].pitch};
#endif
    WorldCombatFocusFrame frame;
    for(unsigned n=0;n<24;++n){
      frame=camera.update(goal,pitch,1/hz,true,1,1,false,fov,aspect
#ifdef FRAMING_CONSTRAINT
       ,&framing
#endif
      );++frames;
      for(auto b:points)if(!visible(frame.yaw,frame.pitch,fov,aspect,b)){
        std::fprintf(stderr,"FAIL player left frame: aspect%.3f hz%.0f goal%.2f side%.0f pitch%.2f frame%u\n",aspect,hz,goal,side,pitch,n);return 1;}
      if(!visible(frame.yaw,frame.pitch,fov,aspect,{goal,pitch})||!world_combat_focus_frame_valid(frame,frame.yaw,frame.pitch,goal,pitch,true))return 2;
    }
    if(frame.transitioning||frame.yaw!=goal||frame.pitch!=pitch)return 3;
#ifdef FRAMING_CONSTRAINT
    clamps+=camera.stats().player_framing_clamps;
#endif
    ++cases;
 }
 std::printf("PASS %u retarget cases, %u projected player/target frames, %u safety clamps; exact settled orientation retained\n",cases,frames,clamps);
}
