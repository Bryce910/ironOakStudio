#ifndef VALWOOD_WORLD_TEMPORAL_H
#define VALWOOD_WORLD_TEMPORAL_H

#include <cstdint>

/* OpenGL 3.3, linear HDR temporal resolve. All GL calls require the owning
 * context. One resolve pass uses two ping-pong RGBA16F/R32F history sets
 * (24 bytes/pixel total, 32.96 MiB at 1600x900). No motion-vector attachment.
 * The caller owns the input textures; the module owns its returned texture.
 */
struct WorldTemporal;

struct WorldTemporalFrame {
    unsigned int color = 0, depth = 0;
    int width = 0, height = 0;
    /* Column-major OpenGL projection * rotation-only view, including the
     * exact jitter used to rasterize both the scene color and depth. */
    float view_projection[16] = {};
    float jitter_pixels[2] = {};
    double camera[3] = {};
    float near_plane = .4f, far_plane = 15000.f;
    /* Base opaque/moving history limit. Locally supported, nearly static
     * vegetation/explicit rigid-edge coverage can retain
     * min(.96, history_weight+.06), weighted
     * by its reactive confidence and subpixel reprojection movement. */
    float history_weight = .90f;
    bool reset = false;
    /* Optional scene alpha = confidence/source class:1 ordinary opaque/sky,
     * .995 transform-static rigid assets, .4-.99 vegetation(wind-aware
     * .45-.98), <.4 ordinary actor/water masks. The rigid reserved band
     * (.993,.997) requires zero wind/actor flag and full placement opacity.
     * Explicit non-sky classes enable local depth-supported edge coverage.
     * Alpha is NOT transparency. Leave false until this contract is followed.
     * Without object motion vectors animated pixels retain less history;
     * this is deliberately conservative rather than exact wind reprojection.
     */
    bool reactive_alpha = false;
    /* Optional RGBA16F per-object motion: rgb = previous-NDC offset of an
     * actor surface relative to static reprojection, a = 1 where valid,
     * cleared to 0 elsewhere. Same size as color. 0 = none. */
    unsigned int object_motion = 0;
};

WorldTemporal *world_temporal_create(const char *vertex_shader, const char *fragment_shader);
/* Resize, >80 m/frame camera travel, >45-degree/frame view turns or changed
 * clipping planes invalidate history automatically. Previous projections
 * outside the half-texel image bounds or behind the previous camera reject
 * history per pixel. Call reset explicitly
 * on teleport, lighting discontinuity or any changed scene with held frames.
 * Call resolve ONLY after rendering a fresh color/depth pair. During loading
 * holds display world_temporal_output() without resolving/replacing matrices.
 * Failure returns frame.color; output() then also returns the caller fallback.
 * Leaves its FBO/program/VAO bound, depth/blend/cull/scissor disabled, color
 * mask enabled, depth writes disabled and viewport at input dimensions.
 * Owns two loading-created GL3.3 sampler objects: current/history color on
 * units0/2 always reconstruct linearly with edge clamping; depth on units1/3
 * stays point filtered. Input texture filters are never changed. A non-null
 * resolve leaves sampler0 bound on all four units on success or failure,
 * matching the scene renderer's ordinary texture-object sampling contract.
 * No per-frame sampler allocation, state query, readback or manual four-tap
 * color reconstruction is required; teardown deletes both owned samplers.
 * Re-establish post/UI state normally; no per-frame GL state readbacks occur.
 * At thin geometry edges, history uses matching-depth taps plus local source
 * coverage. An alternate layer is admitted only when current3x3 depth
 * support still contains it, one side is explicitly tagged vegetation/rigid,
 * and reprojection
 * moves less than 1.5 pixels. Unsupported old layers are rejected immediately;
 * opaque disocclusion stays strict and moving leaf confidence remains bounded.
 * Current depth is reconstructed onto the same unjittered grid as color using
 * one-sided screen-linear surface slopes. Silhouette/isolated-leaf depth is
 * never averaged across unrelated layers. This prevents phase-dependent
 * history rejection on oblique walls, roofs and terrain.
 * Only depth-supported tagged edges widen the variance-clipping box. Fully
 * supported rigid texture interiors decode their ordinary confidence to1;
 * untagged opaque/terrain, actors, clouds and unsupported old layers retain
 * strict clipping/depth rejection. Clear-sky pixels next to a tagged non-sky
 * surface share its area-coverage confidence.
 */
unsigned int world_temporal_resolve(WorldTemporal *, const WorldTemporalFrame &);
unsigned int world_temporal_output(const WorldTemporal *, unsigned int fallback);
/* Matching unjittered normalized OpenGL depth, stored as R32F. Bind this
 * alongside output() for the existing depth-based postprocess/contact AO. */
unsigned int world_temporal_depth(const WorldTemporal *, unsigned int fallback);
void world_temporal_reset(WorldTemporal *);
void world_temporal_free(WorldTemporal *);

/* Eight bounded Halton(2,3) offsets, in PIXELS, each within [-.5,.5]. For
 * this engine's perspective matrix: P[8] -= 2*jitter[0]/width;
 * P[9] -= 2*jitter[1]/height. Sky ray NDC must subtract the same jitterNDC.
 * Do not jitter shadow/reflection matrices, and do not advance on held views.
 */
void world_temporal_jitter(std::uint64_t sample, float jitter_pixels[2]);

/* Pure CPU helper shared by resolve and tests. Maps current clip coordinates
 * to previous clip coordinates. Origins are subtracted in double precision
 * before converting to the camera-relative float transform. Does not need GL.
 * Inputs/outputs may alias. Returns false for non-finite or singular matrices.
 */
bool world_temporal_reprojection(const float current_vp[16], const float previous_vp[16],
                                 const double current_origin[3], const double previous_origin[3],
                                 float reprojection[16]);

#endif
