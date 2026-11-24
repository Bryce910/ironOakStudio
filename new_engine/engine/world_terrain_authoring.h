#ifndef VALWOOD_WORLD_TERRAIN_AUTHORING_H
#define VALWOOD_WORLD_TERRAIN_AUTHORING_H
#include "engine/world_studio.h"
#include <cstdint>
namespace world_terrain_authoring {
// All algorithms run on explicit authoring operations, never terrain workers.
// Regions use the existing global 2m sample lattice and 16x128m tile budget.
struct Region {double x=0,z=0;unsigned columns=65,rows=65;};
struct Source {float(*base)(double,double)=nullptr;
    bool(*protected_point)(double,double,const void *)=nullptr;const void *context=nullptr;};
struct Report {unsigned changed_nodes=0,clamped_nodes=0,added_tiles=0,added_masks=0;
    double largest_change=0,height_sum_before=0,height_sum_after=0;};
struct Heightmap {Region region;float minimum=-64,maximum=256;std::vector<std::uint16_t> samples;};
struct Import {float strength=1,border=8;};
struct Erosion {Region region;unsigned iterations=32;std::uint32_t seed=1;
    float rain=.025f,evaporation=.18f,capacity=3,erosion=.2f,deposition=.25f;
    float thermal=.12f,talus=.65f,border=8;};
struct Biome {double x=0,z=0;float radius=12,density=0;
    int layer=0;unsigned divisions=3;float minimum_height=-2000,maximum_height=12000;
    float minimum_slope=0,maximum_slope=90;};
enum class CorridorKind {Road,River};
struct Point {double x=0,z=0,height=0;};
struct Corridor {std::vector<Point> points;CorridorKind kind=CorridorKind::Road;
    float half_width=3,bank_width=6,depth=1,maximum_grade=.35f;
    // Bits 0..3 suppress grass, trees, rocks and ferns. Zero leaves masks alone.
    unsigned vegetation_layers=3;float vegetation_density=0;};
bool sample_heightmap(const world_studio::Document &,const Region &,float minimum,float maximum,
                      Source,Heightmap &,std::string &error);
// Portable PGM P5, 65535 maxval, big-endian samples. A VALWOOD_HEIGHTMAP comment
// stores version, region origin and metre range; dimensions imply 2m spacing.
// Reader accepts this documented metadata-bearing format, not arbitrary PGM.
bool save_heightmap(const char *path,const Heightmap &,std::string &error);
bool load_heightmap(const char *path,Heightmap &,std::string &error);
bool import_heightmap(world_studio::Document &,const Heightmap &,const Import &,Source,Report &,std::string &error);
// Deterministic grid hydraulic transport/deposition plus conservative thermal
// relaxation. Fixed border blends back into the original shared tile edges.
bool erode(world_studio::Document &,const Erosion &,Source,Report &,std::string &error);
// Emits real existing Studio foliage stamps. Each accepted circle is checked
// conservatively against height/slope constraints over its 2m footprint. This
// bounded brush is not a new unpersisted biome raster or a full biome editor.
bool paint_biome(world_studio::Document &,const Biome &,Source,Report &,std::string &error);
// Roads grade to the polyline; rivers carve a parabolic channel below it.
// River control heights must descend. Banks blend into existing ground. These
// are terrain/mask tools; water meshes and road materials remain separate.
bool sculpt_corridor(world_studio::Document &,const Corridor &,Source,Report &,std::string &error);
}
#endif
