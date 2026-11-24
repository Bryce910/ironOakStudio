#ifndef ENGINE_APP_BOOTSTRAP_H
#define ENGINE_APP_BOOTSTRAP_H

#include <stdbool.h>

#include <SDL2/SDL.h>

#include "engine/editor_ui.h"
#include "engine/input_runtime.h"

typedef struct{
    const char *title;
    int win_w;
    int win_h;
    int master_vol;
    int music_vol;
    int sfx_vol;
    EditorUIState *editor_ui;
    InputRuntimeState *input_runtime;
}AppBootstrapConfig;

typedef struct{
    SDL_Window *window;
    SDL_GLContext gl_context;
    int win_w;
    int win_h;
}AppBootstrapResult;

/* Display-synchronized presentation, or immediate when disabled in options. */
void app_bootstrap_apply_vsync(bool enabled);

bool app_bootstrap_run(const AppBootstrapConfig *config,AppBootstrapResult *result);

#endif
