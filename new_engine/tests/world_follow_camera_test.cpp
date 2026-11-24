#include "engine/world_follow_camera.h"
#include <cassert>
#include <cmath>
int main(){
    unsigned checks=0;
    auto floor=[](double,double){return 0.;};
    auto open=[&](double,double,double,double,float,float,float){checks++;return true;};
    auto a=world_follow_camera(1000000,1.75,-2000000,0,0,3.6,.016,true,floor,open);
    assert(checks==24&&std::abs(a.x-1000000)<1e-6&&std::abs(a.z+1999996.4)<1e-5&&a.distance>3.59);
    // Thin wall crossing and sphere padding contract the camera immediately.
    auto wall=[](double,double az,double,double bz,float,float,float radius){return !(az<1-radius&&bz>=1-radius);};
    auto b=world_follow_camera(0,1.75,0,0,0,3.6,.016,false,floor,wall);
    assert(b.distance<.79&&b.distance>.5);
    auto c=world_follow_camera(0,1.75,0,0,0,b.distance,.016,false,floor,open);
    assert(c.distance>b.distance&&c.distance<3.6);
    auto terrain=[](double,double z){return z>.8?4.:0.;};
    auto d=world_follow_camera(0,1.75,0,0,0,3.6,.016,true,terrain,open);
    assert(d.distance<=.8);
    for(float yaw:{-3.f,-1.f,0.f,2.f})for(float pitch:{-1.48f,0.f,1.48f}){
        auto e=world_follow_camera(0,1.75,0,yaw,pitch,3.6,.016,true,floor,open);
        assert(std::isfinite(e.y)&&e.y>=.22&&e.distance>=0&&e.distance<=3.6f);
    }
}
