#ifndef VALWOOD_WORLD_ECOLOGY_H
#define VALWOOD_WORLD_ECOLOGY_H
#include <stdint.h>
#include <stddef.h>
#define WORLD_COVER_REGION_LIMIT 32u

/* Density order is grass, trees, rocks, ferns. Preset's low8 bits select the
 * habitat: 0 meadow, 1 woodland, 2 jungle. Bits8..11 select owned layers
 * (grass1,trees2,rocks4,ferns8); zero mask retains legacy ownership of all4.
 * A zero density intentionally clears an owned layer. For each layer, the
 * last containing circle that owns it wins. Struct layout is unchanged. */
typedef struct {
#ifdef __cplusplus
    double x=0,z=0;
    float radius=32;
    float density[4]={.8f,.65f,.15f,.6f};
    uint32_t seed=1;
    unsigned preset=1;
    float max_slope=50;
#else
    double x,z;
    float radius,density[4];
    uint32_t seed;
    unsigned preset;
    float max_slope;
#endif
} WorldCoverRegion;

#ifdef __cplusplus
#include <algorithm>
#include <cmath>
namespace world_ecology {
inline unsigned habitat(const WorldCoverRegion &r){return r.preset&0xffu;}
inline unsigned layer_mask(const WorldCoverRegion &r){
    const unsigned mask=(r.preset>>8)&0xfu;return mask?mask:0xfu;
}
inline bool owns(const WorldCoverRegion &r,int kind){
    return kind>=0&&kind<4&&(layer_mask(r)&(1u<<unsigned(kind)))!=0;
}
inline bool valid(const WorldCoverRegion &r){
    if(!std::isfinite(r.x)||!std::isfinite(r.z)||!std::isfinite(r.radius)||
       r.radius<2||r.radius>256||std::abs(r.x)+r.radius>50400||
       std::abs(r.z)+r.radius>28400||(r.preset&~0xfffu)!=0||habitat(r)>2||
       !std::isfinite(r.max_slope)||r.max_slope<0||r.max_slope>85)return false;
    for(float d:r.density)if(!std::isfinite(d)||d<0||d>1)return false;
    return true;
}
inline int at(const WorldCoverRegion *r,size_t count,double x,double z){
    if(!r||count>WORLD_COVER_REGION_LIMIT||!std::isfinite(x)||!std::isfinite(z))return -1;
    for(size_t i=count;i>0;--i){const auto &p=r[i-1];double dx=x-p.x,dz=z-p.z;
        if(dx*dx+dz*dz<double(p.radius)*p.radius)return int(i-1);}
    return -1;
}
inline int at_layer(const WorldCoverRegion *r,size_t count,double x,double z,int kind){
    if(!r||count>WORLD_COVER_REGION_LIMIT||kind<0||kind>3||!std::isfinite(x)||!std::isfinite(z))return -1;
    for(size_t i=count;i>0;--i){const auto &p=r[i-1];if(!owns(p,kind))continue;
        const double dx=x-p.x,dz=z-p.z;
        if(dx*dx+dz*dz<double(p.radius)*p.radius)return int(i-1);}
    return -1;
}
inline float density(const WorldCoverRegion &r,int kind,double x,double z){
    if(!owns(r,kind))return 0;
    const double distance=std::hypot(x-r.x,z-r.z),feather=std::min(8.,double(r.radius)*.15);
    double t=std::clamp((r.radius-distance)/feather,0.,1.);
    return r.density[kind]*float(t*t*(3-2*t));
}
inline bool same(const WorldCoverRegion &a,const WorldCoverRegion &b){
    if(a.x!=b.x||a.z!=b.z||a.radius!=b.radius||a.seed!=b.seed||a.preset!=b.preset||a.max_slope!=b.max_slope)return false;
    for(unsigned i=0;i<4;++i)if(a.density[i]!=b.density[i])return false;
    return true;
}
}
#endif
#endif
