#ifndef VALWOOD_WORLD_FOREST_SHELTER_H
#define VALWOOD_WORLD_FOREST_SHELTER_H
#include "engine/world_scenery.h"
#include "engine/world_transform_math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
// Startup-only crown projection: a bounded diffuse-sky approximation, not GI
// or a replacement for the existing direct shadows. Uses the real source
// tree bounds and root heights. One extra layer reuses the landscape sampler.
struct WorldForestShelter {
    std::array<float,4> bounds={};
    float height_min=112, height_range=96;
    bool arrival_region=false;
    unsigned trees=0;float minimum_access=1,mean_access=1;
    template<class Height> bool build(const WorldScenery *scene,double cx,double cz,
                                     int output_side,std::vector<unsigned char> &out,Height height,bool adaptive_heights=false) {
        if(!scene||output_side<2||output_side>1025||!std::isfinite(cx)||!std::isfinite(cz))return false;
        height_min=112; height_range=96; arrival_region=false;
        bounds={float(cx-70),float(cz-145),1.f/140,1.f/270};
        constexpr int n=129;constexpr unsigned count=n*n;
        std::vector<float> access(count,1),ground(count),top(count);
        // The lowland arrival woodland uses denser overlapping source crowns.
        // Retain a modest diffuse-sky floor there; legacy rainforest fields
        // keep their original response and encoded bytes exactly.
        const float sky_floor=adaptive_heights?.60f:.52f;
        const float crown_strength=adaptive_heights?.32f:.38f;
        const double dx=140./(n-1),dz=270./(n-1);
        for(int z=0;z<n;z++)for(int x=0;x<n;x++){
            const auto p=z*n+x;ground[p]=height(bounds[0]+x*dx,bounds[1]+z*dz);
            if(!std::isfinite(ground[p]))return false;
            top[p]=ground[p];
        }
        trees=0;const auto stats=world_scenery_stats(scene);
        for(std::size_t k=0;k<stats.source_instances;k++){
            WorldSceneryPlacement a;if(!world_scenery_placement(scene,k,&a)||!a.tree)continue;
            double rx=(a.maximum[0]-a.minimum[0])*.5*a.scale,rz=(a.maximum[2]-a.minimum[2])*.5*a.scale;
            double c=std::cos(a.yaw),s=std::sin(a.yaw),lx=(a.maximum[0]+a.minimum[0])*.5*a.scale,lz=(a.maximum[2]+a.minimum[2])*.5*a.scale,x0=a.x+c*lx+s*lz,z0=a.z-s*lx+c*lz;
            float canopy_top=a.y+a.maximum[1]*a.scale;
            if(a.pitch!=0||a.roll!=0||a.sx!=1||a.sy!=1||a.sz!=1){auto matrix=vw_transform::matrix({a.x,a.y,a.z,a.yaw*57.29577951308232,a.scale,a.pitch*57.29577951308232,a.roll*57.29577951308232,a.sx,a.sy,a.sz});double xmin=1e30,xmax=-1e30,zmin=1e30,zmax=-1e30,top_y=-1e30;for(unsigned j=0;j<8;j++){auto p=vw_transform::point(matrix,{j&1?a.maximum[0]:a.minimum[0],j&2?a.maximum[1]:a.minimum[1],j&4?a.maximum[2]:a.minimum[2]});xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);zmin=std::min(zmin,p[2]);zmax=std::max(zmax,p[2]);top_y=std::max(top_y,p[1]);}x0=(xmin+xmax)*.5;z0=(zmin+zmax)*.5;rx=(xmax-xmin)*.7071067811865476;rz=(zmax-zmin)*.7071067811865476;c=1;s=0;canopy_top=float(top_y);}
            if(rx<.5||rz<.5)continue;
            const double r=std::hypot(rx,rz);
            if(x0+r<bounds[0]||x0-r>bounds[0]+140||z0+r<bounds[1]||z0-r>bounds[1]+270)continue;
            const int xa=std::clamp(int(std::floor((x0-r-bounds[0])/dx)),0,n-1),
                xb=std::clamp(int(std::ceil((x0+r-bounds[0])/dx)),0,n-1),
                za=std::clamp(int(std::floor((z0-r-bounds[1])/dz)),0,n-1),
                zb=std::clamp(int(std::ceil((z0+r-bounds[1])/dz)),0,n-1);
            for(int z=za;z<=zb;z++)for(int x=xa;x<=xb;x++){
                const double wx=bounds[0]+x*dx-x0,wz=bounds[1]+z*dz-z0,
                    u=(c*wx-s*wz)/rx,v=(s*wx+c*wz)/rz,q=u*u+v*v;
                if(q>=1)continue;
                const auto p=z*n+x;const float t=float(1-q);
                access[p]=std::max(sky_floor,access[p]*(1-crown_strength*t*t));
                top[p]=std::max(top[p],canopy_top);
            }
            trees++;
        }
        // Preserve the legacy codec exactly. Lowland fallback fields encode
        // their actual sampled ground and source crowns, not112..208m.
        if(adaptive_heights) {
            const float low=*std::min_element(ground.begin(),ground.end());
            const float high=*std::max_element(top.begin(),top.end());
            if(!std::isfinite(low)||!std::isfinite(high)||high<low)return false;
            height_min=std::floor(low)-1.f;
            height_range=std::max(8.f,std::ceil(high)+1.f-height_min);
            if(!std::isfinite(height_range))return false;
        }
        out.resize(std::size_t(output_side)*output_side*4);minimum_access=1;double sum=0;
        for(int z=0;z<output_side;z++)for(int x=0;x<output_side;x++){
            const double gx=double(x)*(n-1)/(output_side-1),gz=double(z)*(n-1)/(output_side-1);
            const int ix=std::min(n-2,int(gx)),iz=std::min(n-2,int(gz));
            const float tx=float(gx-ix),tz=float(gz-iz);
            auto sample=[&](const std::vector<float>&a){const auto i=iz*n+ix;
                return (a[i]*(1-tx)+a[i+1]*tx)*(1-tz)+(a[i+n]*(1-tx)+a[i+n+1]*tx)*tz;};
            const float sky=sample(access);minimum_access=std::min(minimum_access,sky);sum+=sky;
            const std::size_t p=(std::size_t(z)*output_side+x)*4;
            out[p]=static_cast<unsigned char>(std::lround(sky*255));
            out[p+1]=static_cast<unsigned char>(std::lround(std::clamp((sample(ground)-height_min)/height_range,0.f,1.f)*255));
            out[p+2]=static_cast<unsigned char>(std::lround(std::clamp((sample(top)-height_min)/height_range,0.f,1.f)*255));
            out[p+3]=255;
        }
        mean_access=float(sum/(output_side*output_side));return true;
    }
    template<class Height> bool build_with_arrival(const WorldScenery *scene,double cx,double cz,
            bool has_fallback,double fx,double fz,int output_side,
            std::vector<unsigned char> &out,Height height) {
        // A declared, populated arrival woodland is the project's first
        // region even if a remote legacy jungle later acquires crowns.
        // Projects without one retain the original rainforest field/codec.
        // This follows startup/editor invalidation, never a moving camera.
        if(has_fallback) {
            WorldForestShelter next;std::vector<unsigned char> pixels;
            if(!next.build(scene,fx,fz,output_side,pixels,height,true))return false;
            if(next.trees) {
                next.arrival_region=true;*this=next;out.swap(pixels);return true;
            }
        }
        return build(scene,cx,cz,output_side,out,height);
    }
};
#endif
