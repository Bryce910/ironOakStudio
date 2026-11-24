#ifndef VALWOOD_WORLD_POND_FIELD_H
#define VALWOOD_WORLD_POND_FIELD_H
#include "engine/world_terrain_authoring.h"

// CPU-only, atomic preparation of editable pond basins and road/river grades.
// Water::basin_depth>0 opts a custom circular pond into derived terrain;
// legacy zero-depth water retains its existing baked sculpt unchanged.
namespace world_pond_field {
// Rebuild from doc.terrain, never from an earlier derived field. Success only
// replaces doc.surface_field and generated water levels; no terrain, entity,
// material, style, or path data is rewritten. Disabled derived ponds do not
// carve. A failed preparation leaves the entire input document unchanged.
// Source's protection callback must represent static objects/routes, excluding
// custom authored water: this module validates water overlap during replay.
bool prepare(world_studio::Document &,world_terrain_authoring::Source,std::string &error);
// Validate a new pond with the existing carving tool, then persist its basin
// recipe while restoring the original sculpt. One success is one transaction.
bool create(world_studio::Document &,double x,double z,float radius,float depth,
            world_terrain_authoring::Source,world_terrain_authoring::Report &,
            std::string &error);
}
#endif
