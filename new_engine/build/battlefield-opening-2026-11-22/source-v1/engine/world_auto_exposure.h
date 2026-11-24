#ifndef VALWOOD_WORLD_AUTO_EXPOSURE_H
#define VALWOOD_WORLD_AUTO_EXPOSURE_H

#include <cstdint>

/* GL3.3 eye adaptation. Each frame the scene-linear HDR image (before
 * exposure) is metered into a 64x64 centre-weighted log-luminance image,
 * reduced by mipmapping to 1x1 and read back through a ring of pixel-buffer
 * objects with fences, so the CPU never waits on the GPU (a few frames of
 * latency). The adapted value is a multiplier on the authored exposure:
 * views metering inside the neutral band keep their authored exposure; darker
 * or brighter views are partly compensated within a bounded EV range.
 *
 * Environment overrides (all optional):
 *   VALWOOD_AUTO_EXPOSURE=0            disable (multiplier stays 1)
 *   VALWOOD_AUTO_EXPOSURE_KEY_LOW / _KEY_HIGH=<log2>  neutral metering band (1x)
 *   VALWOOD_AUTO_EXPOSURE_STRENGTH=<0..1> share of the difference compensated
 *   VALWOOD_AUTO_EXPOSURE_MIN_EV / _MAX_EV  limits of the adapted offset
 */
struct WorldAutoExposure;
struct WorldAutoExposureStats {
    bool enabled=false;
    float measured_log2=0,target_ev=0,adapted_ev=0,min_measured=0,max_measured=0;
    std::uint64_t meters=0,readbacks=0,skipped=0;
};
WorldAutoExposure *world_auto_exposure_create(const char *vertex,const char *meter);
/* Returns the current exposure multiplier (>0). dt is in seconds. Leaves
 * texture unit 0 active and the framebuffer, viewport and program changed,
 * like world_bloom_resolve. Returns 1 when disabled or before a first reading. */
float world_auto_exposure_update(WorldAutoExposure *,unsigned hdr_color,int width,int height,float dt);
WorldAutoExposureStats world_auto_exposure_stats(const WorldAutoExposure *);
void world_auto_exposure_free(WorldAutoExposure *);

#endif
