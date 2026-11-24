#ifndef VALWOOD_WORLD_GRAPHICS_SETTINGS_H
#define VALWOOD_WORLD_GRAPHICS_SETTINGS_H
#include <cstddef>

enum class WorldGraphicsPreset { Performance, Balanced, Cinematic, Custom };
enum class WorldDisplayMode { Windowed, Borderless };

struct WorldGraphicsSettings {
    WorldGraphicsPreset preset=WorldGraphicsPreset::Balanced;
    bool auto_quality=true;
    int target_fps=60;
    float render_scale=1.f; // .70 through1; shared UI/controller quality ceiling.
    float min_render_scale=.70f; // .70 through render_scale.
    int shadow_side=2048;
    bool reflections=true;
    bool contact_shadows=true;
    bool volumetric_fog=true;
    bool clouds=true;
    float reflection_scale=.5f;
    WorldDisplayMode display_mode=WorldDisplayMode::Windowed;
    int window_width=0,window_height=0; // 0/0 uses the launch size and leaves a live window unchanged.
    bool vsync=true;
};

enum class WorldGraphicsSettingsLoadResult { Loaded, Missing, Rejected, IOError };
inline constexpr std::size_t world_graphics_settings_max_bytes=4096;

WorldGraphicsSettings world_graphics_settings_default();
const char *world_graphics_preset_name(WorldGraphicsPreset);
bool world_graphics_settings_valid(const WorldGraphicsSettings &);
// Preserve automatic-quality/target preferences; replace visual settings and
// reset the minimum scale to .70. Custom only marks a valid model as Custom.
// Invalid input leaves the caller unchanged.
bool world_graphics_settings_apply_preset(WorldGraphicsSettings *,WorldGraphicsPreset);
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
