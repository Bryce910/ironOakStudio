#include "engine/world_follow_camera.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)){std::fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);std::exit(1);} } while(0)

static double projected_y(const WorldFollowCamera &camera,double actor_y,float yaw,float pitch,double fov){
    const double dx=-camera.x,dy=actor_y-camera.y,dz=-camera.z;
    const double fx=std::sin(yaw)*std::cos(pitch),fy=std::sin(pitch),fz=-std::cos(yaw)*std::cos(pitch);
    const double ux=-std::sin(yaw)*std::sin(pitch),uy=std::cos(pitch),uz=std::cos(yaw)*std::sin(pitch);
    const double depth=dx*fx+dy*fy+dz*fz;
    CHECK(depth>.1);
    return (dx*ux+dy*uy+dz*uz)/(depth*std::tan(fov*.5));
}

int main(){
    // A neutral 60-degree view must leave visible room below the boots. The
    // previous 3.6m boom put the floor below the bottom of this projection.
    for(unsigned heading=0;heading<32;++heading){
        const float yaw=float(heading*6.283185307179586/32.);
        float longest_step=0;unsigned sweeps=0;
        const auto clear=[&](double ax,double az,double bx,double bz,float y,float height,float radius){
            longest_step=std::max(longest_step,float(std::hypot(bx-ax,bz-az)));++sweeps;
            CHECK(height>=.36f&&radius==.22f&&std::isfinite(y));return true;
        };
        const auto camera=world_follow_camera(0,1.65,0,yaw,0,0,1.f/60,true,[](double,double){return 0.;},clear);
        CHECK(std::abs(camera.distance-4.2f)<1.e-5f);
        CHECK(longest_step<=.150001f&&sweeps==28);
        for(double fov:{1.047197551196598,1.08}){
            const double boots=projected_y(camera,0,yaw,0,fov);
            const double head=projected_y(camera,1.82,yaw,0,fov);
            const double raised_blade=projected_y(camera,3.0,yaw,0,fov);
            CHECK(boots>-.92&&boots<-.80);
            CHECK(head>-.2&&head<.1);
            CHECK(raised_blade<.5&&raised_blade>0);
        }
    }
    // Added distance must retain continuous capsule sweeps and immediately
    // retract at a wall or a high bank; it never passes through either.
    for(bool cut:{false,true}){
        const auto wall=world_follow_camera(0,1.65,0,0,0,4.2f,1.f/60,cut,
            [](double,double){return 0.;},[](double,double,double,double bz,float,float,float){return bz<2.31;});
        CHECK(wall.distance<=2.25001f&&wall.distance>2.09f);
        const auto bank=world_follow_camera(0,1.65,0,0,0,4.2f,1.f/60,cut,
            [](double,double z){return z>=2.31?3.:0.;},[](double,double,double,double,float,float,float){return true;});
        CHECK(bank.distance<=2.25001f&&bank.distance>2.09f);
    }
    const auto extend=world_follow_camera(0,1.65,0,0,0,1.2f,1.f/60,false,
        [](double,double){return 0.;},[](double,double,double,double,float,float,float){return true;});
    CHECK(extend.distance>1.2f&&extend.distance<1.7f);
    CHECK(world_follow_camera_capsule_half_height(.36f,.22f)>.01f);
    std::printf("PASS follow framing: %u checks; neutral boots/blade visible, 0.15m swept segments, immediate wall/bank contraction and damped extension\n",checks);
}
