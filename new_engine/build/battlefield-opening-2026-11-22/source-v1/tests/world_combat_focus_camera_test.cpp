#include "engine/world_combat_focus_camera.h"
#include "engine/world_follow_camera.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#v);std::exit(1);}}while(0)
static constexpr double pi=WorldCombatFocusCamera::pi,fov=pi/3;
static double angle(double a,double b){return std::remainder(a-b,2*pi);}
static bool visible(const WorldCombatFocusFrame &frame,double yaw,double pitch,double aspect){
    const double dx=std::sin(yaw)*std::cos(pitch),dy=std::sin(pitch),dz=-std::cos(yaw)*std::cos(pitch);
    const double sy=std::sin(frame.yaw),cy=std::cos(frame.yaw),sp=std::sin(frame.pitch),cp=std::cos(frame.pitch);
    const double depth=dx*sy*cp+dy*sp-dz*cy*cp;
    const double right=dx*cy+dz*sy,up=-dx*sy*sp+dy*cp+dz*cy*sp;
    return depth>0&&std::abs(right)<depth*std::tan(fov/2)*aspect&&std::abs(up)<depth*std::tan(fov/2);
}
static void check_frame(const WorldCombatFocusFrame &frame,double yaw,double pitch,bool focused,double aspect){
    CHECK(frame.valid&&std::isfinite(frame.yaw)&&std::isfinite(frame.pitch));
    CHECK(world_combat_focus_frame_valid(frame,frame.yaw,frame.pitch,yaw,pitch,focused));
    if(frame.transitioning){CHECK(frame.elapsed_seconds<.12);CHECK(std::abs(angle(frame.yaw,yaw))+std::abs(frame.pitch-pitch)<=frame.maximum_error+1e-10);}
    else CHECK(frame.yaw==yaw&&frame.pitch==pitch);
    if(focused)CHECK(visible(frame,yaw,pitch,aspect));
}
int main(){
    // Actual 3D frustum projection across portrait,4:3,16:9 and ultrawide,
    // all tested render rates, near-pole pitch and shortest-arc wrap headings.
    for(double aspect:{.6,4./3,16./9,32./9})for(double hz:{30.,60.,144.})
        for(double start:{-3.13,-1.7,0.,1.7,3.13})for(double goal:{-3.12,-.4,.4,3.12})
        for(double pitch:{-.9,0.,.9}){
            WorldCombatFocusCamera camera;
            auto frame=camera.update(start,-pitch,1/hz,false,8,1,false,fov,aspect);
            CHECK(frame.reset&&!frame.transitioning);
            double elapsed=0,last_error=10;
            for(unsigned n=0;n<24;++n){
                frame=camera.update(goal,pitch,1/hz,true,2,1,false,fov,aspect);elapsed+=1/hz;
                check_frame(frame,goal,pitch,true,aspect);
                const double error=std::abs(angle(frame.yaw,goal))+std::abs(frame.pitch-pitch);
                CHECK(error<=last_error+1e-10);last_error=error;
                if(elapsed>=.12-1e-12&&!frame.transitioning)break;
            }
            CHECK(!frame.transitioning&&elapsed<=.12+1/hz+1e-10);
            CHECK(camera.stats().transitions==1);
            CHECK(camera.stats().maximum_completion_seconds<=.12+1/hz+1e-10);
            // Target switching is a new finite transition, not a new aim state.
            frame=camera.update(goal+.25,pitch-.07,1/hz,true,3,1,false,fov,aspect);
            CHECK(frame.transitioning&&camera.stats().transitions==2);
            check_frame(frame,goal+.25,pitch-.07,true,aspect);
            // Releasing focus blends to the latest free-look goal and settles.
            elapsed=0;
            do{frame=camera.update(start,.1,1/hz,false,8,1,false,fov,aspect);elapsed+=1/hz;check_frame(frame,start,.1,false,aspect);}while(frame.transitioning);
            CHECK(elapsed<=.12+1/hz+1e-10&&camera.stats().transitions==3);
        }
    // Time-based shape is identical at shared timestamps regardless of steps.
    for(double hz:{30.,60.,144.}){
        WorldCombatFocusCamera split,one;
        split.update(0,0,0,false,8,1,false,fov,16./9);one.update(0,0,0,false,8,1,false,fov,16./9);
        const auto reference=one.update(.25,.08,.1,true,0,1,false,fov,16./9);
        double elapsed=0;WorldCombatFocusFrame frame;
        while(elapsed<.1-1e-12){const double step=std::min(1/hz,.1-elapsed);frame=split.update(.25,.08,step,true,0,1,false,fov,16./9);elapsed+=step;}
        CHECK(std::abs(frame.yaw-reference.yaw)<1e-12&&std::abs(frame.pitch-reference.pitch)<1e-12);
    }
    // Continuous motion of the same target is not smoothed forever: only the
    // original acquire residual remains, and it is exactly gone after120ms.
    for(double hz:{30.,60.,144.}){
        WorldCombatFocusCamera camera;camera.update(0,0,0,false,8,1,false,fov,16./9);
        double elapsed=0;WorldCombatFocusFrame frame;
        for(unsigned n=0;n<60;++n){elapsed+=1/hz;const double goal=.3+elapsed*.2,pitch=.05+elapsed*.03;
            frame=camera.update(goal,pitch,1/hz,true,1,1,false,fov,16./9);check_frame(frame,goal,pitch,true,16./9);
            if(elapsed>.12)CHECK(!frame.transitioning&&frame.yaw==goal&&frame.pitch==pitch);
        }
        CHECK(camera.stats().transitions==1);
    }
    // Cut, first-person/editor/free-look mode and water/dive context changes
    // discard the residual. It must never resume after a pause or teleport.
    for(unsigned mode:{0u,2u,4u,8u,16u,32u})for(bool cut:{false,true}){
        WorldCombatFocusCamera camera;camera.update(0,0,0,false,8,1,false,fov,16./9);
        auto frame=camera.update(.2,.1,1./60,true,1,1,false,fov,16./9);CHECK(frame.transitioning);
        frame=camera.update(-.7,-.2,1./60,true,1,mode,cut,fov,16./9);
        CHECK(frame.reset&&!frame.transitioning&&frame.yaw==-.7&&frame.pitch==-.2);
        frame=camera.update(-.6,-.1,1./60,true,1,mode,false,fov,16./9);
        CHECK(!frame.transitioning&&frame.yaw==-.6&&frame.pitch==-.1);
    }
    // Rapid repeated target changes remain bounded with no wrong-arc spin.
    WorldCombatFocusCamera rapid;rapid.update(3.13,0,0,false,8,1,false,fov,16./9);
    for(unsigned i=0;i<200;++i){const double goal=(i%2?3.12:-3.12);const auto frame=rapid.update(goal,.1,1./144,true,i%8,1,false,fov,16./9);check_frame(frame,goal,.1,true,16./9);CHECK(std::abs(angle(frame.yaw,goal))<.2);}
    // The first switch spans two degrees over the wrap, not nearly360 degrees.
    WorldCombatFocusCamera wrap;wrap.update(pi-.02,0,0,false,8,1,false,fov,16./9);
    auto frame=wrap.update(-pi+.02,0,1./60,true,0,1,false,fov,16./9);
    CHECK(frame.transitioning&&!frame.visibility_clamped&&std::abs(angle(frame.yaw,-pi+.02))<.04);
    // Wide switches stay visible, but do not falsely claim the safety clamp is
    // an entirely smooth off-screen-to-on-screen move.
    WorldCombatFocusCamera wide;wide.update(0,0,0,false,8,1,false,fov,16./9);
    frame=wide.update(pi,.8,1./60,true,0,1,false,fov,16./9);
    CHECK(frame.visibility_clamped&&wide.stats().visibility_clamps==1);check_frame(frame,pi,.8,true,16./9);
    // Policy negatives: a stale budget cannot excuse errors after the deadline,
    // excess angular error, off-frustum targets or nonfinite rendered angles.
    auto bad=frame;bad.elapsed_seconds=.12;CHECK(!world_combat_focus_frame_valid(bad,bad.yaw,bad.pitch,pi,.8,true));
    bad=frame;bad.maximum_error=0;CHECK(!world_combat_focus_frame_valid(bad,bad.yaw,bad.pitch,pi,.8,true));
    bad=frame;bad.visibility_limit=.001;CHECK(!world_combat_focus_frame_valid(bad,bad.yaw,bad.pitch,pi,.8,true));
    bad=frame;bad.transitioning=false;CHECK(!world_combat_focus_frame_valid(bad,bad.yaw,bad.pitch,pi,.8,true));
    CHECK(!world_combat_focus_frame_valid(frame,std::numeric_limits<double>::quiet_NaN(),0,pi,.8,true));
    for(unsigned i=0;i<30;++i)frame=wide.update(pi,.8,1./60,true,0,1,false,fov,16./9);
    CHECK(world_combat_focus_frame_valid(frame,pi,.8,pi,.8,true));
    CHECK(!world_combat_focus_frame_valid(frame,pi+.0002,.8,pi,.8,true));
    for(double invalid:{-1.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}){
        frame=wide.update(0,0,invalid,true,0,1,false,fov,16./9);CHECK(!frame.valid&&frame.reset&&std::isfinite(frame.yaw));
    }
    frame=wide.update(0,0,.5,true,0,1,false,fov,16./9);CHECK(frame.valid&&frame.reset&&!frame.transitioning);
    // The existing boom still contracts immediately while orientation blends.
    const auto boom=world_follow_camera(0,1.75,0,0,0,4.2f,1.f/60,false,
        [](double,double){return 0.;},[](double,double,double,double z,float,float,float){return z<2.31;});
    CHECK(boom.distance<=2.25001f&&boom.distance>2.09f);
    const double saved_x=boom.x,saved_y=boom.y,saved_z=boom.z;
    wide.update(.2,.1,1./60,false,8,1,false,fov,16./9);
    CHECK(boom.x==saved_x&&boom.y==saved_y&&boom.z==saved_z);
    std::printf("PASS combat focus camera: %u checks;120ms finite acquire/switch/release,30/60/144Hz,live-goal tracking,3D frustum bounds,strict settled policy,mode/cut resets and unchanged immediate collision contraction\n",checks);
}
