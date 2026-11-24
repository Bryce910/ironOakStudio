#ifndef VALWOOD_WORLD_VILLAGE_SKY_H
#define VALWOOD_WORLD_VILLAGE_SKY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

struct WorldVillageSkyConfig {
    std::size_t columns=513,rows=513;
    double cell_width=1.,cell_depth=1.;
    float reference_y=25.75f;
};

/* Startup-only, pure CPU sky-access approximation. Roof heights are row-major
 * world Y; the caller normally initializes them to the village ground24m,
 * then rasterizes grounded building roofs over that field. World origin is
 * irrelevant: set cell_width=worldWidth/(columns-1), likewise depth.
 *
 * Eight azimuths sample1,2,3,5,8,13,21,34,55 cells. Each azimuth's maximum
 * nonnegative horizon slope gives cosine-weighted visible sky1/(1+slope^2).
 * Average all eight, clamp to[.30,1], and round to UNORM8. Off-grid samples
 * contain no additional obstruction. This is bounded roof occlusion, not a
 * substitute for direct-light shadows or full hemispherical ray tracing.
 *
 * Requires exactly columns*rows finite heights, dimensions2..4097, positive
 * finite cell spacing and finite referenceY. Invalid input or allocation
 * failure returns false and leaves output unchanged. No GL/atlas dependency.
 */
inline bool world_village_sky_build(const float *roof_heights,std::size_t height_count,
                                  const WorldVillageSkyConfig &config,
                                  std::vector<std::uint8_t> &output) {
    if(!roof_heights||config.columns<2||config.rows<2||config.columns>4097||config.rows>4097||
       !std::isfinite(config.cell_width)||!std::isfinite(config.cell_depth)||
       config.cell_width<=0||config.cell_depth<=0||!std::isfinite(config.reference_y))return false;
    const std::size_t count=config.columns*config.rows;
    if(height_count!=count)return false;
    for(std::size_t k=0;k<count;k++)if(!std::isfinite(roof_heights[k]))return false;

    constexpr int strides[]={1,2,3,5,8,13,21,34,55};
    constexpr int directions[][2]={{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    struct Sample {int x,z;double inverse_distance;};
    std::array<std::array<Sample,9>,8> rays;
    for(std::size_t ray=0;ray<rays.size();ray++)for(std::size_t k=0;k<rays[ray].size();k++) {
        const int x=directions[ray][0]*strides[k],z=directions[ray][1]*strides[k];
        const double distance=std::hypot(double(x)*config.cell_width,double(z)*config.cell_depth);
        const double inverse=1./distance;
        if(!std::isfinite(distance)||distance<=0||!std::isfinite(inverse))return false;
        rays[ray][k]={x,z,inverse};
    }
    std::vector<std::uint8_t> next;
    try {next.resize(count);}catch(const std::bad_alloc &){return false;}
    const int columns=int(config.columns),rows=int(config.rows);
    const double reference=config.reference_y;
    for(int z=0;z<rows;z++)for(int x=0;x<columns;x++) {
        double visible=0;
        for(const auto &ray:rays) {
            double horizon=0;
            for(const auto &sample:ray) {
                const int sx=x+sample.x,sz=z+sample.z;
                if(sx<0||sz<0||sx>=columns||sz>=rows)continue;
                const double roof=roof_heights[std::size_t(sz)*config.columns+std::size_t(sx)];
                horizon=std::max(horizon,(roof-reference)*sample.inverse_distance);
            }
            visible+=1./(1.+horizon*horizon);
        }
        const double access=std::clamp(visible/8.,.30,1.);
        next[std::size_t(z)*config.columns+std::size_t(x)]=std::uint8_t(std::lround(access*255.));
    }
    output.swap(next);return true;
}

#endif
