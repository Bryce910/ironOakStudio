#include "engine/world_exploration_camera.h"
#include "engine/world_follow_camera.h"
#include "engine/world_combat_shoulder_camera.h"
#include "engine/world_opening_dialogue_camera.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static double projected_y(const WorldFollowCamera &c,double body_y){return (body_y-c.y)/(c.distance*std::tan(1.0471975512*.5));}
int main(){
 WorldExplorationCamera profile;
 auto indoor=profile.update(true,true,4.2f,1.f/60,true);
 CHECK(indoor.boom==2.25f&&indoor.pivot_drop==.55f&&indoor.shoulder==.35f);
 auto ground=[](double,double){return 0.;};
 auto clear=[](double,double,double,double,float,float,float){return true;};
 auto apartment=world_follow_camera(0,1.75,0,0,0,4.2f,1.f/60,true,ground,clear,indoor.boom,0,indoor.pivot_drop);
 CHECK(std::abs(apartment.distance-2.25f)<1.e-5);
 // A readable shoulder portrait: head above center, knees still on screen.
 CHECK(projected_y(apartment,1.82)>.20&&projected_y(apartment,1.82)<.35);
 CHECK(projected_y(apartment,.50)>-.85&&projected_y(apartment,.50)<-.65);
 auto outdoors=profile.update(true,false,4.2f,1.f/60,true);
 CHECK(outdoors.boom==2.95f&&outdoors.shoulder==.45f);
 auto road=world_follow_camera(0,1.75,0,0,0,4.2f,1.f/60,true,ground,clear,outdoors.boom,0,outdoors.pivot_drop);
 CHECK(projected_y(road,0)>-.95&&projected_y(road,1.82)>.10);
 const auto before=outdoors;
 auto paused=profile.update(true,true,4.2f,0,false);
 CHECK(paused.boom==before.boom&&paused.pivot_drop==before.pivot_drop&&paused.shoulder==before.shoulder);
 auto eased=profile.update(true,true,4.2f,1.f/60,false);
 CHECK(eased.boom>2.25f&&eased.boom<2.95f&&eased.shoulder>.35f&&eased.shoulder<.45f);
 for(float combat_boom:{3.35f,3.85f,4.2f}){
  auto fight=profile.update(false,false,combat_boom,1.f/60,false);
  CHECK(fight.boom==combat_boom); // No second smoothing layer on combat widening.
 }
 for(unsigned i=0;i<180;i++)profile.update(false,false,4.2f,1.f/60,false);
 auto settled=profile.update(false,false,4.2f,1.f/60,false);
 CHECK(settled.pivot_drop<.00001f&&settled.shoulder<.00001f);
 profile.reset();auto invalid=profile.update(true,true,std::numeric_limits<float>::quiet_NaN(),1.f/60,false);
 CHECK(invalid.boom==4.2f&&invalid.pivot_drop==0&&invalid.shoulder==0);
 // The lowered pivot is rejected by a real obstruction rather than starting
 // the boom on the other side of it. Existing floor and boom sweeps remain.
 unsigned vertical=0;
 auto blocked_pivot=world_follow_camera(0,1.75,0,0,0,2.25f,1.f/60,true,ground,
  [&](double ax,double az,double bx,double bz,float y,float,float){if(ax==bx&&az==bz){++vertical;return y>=1.63f;}return true;},2.25f,0,.55f);
 CHECK(vertical>0&&blocked_pivot.y>=1.85);
 auto wall=world_follow_camera(0,1.75,0,0,0,2.25f,1.f/60,false,ground,
  [](double,double,double,double bz,float,float,float){return bz<1.1;},2.25f,0,.55f);
 CHECK(wall.distance<1.1f);
 auto shoulder=world_combat_shoulder_camera(0,1.6,0,0,0,2.25f,1.f/60,true,true,ground,clear,.35f);
 CHECK(shoulder.offset>0&&shoulder.offset<=.35f);
 auto sidewall=world_combat_shoulder_camera(0,1.6,0,0,.35f,2.25f,1.f/60,true,true,ground,
  [](double,double,double bx,double,float,float,float){return bx<.12;},.35f);
 CHECK(sidewall.blocked&&sidewall.offset<=.08f);
 // Authored intro handoff changes render framing only and obeys every cast.
 using namespace world_opening_dialogue_camera;
 const Pose shot{0,1.5,1,0,0,.9,.055},base{0,1.5,3,0,0,1.047,0};
 State handoff;CHECK(handoff.handoff(shot,base));
 CHECK(handoff.transitioning()&&!handoff.conversation_transitioning()); // Keep the actual dad visible during intro handoff.
 auto first=handoff.update(base,base,false,1./60,false,false,[](const Pose&,const Pose&){return true;});
 CHECK(first.z>shot.z&&first.z<base.z&&first.fov>shot.fov&&first.fov<base.fov);
 const auto hold=handoff.update(base,base,false,1./60,true,false,[](const Pose&,const Pose&){return true;});
 CHECK(hold.z==first.z);
 for(unsigned i=0;i<60;i++)handoff.update(base,base,false,1./60,false,false,[](const Pose&,const Pose&){return true;});
 CHECK(!handoff.transitioning()&&std::abs(handoff.current.z-base.z)<1.e-10);
 CHECK(!handoff.handoff({0,1.5,50,0,0,1,0},base));
 State blocked;CHECK(blocked.handoff(shot,base));
 const auto safe=blocked.update(base,base,false,1./60,false,false,[](const Pose&,const Pose&){return false;});
 CHECK(safe.z==base.z&&blocked.blocked_frames==1);
 State reduced;CHECK(reduced.handoff(shot,base));
 CHECK(reduced.update(base,base,false,1./60,false,true,[](const Pose&,const Pose&){return true;}).z==base.z);
 State conversation;
 conversation.update(base,shot,true,1./60,false,false,[](const Pose&,const Pose&){return true;});
 CHECK(conversation.transitioning()&&conversation.conversation_transitioning());
 conversation.update(base,base,false,1./60,false,false,[](const Pose&,const Pose&){return true;});
 CHECK(conversation.conversation_transitioning()); // Existing conversation exit still hides the player.
 conversation.forget();CHECK(!conversation.conversation_transitioning()&&!conversation.transitioning());
 const Pose head_eye{0,1.75,0,0,-.03,.90,.05},follow_eye{.3,1.50,2.25,0,-.03,1.047,0};
 const auto safe_start=body_safe_handoff_start(head_eye,follow_eye,0,0,0);
 CHECK(safe_start.x==follow_eye.x&&safe_start.y==follow_eye.y&&safe_start.z==follow_eye.z);
 CHECK(safe_start.yaw==head_eye.yaw&&safe_start.pitch==head_eye.pitch&&safe_start.fov==head_eye.fov&&safe_start.letterbox==head_eye.letterbox);
 State safe_intro;CHECK(safe_intro.handoff(safe_start,follow_eye,true));
 for(unsigned i=0;i<30;i++){
  const auto p=safe_intro.update(follow_eye,follow_eye,false,1./60,false,false,[](const Pose&,const Pose&){return true;});
  CHECK(p.x==follow_eye.x&&p.y==follow_eye.y&&p.z==follow_eye.z); // No camera sweep through a visible head.
  CHECK(!safe_intro.conversation_transitioning());
 }
 // Safe endpoints on opposite sides still cannot chord through the torso.
 const Pose opposite{-2,1.5,0,1,0,.9,.05},opposite_base{2,1.5,0,0,0,1.047,0};
 CHECK(body_safe_handoff_start(opposite,opposite_base,0,0,0).x==opposite_base.x);
 const Pose clear_shot{2,1.5,1,0,0,.9,0},clear_follow{2,1.5,3,0,0,1.047,0};
 CHECK(body_safe_handoff_start(clear_shot,clear_follow,0,0,0).z==clear_shot.z);
 CHECK(body_safe_handoff_start({-2,3,0,0,0,1,0},{2,3,0,0,0,1,0},0,0,0).x==-2);
 State moving_intro;CHECK(moving_intro.handoff(safe_start,follow_eye,true));
 for(unsigned i=0;i<30;i++){
  Pose moving=follow_eye;moving.x+=i*.035;moving.z-=i*.02;moving.y+=std::sin(i*.1)*.05;
  const auto p=moving_intro.update(moving,moving,false,1./60,false,false,[](const Pose&,const Pose&){return true;});
  CHECK(p.x==moving.x&&p.y==moving.y&&p.z==moving.z);
 }
 std::printf("PASS exploration framing: %u checks; indoor knees/head, outdoor boots, swept pivot/shoulder, combat distance preservation, damped handoff/pause/blocked/reduced-motion\n",checks);
}
