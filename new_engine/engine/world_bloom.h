#ifndef VALWOOD_WORLD_BLOOM_H
#define VALWOOD_WORLD_BLOOM_H

#include <cstdint>

struct WorldBloom;
struct WorldBloomStats {
    int width=0,height=0,quarter_width=0,quarter_height=0;
    std::uint64_t bytes=0,allocations=0,resolves=0,failures=0;
};

/* GL3.3 HDR highlight bloom. Two ceil(width/4) x ceil(height/4) RGBA16F
 * images only: one exposure-aware 13-sample extraction/downsample, followed
 * by a normalized nine-tap horizontal and vertical Gaussian. The result is
 * scene-linear radiance, without exposure, tone mapping or gamma applied.
 * allocations counts successful replacement pairs; bytes is live storage.
 */
WorldBloom *world_bloom_create(const char *vertex,const char *extract,const char *blur);
/* Optional loading-time preparation. A failed resize preserves the previous
 * complete allocation. Stable quarter dimensions perform no allocation/query.
 * Returns false for invalid dimensions or failure; no stale result is valid.
 * Preparation may change framebuffer and texture-unit-0 bindings. */
bool world_bloom_prepare(WorldBloom *,int width,int height);
/* Input is a separate, same-size, single-sample scene-linear HDR color image.
 * Returns zero on invalid input or failed preparation; never samples an owned
 * bloom image as input. Exposed highlight threshold=1.25 and soft knee=.5.
 * RGB is sanitized/clamped to prevent non-finite or excessive fireflies.
 * No per-frame allocations, state queries, CPU readback or full-res target.
 * Leaves its result FBO/program/VAO bound, texture unit 0 active, the viewport
 * at quarter dimensions, and depth/blend/cull/scissor/depth writes disabled.
 */
unsigned world_bloom_resolve(WorldBloom *,unsigned hdr_color,int width,int height,float exposure);
WorldBloomStats world_bloom_stats(const WorldBloom *);
void world_bloom_free(WorldBloom *);

#endif
