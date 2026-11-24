#ifndef VALWOOD_WORLD_WATER_SCENE_H
#define VALWOOD_WORLD_WATER_SCENE_H

#include <cstdint>

/* Read-only opaque HDR/depth snapshot for water. Copy AFTER terrain and BEFORE
 * drawing water into the source. Sampling the source's attached depth/color
 * while writing it would be a framebuffer feedback loop. This module owns a
 * separate RGBA16F/DEPTH_COMPONENT24 pair, 12 bytes/pixel (16.48MiB at1600x900).
 * It is allocated lazily only where shallow-water rendering is useful.
 */
struct WorldWaterScene;
WorldWaterScene *world_water_scene_create();
/* Source must be a complete same-size, single-sample RGBA16F/DEPTH24 FBO.
 * Returns false on invalid size/source or allocation failure; prior allocation
 * remains owned but must not be sampled as the current view. Leaves source
 * bound for read/draw, scissor disabled and texture unit0 active. No CPU readback, GL state query,
 * shader pass or per-frame heap allocation on the stable-size path.
 */
bool world_water_scene_capture(WorldWaterScene *, unsigned source, int width, int height);
unsigned world_water_scene_color(const WorldWaterScene *);
unsigned world_water_scene_depth(const WorldWaterScene *);
std::uint64_t world_water_scene_bytes(const WorldWaterScene *);
void world_water_scene_free(WorldWaterScene *);

#endif
