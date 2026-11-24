#ifndef VALWOOD_WORLD_BLOOD_RENDER_H
#define VALWOOD_WORLD_BLOOD_RENDER_H

#include "engine/world_blood_effects.h"
#include "engine/world_combat_vfx_render.h"
#include <cstddef>
#include <cstdint>

struct WorldBloodRenderer;
struct WorldBloodFrame {
    WorldCombatVfxFrame target;
    // World-space direction towards the sun, linear incident/ambient colors.
    float sun[3]={0,1,0},sun_color[3]={1,1,1},ambient[3]={.25f,.25f,.25f};
    float scale=.48f; // source geometry retained; deliberate cinematic blood size
};
struct WorldBloodStats {
    unsigned loaded_variants=0,source_frames=0;
    std::uint64_t source_vertices=0,triangles=0,geometry_bytes=0,texture_bytes=0;
    double max_duration=0;
    std::uint64_t draws=0,mesh_draws=0,stain_draws=0,drawn_triangles=0;
    std::uint64_t animation_frames=0,frame_transitions=0,failures=0;
    std::uint64_t secondary_draws=0,finisher_draws=0;
    unsigned active_meshes=0,active_stains=0,peak_meshes=0,peak_stains=0,last_frame=0;
    std::uint64_t last_serial=0;
    bool provenance_loaded=false;
    float minimum_stain_fade=1;
    double oldest_stain_age=0;
    unsigned persistent_stains_drawn=0;
};

// Render-thread/current GL3.3 context only. Loads/certifies a bounded cooked bank
// once and warms its shaders. Failure supplies a short error and owns no objects.
WorldBloodRenderer *world_blood_create(const char *bank,char *error,std::size_t error_capacity);
world_blood_effects::Config world_blood_config(const WorldBloodRenderer *);
// Explicit loading/settings/render-target resize boundary. Transactionally
// prepares the private fluid depth buffer; same dimensions are a no-op.
bool world_blood_prepare(WorldBloodRenderer *,int width,int height);
// Samples source mesh animation at original timing, blends nonemissive fluid and
// surface stains into target.color, and samples only the separate opaque depth.
// No file I/O, decoding, shader compilation or GL storage allocation occurs here.
bool world_blood_draw(WorldBloodRenderer *,const world_blood_effects::System &,const WorldBloodFrame &);
WorldBloodStats world_blood_stats(const WorldBloodRenderer *);
void world_blood_free(WorldBloodRenderer *);

#endif
