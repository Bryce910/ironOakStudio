#ifndef VALWOOD_WORLD_WEATHER_H
#define VALWOOD_WORLD_WEATHER_H
#include <cstdint>

struct WorldWeather;
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
};
struct WorldWeatherStats {
    int width=0,height=0,half_width=0,half_height=0;
    std::uint64_t bytes=0,resizes=0,resolves=0,height_queries=0,cache_generations=0,cover_queries=0;
    unsigned rain_instances=0,splash_instances=0,terrain_rows_this_update=0;
    bool terrain_cached=false;
};
/* Owning render-thread GL3.3 context required. Compiles five world_weather_*
 * shaders; no size-dependent targets until active resolve. At1600x900 one
 * RGBA16F full result + one half fog field +65² RG32F heights total~13.77MiB.
 * No steady-state allocation or state readbacks. Existing scene depth and
 * indirect-radiance attachment are never modified. */
WorldWeather *world_weather_create(const char *shader_directory="shaders");
/* Call before resolve, outside scene drawing. At most8 rows×65 height queries;
 * 192m cache (3m texels) recenters on32m cells; only complete generations publish.
 * At most520 terrain and520 optional cover callbacks/update. Both rain0/fog0
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
 * extra cards are vertex-rejected past20m. Two fixed draws,38400 vertices max.
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
