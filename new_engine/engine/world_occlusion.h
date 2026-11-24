#ifndef VALWOOD_WORLD_OCCLUSION_H
#define VALWOOD_WORLD_OCCLUSION_H
#include <cstdint>
struct WorldOcclusion;
struct WorldOcclusionFrame {
    unsigned color=0,depth=0,indirect=0;
    int width=0,height=0;
    float near_plane=.4f,far_plane=15000.f,tan_half_fov=.5994296f;
    float jitter_ndc[2]={},sun_view[3]={0,1,0};
    float ambient_strength=.90f,contact_strength=.70f;
};
struct WorldOcclusionStats {
    int width=0,height=0,half_width=0,half_height=0;
    std::uint64_t bytes=0,resizes=0,resolves=0;
};
/* GL3.3, half-resolution bounded16-tap ambient obscurance +8-step short sun
 * contact rays, edge-aware separable filtering and HDR composition BEFORE TAA.
 * The caller provides a separate indirect-radiance attachment, so ambient AO
 * does not darken direct sun or aerial fog. Contact visibility only attenuates
 * the remaining direct component. This is screen-space lighting, not GI.
 * Owns two half-resRGBA16F fields and one full-resRGBA16F result (~16.48MiB
 * at1600x900). No per-frame allocation/state readbacks; resize is bounded.
 */
WorldOcclusion *world_occlusion_create(const char *vertex,const char *field,const char *blur,const char *compose);
/* Returns frame.color on invalid input/failed resize; never reads and writes
 * the same attachment. Leaves its resultFBO/program/VAO bound, depth writes
 * disabled and depth/blend/cull/scissor disabled, viewport at full dimensions.
 * Input alpha is temporal stability and passes through unchanged. */
unsigned world_occlusion_resolve(WorldOcclusion *,const WorldOcclusionFrame &);
WorldOcclusionStats world_occlusion_stats(const WorldOcclusion *);
unsigned world_occlusion_field(const WorldOcclusion *);
void world_occlusion_free(WorldOcclusion *);
#endif
