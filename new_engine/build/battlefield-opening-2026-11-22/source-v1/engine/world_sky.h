#ifndef VALWOOD_WORLD_SKY_H
#define VALWOOD_WORLD_SKY_H

#include <cstdint>
#include "engine/world_weather_authoring.h"
#include "engine/world_storm_lighting.h"

struct WorldSky;
constexpr unsigned WORLD_SKY_VIEW_COUNT=3;
constexpr unsigned WORLD_SKY_MAIN_VIEW=0,WORLD_SKY_WATER_VIEW=1,WORLD_SKY_PROBE_VIEW=2;
struct WorldSkyFrame {
    float right[3]={1,0,0},up[3]={0,1,0},forward[3]={0,0,-1};
    float eye[3]={},sun[3]={0,1,0},sun_color[3]={1,1,1};
    float fog[3]={.3f,.4f,.5f},ambient[3]={.2f,.25f,.3f};
    float sky_light[3]={.2f,.25f,.3f},ground_light[3]={.06f,.06f,.04f};
    float sky_zenith[3]={.04f,.12f,.26f};
    float aspect=16.f/9.f,tan_half_fov=.5994296f,cloud=.28f,time=0;
    float jitter_ndc[2]={};
    // Draw atmosphere/sun without cloud raymarching when false. The weather
    // coverage and illumination inputs remain unchanged for other renderers.
    bool volumetric_clouds=true;
    unsigned environment_texture=0;float environment_rotation=0,environment_intensity=1,environment_strength=0;
    bool sun_disc=true; // Local irradiance captures exclude the direct sun disk.
    // Visible panorama only: blend its lowest elevations into the same aerial
    // color used by distant terrain. Local irradiance captures leave this zero.
    float aerial_horizon=0;
    // Same immutable world-space packet as rain, fog and material receivers.
    world_weather_authoring::PackedFrame authored{};
    WorldStormLightFrame lightning{};
};
struct WorldSkyStats {
    std::uint64_t target_bytes=0,noise_bytes=0,target_allocations=0,target_resizes=0,draw_calls=0,noise_uploads=0;
    int width[WORLD_SKY_VIEW_COUNT]={},height[WORLD_SKY_VIEW_COUNT]={}; /* actual half-resolution target dimensions */
    unsigned view_steps=12,light_steps=2;
    std::uint64_t authored_draws=0,lightning_draws=0;
};

/* GL3.3 render-thread API. One shared deterministic64^3 RGBA8 noise volume
 * with mipmaps, and three independently cached half-resolution RGBA16F targets.
 * Channel0 is the main view, channel1 its water reflection, channel2 a loading-
 * time local probe. A128-square probe uses its own64-square32KiB cloud target,
 * preserving warmed main/water sizes while capturing all cubemap faces. Each
 * channel shares the same existing finite dimension limits; no per-frame
 * allocation or resize thrash when alternating unchanged view sizes. */
WorldSky *world_sky_create(const char *vertex_path="shaders/world_sky.vs.glsl",
                           const char *fragment_path="shaders/world_sky.fs.glsl");
/* Draws the sky BEFORE opaque geometry. destination_fbo must already exist and
 * must not be this module's private target. No CPU readback or GL state query.
 * Leaves destination bound for both read/draw, full destination viewport,
 * scissor/blend/cull/depth test disabled, depth writes false, RGBA writes true,
 * texture unit0 active. Caller restores its ordinary terrain raster state.
 * Returns false on invalid input/allocation failure; old targets remain owned.
 */
bool world_sky_draw(WorldSky *,const WorldSkyFrame &,unsigned destination_fbo,
                     int width,int height,unsigned view=0);
WorldSkyStats world_sky_stats(const WorldSky *);
void world_sky_free(WorldSky *);

#endif
