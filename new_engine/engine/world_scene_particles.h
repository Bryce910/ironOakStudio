#ifndef VALWOOD_WORLD_SCENE_PARTICLES_H
#define VALWOOD_WORLD_SCENE_PARTICLES_H
#include <cstddef>
#include <cstdint>

constexpr std::size_t WORLD_SCENE_PARTICLE_EMITTER_CAPACITY=48;
constexpr std::size_t WORLD_SCENE_PARTICLE_CAPACITY=512;
enum class WorldSceneParticleKind : unsigned { Smoke=0,Dust=1,Sparks=2,Wisps=3 };
struct WorldSceneParticleEmitter {
    double position[3]={};
    WorldSceneParticleKind kind=WorldSceneParticleKind::Smoke;
    // Entire sprite geometry stays inside radius in XZ and [birthY, ceilingY]
    // vertically. Author birth above a chimney mouth; no CPU roof collision.
    float radius=1,height=5,strength=1,warmth=0;
    std::uint32_t seed=1;
    double ceiling_y=1000000;
    float rate=1,lifetime=1,size=1,wind=1;
    float tint[3]={1,1,1},glow=0;
};
struct WorldSceneParticlesFrame {
    unsigned color=0,depth=0,shadow=0;
    int width=0,height=0;
    // Jittered camera-relative OpenGL VP, matching depth exactly. Original
    // opaque depth is sampled only; it is never attached to our destination.
    float vp[16]={},light_vp[16]={};
    double camera[3]={},time=0;
    float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
    float sun[3]={0,1,0},sun_color[3]={1,.95f,.85f};
    float sky_light[3]={.18f,.23f,.28f},ground_light[3]={.04f,.05f,.04f};
    float fog[3]={.25f,.32f,.36f};
    float near_plane=.4f,far_plane=15000,tan_half_fov=.5994296f,haze=0;
    float wind_xz[2]={};
    void (*local_wind)(void *,double,double,double,double,float[2])=nullptr;void *wind_userdata=nullptr;
};
struct WorldSceneParticlesStats {
    int width=0,height=0;
    unsigned emitters=0,visible_emitters=0,smoke_instances=0,dust_instances=0;
    std::uint64_t smoke_texture_bytes=0;
    bool purchased_smoke=false;
    std::uint64_t target_bytes=0,gpu_buffer_bytes=0,allocations=0,resolves=0;
    std::uint64_t draws=0,instance_uploads=0,failures=0;
};
struct WorldSceneParticles;
WorldSceneParticles *world_scene_particles_create(const char *shader_directory="shaders");
// Optional startup VFX1 RGBA8 128x128x1 mask; failed load retains old mask.
bool world_scene_particles_load_smoke(WorldSceneParticles *,const char *path);
// Atomic validation: invalid updates preserve the previously authored set.
bool world_scene_particles_validate_emitters(const WorldSceneParticleEmitter *,std::size_t);
bool world_scene_particles_set_emitters(WorldSceneParticles *,const WorldSceneParticleEmitter *,std::size_t);
// Optional loading-time allocation. Stable dimensions do no allocation/query;
// failed resize retains the previous complete target. Allocates only RGBA16F.
bool world_scene_particles_prepare(WorldSceneParticles *,int width,int height);
// Loading-only shader first use with a private visible emitter. Restores the
// authored emitter set and preview clock even if nothing authored is visible.
bool world_scene_particles_warm(WorldSceneParticles *,const WorldSceneParticlesFrame &);
// AFTER contact/weather and BEFORE TAA/bloom. Returns frame.color unchanged
// when inactive, outside view, invalid or failed. One fullscreen copy plus one
// back-to-front instanced draw; fixed512 staging, no hot-path heap allocation.
// Premultiplied RGB ONE/ONE_MINUS_SRC_ALPHA; written temporal metadata becomes
// zero. Untouched alpha passes through exactly. Indirect lighting is untouched.
// Leaves result FBO/program/VAO, viewport, unit0 active; depth writes/testing,
// blend/cull/scissor/stencil disabled, RGBA writes enabled, polygon mode fill.
unsigned world_scene_particles_resolve(WorldSceneParticles *,const WorldSceneParticlesFrame &);
WorldSceneParticlesStats world_scene_particles_stats(const WorldSceneParticles *);
// Editor-only simulation clock; negative restores the world clock.
void world_scene_particles_preview_time(WorldSceneParticles *,double);
void world_scene_particles_free(WorldSceneParticles *);

#endif
