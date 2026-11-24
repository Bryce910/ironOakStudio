#include "engine/world_follow_camera.h"
#include "engine/world_combat_shoulder_camera.h"
#include "engine/world_physics.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 world_physics::World physics;world_physics::Document document;std::string error;unsigned queries=0,failures=0;
 const auto floor=[](double,double){return 0.;};
 const auto clear=[&](double ax,double az,double bx,double bz,float y,float h,float radius){
  world_physics::Hit hit;++queries;const bool ok=physics.cast_capsule({ax,y+h*.5,az},radius,world_follow_camera_capsule_half_height(h,radius),{bx-ax,0,bz-az},hit);failures+=!ok;return ok&&(!hit.hit||hit.fraction>=.999f);
 };
 CHECK(physics.configure(document,error));
 auto boom=world_follow_camera(0,1.75,0,0,-.06f,4.2,.016,true,floor,clear,3.2,.2);
 auto side=world_combat_shoulder_camera(boom.x,boom.y,boom.z,0,0,boom.distance,.016,true,true,floor,clear,2.3);
 CHECK(std::abs(boom.distance-3.2)<.00001&&std::abs(side.offset-2.3)<.00001&&!side.blocked&&failures==0);
 world_physics::Body wall;wall.id=1;wall.name="right camera wall";wall.shape=world_physics::Shape::Box;wall.motion=world_physics::Motion::Static;wall.pose.position={.9,2,2};wall.size={.05,4,10};document.bodies.push_back(wall);
 CHECK(physics.configure(document,error));
 side=world_combat_shoulder_camera(boom.x,boom.y,boom.z,0,2.3,boom.distance,.016,true,false,floor,clear,2.3);
 CHECK(side.blocked&&side.offset<.7&&side.offset>=0&&failures==0);
 document.bodies[0].pose.position={0,2,1.2};document.bodies[0].size={8,4,.05};CHECK(physics.configure(document,error));
 boom=world_follow_camera(0,1.75,0,0,-.06f,3.2,.016,false,floor,clear,3.2,.2);
 CHECK(boom.distance>0&&boom.distance<1.0&&failures==0);
 document.bodies[0].pose.position={0,2.45,2};document.bodies[0].size={12,.05,12};CHECK(physics.configure(document,error));
 boom=world_follow_camera(0,1.75,0,0,-.06f,3.2,.016,true,floor,clear,3.2,.2);
 CHECK(boom.y+.18<2.425+.00001&&failures==0);
 CHECK(physics.stats().query_allocations==0);
 std::printf("PASS actual Jolt duel boom/2.3m shoulder, immediate side/rear wall contraction, low ceiling: %u queries, zero failures, zero query heap fallback\n",queries);
}
