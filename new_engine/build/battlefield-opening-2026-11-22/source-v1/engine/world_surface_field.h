#ifndef VALWOOD_WORLD_SURFACE_FIELD_H
#define VALWOOD_WORLD_SURFACE_FIELD_H
#include "engine/world_surface_authoring.h"
#include "engine/world_river_network.h"
#include <memory>
#include <utility>
namespace world_surface {
using Ground=float(*)(double x,double z,void *);
struct TerrainTile {int x=0,z=0;std::array<float,paint_side*paint_side> delta={},road={};std::array<int,16> ribbons={};unsigned count=0;};
struct TerrainField {std::vector<TerrainTile> tiles;std::array<std::array<int,4>,paint_hash_size> lookup={};std::vector<BakedRibbon> ribbons;std::shared_ptr<const RiverNetwork> river_network;};
bool same_paths(const Data &,const Data &);
// Bake offsets relative to the caller's already sculpted ground. Separate
// derived fields make moving/removing a spline restore its previous ground.
// The caller performs foundation/route safety checks before committing.
bool bake_terrain(const Data &,Ground,void *,TerrainField &,std::string &error);
// Re-evaluate only derived tiles touched by a sculpt when the ribbon network
// is unchanged. Adjacent tiles are included for shared grid-edge samples.
bool rebake_sculpt(const TerrainField &,Ground,void *,const std::vector<std::pair<int,int>> &changed,
                   TerrainField &,std::string &error);
float terrain_offset(const TerrainField &,double x,double z);
// Cached ribbon coverage: constant-time terrain-grid lookup for vegetation.
float road_mask(const TerrainField &,double x,double z);
bool river(const TerrainField &,double x,double z,float &level,float &shore);
}
#endif
