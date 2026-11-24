#include "engine/world_follow_camera.h"
#include "engine/world_playtest_walk.h"
#include "engine/world_playtest_slice.h"
#include <cstdio>
#include <cstdlib>

#define CHECK(value) do{if(!(value)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value);return 1;}}while(0)
int main(int argc,char **argv){
    CHECK(argc==1||argc==5);
    world_physics::World physics;world_physics::Document document;std::string error;
    CHECK(physics.configure(document,error));
    const auto floor=[](double,double){return 0.;};
    unsigned queries=0;
    const auto clear=[&](double ax,double az,double bx,double bz,float y,float h,float radius){
        world_physics::Hit hit;++queries;
        return physics.cast_capsule({ax,y+h*.5,az},radius,world_follow_camera_capsule_half_height(h,radius),{bx-ax,0,bz-az},hit)&&(!hit.hit||hit.fraction>=.999f);
    };
    // A clear actual Jolt world must extend the boom; a rejected shape query
    // previously contracted it to zero and hid the third-person hero.
    auto camera=world_follow_camera(0,1.75,0,0,0,3.6,.016,true,floor,clear);
    CHECK(queries==24&&camera.distance>3.59f);
    world_physics::Hit hit;
    CHECK(physics.cast_capsule({0,2,0},.22f,world_follow_camera_capsule_half_height(.36f,.22f),{0,0,.15},hit)&&!hit.hit);
    // A thin physical wall still contracts the camera before penetrating it.
    world_physics::Body wall;wall.id=1;wall.name="camera wall";
    wall.shape=world_physics::Shape::Box;wall.motion=world_physics::Motion::Static;
    wall.pose.position={0,2,2};wall.size={2,2,.05};document.bodies.push_back(wall);
    CHECK(physics.configure(document,error));
    camera=world_follow_camera(0,1.75,0,0,0,3.6,.016,true,floor,clear);
    CHECK(camera.distance>1.5f&&camera.distance<1.8f);
    CHECK(physics.stats().query_allocations==0);
    std::puts("PASS actual Jolt open boom and thin-wall contraction; zero query heap fallback");
    // Optional source-sector checks use the same elevated-floor service and
    // camera shape helper as native Grandstone. Scene objects are additionally
    // checked by the native opening acceptance run.
    for(int i=1;i+1<argc;i+=2){
        world_playtest_walk::Support support;world_playtest_slice::Path path;
        CHECK(support.load(argv[i],error));CHECK(world_playtest_slice::load(argv[i+1],path,error));
        const auto spawn=path.spawn;
        const auto height=[&](double x,double z){float feet=0;return support.sample(x,z,spawn.eye_y-1.75,feet)?feet:-100000.f;};
        const auto source_clear=[&](double ax,double az,double bx,double bz,float y,float h,float radius){
            world_physics::Hit source_hit;
            return support.cast_capsule({ax,y+h*.5,az},radius,world_follow_camera_capsule_half_height(h,radius),{bx-ax,0,bz-az},source_hit)&&(!source_hit.hit||source_hit.fraction>=.999f);
        };
        const auto source_camera=world_follow_camera(spawn.x,spawn.eye_y,spawn.z,float(spawn.yaw),float(spawn.pitch),3.6,.016,true,height,source_clear);
        CHECK(source_camera.distance>3.59f&&support.stats().query_allocations==0);
        std::printf("PASS source-sector opening boom %.3f m: %s\n",source_camera.distance,argv[i+1]);
    }
    return 0;
}
