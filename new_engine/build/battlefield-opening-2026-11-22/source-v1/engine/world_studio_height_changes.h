#ifndef VALWOOD_WORLD_STUDIO_HEIGHT_CHANGES_H
#define VALWOOD_WORLD_STUDIO_HEIGHT_CHANGES_H

#include "engine/world_studio.h"
#include "engine/world_surface_field.h"
#include <algorithm>
#include <limits>
#include <vector>

// CPU-only conservative bounds for the actual height samples in two prepared
// documents. Call after preparing the new derived road/river terrain field.
// A changed 2m-grid node can affect the cells on either side, including copied
// tile edges. These rectangles include that interpolation support; clients
// sampling height derivatives must add their own normal-stencil margin.
namespace world_studio_height_changes {
struct Rect {double xmin=0,zmin=0,xmax=0,zmax=0;};

namespace detail {
template<class Tile>
const Tile *find(const std::vector<Tile> &tiles,int x,int z) {
    for(const auto &tile:tiles)if(tile.x==x&&tile.z==z)return &tile;
    return nullptr;
}

template<class Tile>
void compare_tile(const Tile *before,const Tile *after,std::vector<Rect> &out) {
    const auto &tile=after?*after:*before;
    unsigned xmin=65,zmin=65,xmax=0,zmax=0;
    bool any=false;
    for(unsigned z=0;z<65;++z)for(unsigned x=0;x<65;++x) {
        const unsigned at=z*65+x;
        const float old_value=before?before->delta[at]:0.f;
        const float new_value=after?after->delta[at]:0.f;
        if(old_value==new_value)continue;
        any=true;xmin=std::min(xmin,x);zmin=std::min(zmin,z);
        xmax=std::max(xmax,x);zmax=std::max(zmax,z);
    }
    if(any)out.push_back({tile.x*128.+xmin*2.-2.,tile.z*128.+zmin*2.-2.,
                          tile.x*128.+xmax*2.+2.,tile.z*128.+zmax*2.+2.});
}

template<class Tile>
void compare(const std::vector<Tile> &before,const std::vector<Tile> &after,
             std::vector<Rect> &out) {
    // Coordinates, rather than vector order, identify persisted terrain tiles.
    for(const auto &tile:before)compare_tile(&tile,find(after,tile.x,tile.z),out);
    for(const auto &tile:after)if(!find(before,tile.x,tile.z))
        compare_tile<Tile>(nullptr,&tile,out);
}
} // namespace detail

inline std::vector<Rect> changed(const world_studio::Document &before,
                                 const world_studio::Document &after) {
    std::vector<Rect> out;
    detail::compare(before.terrain,after.terrain,out);
    if(before.surface_field!=after.surface_field) {
        static const std::vector<world_surface::TerrainTile> empty;
        detail::compare(before.surface_field?before.surface_field->tiles:empty,
                        after.surface_field?after.surface_field->tiles:empty,out);
    }
    return out;
}
} // namespace world_studio_height_changes
#endif
