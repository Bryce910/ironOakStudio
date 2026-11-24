#include <initializer_list>
#include "engine/world_terrain_visibility.h"
#include "engine/world_shadow_math.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <limits>
// Independent homogeneous-corner oracle: a rejected box must have all8
// corners outside at least one exact clip half-space. No projected-center
// approximation, no post-divide corner-inside assumptions.
int main() {
    uint32_t state=77181;auto random=[&](){state=state*1664525u+1013904223u;return double(state>>8)/16777216.;};
    unsigned rejected=0;
    for(int n=0;n<150000;n++) {
        const double yaw=random()*6.283185307179586,pitch=(random()-.5)*2.8,aspect=.6+random()*2.8;
        const double cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
        const double r[3]={cy,0,sy},u[3]={-sy*sp,cp,cy*sp},f[3]={sy*cp,sp,-cy*cp};
        const double q=1./std::tan(.54),near=.4,far=15000.,a=-(far+near)/(far-near),b=-2*far*near/(far-near);
        float m[16]={};
        for(int k=0;k<3;k++){m[k*4]=float(q/aspect*r[k]);m[k*4+1]=float(q*u[k]);m[k*4+2]=float(-a*f[k]);m[k*4+3]=float(f[k]);}
        m[14]=float(b);
        const double x=(random()-.5)*60000,y=(random()-.5)*10000,z=(random()-.5)*60000;
        const double h[3]={16+random()*16384,2+random()*2500,16+random()*16384};
        if(world_terrain_box_visible(m,x,y,z,h[0],h[1],h[2]))continue;
        rejected++;bool separating=false;
        for(int axis=0;axis<3;axis++)for(int sign:{-1,1}) {
            bool all=true;
            for(int cx:{-1,1})for(int cy2:{-1,1})for(int cz:{-1,1}) {
                const double v[4]={x+cx*h[0],y+cy2*h[1],z+cz*h[2],1};double clip[4]={};
                for(int row=0;row<4;row++)for(int col=0;col<4;col++)clip[row]+=m[col*4+row]*v[col];
                if(clip[3]+sign*clip[axis]>=0)all=false;
            }
            separating=separating||all;
        }
        assert(separating);
    }
    unsigned shadow_rejected=0;
    for(int n=0;n<50000;n++) {
        const double camera[3]={(random()-.5)*100800.,random()*1800.,(random()-.5)*56709.};
        const float sun[3]={float(random()*2.-1.),float(.02+random()),float(random()*2.-1.)};
        const float center[3]={0,-25,0};float m[16]={};
        assert(world_shadow_directional_matrix(camera,sun,160.f,2048,center,m));
        const double x=(random()-.5)*1800.,y=(random()-.5)*2000.,z=(random()-.5)*1800.;
        const double h[3]={16+random()*1024.,2+random()*300.,16+random()*1024.};
        if(world_terrain_box_visible(m,x,y,z,h[0],h[1],h[2]))continue;
        shadow_rejected++;bool separating=false;
        for(int axis=0;axis<3;axis++)for(int sign:{-1,1}) {
            bool all=true;
            for(int cx:{-1,1})for(int cy:{-1,1})for(int cz:{-1,1}) {
                const double v[4]={x+cx*h[0],y+cy*h[1],z+cz*h[2],1};double clip[4]={};
                for(int row=0;row<4;row++)for(int col=0;col<4;col++)clip[row]+=m[col*4+row]*v[col];
                if(clip[3]+sign*clip[axis]>=0)all=false;
            }
            separating=separating||all;
        }
        assert(separating);
    }
    assert(shadow_rejected>10000);
    std::printf("PASS50000 world-anchored directional shadow/terrain bounds; %u independently proven clip rejections\n",shadow_rejected);
    float m[16]={};m[0]=std::numeric_limits<float>::quiet_NaN();assert(world_terrain_box_visible(m,0,0,0,1,1,1));
    assert(rejected>50000);std::printf("PASS150000 camera/tile/skirts cases; %u independently proven full clip rejections; invalid input fails open\n",rejected);
}
