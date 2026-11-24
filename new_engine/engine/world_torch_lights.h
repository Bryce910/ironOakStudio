#ifndef VALWOOD_WORLD_TORCH_LIGHTS_H
#define VALWOOD_WORLD_TORCH_LIGHTS_H

#include "engine/world_torch_flames.h"
#include <algorithm>
#include <array>
#include <utility>

// Generated source sockets use village-local XZ and absolute world height.
// The wall plane and clipping box use that same local-XZ/world-Y space.
struct VillageTorchEmitter {
    double x,y,z;
    float height,width,intensity;
    unsigned seed;
    float nx,ny,nz,plane_d;
    float xmin,zmin,xmax,zmax,ymin,ymax;
};
struct WorldTorchLights {
    static constexpr unsigned capacity=8;
    unsigned count=0;
    unsigned actor_only=0;
    std::array<std::array<float,4>,capacity> position_radius={},color_intensity={},plane={},bounds={};
    std::array<std::array<float,2>,capacity> heights={};
    std::array<std::array<float,4>,capacity> direction_cone=[] {std::array<std::array<float,4>,capacity> a={};for(auto &v:a)v[3]=-1;return a;}();
    std::array<float,3> cluster_min={},cluster_max={};
};
inline float world_torch_smooth(float lo,float hi,float x) {
    const float t=std::clamp((x-lo)/(hi-lo),0.f,1.f);
    return t*t*(3.f-2.f*t);
}
// Fixed bounded work, no allocation or frame RNG. The ninth-nearest distance
// defines a continuous fade for the eighth light; rank changes cannot pop a
// bright source on/off. Camera-distance fading removes far zero contributions.
inline bool world_torch_lights_build(const VillageTorchEmitter *emitters,std::size_t count,
    double ox,double oz,double cx,double cy,double cz,double time,WorldTorchLights &out,
    float radius=8.f,bool actor_only=false) {
    if(count>WORLD_TORCH_FLAME_CAPACITY||(count&&!emitters)||!std::isfinite(ox)||!std::isfinite(oz)||
       !std::isfinite(cx)||!std::isfinite(cy)||!std::isfinite(cz)||!std::isfinite(time)||
       std::abs(ox)>100800||std::abs(oz)>100800||std::abs(cx)>100800||std::abs(cz)>100800||std::abs(cy)>20000||
       !std::isfinite(radius)||radius<.25f||radius>32.f)return false;
    std::array<std::pair<double,std::size_t>,WORLD_TORCH_FLAME_CAPACITY> nearest={};
    std::size_t candidates=0;
    for(std::size_t i=0;i<count;i++) {
        const auto &e=emitters[i];
        const double values[]={e.x,e.y,e.z,e.height,e.width,e.intensity,e.nx,e.ny,e.nz,e.plane_d,e.xmin,e.zmin,e.xmax,e.zmax,e.ymin,e.ymax};
        for(double v:values)if(!std::isfinite(v))return false;
        const double normal2=e.nx*e.nx+e.ny*e.ny+e.nz*e.nz;
        if(e.height<=0||e.height>5||e.width<=0||e.width>5||e.intensity<0||e.intensity>1000||
           std::abs(e.x)>1000||std::abs(e.z)>1000||std::abs(e.y)>20000||normal2>1.001||
           std::abs(e.plane_d)>30000||e.xmin>=e.xmax||e.zmin>=e.zmax||e.ymin>=e.ymax||
           std::max({std::abs(e.xmin),std::abs(e.zmin),std::abs(e.xmax),std::abs(e.zmax),std::abs(e.ymin),std::abs(e.ymax)})>30000)return false;
        if(e.intensity<=0)continue; // Disabled sources must not consume a light-selection rank.
        const double dx=ox+e.x-cx,dy=e.y+e.height*.25-cy,dz=oz+e.z-cz;
        nearest[candidates++]={std::sqrt(dx*dx+dy*dy+dz*dz),i};
    }
    std::sort(nearest.begin(),nearest.begin()+candidates);
    const double cutoff=candidates>WorldTorchLights::capacity?nearest[WorldTorchLights::capacity].first:100000.;
    WorldTorchLights result;result.actor_only=actor_only;
    for(std::size_t k=0;k<std::min<std::size_t>(candidates,WorldTorchLights::capacity);k++) {
        const auto &e=emitters[nearest[k].second];
        const float fade=(1.f-world_torch_smooth(60.f,80.f,float(nearest[k].first)))*
            world_torch_smooth(0.f,6.f,float(cutoff-nearest[k].first));
        if(fade<=0||e.intensity<=0)continue;
        const unsigned i=result.count++;
        result.position_radius[i]={{float(ox+e.x),float(e.y+e.height*.25),float(oz+e.z),radius}};
        result.color_intensity[i]={{1.f,.47f,.12f,e.intensity*fade*world_torch_flicker(time,e.seed)}};
        result.plane[i]={{e.nx,e.ny,e.nz,float(e.plane_d-e.nx*ox-e.nz*oz)}};
        result.bounds[i]={{float(ox+e.xmin),float(oz+e.zmin),float(ox+e.xmax),float(oz+e.zmax)}};
        result.heights[i]={{e.ymin,e.ymax}};
        for(unsigned axis=0;axis<3;axis++) {
            const float low=result.position_radius[i][axis]-radius,high=result.position_radius[i][axis]+radius;
            result.cluster_min[axis]=i?std::min(result.cluster_min[axis],low):low;
            result.cluster_max[axis]=i?std::max(result.cluster_max[axis],high):high;
        }
    }
    out=result;return true;
}

#endif
