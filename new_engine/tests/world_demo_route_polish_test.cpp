#include "engine/world_demo_contours.h"
#include "engine/world_demo_polish_layout.h"
#include "engine/world_forest_shelter.h"
#include <cassert>
#include <cstdio>
struct WorldScenery {std::vector<WorldSceneryPlacement> items;};
WorldSceneryStats world_scenery_stats(const WorldScenery *s){WorldSceneryStats r;r.source_instances=s?s->items.size():0;return r;}
bool world_scenery_placement(const WorldScenery *s,std::size_t i,WorldSceneryPlacement *out){if(!s||!out||i>=s->items.size())return false;*out=s->items[i];return true;}
int main(){
    assert(world_demo_contour_delta(NAN,0)==0&&world_demo_contour_delta(0,INFINITY)==0);
    for(const auto &b:world_demo_banks){
        assert(std::abs(world_demo_contour_delta(b.x,b.z)-b.height)<1.e-12);
        const double e=1.e-5;
        assert(world_demo_contour_delta(b.x+b.rx,b.z)==0);
        assert(world_demo_contour_delta(b.x+b.rx-e,b.z)<1.e-9);
    }
    // Three complete walking lanes and the public dirt feather remain exact.
    constexpr double path[][2]={{-15,95},{2,60},{-8,20},{5,-25},{-5,-65},{12,-110}};
    unsigned lanes=0;
    for(unsigned i=1;i<std::size(path);i++){
        double dx=path[i][0]-path[i-1][0],dz=path[i][1]-path[i-1][1],length=std::hypot(dx,dz);
        for(int j=0;j<=500;j++)for(double side:{-2.8,0.,2.8}){
            double t=j/500.,x=path[i-1][0]+dx*t-dz/length*side,z=path[i-1][1]+dz*t+dx/length*side;
            assert(world_demo_contour_delta(x,z)==0);lanes++;
        }
    }
    // The whole old pool support envelope and the campsite's inner ellipse.
    for(int z=-47;z<=-3;z++)for(int x=-38;x<=-6;x++)assert(world_demo_contour_delta(x,z)==0);
    for(int j=0;j<360;j++)for(double r:{0.,4.,8.}){
        double t=j*6.283185307179586/360.;assert(world_demo_contour_delta(24+r*std::cos(t)/1.6,30+r*std::sin(t)/.65)==0);
    }
    WorldScenery scene;WorldSceneryPlacement tree;tree.tree=true;tree.scale=1;tree.y=130;
    tree.minimum[0]=-6;tree.minimum[2]=-3;tree.maximum[0]=6;tree.maximum[1]=20;tree.maximum[2]=3;
    scene.items.push_back(tree);auto building=tree;building.tree=false;building.maximum[1]=80;scene.items.push_back(building);
    WorldForestShelter field;std::vector<unsigned char> a,b;
    auto ground=[](double,double){return 130.f;};
    assert(field.build(&scene,0,0,513,a,ground)&&field.trees==1);
    assert(a.size()==513*513*4&&field.minimum_access>=.52&&field.minimum_access<.9&&field.mean_access<1);
    auto pixel=[&](double x,double z){int ix=int(std::lround((x-field.bounds[0])*field.bounds[2]*512)),iz=int(std::lround((z-field.bounds[1])*field.bounds[3]*512));return(std::size_t(iz)*513+ix)*4;};
    assert(a[pixel(0,0)]<230&&a[pixel(30,30)]==255);
    assert(a[pixel(0,0)+2]>a[pixel(0,0)+1]);
    assert(field.build(&scene,0,0,513,b,ground)&&a==b); // deterministic startup bake
    scene.items[0].yaw=1.57079633f;
    assert(field.build(&scene,0,0,513,b,ground)&&b!=a); // actual source ellipse rotates
    scene.items[0].tree=false;
    assert(field.build(&scene,0,0,33,b,ground)&&field.trees==0);
    for(std::size_t i=0;i<b.size();i+=4)assert(b[i]==255);
    const auto previous=b;assert(!field.build(nullptr,0,0,33,b,ground)&&b==previous);
    std::printf("PASS route polish: eight C1 banks; %u three-lane probes; unchanged pool/camp core; crown/rock distinction, rotated bounds, finite deterministic field, encoded height recovery and bounded shared-layer storage\n",lanes);
}
