#ifndef VALWOOD_WORLD_SCENERY_PIVOT_H
#define VALWOOD_WORLD_SCENERY_PIVOT_H
#include <cstddef>
struct WorldScenery;
// Provenance of the same record index accepted by world_scenery_placement.
// Explicit imported pivots remain absolute when unrelated terrain is edited.
// Null scenes and out-of-range indices return false; no scene data is changed.
bool world_scenery_explicit_pivot(const WorldScenery *,std::size_t);
#endif
