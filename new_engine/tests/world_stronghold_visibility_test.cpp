#include "engine/world_stronghold_visibility.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>

int main(){
    std::array<float,16> p={1,0,0,0,0,1,0,0,0,0,-101.f/99.f,-1,0,0,-200.f/99.f,0};
    auto outside=[&](double x,double y,double z,double r){return world_stronghold_sphere_outside_clip(p.data(),x,y,z,r);};
    assert(!outside(0,0,-5,1));
    assert(outside(0,0,5,1));
    assert(outside(8,0,-5,1));
    assert(outside(-8,0,-5,1));
    assert(outside(0,8,-5,1));
    assert(outside(0,-8,-5,1));
    assert(outside(0,0,-105,1));
    assert(!outside(0,0,-.5,.6));
    assert(!outside(0,0,-101,2));
    assert(!outside(6,0,-5,1));
    assert(!outside(5,0,-5,0));
    assert(!world_stronghold_sphere_outside_clip(nullptr,100,0,0,1));
    assert(!outside(100,0,0,-1));
    assert(!outside(std::numeric_limits<double>::infinity(),0,0,1));
    auto broken=p;broken[7]=std::numeric_limits<float>::quiet_NaN();
    assert(!world_stronghold_sphere_outside_clip(broken.data(),100,0,0,1));
    // Every sphere containing a known point inside this 90-degree view must
    // survive, including centers well outside the view and near/far planes.
    unsigned checked=0;
    for(int z=-99;z<=-2;z++)for(int x=-3;x<=3;x++)for(int y=-3;y<=3;y++){
        const double px=x*.2*(-z),py=y*.2*(-z);
        for(auto d:std::array<std::array<double,3>,4>{{{{10,0,0}},{{0,-10,0}},{{0,0,15}},{{10,-10,15}}}}){
            const double radius=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2])+.001;
            assert(!outside(px+d[0],py+d[1],z+d[2],radius));++checked;
        }
    }
    // A reflected camera's Y flip is valid, as is a directional-light
    // orthographic matrix. Cull against the supplied pass, not the main eye.
    p[5]=-1;assert(outside(0,8,-5,1));assert(!outside(0,0,-5,1));
    std::array<float,16> ortho={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    assert(world_stronghold_sphere_outside_clip(ortho.data(),3,0,0,.5));
    assert(!world_stronghold_sphere_outside_clip(ortho.data(),1,0,0,.5));
    std::printf("PASS: dwarf color/reflection/shadow visibility, invalid-input fallback and %u conservative-bound cases\n",checked);
}
