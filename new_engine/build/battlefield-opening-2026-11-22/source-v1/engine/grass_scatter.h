#ifndef VALWOOD_GRASS_SCATTER_H
#define VALWOOD_GRASS_SCATTER_H

#define GRASS_SCATTER_STEP 0.25f
#define GRASS_SCATTER_CELL_MAX 16

typedef struct {
    float x, z;
    float length_multiplier, width_multiplier;
    float yaw, phase, variation;
} GrassScatterSeed;

/* World-anchored seeds never depend on traversal order or the frame clock. */
float grass_scatter_random(int x, int z, unsigned salt);
float grass_scatter_cover(float x, float z);
void grass_scatter_center(int x, int z, float *world_x, float *world_z);
/* Split count from seed emission so terrain/water/prop rejection can happen
   before expensive per-blade trigonometry. Pass the center computed above. */
int grass_scatter_count_at(int x, int z, float world_x, float world_z,
                           float mask, float density, float distance);
void grass_scatter_emit_at(int x, int z, float world_x, float world_z, int count,
                           GrassScatterSeed out[GRASS_SCATTER_CELL_MAX]);
int grass_scatter_cell(int x, int z, float mask, float density, float distance,
                       GrassScatterSeed out[GRASS_SCATTER_CELL_MAX]);
#endif
