#ifndef VALWOOD_WORLD_RUNTIME_EDITORS_H
#define VALWOOD_WORLD_RUNTIME_EDITORS_H
#include <array>
#include <string>
#include <vector>
namespace world_studio {
struct NavigationArea {std::string name="Navigation";double x=0,z=0;float span=64,cell=1,radius=.32f,slope=.65f;};
struct Actor {std::string name="Traveler";int source=-1,navigation=-1;unsigned appearance=3;bool enabled=true,loop=false;float speed=1.35f,dwell=1.2f,yaw=0;std::vector<std::array<double,2>> points;};
struct Particle {int source=-1;unsigned kind=0;bool enabled=true;double x=0,y=0,z=0,ceiling=1000;float radius=1,height=4,power=.6f,warmth=.2f,rate=1,lifetime=1,size=1,wind=1,light=0;unsigned seed=1;std::array<float,3> tint={1,1,1};float glow=0;};
struct Water {unsigned source=1;bool enabled=true;double x=0,z=0,level=0;float radius_x=8,radius_z=8;std::array<float,3> tint={1,1,1},absorption={1,1,1};float roughness=1,waves=1,flow=1,foam=1;};
}
#endif
