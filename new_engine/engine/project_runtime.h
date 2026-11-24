#ifndef VALWOOD_PROJECT_RUNTIME_H
#define VALWOOD_PROJECT_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char project[1024], capture[1024], profile[1024], character[256], view[32], landmark[64], scenic[32];
    bool direct, editor, headless, no_hud, active, finished, benchmark, hidden, tour;
    int width, height, frames, rendered_frames, exit_code, warmup;
    unsigned seed;
    Uint64 started;
    /* Unspecified graphics values preserve the saved project settings. */
    char graphics_preset[16];
    int auto_quality; /* -1 unspecified, 0 off, 1 on */
    float render_scale; /* 0 unspecified; otherwise .70..1 */
    bool graphics_menu;
    int target_fps; /* 0 unspecified; otherwise 45, 60, 90 or 120 */
} EngineLaunch;

typedef struct {
    bool loaded, atlas_world;
    char name[128];
    float spawn_x, spawn_z, yaw, camera_yaw, camera_pitch, camera_distance, day_fraction;
} EngineProject;

extern EngineLaunch g_launch;
extern EngineProject g_project;

/* Returns 1 to launch, 0 after --help, and -1 for an invalid invocation. */
int engine_launch_parse(int argc, char **argv, EngineLaunch *launch);
bool engine_project_open(EngineLaunch *launch, EngineProject *project);
void engine_launch_frame(SDL_Window *window);
bool engine_project_import_glb(const char *source, char *destination, size_t capacity,
                               char *message, size_t message_capacity);

#ifdef __cplusplus
}
#endif
#endif
