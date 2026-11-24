#ifndef VALWOOD_WORLD_DRAGON_BREATH_H
#define VALWOOD_WORLD_DRAGON_BREATH_H

#include <cstddef>
#include <cstdint>

constexpr unsigned WORLD_DRAGON_BREATH_EMITTER_CAPACITY=2;
constexpr unsigned WORLD_DRAGON_BREATH_PARTICLE_CAPACITY=224;
enum class WorldDragonBreathVariant : unsigned { Fire=0,Frost=1 };
struct WorldDragonBreathEmitter {
    double position[3]={};             // Measured mouth in absolute world metres.
    float direction[3]={0,0,-1};       // Finite unit vector, independent of camera.
    float strength=0,length=18,radius=1.8f;
    std::uint32_t seed=1;
    WorldDragonBreathVariant variant=WorldDragonBreathVariant::Fire;
};
struct WorldDragonBreathFrame {
    // Existing single-sample HDR color and opaque depth, same dimensions.
    // The renderer attaches ONLY color to its own FBO: no sampling feedback.
    unsigned color=0,depth=0;
    int width=0,height=0;
    float vp[16]={};                  // Matching jittered camera-relative VP.
    double camera[3]={},time=0;
    float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
    float near_plane=.4f,far_plane=15000,tan_half_fov=.5994296f;
    float haze=0,fog[3]={.25f,.32f,.36f},wind_xz[2]={};
};
struct WorldDragonBreathLight {
    double position[3]={};
    float color[3]={},strength=0,radius=0;
};
struct WorldDragonBreathStats {
    unsigned emitters=0,visible_emitters=0,flames=0,cores=0,smoke=0,embers=0;
    std::uint64_t draws=0,instance_uploads=0,failures=0;
    std::uint64_t warmups=0,warmup_draws=0,warmup_failures=0;
    std::size_t gpu_buffer_bytes=0;
    std::uint64_t sprite_bytes=0,smoke_texture_bytes=0;
    bool purchased_fire=false,purchased_smoke=false;
};
struct WorldDragonBreath;
WorldDragonBreath *world_dragon_breath_create(const char *shader_directory="shaders");
// Optional immutable startup art. Existing purchased_fire.vfx is VFX1 RGBA8
// premultiplied sRGB 128x256x144; purchased_smoke.vfx is128x128x1.
// Failed replacement retains the previous complete texture. No asset access
// occurs from set_emitters(), lights(), or draw().
bool world_dragon_breath_load_fire(WorldDragonBreath *,const char *path);
bool world_dragon_breath_load_smoke(WorldDragonBreath *,const char *path);
// Atomic validation; zero count clears. Strength is[0,1], length[2,36]m,
// radius[.2,4]m. At most2 emitters and224 total analytic quads.
bool world_dragon_breath_set_emitters(WorldDragonBreath *,const WorldDragonBreathEmitter *,std::size_t count);
// Loading boundaries only. Exercises BOTH palettes and both real blend/draw
// layouts against the supplied destination color/depth using synthetic visible
// emitters. Restores the complete authored emitter set and live draw counters;
// only warmup counters change. This intentionally writes the loading image,
// so call before its final clear/publication and the caller's loading fence.
// Pass loading=false to reject any accidental ready-frame preparation with
// no GL work. No targets, buffers, or textures are allocated by this helper.
bool world_dragon_breath_prepare(WorldDragonBreath *,const WorldDragonBreathFrame &,bool loading);
// Returns up to2 bounded visual fill lights using the same smooth signal as
// flame staging. No gameplay damage, automatic ignition, or shadow casters.
std::size_t world_dragon_breath_lights(const WorldDragonBreath *,double time,WorldDragonBreathLight *,std::size_t capacity);
// Call after opaque/foliage, before weather/TAA/bloom. Samples opaque depth for
// soft intersections; never changes that depth or the indirect attachment.
// 2 bounded instanced draws, fixed staging/VBO, no heap allocation, GPU resize,
// fullscreen pass, readback, simulation catch-up, or wall-clock dependence.
// Preserves draw FBO, viewport, program, VAO/array buffer, units0/1/2 bindings,
// active texture, blend/depth/cull/scissor/stencil/sRGB enables, blend functions,
// depth/color masks and polygon mode. Read FBO/draw-buffer routing untouched.
// Empty/offscreen/invalid frames perform no GL work. Invalid returns false.
bool world_dragon_breath_draw(WorldDragonBreath *,const WorldDragonBreathFrame &);
WorldDragonBreathStats world_dragon_breath_stats(const WorldDragonBreath *);
void world_dragon_breath_free(WorldDragonBreath *);

#endif
