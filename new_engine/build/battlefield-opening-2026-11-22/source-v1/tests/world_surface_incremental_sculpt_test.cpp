#include "engine/world_surface_field.h"
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
float ground(double x,double z,void *context){
    const bool edited=*static_cast<const bool *>(context);
    float y=8.f+.004f*float(x)+.002f*float(z);
    if(edited&&x>=0&&x<=128&&z>=0&&z<=128)
        y+=2.f*std::sin(float(x)*3.14159265f/128.f)*std::sin(float(z)*3.14159265f/128.f);
    return y;
}
}
int main(){
    using namespace world_surface;
    Data data;data.images.push_back({"Stone","surfaces/stone.png",ImageRole::Color});
    Layer layer;layer.name="Stone";layer.color=0;data.layers.push_back(layer);
    Ribbon road;road.name="Road";road.layer=0;road.width=8;road.shoulder=5;
    road.points={{-40,9,56},{190,9,56}};data.ribbons.push_back(road);
    Ribbon river;river.name="River";river.kind=RibbonKind::River;river.layer=0;
    river.width=10;river.shoulder=5;river.depth=2;
    river.points={{60,10,-40},{60,10,190}};data.ribbons.push_back(river);
    bool original=false,edited=true;std::string error;TerrainField before,full,incremental;
    assert(bake_terrain(data,ground,&original,before,error));
    assert(bake_terrain(data,ground,&edited,full,error));
    assert(rebake_sculpt(before,ground,&edited,{{0,0}},incremental,error));
    assert(full.tiles.size()==incremental.tiles.size());
    for(size_t i=0;i<full.tiles.size();++i){
        const auto &a=full.tiles[i],&b=incremental.tiles[i];
        assert(a.x==b.x&&a.z==b.z&&a.road==b.road);
        for(size_t j=0;j<a.delta.size();++j)assert(std::abs(a.delta[j]-b.delta[j])<1e-5f);
    }
    std::puts("PASS incremental sculpt matches full road/river bake across shared tile edges");
}
