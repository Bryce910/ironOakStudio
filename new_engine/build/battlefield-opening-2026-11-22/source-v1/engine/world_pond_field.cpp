#include "engine/world_pond_field.h"
#include "engine/world_surface_field.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace world_pond_field {
namespace {
struct Ground {const world_studio::Document *document;world_terrain_authoring::Source source;};
float ground(double x,double z,void *opaque) {
    const auto &g=*static_cast<Ground *>(opaque);
    const float base=g.source.base(x,z);
    return std::isfinite(base)?base+world_studio::height_delta(*g.document,x,z):std::numeric_limits<float>::quiet_NaN();
}
const world_studio::TerrainTile *original_tile(const world_studio::Document &d,int x,int z) {
    for(const auto &t:d.terrain)if(t.x==x&&t.z==z)return &t;
    return nullptr;
}
world_surface::TerrainTile *field_tile(world_surface::TerrainField &field,int x,int z) {
    for(auto &t:field.tiles)if(t.x==x&&t.z==z)return &t;
    return nullptr;
}
void rebuild_lookup(world_surface::TerrainField &field) {
    for(auto &key:field.lookup)key={0,0,-1,0};
    for(unsigned i=0;i<field.tiles.size();++i) {
        const auto &t=field.tiles[i];unsigned at=world_surface::paint_hash(t.x,t.z);
        while(field.lookup[at][2]>=0)at=(at+1)&(world_surface::paint_hash_size-1);
        field.lookup[at]={t.x,t.z,int(i),0};
    }
}
double height(const world_studio::Document &d,world_terrain_authoring::Source source,double x,double z) {
    return double(source.base(x,z))+world_studio::height_delta(d,x,z)+
        (d.surface_field?world_surface::terrain_offset(*d.surface_field,x,z):0);
}
bool enclosed(const world_studio::Document &d,const world_studio::Water &w,
              world_terrain_authoring::Source source,std::string &error) {
    const double center=height(d,source,w.x,w.z);
    if(!std::isfinite(center)||w.level-center<.08) {
        error="Editable pond has no submerged center; choose flatter ground or adjust its depth";return false;
    }
    for(unsigned i=0;i<2048;++i) {
        const double a=i*6.283185307179586/2048;
        const double h=height(d,source,w.x+std::cos(a)*w.radius_x,w.z+std::sin(a)*w.radius_z);
        if(!std::isfinite(h)||h<w.level+.035) {
            error="Editable pond cannot form a closed dry bank here; choose flatter ground or a smaller basin";return false;
        }
    }
    return true;
}
}
bool prepare(world_studio::Document &d,world_terrain_authoring::Source source,std::string &error) {
    error.clear();
    if(!source.base){error="Editable pond preparation needs the unedited ground sampler";return false;}
    if(!world_studio::valid(d,error))return false;
    auto next=d,working=d;
    // Entity bindings refer to the published water indices. The temporary
    // replay list has different indices and must never rewrite that graph.
    working.entities={};working.surface_field.reset();
    working.waters.erase(std::remove_if(working.waters.begin(),working.waters.end(),
        [](const auto &w){return w.basin_depth>0;}),working.waters.end());
    for(unsigned i=0;i<d.waters.size();++i) {
        const auto &w=d.waters[i];if(w.basin_depth<=0||!w.enabled)continue;
        if(w.source!=3||std::abs(w.radius_x-w.radius_z)>1.e-6) {
            error="Editable pond basins require custom circular water";return false;
        }
        world_terrain_authoring::Report report;
        if(!world_terrain_authoring::simple_pond(working,w.x,w.z,w.radius_x,w.basin_depth,source,report,error))return false;
        // Keep all original style, identity, geometry and enable settings.
        next.waters[i].level=working.waters.back().level;
    }
    auto field=std::make_shared<world_surface::TerrainField>();
    Ground g{&working,source};
    if(!world_surface::bake_terrain(working.surfaces,ground,&g,*field,error))return false;
    // Convert only the replay's sculpt difference into a derived field. This
    // preserves the original sculpt exactly when a pond moves or is deleted.
    for(const auto &tile:working.terrain) {
        const auto *old=original_tile(d,tile.x,tile.z);bool changed=false;
        for(unsigned j=0;j<tile.delta.size();++j)
            if(tile.delta[j]!=(old?old->delta[j]:0.f)){changed=true;break;}
        if(!changed)continue;
        auto *target=field_tile(*field,tile.x,tile.z);
        if(!target) {
            if(field->tiles.size()>=world_surface::paint_tile_limit) {
                error="Ponds and road/river grades exceed64 local derived terrain tiles";return false;
            }
            field->tiles.push_back({});target=&field->tiles.back();target->x=tile.x;target->z=tile.z;
        }
        for(unsigned j=0;j<tile.delta.size();++j)
            target->delta[j]+=tile.delta[j]-(old?old->delta[j]:0.f);
    }
    rebuild_lookup(*field);
    if(!world_terrain_authoring::combined_height_valid(d,*field,error))return false;
    next.surface_field=field->tiles.empty()&&field->ribbons.empty()?nullptr:std::move(field);
    for(const auto &w:next.waters)
        if(w.basin_depth>0&&w.enabled&&!enclosed(next,w,source,error))return false;
    // Commit only derived outputs after all candidates and rims have passed.
    d.surface_field=std::move(next.surface_field);
    for(unsigned i=0;i<d.waters.size();++i)d.waters[i].level=next.waters[i].level;
    return true;
}
bool create(world_studio::Document &d,double x,double z,float radius,float depth,
            world_terrain_authoring::Source source,world_terrain_authoring::Report &out,
            std::string &error) {
    auto carved=d;world_terrain_authoring::Report report;
    if(!world_terrain_authoring::simple_pond(carved,x,z,radius,depth,source,report,error))return false;
    auto next=d;auto water=carved.waters.back();water.basin_depth=depth;next.waters.push_back(water);
    if(!prepare(next,source,error))return false;
    d=std::move(next);out=report;return true;
}
}
