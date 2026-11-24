#include "engine/world_rigid_weapon_pose.h"
#include "engine/world_combat_shoulder_camera.h"
#include "engine/world_bow_presentation.h"
#include <cstdio>
#include <limits>
using namespace world_rigid_weapon_pose;
static unsigned checks=0;
#define CHECK(v) do{++checks;if(!(v)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#v);return 1;}}while(0)
static float distance(Point a,Point b){const auto d=sub(a,b);return std::sqrt(dot(d,d));}
int main(){
    const std::array<Point,3> source={{{.2f,.3f,.4f},{1.2f,.3f,.4f},{.2f,1.3f,.4f}}};
    const Point vertex={.71f,-.25f,1.8f};const float pivot_a[3]={.2f,.3f,.4f},pivot_b[3]={-.3f,.7f,.1f};
    for(unsigned angle=0;angle<=180;angle+=3)for(unsigned axis=0;axis<7;++axis){
        Point vector={float((axis+1)%3)-.7f,float((axis+2)%4)-1.1f,float(axis%3)+.2f};CHECK(unit(vector));
        const float radians=angle*.017453292519943295f;const float q[4]={vector[0]*std::sin(radians*.5f),vector[1]*std::sin(radians*.5f),vector[2]*std::sin(radians*.5f),std::cos(radians*.5f)};
        std::array<Point,3> target;for(unsigned i=0;i<3;++i){target[i]=rotate(sub(source[i],{pivot_a[0],pivot_a[1],pivot_a[2]}),q);for(unsigned k=0;k<3;++k)target[i][k]+=pivot_b[k];}
        float derived[4];CHECK(rotation(source,target,derived));
        for(unsigned i=0;i<3;++i)CHECK(distance(interpolate(source[i],pivot_a,pivot_b,derived,1),target[i])<.00001f);
        for(unsigned step=0;step<=20;++step){const float w=step/20.f;const Point moving_grip={pivot_a[0]+(pivot_b[0]-pivot_a[0])*w,pivot_a[1]+(pivot_b[1]-pivot_a[1])*w,pivot_a[2]+(pivot_b[2]-pivot_a[2])*w};
            const auto p=interpolate(vertex,pivot_a,pivot_b,derived,w);
            CHECK(std::abs(distance(p,moving_grip)-distance(vertex,{pivot_a[0],pivot_a[1],pivot_a[2]}))<.00001f);
            CHECK(distance(interpolate({pivot_a[0],pivot_a[1],pivot_a[2]},pivot_a,pivot_b,derived,w),moving_grip)<.000001f);
        }
    }
    float q[4];CHECK(!rotation({Point{0,0,0},Point{1,0,0},Point{2,0,0}},source,q));
    auto invalid=source;invalid[1][0]=std::numeric_limits<float>::quiet_NaN();CHECK(!rotation(invalid,source,q));
    const float origin[3]={0,0,0},halfturn[4]={0,1,0,0};
    CHECK(std::abs(distance(interpolate({1,0,0},origin,origin,halfturn,.5f),{0,0,0})-1)<.000001f); // A linear vertex morph collapses this blade to zero.
    unsigned casts=0;bool valid_casts=true;
    auto clear=[&](double ax,double az,double bx,double bz,float y,float h,float r){++casts;valid_casts&=std::hypot(bx-ax,bz-az)<=.08001&&std::abs(y-.82f)<.00001&&std::abs(h-.36f)<.00001&&std::abs(r-.22f)<.00001;return true;};
    auto camera=world_combat_shoulder_camera(0,1,0,0,0,3.6f,1.f/60,true,true,[](double,double){return 0.;},clear);
    CHECK(std::abs(camera.x-world_combat_shoulder_maximum)<.00001&&std::abs(camera.z)<.00001&&!camera.blocked&&valid_casts&&casts==13);
    camera=world_combat_shoulder_camera(0,1,0,0,.72f,3.6f,1.f/60,true,false,[](double,double){return 0.;},[](double,double,double bx,double,float,float,float){return bx<.3;});
    CHECK(camera.blocked&&camera.offset<=.24001f); // Obstruction retracts immediately even from a fully extended shoulder.
    camera=world_combat_shoulder_camera(0,1,0,0,.72f,3.6f,1.f/60,true,false,[](double x,double){return x>.35?2.:0.;},[](double,double,double,double,float,float,float){return true;});
    CHECK(camera.blocked&&camera.offset<=.32001f);
    camera=world_combat_shoulder_camera(0,1,0,0,.72f,3.6f,1.f/60,false,false,[](double,double){return 0.;},[](double,double,double,double,float,float,float){return true;});
    CHECK(camera.offset>0&&camera.offset<.72f); // Focus release eases toward ordinary framing.
    camera=world_combat_shoulder_camera(0,1,0,0,0,.9f,1.f/60,true,true,[](double,double){return 0.;},[](double,double,double,double,float,float,float){return true;});
    CHECK(camera.offset<.06f&&!camera.blocked); // Tight boom clearance also limits the lateral extension.
    // The gameplay charge resets immediately on release. The rendered source
    // arms/body/socket parameter must remain continuous and finish at neutral.
    for(float hz:{30.f,60.f,120.f}){
        WorldBowPresentation bow;const float dt=1/hz;
        CHECK(bow.update(0,false,true,dt)==0);
        for(unsigned i=1;i<=unsigned(hz);++i)bow.update(std::min(1.f,i/hz),true,true,dt);
        CHECK(std::abs(bow.draw-1)<.00001f);
        const float before=bow.draw,first=bow.update(0,false,true,dt);
        CHECK(first>0&&first<before&&before-first<.06f);
        float previous=first;
        for(unsigned i=0;i<unsigned(hz);++i){const float next=bow.update(0,false,true,dt);CHECK(next<=previous&&next>=0);previous=next;}
        CHECK(previous==0);
        bow.update(1,true,true,0,true);const float paused=bow.update(0,false,true,0);
        CHECK(paused==1&&bow.update(0,false,true,0)==paused);
        bow.update(0,false,true,dt);const float interrupted=bow.draw;
        CHECK(std::abs(bow.update(0,true,true,dt)-interrupted)<=4*dt+.000001f);
        CHECK(bow.update(0,false,false,dt)==0);
        CHECK(bow.update(0,false,true,dt)==0);
    }
    std::printf("PASS motion presentation: %u checks; rigid size/grip/endpoints through180degrees, degenerate rejection, bounded shoulder/floor/capsule sweeps, collision retraction and focus release\n",checks);
}
