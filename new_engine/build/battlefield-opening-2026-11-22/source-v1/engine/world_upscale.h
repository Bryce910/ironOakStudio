#ifndef VALWOOD_WORLD_UPSCALE_H
#define VALWOOD_WORLD_UPSCALE_H
/* AAA merge: temporal upscaler (TAAU). Runs after the render-resolution
 * temporal resolve. Each display pixel accumulates the raw jittered render
 * samples that land near it into a display-resolution history, reprojected
 * with the resolved depth (+ optional per-object motion), and clamped to the
 * neighbourhood of the stable render-resolution image. When history is
 * invalid it returns the Catmull-Rom upsample of the stable image, so it can
 * never look worse than the previous spatial upscale. */
struct WorldUpscale;
struct WorldUpscaleFrame {
    unsigned raw=0,stable=0,depth=0,object_motion=0; // depth = resolved (unjittered) depth
    int in_width=0,in_height=0,out_width=0,out_height=0;
    float view_projection[16]={};   // jittered, same as the temporal resolve
    double camera[3]={};
    float jitter_pixels[2]={};
    bool reset=false;
};
WorldUpscale *world_upscale_create(const char *vertex,const char *fragment);
/* Returns the display-resolution texture, or 0 (caller keeps its own path). */
unsigned world_upscale_resolve(WorldUpscale *,const WorldUpscaleFrame &);
unsigned long long world_upscale_frames(const WorldUpscale *);
void world_upscale_free(WorldUpscale *);
#endif
