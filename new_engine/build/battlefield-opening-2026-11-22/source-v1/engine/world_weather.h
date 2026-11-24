#ifndef VALWOOD_WORLD_WEATHER_H
#define VALWOOD_WORLD_WEATHER_H
#include <cstdint>
#include "engine/world_weather_authoring.h"
#include "engine/world_weather_wetness.h"

struct WorldWeather;
// Resolve locations once after linking (or use the viewer Program cache).
// Bind only while that program is current. No program switch or allocation.
struct WorldWeatherAuthoredUniforms {
    int authored_count=-1,area_count=-1,global=-1,bounds=-1,channels=-1;
};
WorldWeatherAuthoredUniforms world_weather_authored_uniforms(unsigned program);
bool world_weather_bind_authored(const WorldWeatherAuthoredUniforms &,
                                const world_weather_authoring::PackedFrame &);
struct WorldWeatherWetnessUniforms {int enabled=-1,count=-1,global=-1,bounds=-1,channels=-1;};
WorldWeatherWetnessUniforms world_weather_wetness_uniforms(unsigned program);
bool world_weather_bind_wetness(const WorldWeatherWetnessUniforms&,
                               const world_weather_wetness::Packet&);
enum class WorldRainMaterial : unsigned { Generic=0,Water=1,Stone=2,Mud=3 };
struct WorldRainContact {
    WorldRainMaterial material=WorldRainMaterial::Generic;
    // Actual leaf/canopy contact geometry only. Negative/nonfinite disables it;
    // a conservative roof cap must never be supplied as a visible leaf height.
    float foliage_height=-10000;
};
struct WorldWeatherDrip {
    double x=0,y=0,z=0;
    float ground_y=0,exposure=1;
    unsigned kind=0,seed=1; // 0 leaf, 1 roof/eave; stable source seed
};

struct WorldWeatherFrame {
    unsigned color=0,depth=0,shadow=0,macro_height=0;
    int width=0,height=0;
    float near_plane=.4f,far_plane=15000.f,tan_half_fov=.5994296f;
    float jitter_ndc[2]={};
    float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
    double camera[3]={};
    float sun[3]={0,1,0},sun_color[3]={1,.95f,.85f};
    float fog[3]={.25f,.32f,.36f},sky_light[3]={.18f,.23f,.28f},ground_light[3]={.04f,.05f,.04f};
    /* Column-major shadow VP transforms CAMERA-RELATIVE world positions.
     * Shadow texture is ordinary non-comparison normalized OpenGL depth. */
    float light_vp[16]={};
    float world_size[2]={100800.f,56708.f};
    float time=0,intensity=0,fog_strength=0,wind_xz[2]={};
    world_weather_authoring::PackedFrame authored{};
    world_weather_wetness::Packet wetness{};
    // An authored mist area must still respect the user's volumetric-fog option.
    bool fog_enabled=true;
    // Environment-effects tiers alter bounded volumetric integration quality,
    // not the authored weather or global exposure. Zero shafts keeps mist.
    float shaft_strength=1;
    unsigned fog_steps=8;
    /* Optional constant water plane inside a conservative ellipse: centerXZ,
     * radiiXZ. Positive radii enable it. Particle surfaces use the plane here;
     * scene depth hides its dry bank portion. Avoids coarse-cache float at a
     * narrow pool edge; fog still uses the full cached terrain/water support. */
    float pool_bounds[4]={},pool_level=0;
    /* Optional stable terrain/water-surface callback, used ONLY by update().
     * Return max(terrain, local pool surface) to place rain/splashes on water.
     * Null falls back to world_atlas_height. No callback in GL drawing. */
    float (*surface_height)(double x,double z,void *userdata)=nullptr;
    void *height_userdata=nullptr;
    /* Optional conservative roof/overhang cap, used ONLY by update(). Return
     * <= terrain outside cover; nonfinite values also mean no cover. Shares
     * the fixed local cache: R terrain/water, G max(surface,cover). Rain stops
     * at G; splashes remain on R and are suppressed under cover. These upper
     * bounds must never be interpreted as actual sloping roof geometry. */
    float (*cover_height)(double x,double z,void *userdata)=nullptr;
    void *cover_userdata=nullptr;
    // Optional material/true-leaf query; same bounded cache-build budget as
    // height/cover. No allocation/IO or query occurs in particle drawing.
    bool (*contact_surface)(double x,double z,WorldRainContact&,void *userdata)=nullptr;
    void *contact_userdata=nullptr;
    // Explicit real geometry/alpha-tested leaf/eave anchors, prepared on scene
    // commit. Pointer lifetime covers this frame's update+resolve only. Draw
    // copies <=64 entries into stack/uniform storage and retains no pointer.
    const WorldWeatherDrip *drips=nullptr;
    unsigned drip_count=0;
};
// Actual authored/legacy rain-or-fog eligibility; CPU-only, no GL calls.
bool world_weather_active(const WorldWeatherFrame &);
struct WorldWeatherStats {
    int width=0,height=0,half_width=0,half_height=0;
    std::uint64_t bytes=0,resizes=0,resolves=0,height_queries=0,cache_generations=0,cover_queries=0,contact_queries=0;
    unsigned rain_instances=0,splash_instances=0,drip_instances=0,terrain_rows_this_update=0;
    bool terrain_cached=false;
};
/* Owning render-thread GL3.3 context required. Compiles five world_weather_*
 * shaders; no size-dependent targets until active resolve. At1600x900 one
 * RGBA16F full result + one half fog field +65² RGBA32F support total~13.80MiB.
 * No steady-state allocation or state readbacks. Existing scene depth and
 * indirect-radiance attachment are never modified. */
WorldWeather *world_weather_create(const char *shader_directory="shaders");
/* Call before resolve, outside scene drawing. At most8 rows×65 height queries;
 * 192m cache (3m texels) recenters on32m cells; only complete generations publish.
 * At most520 terrain,520 optional cover and520 optional contact callbacks/update. Both rain0/fog0
 * is a no-op. Without cover, teleports use macro height until cache is ready;
 * with cover, particles wait for complete local cover data to avoid roof leaks. */
void world_weather_update(WorldWeather *,const WorldWeatherFrame &,unsigned row_budget=8);
/* Owner-thread terrain/roof commit; rebuild the bounded local cache even when
 * the callback and userdata addresses have not changed. No GL allocation. */
void world_weather_invalidate_surface(WorldWeather *);
/* Run AFTER contact/AO, BEFORE temporal resolve. Both rain0 AND fog0 returns
 * frame.color without GL work/target allocation. Fog is eight bounded steps
 * at half resolution; a four-tap depth-aware upsample composes into a distinct
 * HDR target. Up to4096 world-space wind-tilted drops and768 ground splashes
 * soft-test scene depth; surviving fragments set temporal-stability alpha0.
 * Splashes use a ground ring and two crossed close-range crown/droplet cards;
 * extra cards are vertex-rejected past20m. Two fixed draws,38400 vertices max;
 * <=64 optional real source drips add one draw/384 vertices under20m.
 * Fog preserves alpha. Does not restore incoming state: leaves result FBO,
 * program/VAO bound, full viewport, fill polygon mode, depth/blend/cull/scissor
 * disabled, color writes enabled, depth writes disabled. Invalid input/failed
 * allocation returns frame.color. Never resolve into any owned input texture.
 */
unsigned world_weather_resolve(WorldWeather *,const WorldWeatherFrame &);
WorldWeatherStats world_weather_stats(const WorldWeather *);
unsigned world_weather_fog_field(const WorldWeather *);
void world_weather_free(WorldWeather *);
#endif
