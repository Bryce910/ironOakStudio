// CPU-only source navigation audit. Links the same Jolt adapter as the game.
#include "engine/world_physics.h"
#include "engine/world_height_region.h"
#include "engine/world_walk.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
struct Point{std::string label;double x=0,z=0;};
struct Heights{WorldHeightRegion primary={},surrounding={};};
static bool sample_height(const Heights &data,double x,double z,float &height){return world_height_region_sample(&data.primary,x,z,&height)||world_height_region_sample(&data.surrounding,x,z,&height);}
static float source_height(double x,double z,void *data){float y=0;return sample_height(*static_cast<const Heights*>(data),x,z,y)?y:NAN;}
int main(int argc,char **argv){
    if(argc!=4&&argc!=5){std::fprintf(stderr,"Usage: check-city-collision city.physics showcase.height city.walk [countryside.height]\n");return 2;}
    world_physics::Document document;std::string error;world_physics::World world;
    if(!world_physics::load(argv[1],document,error)||!world.configure(document,error)){std::fprintf(stderr,"%s\n",error.c_str());return 1;}
    for(const auto &body:document.bodies)if(body.motion!=world_physics::Motion::Static||body.sensor){std::fprintf(stderr,"Source city collision must be static nonsensor geometry\n");return 1;}
    Heights heights;char message[256];if(world_height_region_load(&heights.primary,argv[2],message,sizeof(message))!=1){std::fprintf(stderr,"Height region: %s\n",message);return 1;}
    if(argc==5&&world_height_region_load(&heights.surrounding,argv[4],message,sizeof(message))!=1){std::fprintf(stderr,"Surrounding height region: %s\n",message);world_height_region_clear(&heights.primary);return 1;}
    std::ifstream input(argv[3]);std::string line;std::getline(input,line);if(line!="VALWOOD_CITY_WALK 1"){std::fprintf(stderr,"Invalid source city walk route\n");return 1;}
    std::vector<Point> points;while(std::getline(input,line)){
        if(line.empty()||line[0]=='#')continue;
        Point p;std::string extra;std::istringstream row(line);
        if(!(row>>p.label>>p.x>>p.z)||(row>>extra)||!std::isfinite(p.x)||!std::isfinite(p.z)||p.label.size()>95||points.size()>=8192){std::fprintf(stderr,"Invalid walk point\n");return 1;}points.push_back(p);
    }
    if(points.size()<2){std::fprintf(stderr,"Walk audit needs at least two points\n");return 1;}
    unsigned queries=0,blocked=0,slopes=0,outside=0;double walked=0;
    for(size_t i=1;i<points.size();i++){
        const auto &a=points[i-1],&b=points[i];const double distance=std::hypot(b.x-a.x,b.z-a.z);const unsigned steps=std::max(1u,unsigned(std::ceil(distance/.125)));
        for(unsigned step=0;step<steps;step++){
            const double f=double(step)/steps,g=double(step+1)/steps;
            const double x=a.x+(b.x-a.x)*f,z=a.z+(b.z-a.z)*f,nx=a.x+(b.x-a.x)*g,nz=a.z+(b.z-a.z)*g;
            float h=0,nh=0;if(!sample_height(heights,x,z,h)||!sample_height(heights,nx,nz,nh)){outside++;continue;}
            if(!world_walk_can_move(x,z,nx,nz,source_height,&heights,1.2))slopes++;
            world_physics::Hit hit;queries++;
            if(!world.cast_capsule({x,h+.9,z},.37f,.5f,{nx-x,0,nz-z},hit)){std::fprintf(stderr,"Jolt query failed\n");return 1;}
            if(hit.hit&&hit.normal.y<.65&&hit.fraction<.999f){
                if(blocked<12)std::fprintf(stderr,"Blocked %s -> %s at %.3f %.3f by body %llu, fraction %.4f\n",a.label.c_str(),b.label.c_str(),x,z,(unsigned long long)hit.body,hit.fraction);
                blocked++;
            }else walked+=distance/steps;
        }
    }
    const auto stats=world.stats();world_height_region_clear(&heights.primary);world_height_region_clear(&heights.surrounding);
    std::printf("{\"kind\":\"CPU actual Jolt source-UCX capsule route\",\"points\":%zu,\"bodies\":%zu,\"queries\":%u,\"walked_meters\":%.6f,\"blocked\":%u,\"slope_failures\":%u,\"outside_source_ground\":%u,\"query_heap_fallback\":%llu}\n",points.size(),document.bodies.size(),queries,walked,blocked,slopes,outside,(unsigned long long)stats.query_allocations);
    return blocked||slopes||outside||stats.query_allocations?1:0;
}
