#include "engine/grass_scatter.h"
#include <math.h>
#include <stdint.h>

static float saturate(float x) {
    return fminf(fmaxf(x, 0.0f), 1.0f);
}

float grass_scatter_random(int x, int z, unsigned salt) {
    uint32_t h = (uint32_t)x * 0x1f123bb5u ^ (uint32_t)z * 0x5f356495u
               ^ salt * 0x6c8e9cf5u;
    h ^= h >> 16; h *= 0x7feb352du;
    h ^= h >> 15; h *= 0x846ca68bu; h ^= h >> 16;
    return (float)(h & 0xffffffu) / 16777216.0f;
}

static float field_noise(float x, float z) {
    int ix = (int)floorf(x), iz = (int)floorf(z);
    float u = x - (float)ix, v = z - (float)iz;
    u = u*u*(3.0f - 2.0f*u); v = v*v*(3.0f - 2.0f*v);
    float a = grass_scatter_random(ix, iz, 977);
    float b = grass_scatter_random(ix+1, iz, 977);
    float c = grass_scatter_random(ix, iz+1, 977);
    float d = grass_scatter_random(ix+1, iz+1, 977);
    return (a+(b-a)*u)*(1.0f-v) + (c+(d-c)*u)*v;
}

float grass_scatter_cover(float x, float z) {
    float broad = field_noise(x*0.19f, z*0.19f);
    float small = field_noise(x*0.87f+8.3f, z*0.87f-6.1f);
    return saturate((broad*0.67f + small*0.33f - 0.18f) / 0.65f);
}

void grass_scatter_center(int x, int z, float *world_x, float *world_z) {
    *world_x = ((float)x + grass_scatter_random(x,z,11)*0.94f) * GRASS_SCATTER_STEP;
    *world_z = ((float)z + grass_scatter_random(x,z,29)*0.94f) * GRASS_SCATTER_STEP;
}

int grass_scatter_count_at(int x, int z, float wx, float wz,
                           float mask, float density, float distance) {
    if (!isfinite(mask) || !isfinite(density) || !isfinite(distance)
        || !isfinite(wx) || !isfinite(wz)) return 0;
    mask = saturate((mask - 0.035f) / 0.965f);
    if (mask <= 0.0f || density <= 0.0f) return 0;
    float cover = grass_scatter_cover(wx,wz);
    float d = fmaxf(distance-4.0f, 0.0f) / 10.0f;
    float lod = 1.0f / (1.0f+d*d*d*d);
    float target = mask * fminf(density,3.0f) * (0.28f+0.72f*cover)
                  * (0.08f+17.0f*lod);
    target = fminf(target,(float)GRASS_SCATTER_CELL_MAX);
    int count = (int)floorf(target);
    if (count < GRASS_SCATTER_CELL_MAX && grass_scatter_random(x,z,47)<target-count) count++;
    return count;
}

void grass_scatter_emit_at(int x, int z, float wx, float wz, int count,
                           GrassScatterSeed out[GRASS_SCATTER_CELL_MAX]) {
    if (count <= 0 || !out || !isfinite(wx) || !isfinite(wz)) return;
    if (count > GRASS_SCATTER_CELL_MAX) count = GRASS_SCATTER_CELL_MAX;
    float cluster = grass_scatter_random(x,z,83);
    float angle = grass_scatter_random(x,z,131)*6.283185307f;
    for (int i=0; i<count; i++) {
        GrassScatterSeed *seed = &out[i];
        float a = angle + (float)i*2.39996323f + (grass_scatter_random(x,z,401u+i)-0.5f)*0.65f;
        float radius = 0.018f + sqrtf(grass_scatter_random(x,z,419u+i))*0.16f;
        seed->x = wx + cosf(a)*radius;
        seed->z = wz + sinf(a)*radius;
        seed->length_multiplier = (0.66f+cluster*0.56f)*(0.68f+grass_scatter_random(x,z,61u+i)*0.48f);
        seed->width_multiplier = 0.72f+grass_scatter_random(x,z,79u+i)*0.52f;
        seed->yaw = a+(grass_scatter_random(x,z,97u+i)-0.5f)*0.8f;
        seed->phase = grass_scatter_random(x,z,173u+i)*6.283185307f;
        seed->variation = (cluster*0.68f+grass_scatter_random(x,z,211u+i)*0.32f)*2.0f-1.0f;
    }
}

int grass_scatter_cell(int x, int z, float mask, float density, float distance,
                       GrassScatterSeed out[GRASS_SCATTER_CELL_MAX]) {
    float wx, wz;
    grass_scatter_center(x, z, &wx, &wz);
    int count = grass_scatter_count_at(x, z, wx, wz, mask, density, distance);
    grass_scatter_emit_at(x, z, wx, wz, count, out);
    return count;
}
