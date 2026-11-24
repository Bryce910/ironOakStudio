#ifndef VALWOOD_WORLD_GRAPHICS_SETTINGS_H
#define VALWOOD_WORLD_GRAPHICS_SETTINGS_H
#include <cstddef>
#include <cstdint>

enum class WorldGraphicsPreset { Performance, Balanced, Cinematic, Custom };
enum class WorldDisplayMode { Windowed, Borderless };
enum class WorldFoliageDistance { Low, Medium, High, Ultra };
enum class WorldFoliageDensity { Low, Medium, High, Ultra };
enum class WorldCharacterTextures { Low, Medium, High, Ultra };
enum class WorldEnvironmentTextures { Low, Medium, High, Ultra };
enum class WorldWaterQuality { Low, Medium, High, Ultra };
enum class WorldEnvironmentEffects { Low, Medium, High, Ultra };

// Fixed budgets, independent of texture and water reflection quality.
struct WorldEnvironmentEffectsProfile {
    unsigned smoke_particles=48,ambient_particles=24;
    float smoke_distance=100,life_distance=35,shaft_strength=.75f;
    int fog_steps=4;
};

// Pure per-tier policy. The caller also applies the independent master
// reflections switch and min(saved reflection_scale, mirror_scale_cap).
struct WorldWaterQualityProfile {
    bool planar=false;
    float mirror_scale_cap=.25f;
    int ssr_steps=0;
    int ssr_refine_steps=0;
    bool filtered=false;
};

struct WorldGraphicsSettings {
    WorldGraphicsPreset preset=WorldGraphicsPreset::Balanced;
    bool auto_quality=true;
    int target_fps=60;
    float render_scale=1.f; // .70 through1; shared UI/controller quality ceiling.
    float min_render_scale=.70f; // .70 through render_scale.
    int shadow_side=2048;
    bool reflections=true;
    WorldWaterQuality water_quality=WorldWaterQuality::Medium;
    WorldEnvironmentEffects environment_effects=WorldEnvironmentEffects::Medium;
    bool contact_shadows=true;
    bool volumetric_fog=true;
    bool clouds=true;
    float reflection_scale=.25f;
    WorldDisplayMode display_mode=WorldDisplayMode::Windowed;
    int window_width=0,window_height=0; // 0/0 uses the launch size and leaves a live window unchanged.
    bool vsync=true;
    WorldFoliageDistance foliage_distance=WorldFoliageDistance::Medium;
    WorldFoliageDensity foliage_density=WorldFoliageDensity::High;
    WorldCharacterTextures character_textures=WorldCharacterTextures::Medium;
    WorldEnvironmentTextures environment_textures=WorldEnvironmentTextures::High;
    int frame_rate_limit=0; // 0 is uncapped; independent from adaptive target.
};

enum class WorldGraphicsSettingsLoadResult { Loaded, Missing, Rejected, IOError };
inline constexpr std::size_t world_graphics_settings_max_bytes=4096;

WorldGraphicsSettings world_graphics_settings_default();
const char *world_graphics_preset_name(WorldGraphicsPreset);
const char *world_water_quality_name(WorldWaterQuality);
const char *world_environment_effects_name(WorldEnvironmentEffects);
WorldEnvironmentEffectsProfile world_environment_effects_profile(WorldEnvironmentEffects);
WorldWaterQualityProfile world_water_quality_profile(WorldWaterQuality);
const char *world_character_textures_name(WorldCharacterTextures);
unsigned world_character_texture_edge(WorldCharacterTextures);
const char *world_environment_textures_name(WorldEnvironmentTextures);
unsigned world_environment_texture_edge(WorldEnvironmentTextures);
std::uint64_t world_environment_texture_budget_bytes(WorldEnvironmentTextures);
const char *world_foliage_distance_name(WorldFoliageDistance);
float world_foliage_distance_scale(WorldFoliageDistance);
const char *world_foliage_density_name(WorldFoliageDensity);
float world_foliage_density_scale(WorldFoliageDensity);
const char *world_graphics_frame_limit_name(int);
const char *world_graphics_shadow_quality_name(int shadow_side);
// The saved adaptive target stays intact; intentional caps and a known VSync
// refresh rate constrain the controller's actual goal. Unknown refresh is0.
int world_graphics_effective_target_fps(const WorldGraphicsSettings &,int refresh_hz=0);
bool world_graphics_settings_valid(const WorldGraphicsSettings &);
// Preserve display, VSync, frame-cap and automatic-quality/target preferences;
// replace visual settings and
// reset the minimum scale to .70. Custom only marks a valid model as Custom.
// Invalid input leaves the caller unchanged.
bool world_graphics_settings_apply_preset(WorldGraphicsSettings *,WorldGraphicsPreset);
// Version8 adds independent environment texture quality and its bounded memory
// policy: Low512/1024MiB, Medium1K/1280MiB, High2K/1536MiB, Ultra4K/2048MiB.
// New and versions1–7 profiles use High; presets preserve this restart-required
// texture choice. The limit affects scenery only, never characters or UI.
// Version7 adds independent environmental effects budgets; older profiles
// migrate to Medium without changing any existing display/visual choices.
// Version6 adds independent Low/Medium/High/Ultra water quality. Versions1–5
// migrate to High, retaining the prior24-step/6-refinement water policy. New
// profiles default to Medium. The master reflections switch remains separate.
// Version5 adds character texture quality (Low512/Medium1K/High2K/Ultra native4K).
// Ultra is explicit opt-in; presets never change the selected texture tier.
// Versions1–4 migrate to Medium. Texture changes require restarting the game.
// Version4 adds independent foliage amount and a frame-rate cap. Versions1–3
// retain the full prior foliage amount (Ultra) and no cap. Version3 adds
// foliage draw distance. Versions1/2 retain Medium distance.
// Version2 adds display mode, window size and VSync. Version1 files migrate
// with the previous window behavior and VSync enabled. Every version requires
// its serialized fields exactly once. Blank lines,
// leading/trailing whitespace and full-line # comments are permitted. Boolean
// values are0/1. Nonfinite/out-of-range/duplicate/unknown/incomplete data reject
// the entire file. Missing and all failures leave the caller unchanged.
// Pass project-root "graphics.settings" from the viewer; no hidden global path.
WorldGraphicsSettingsLoadResult world_graphics_settings_load(
    const char *path,WorldGraphicsSettings *,char *error=nullptr,std::size_t capacity=0);
// Validate before touching disk, then write/fsync a unique sibling temporary
// file and atomically rename it. Failed writes remove their temporary file.
bool world_graphics_settings_save(const char *path,const WorldGraphicsSettings &,
    char *error=nullptr,std::size_t capacity=0);
#endif
