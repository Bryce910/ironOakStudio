#ifndef VALWOOD_WORLD_TORCH_FLAMES_H
#define VALWOOD_WORLD_TORCH_FLAMES_H

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

constexpr unsigned WORLD_TORCH_FLAME_CAPACITY=256;

/* Flame BASE is the measured bowl/wick mouth, in absolute world metres.
 * width is the horizontal half-width, not a diameter. The complete effect
 * stays inside baseY..baseY+3.2*height and horizontal radius
 * 2*width+.12*height+min(length(wind_xz),3)*.65*height+.05m, including
 * eight rising embers and their filtered quads. No source geometry is loaded. */
struct WorldTorchFlameEmitter {
    double x=0,y=0,z=0;
    float height=.45f,width=.18f,intensity=1.f;
    std::uint32_t seed=0;
};

/* A periodic, continuous64-second signal shared with the actual PBR lights.
 * Three bounded smooth oscillators give [.82,1.18], without frame RNG or
 * camera dependence. Height uses1+.2*(flicker-1), at most3.6% variation.
 * The hashed seed phase is identical for flame staging and light evaluation. */
inline float world_torch_seed_phase(std::uint32_t seed) {
    seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    return float(double(seed)*(6.2831853071795864769/4294967296.));
}
inline float world_torch_flicker(double time,std::uint32_t seed) {
    if(!std::isfinite(time))return 1.f;
    const double phase=std::remainder(time,64.)*(6.2831853071795864769/64.);
    const double s=world_torch_seed_phase(seed);
    return float(1.+.10*std::sin(phase*79.+s)+.055*std::sin(phase*137.+s*1.713)+
        .025*std::sin(phase*211.+s*2.37));
}

struct WorldTorchFlameFrame {
    unsigned framebuffer=0;
    int width=0,height=0;
    double camera[3]={};
    /* Column-major VP transforms camera-relative world positions. Supply
     * the same jittered/unjittered matrix and basis as the destination pass. */
    float vp[16]={};
    float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
    float tan_half_fov=.5994296232f;
    double time=0;
    float fog[3]={},haze=0;
    bool clip_below=false;
    float clip_height=0;
    /* World-space wind in metres/second. Geometry clamps its magnitude to3;
     * zero preserves a calm upright flame. Shared for main/reflection views. */
    float wind_xz[2]={};
};
struct WorldTorchFlameStats {
    unsigned emitters=0,visible_emitters=0,flame_slices=0,sparks=0;
    std::uint64_t draws=0,instance_uploads=0;
    std::size_t gpu_buffer_bytes=0;
    std::uint64_t sprite_bytes=0;
    bool purchased_sprites=false;
};
struct WorldTorchFlames;

/* Owner-thread GL3.3 context. One tiny static-geometry-free VAO, one fixed
 * instance buffer and one program; optional immutable sprites, no owned render targets. */
WorldTorchFlames *world_torch_flames_create(const char *shader_directory="shaders");
/* Optional immutable startup sprite array: VFX1 RGBA8 premultiplied-sRGB,
 * 128x256x144 (72torch+72camp frames). Mips stay within independent frames.
 * Failed load retains the previous complete array. Never called in drawing. */
bool world_torch_flames_load_sprites(WorldTorchFlames *,const char *path);
/* Copies at most256 emitters into fixed storage. Invalid input is atomic and
 * leaves the previous set intact. Zero count clears the set; null is valid
 * only with zero count. No GL work, allocations or asset access. */
bool world_torch_flames_validate_emitters(const WorldTorchFlameEmitter *,std::size_t);
bool world_torch_flames_set_emitters(WorldTorchFlames *,const WorldTorchFlameEmitter *,std::size_t count);
enum class WorldTorchFlameFileStatus {Missing,Loaded,Invalid};
/* Optional source-scene data, parsed once at startup. Header is
 * VALWOOD_SOURCE_FLAMES 1, followed by
 * x y z height width intensity uint32Seed. Missing and invalid files leave
 * output unchanged; invalid files also return a diagnostic. No GL work.
 * The default reserves16 renderer slots for authored Studio overlays. */
WorldTorchFlameFileStatus world_torch_flames_read_file(const char *path,
    std::vector<WorldTorchFlameEmitter> &output,std::string &error,
    std::size_t maximum=WORLD_TORCH_FLAME_CAPACITY-16);
/* One instanced draw: four soft flame slices +eight tiny embers per visible
 * emitter. Depth tests against the target's opaque depth, never writes it.
 * Adds premultiplied HDR RGB and reduces temporal-stability alpha only in
 * attachment0; indirect-radiance attachment1 is never selected or written.
 * No per-frame heap allocation/readbacks, no shadow-caster path.
 *
 * Invalid frame returns false without GL work; empty/offscreen returns true
 * without GL work. Otherwise leaves target FBO/viewport/program/VAO bound,
 * draw buffer attachment0, depth test GL_LESS on and depth writes off,
 * cull/scissor/stencil/blend off, color mask alltrue, fill mode. Caller must
 * restore its MRT draw buffers/depth mask before subsequent opaque draws. */
bool world_torch_flames_draw(WorldTorchFlames *,const WorldTorchFlameFrame &);
WorldTorchFlameStats world_torch_flames_stats(const WorldTorchFlames *);
void world_torch_flames_preview_time(WorldTorchFlames *,double);
void world_torch_flames_free(WorldTorchFlames *);

#endif
