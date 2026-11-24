#include "engine/world_city_sky.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <unistd.h>
using namespace world_city_sky;
static bool near(double a,double b){return std::abs(a-b)<1e-9;}
int main(){
    auto east=uv({1,0,0},0),south=uv({0,0,1},0),north=uv({0,0,-1},0),up=uv({0,1,0},0);
    assert(near(east[0],.75)&&near(east[1],.5));assert(near(south[0],.5));assert(near(north[0],0)||near(north[0],1));assert(near(up[1],0));
    // UE +Z positive quarterturn takes UE+X to UE+Y, the panorama seam.
    auto rotated=uv({1,0,0},.25);assert(near(rotated[0],0)||near(rotated[0],1));
    Config c;c.turns=0;c.contrast=2;c.brightness=1.5;
    const float pixels[]={1,1,1,2,2,2,3,3,3,4,4,4,1,1,1,2,2,2,3,3,3,4,4,4};
    auto rgb=lookup(pixels,4,2,{1,0,0},c);assert(rgb[0]==24&&rgb[1]==24&&rgb[2]==24);
    // UE pixel coordinates have no -.5 offset; U wraps and V clamps.
    rgb=lookup(pixels,4,2,{0,0,-1},c);assert(rgb[0]==1.5f);
    for(unsigned f=0;f<6;f++){auto d=face(f,.1,-.4);assert(near(d[0]*d[0]+d[1]*d[1]+d[2]*d[2],1));}
    char path[]="/tmp/valwood-city-sky-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);std::string error;
    const std::string valid="VALWOOD_CITY_SKY 1\npanorama \"environments/publisher.hdr\"\nmaterial 1.5 2.5 -.1687459945678711\ncube 256\n";
    auto write=[&](const std::string&s){std::ofstream file(path);file<<s;};write(valid);assert(read(path,c,error)==1&&error.empty()&&c.side==256&&c.brightness==1.5);const auto old=c;
    for(auto bad:{valid+"cube 512\n",std::string("VALWOOD_CITY_SKY 2\n"),valid+"unknown 1\n",valid.substr(0,valid.find("cube"))}){write(bad);assert(read(path,c,error)==-1&&!error.empty());assert(c.panorama==old.panorama&&c.turns==old.turns);}
    unlink(path);assert(read(path,c,error)==0);puts("PASS source city sky: UE cardinal/rotation mapping, original longlat pixel convention, source power/brightness order, bounded atomic config; no GPU");
}
