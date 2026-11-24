#ifndef ENGINE_EDITOR_UI_IMGUI_H
#define ENGINE_EDITOR_UI_IMGUI_H

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "engine/editor_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EditorFrameDebuggerPanelData{
    bool  enabled;
    bool  toon_shading;
    bool  gpu_timers_supported;
    float fps;
    float frame_ms;
    float frame_history[180];
    int   frame_history_count;
    int   frame_history_head;
    float update_ms;
    float stream_upload_ms;
    float update_actor_ms;
    float update_fx_ms;
    float update_particles_ms;
    float update_world_ms;
    float shadow_ms;
    float shadow_gpu_ms;
    float reflection_ms;
    float reflection_gpu_ms;
    float terrain_ms;
    float terrain_gpu_ms;
    float props_ms;
    float props_gpu_ms;
    float grass_ms;
    float grass_gpu_ms;
    float forest_ms;
    float forest_gpu_ms;
    float characters_ms;
    float characters_gpu_ms;
    float birds_ms;
    float birds_gpu_ms;
    float animals_ms;
    float animals_gpu_ms;
    float actors_ms;
    float actors_gpu_ms;
    float particles_ms;
    float particles_gpu_ms;
    float weather_ms;
    float weather_gpu_ms;
    float postfx_ms;
    float postfx_gpu_ms;
    float render_total_ms;
    float render_total_gpu_ms;
    int   terrain_tris;
    int   prop_count;
    int   active_props;
    int   stream_uploads;
    int   forest_count;
    int   particle_count;
    int   emitter_count;
    int   point_light_count;
} EditorFrameDebuggerPanelData;

bool editor_ui_imgui_init(void *window,void *gl_context);
void editor_ui_imgui_shutdown(void);
bool editor_ui_imgui_process_event(const SDL_Event *event);
void editor_ui_imgui_begin_frame(void);
void editor_ui_imgui_begin_passive_frame(float seconds,int width,int height);
void editor_ui_imgui_render(void);
bool editor_ui_imgui_wants_mouse(void);
bool editor_ui_imgui_wants_keyboard(void);
void editor_ui_imgui_draw_frame_debugger(EditorFrameDebuggerPanelData *data);
void editor_ui_imgui_draw_world_forge_shell(const EditorWorldForgeShellState *state,
                                            int win_w,int win_h);

#ifdef __cplusplus
}
#endif

#endif
